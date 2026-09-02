use std::collections::HashMap;
use std::ffi::{c_char, c_int, c_void};
use std::ptr;
use std::sync::{Arc, Condvar, Mutex, OnceLock};
use std::thread;
use std::time::{Duration, Instant};

// ── Result codes ─────────────────────────────────────────────────────────────

const NOTIFLY_SUCCESS: i32 = 0;
const NOTIFLY_OBSERVER_NOT_FOUND: i32 = -1;
const NOTIFLY_NOTIFICATION_NOT_FOUND: i32 = -2;
const NOTIFLY_NO_MORE_OBSERVER_IDS: i32 = -4;
const NOTIFLY_TIMEOUT: i32 = -5;
const NOTIFLY_INVALID_HANDLE: i32 = -6;

/// Result codes matching the C++ notifly_result enum.
#[repr(C)]
#[allow(non_camel_case_types, dead_code)]
pub enum notifly_result_t {
    NOTIFLY_SUCCESS = 0,
    NOTIFLY_OBSERVER_NOT_FOUND = -1,
    NOTIFLY_NOTIFICATION_NOT_FOUND = -2,
    NOTIFLY_PAYLOAD_TYPE_NOT_MATCH = -3,
    NOTIFLY_NO_MORE_OBSERVER_IDS = -4,
    NOTIFLY_TIMEOUT = -5,
    NOTIFLY_INVALID_HANDLE = -6,
}

/// What an exchange handler decides about one delivery.
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
#[allow(non_camel_case_types)]
pub enum notifly_verdict_t {
    /// Not what is being waited for. Stay subscribed, record nothing.
    NOTIFLY_VERDICT_SKIP = 0,
    /// Part of a streamed reply. Record it and keep waiting.
    NOTIFLY_VERDICT_KEEP = 1,
    /// This delivery completes the exchange.
    NOTIFLY_VERDICT_DONE = 2,
}

// ── Internal callback type ────────────────────────────────────────────────────
//
// Raw pointers (*mut c_void) are !Send + !Sync, so we smuggle them as usize
// (which is Send+Sync) across thread boundaries. Casting back is safe because
// the caller is responsible for pointer lifetime (same as in the C++ backend).

type InternalCallback = Arc<dyn Fn(usize) + Send + Sync>;

struct ObserverEntry {
    id: i32,
    callback: InternalCallback,
}

struct NotiflyCore {
    observers: HashMap<i32, Vec<ObserverEntry>>,
    observer_location: HashMap<i32, i32>,
    next_id: i32,
    recycled_ids: Vec<i32>,
}

impl NotiflyCore {
    fn new() -> Self {
        Self {
            observers: HashMap::new(),
            observer_location: HashMap::new(),
            next_id: 1,
            recycled_ids: Vec::new(),
        }
    }

    fn next_observer_id(&mut self) -> Option<i32> {
        if let Some(id) = self.recycled_ids.pop() {
            return Some(id);
        }
        if self.next_id == i32::MAX {
            return None;
        }
        let id = self.next_id;
        self.next_id += 1;
        Some(id)
    }
}

struct NotiflyInner {
    core: Mutex<NotiflyCore>,
    // Async dispatches still in flight, keyed by the observer they run for --
    // the same bookkeeping notifly's C++ backend keeps in m_async_tasks, so
    // removing an observer can wait for work that already cloned its callback.
    pending_tasks: Mutex<HashMap<i32, Vec<thread::JoinHandle<()>>>>,
}

// Join dispatches that are already in flight. Skips the thread we are running
// on: a callback that removes its own observer would otherwise deadlock on
// itself, so its task is left to finish on its own instead.
fn join_tasks(handles: Vec<thread::JoinHandle<()>>) {
    let current = thread::current().id();
    for handle in handles {
        if handle.thread().id() != current {
            let _ = handle.join();
        }
    }
}

// ── Public opaque handle type ─────────────────────────────────────────────────

/// Opaque notifly instance (exposed to C as a pointer via notifly_handle).
#[allow(non_camel_case_types)]
pub struct notifly_instance {
    inner: Arc<NotiflyInner>,
}

impl notifly_instance {
    fn new() -> Self {
        Self {
            inner: Arc::new(NotiflyInner {
                core: Mutex::new(NotiflyCore::new()),
                pending_tasks: Mutex::new(HashMap::new()),
            }),
        }
    }

    fn add_observer_internal(&self, notification_id: i32, callback: InternalCallback) -> i32 {
        let mut core = self.inner.core.lock().unwrap();
        match core.next_observer_id() {
            None => NOTIFLY_NO_MORE_OBSERVER_IDS,
            Some(id) => {
                core.observers
                    .entry(notification_id)
                    .or_default()
                    .push(ObserverEntry { id, callback });
                core.observer_location.insert(id, notification_id);
                id
            }
        }
    }

    // Take the given observers' in-flight dispatches out of the pending map and
    // join them with no lock held, so a callback can still re-enter the centre
    // while it finishes.
    fn wait_for_observer_tasks(&self, observer_ids: &[i32]) {
        let handles: Vec<thread::JoinHandle<()>> = {
            let mut tasks = self.inner.pending_tasks.lock().unwrap();
            observer_ids
                .iter()
                .filter_map(|id| tasks.remove(id))
                .flatten()
                .collect()
        };
        join_tasks(handles);
    }

    fn remove_observer(&self, observer_id: i32) -> i32 {
        {
            let mut core = self.inner.core.lock().unwrap();
            match core.observer_location.remove(&observer_id) {
                None => return NOTIFLY_OBSERVER_NOT_FOUND,
                Some(notification_id) => {
                    if let Some(vec) = core.observers.get_mut(&notification_id) {
                        vec.retain(|e| e.id != observer_id);
                        if vec.is_empty() {
                            core.observers.remove(&notification_id);
                        }
                    }
                }
            }
        }

        // A dispatch that already cloned this callback may still be running. The
        // caller is free to release its user_data once removal returns (and an
        // exchange's Drop runs right after this), so wait for that work here
        // rather than let it dereference freed memory.
        self.wait_for_observer_tasks(&[observer_id]);

        self.inner.core.lock().unwrap().recycled_ids.push(observer_id);
        NOTIFLY_SUCCESS
    }

    fn remove_all_observers(&self, notification_id: i32) -> i32 {
        let observer_ids: Vec<i32> = {
            let mut core = self.inner.core.lock().unwrap();
            match core.observers.remove(&notification_id) {
                None => return 0,
                Some(observers) => {
                    let ids: Vec<i32> = observers.iter().map(|e| e.id).collect();
                    for id in &ids {
                        core.observer_location.remove(id);
                    }
                    ids
                }
            }
        };

        // Same contract as remove_observer(): no id is recycled, and nothing is
        // reported removed, while a dispatch for it is still in flight.
        self.wait_for_observer_tasks(&observer_ids);

        let mut core = self.inner.core.lock().unwrap();
        for id in &observer_ids {
            core.recycled_ids.push(*id);
        }
        observer_ids.len() as i32
    }

    // Snapshot callbacks without holding the lock so callbacks can re-enter.
    fn snapshot_observers(&self, notification_id: i32) -> Option<Vec<(i32, InternalCallback)>> {
        let core = self.inner.core.lock().unwrap();
        core.observers.get(&notification_id).map(|list| {
            list.iter().map(|e| (e.id, Arc::clone(&e.callback))).collect()
        })
    }

    fn post_notification_sync(&self, notification_id: i32, data: *mut c_void) -> i32 {
        let observers = match self.snapshot_observers(notification_id) {
            None => return NOTIFLY_NOTIFICATION_NOT_FOUND,
            Some(obs) => obs,
        };
        let count = observers.len() as i32;
        let data_usize = data as usize;
        for (_, cb) in observers {
            cb(data_usize);
        }
        count
    }

    fn post_notification_async_impl(&self, notification_id: i32, data: *mut c_void) -> i32 {
        let observers = match self.snapshot_observers(notification_id) {
            None => return NOTIFLY_NOTIFICATION_NOT_FOUND,
            Some(obs) => obs,
        };
        if observers.is_empty() {
            return NOTIFLY_NOTIFICATION_NOT_FOUND;
        }
        let count = observers.len() as i32;
        let data_usize = data as usize;
        let mut tasks = self.inner.pending_tasks.lock().unwrap();
        for (id, cb) in observers {
            // thread::Builder rather than thread::spawn: this runs behind an
            // extern "C" export, where the panic spawn() raises when the OS is
            // out of threads cannot unwind to the caller and would abort the
            // process. Report it the way the C++ backend reports its caught
            // thread-creation failure instead.
            let handle = match thread::Builder::new().spawn(move || cb(data_usize)) {
                Ok(handle) => handle,
                Err(_) => return NOTIFLY_INVALID_HANDLE,
            };
            let entry = tasks.entry(id).or_default();
            entry.retain(|h| !h.is_finished());
            entry.push(handle);
        }
        count
    }

    fn post_and_wait_impl(
        &self,
        post_notification_id: i32,
        wait_notification_id: i32,
        timeout_ms: i32,
        post_data: *mut c_void,
        response_data: *mut *mut c_void,
    ) -> i32 {
        // Condvar pair: None = waiting, Some(usize) = response data pointer as usize.
        let pair: Arc<(Mutex<Option<usize>>, Condvar)> =
            Arc::new((Mutex::new(None), Condvar::new()));
        let pair_clone = Arc::clone(&pair);

        // Temporary observer on wait_notification_id signals the condvar.
        let temp_cb: InternalCallback = Arc::new(move |data_usize: usize| {
            let (lock, cvar) = &*pair_clone;
            let mut guard = lock.lock().unwrap();
            *guard = Some(data_usize);
            cvar.notify_all();
        });
        let temp_id = self.add_observer_internal(wait_notification_id, temp_cb);
        if temp_id < 0 {
            return temp_id;
        }

        // Post request synchronously. Callbacks run with the core lock released,
        // so the responder can call back into this instance without deadlocking.
        let post_result = self.post_notification_sync(post_notification_id, post_data);
        if post_result < 0 {
            self.remove_observer(temp_id);
            return post_result;
        }
        if post_result == 0 {
            self.remove_observer(temp_id);
            return NOTIFLY_NOTIFICATION_NOT_FOUND;
        }

        // Wait for response. wait_timeout_while handles both spurious wakeups and
        // the case where the response already arrived synchronously (guard is Some).
        let (lock, cvar) = &*pair;
        let guard = lock.lock().unwrap();
        let (guard, timeout_result) = cvar
            .wait_timeout_while(guard, Duration::from_millis(timeout_ms.max(0) as u64), |g| {
                g.is_none()
            })
            .unwrap();

        self.remove_observer(temp_id);

        if timeout_result.timed_out() && guard.is_none() {
            unsafe { *response_data = ptr::null_mut() };
            NOTIFLY_TIMEOUT
        } else {
            let ptr = guard.map(|u| u as *mut c_void).unwrap_or(ptr::null_mut());
            unsafe { *response_data = ptr };
            NOTIFLY_SUCCESS
        }
    }
}

impl Drop for notifly_instance {
    fn drop(&mut self) {
        let handles: Vec<thread::JoinHandle<()>> = {
            let mut tasks = self.inner.pending_tasks.lock().unwrap();
            tasks.drain().flat_map(|(_, handles)| handles).collect()
        };
        join_tasks(handles);
    }
}

// ── Exchange ─────────────────────────────────────────────────────────────────
//
// A scoped set of subscriptions a thread can block on -- native port of
// notifly::exchange (include/notifly.h). Where notifly_post_and_wait() covers
// one request/reply shape, an exchange covers the rest: waiting on several
// notifications at once and learning which one answered, ignoring deliveries
// that are not the one being waited for, collecting a reply streamed in
// pieces, posting nothing at all, or treating silence as success.

type ExchangeHandler = unsafe extern "C" fn(c_int, *mut c_void, *mut c_void) -> notifly_verdict_t;

struct ExchangeState {
    /// The notification that completed the exchange, or -1 if none has yet.
    fired: i32,
    accepted: usize,
    last_accept: Option<Instant>,
}

/// Opaque exchange handle (exposed to C as a pointer via notifly_exchange_handle).
#[allow(non_camel_case_types)]
pub struct notifly_exchange {
    // The notifly_instance this exchange subscribes against, smuggled as usize --
    // same idiom the rest of this file uses for *mut c_void payloads. The caller
    // must keep it alive for as long as the exchange (see notifly_exchange_create).
    center: usize,
    observers: Mutex<Vec<i32>>,
    status: Mutex<c_int>,
    state: Mutex<ExchangeState>,
    cv: Condvar,
}

impl notifly_exchange {
    fn new(center: *mut notifly_instance) -> Self {
        Self {
            center: center as usize,
            observers: Mutex::new(Vec::new()),
            status: Mutex::new(NOTIFLY_SUCCESS),
            state: Mutex::new(ExchangeState {
                fired: -1,
                accepted: 0,
                last_accept: None,
            }),
            cv: Condvar::new(),
        }
    }

    fn center(&self) -> &notifly_instance {
        // SAFETY: caller-managed lifetime, same contract as every other handle
        // in this file (see notifly_exchange_create's Safety doc).
        unsafe { &*(self.center as *const notifly_instance) }
    }

    /// Subscribe to a notification, letting `handler` judge each delivery.
    /// Returns the exchange's cumulative status right after the call.
    fn on(&self, notification_id: c_int, handler: ExchangeHandler, user_data: *mut c_void) -> c_int {
        // The callback captures this exchange's own address rather than an Arc,
        // mirroring notifly::exchange::on()'s C++ lambda, which captures `this` by
        // raw pointer -- same contract: the exchange must outlive any dispatch
        // that might still be in flight against it.
        let self_addr = self as *const notifly_exchange as usize;
        let user_data_addr = user_data as usize;
        let cb: InternalCallback = Arc::new(move |data_usize: usize| {
            let ex = unsafe { &*(self_addr as *const notifly_exchange) };
            ex.offer(notification_id, data_usize, handler, user_data_addr);
        });

        let id = self.center().add_observer_internal(notification_id, cb);
        if id > 0 {
            self.observers.lock().unwrap().push(id);
        } else {
            let mut status = self.status.lock().unwrap();
            if *status == NOTIFLY_SUCCESS {
                *status = id;
            }
        }

        *self.status.lock().unwrap()
    }

    /// Offer one delivery to its handler and record the verdict. Runs under the
    /// exchange's own lock so "the first done wins" is atomic against a second
    /// delivery arriving on another thread.
    fn offer(&self, notification_id: c_int, data_usize: usize, handler: ExchangeHandler, user_data_addr: usize) {
        {
            let mut state = self.state.lock().unwrap();

            // Already complete: leave whatever the winning handler stored alone.
            if state.fired >= 0 {
                return;
            }

            let data = data_usize as *mut c_void;
            let user_data = user_data_addr as *mut c_void;
            let verdict = unsafe { handler(notification_id, data, user_data) };
            if verdict == notifly_verdict_t::NOTIFLY_VERDICT_SKIP {
                return;
            }

            state.accepted += 1;
            state.last_accept = Some(Instant::now());
            if verdict == notifly_verdict_t::NOTIFLY_VERDICT_DONE {
                state.fired = notification_id;
            }
        }
        // Wakes wait() on done, and drain() on either verdict so it can
        // re-measure the quiet window.
        self.cv.notify_all();
    }

    /// Block until a handler returns done, or the timeout expires. Returns the
    /// notification that completed the exchange, or -1 on timeout.
    fn wait(&self, timeout_ms: c_int) -> c_int {
        let state = self.state.lock().unwrap();
        let (state, _) = self
            .cv
            .wait_timeout_while(state, Duration::from_millis(timeout_ms.max(0) as u64), |s| {
                s.fired < 0
            })
            .unwrap();
        state.fired
    }

    /// Block for the whole window and report whether nothing arrived.
    fn silent_for(&self, window_ms: c_int) -> bool {
        self.wait(window_ms) < 0
    }

    /// Block while a reply is streamed in pieces, until the sender goes quiet.
    /// Returns how many deliveries were accepted (keep or done).
    fn drain(&self, quiet_ms: c_int, deadline_ms: c_int) -> usize {
        let quiet = Duration::from_millis(quiet_ms.max(0) as u64);
        let deadline = Instant::now() + Duration::from_millis(deadline_ms.max(0) as u64);
        let mut state = self.state.lock().unwrap();

        loop {
            if state.fired >= 0 {
                break;
            }
            let now = Instant::now();
            if now >= deadline {
                break;
            }

            // Wake at whichever comes first: the quiet window closing on the
            // last delivery, or the deadline.
            let mut wake = deadline;
            if let Some(last) = state.last_accept {
                if state.accepted > 0 {
                    let quiet_at = last + quiet;
                    if quiet_at <= now {
                        break;
                    }
                    if quiet_at < wake {
                        wake = quiet_at;
                    }
                }
            }

            let (s, _) = self.cv.wait_timeout(state, wake.saturating_duration_since(now)).unwrap();
            state = s;
        }

        state.accepted
    }

    /// The first subscription error, or NOTIFLY_SUCCESS if every on() call took.
    fn status(&self) -> c_int {
        *self.status.lock().unwrap()
    }

    /// The notification that completed the exchange, or -1 if none has.
    fn fired(&self) -> c_int {
        self.state.lock().unwrap().fired
    }

    /// How many deliveries have been accepted so far.
    fn accepted(&self) -> usize {
        self.state.lock().unwrap().accepted
    }
}

impl Drop for notifly_exchange {
    fn drop(&mut self) {
        let observers = self.observers.lock().unwrap();
        for &id in observers.iter() {
            self.center().remove_observer(id);
        }
    }
}

/// Handler notifly_exchange_capture() registers internally: stores the
/// delivered payload into the *mut *mut c_void passed through as user_data,
/// and finishes -- the same relationship exchange::capture() has to on() in
/// the C++ backend.
unsafe extern "C" fn capture_handler(
    _notification_id: c_int,
    data: *mut c_void,
    user_data: *mut c_void,
) -> notifly_verdict_t {
    let out = user_data as *mut *mut c_void;
    if !out.is_null() {
        *out = data;
    }
    notifly_verdict_t::NOTIFLY_VERDICT_DONE
}

// ── Global default instance ───────────────────────────────────────────────────

static DEFAULT_INSTANCE: OnceLock<Mutex<Option<Box<notifly_instance>>>> = OnceLock::new();

fn default_mutex() -> &'static Mutex<Option<Box<notifly_instance>>> {
    DEFAULT_INSTANCE.get_or_init(|| Mutex::new(None))
}

// ── FFI exports ───────────────────────────────────────────────────────────────

/// Create a new notifly instance.
#[no_mangle]
pub extern "C" fn notifly_create() -> *mut notifly_instance {
    Box::into_raw(Box::new(notifly_instance::new()))
}

/// Destroy a notifly instance created with notifly_create.
///
/// # Safety
/// `handle` must be a valid pointer obtained from `notifly_create`.
#[no_mangle]
pub unsafe extern "C" fn notifly_destroy(handle: *mut notifly_instance) {
    if !handle.is_null() {
        drop(Box::from_raw(handle));
    }
}

/// Get (or lazily create) the process-wide default instance.
#[no_mangle]
pub extern "C" fn notifly_default() -> *mut notifly_instance {
    let mutex = default_mutex();
    let mut guard = mutex.lock().unwrap();
    if guard.is_none() {
        *guard = Some(Box::new(notifly_instance::new()));
    }
    guard.as_mut().unwrap().as_mut() as *mut notifly_instance
}

/// Release the default instance (call before process exit or between tests).
#[no_mangle]
pub extern "C" fn notifly_cleanup_default() {
    if let Some(mutex) = DEFAULT_INSTANCE.get() {
        let mut guard = mutex.lock().unwrap();
        *guard = None;
    }
}

/// Add an observer callback for a notification.
/// Returns observer_id > 0 on success, or a negative error code.
///
/// # Safety
/// `handle` must be a valid pointer. `user_data` lifetime is managed by the caller.
#[no_mangle]
pub unsafe extern "C" fn notifly_add_observer(
    handle: *mut notifly_instance,
    notification_id: c_int,
    callback: Option<unsafe extern "C" fn(c_int, *mut c_void, *mut c_void)>,
    user_data: *mut c_void,
) -> c_int {
    if handle.is_null() || callback.is_none() {
        return NOTIFLY_INVALID_HANDLE;
    }
    let instance = &*handle;
    let cb = callback.unwrap();
    // Cast to usize to cross Send+Sync boundaries safely.
    let user_data_usize = user_data as usize;
    let cb_fn: InternalCallback = Arc::new(move |data_usize: usize| {
        let data = data_usize as *mut c_void;
        let ud = user_data_usize as *mut c_void;
        unsafe { cb(notification_id, data, ud) };
    });
    instance.add_observer_internal(notification_id, cb_fn)
}

/// Remove an observer by ID.
///
/// # Safety
/// `handle` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_remove_observer(
    handle: *mut notifly_instance,
    observer_id: c_int,
) -> c_int {
    if handle.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*handle).remove_observer(observer_id)
}

/// Remove all observers for a given notification. Returns count removed.
///
/// # Safety
/// `handle` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_remove_all_observers(
    handle: *mut notifly_instance,
    notification_id: c_int,
) -> c_int {
    if handle.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*handle).remove_all_observers(notification_id)
}

/// Post a notification synchronously. Returns count of observers notified.
///
/// # Safety
/// `handle` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_post_notification(
    handle: *mut notifly_instance,
    notification_id: c_int,
    data: *mut c_void,
) -> c_int {
    if handle.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*handle).post_notification_sync(notification_id, data)
}

/// Post a notification asynchronously. Returns count of observers notified.
///
/// # Safety
/// `handle` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_post_notification_async(
    handle: *mut notifly_instance,
    notification_id: c_int,
    data: *mut c_void,
) -> c_int {
    if handle.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*handle).post_notification_async_impl(notification_id, data)
}

/// Post a notification and wait for a response notification (request/response).
///
/// # Safety
/// `handle` and `response_data` must be valid pointers.
#[no_mangle]
pub unsafe extern "C" fn notifly_post_and_wait(
    handle: *mut notifly_instance,
    post_notification_id: c_int,
    wait_notification_id: c_int,
    timeout_ms: c_int,
    post_data: *mut c_void,
    response_data: *mut *mut c_void,
) -> c_int {
    if handle.is_null() || response_data.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*handle).post_and_wait_impl(
        post_notification_id,
        wait_notification_id,
        timeout_ms,
        post_data,
        response_data,
    )
}

/// Create an exchange bound to the given centre.
///
/// # Safety
/// `handle` must be a valid pointer that outlives the returned exchange.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_create(handle: *mut notifly_instance) -> *mut notifly_exchange {
    if handle.is_null() {
        return ptr::null_mut();
    }
    Box::into_raw(Box::new(notifly_exchange::new(handle)))
}

/// Unsubscribe every handler and destroy the exchange.
///
/// # Safety
/// `exchange` must be a valid pointer obtained from `notifly_exchange_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_destroy(exchange: *mut notifly_exchange) {
    if !exchange.is_null() {
        drop(Box::from_raw(exchange));
    }
}

/// Subscribe to a notification, letting `handler` judge each delivery. Returns
/// notifly_exchange_status() after the call.
///
/// # Safety
/// `exchange` must be a valid pointer. `user_data` lifetime is managed by the caller.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_on(
    exchange: *mut notifly_exchange,
    notification_id: c_int,
    // Written out instead of Option<ExchangeHandler>: cbindgen only collapses
    // Option<fn pointer> into a plain nullable C function pointer when the fn
    // type is spelled out here, not referenced through a type alias (see
    // notifly_add_observer's `callback` parameter, same reasoning).
    handler: Option<unsafe extern "C" fn(c_int, *mut c_void, *mut c_void) -> notifly_verdict_t>,
    user_data: *mut c_void,
) -> c_int {
    if exchange.is_null() || handler.is_none() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*exchange).on(notification_id, handler.unwrap(), user_data)
}

/// Subscribe to a notification and store its first delivery's payload into
/// `*out_data` (set to null until then). Same return convention as
/// `notifly_exchange_on`.
///
/// # Safety
/// `exchange` and `out_data` must be valid pointers.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_capture(
    exchange: *mut notifly_exchange,
    notification_id: c_int,
    out_data: *mut *mut c_void,
) -> c_int {
    if exchange.is_null() || out_data.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    *out_data = ptr::null_mut();
    (*exchange).on(notification_id, capture_handler, out_data as *mut c_void)
}

/// Block until a handler returns done, or the timeout expires. Returns the
/// notification that fired, or -1 on timeout.
///
/// # Safety
/// `exchange` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_wait(exchange: *mut notifly_exchange, timeout_ms: c_int) -> c_int {
    if exchange.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*exchange).wait(timeout_ms)
}

/// Block for the whole window and report whether nothing arrived (1) or a
/// handler completed the exchange within it (0).
///
/// # Safety
/// `exchange` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_silent_for(exchange: *mut notifly_exchange, window_ms: c_int) -> c_int {
    if exchange.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    if (*exchange).silent_for(window_ms) {
        1
    } else {
        0
    }
}

/// Block while a reply is streamed in pieces, until quiet_ms of silence
/// follows the last accepted delivery, deadline_ms elapses, or a handler
/// completes the exchange. Returns how many deliveries were accepted.
///
/// # Safety
/// `exchange` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_drain(
    exchange: *mut notifly_exchange,
    quiet_ms: c_int,
    deadline_ms: c_int,
) -> c_int {
    if exchange.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*exchange).drain(quiet_ms, deadline_ms) as c_int
}

/// The first subscription error, or NOTIFLY_SUCCESS if every on()/capture() call took.
///
/// # Safety
/// `exchange` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_status(exchange: *mut notifly_exchange) -> c_int {
    if exchange.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*exchange).status()
}

/// The notification that completed the exchange, or -1 if none has yet.
///
/// # Safety
/// `exchange` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_fired(exchange: *mut notifly_exchange) -> c_int {
    if exchange.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*exchange).fired()
}

/// How many deliveries have been accepted so far.
///
/// # Safety
/// `exchange` must be a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn notifly_exchange_accepted(exchange: *mut notifly_exchange) -> c_int {
    if exchange.is_null() {
        return NOTIFLY_INVALID_HANDLE;
    }
    (*exchange).accepted() as c_int
}

/// Convert a result code to a human-readable C string.
#[no_mangle]
pub extern "C" fn notifly_result_to_string(result: c_int) -> *const c_char {
    match result {
        NOTIFLY_SUCCESS => c"Success".as_ptr(),
        NOTIFLY_OBSERVER_NOT_FOUND => c"Observer not found".as_ptr(),
        NOTIFLY_NOTIFICATION_NOT_FOUND => c"Notification not found".as_ptr(),
        -3 => c"Payload type mismatch".as_ptr(),
        NOTIFLY_NO_MORE_OBSERVER_IDS => c"No more observer IDs available".as_ptr(),
        NOTIFLY_TIMEOUT => c"Timeout".as_ptr(),
        NOTIFLY_INVALID_HANDLE => c"Invalid handle".as_ptr(),
        _ => c"Unknown error".as_ptr(),
    }
}

/// Convert an exchange verdict to a human-readable C string.
#[no_mangle]
pub extern "C" fn notifly_verdict_to_string(verdict: c_int) -> *const c_char {
    match verdict {
        0 => c"Skip".as_ptr(),
        1 => c"Keep".as_ptr(),
        2 => c"Done".as_ptr(),
        _ => c"Unknown".as_ptr(),
    }
}

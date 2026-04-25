use std::collections::HashMap;
use std::ffi::{c_char, c_int, c_void};
use std::ptr;
use std::sync::{Arc, Condvar, Mutex, OnceLock};
use std::thread;
use std::time::Duration;

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
    pending_tasks: Mutex<Vec<thread::JoinHandle<()>>>,
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
                pending_tasks: Mutex::new(Vec::new()),
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

    fn remove_observer(&self, observer_id: i32) -> i32 {
        let mut core = self.inner.core.lock().unwrap();
        match core.observer_location.remove(&observer_id) {
            None => NOTIFLY_OBSERVER_NOT_FOUND,
            Some(notification_id) => {
                if let Some(vec) = core.observers.get_mut(&notification_id) {
                    vec.retain(|e| e.id != observer_id);
                    if vec.is_empty() {
                        core.observers.remove(&notification_id);
                    }
                }
                core.recycled_ids.push(observer_id);
                NOTIFLY_SUCCESS
            }
        }
    }

    fn remove_all_observers(&self, notification_id: i32) -> i32 {
        let mut core = self.inner.core.lock().unwrap();
        match core.observers.remove(&notification_id) {
            None => 0,
            Some(observers) => {
                let count = observers.len() as i32;
                for entry in &observers {
                    core.observer_location.remove(&entry.id);
                    core.recycled_ids.push(entry.id);
                }
                count
            }
        }
    }

    // Snapshot callbacks without holding the lock so callbacks can re-enter.
    fn snapshot_callbacks(&self, notification_id: i32) -> Option<Vec<InternalCallback>> {
        let core = self.inner.core.lock().unwrap();
        core.observers.get(&notification_id).map(|list| {
            list.iter().map(|e| Arc::clone(&e.callback)).collect()
        })
    }

    fn post_notification_sync(&self, notification_id: i32, data: *mut c_void) -> i32 {
        let callbacks = match self.snapshot_callbacks(notification_id) {
            None => return NOTIFLY_NOTIFICATION_NOT_FOUND,
            Some(cbs) => cbs,
        };
        let count = callbacks.len() as i32;
        let data_usize = data as usize;
        for cb in callbacks {
            cb(data_usize);
        }
        count
    }

    fn post_notification_async_impl(&self, notification_id: i32, data: *mut c_void) -> i32 {
        let callbacks = match self.snapshot_callbacks(notification_id) {
            None => return NOTIFLY_NOTIFICATION_NOT_FOUND,
            Some(cbs) => cbs,
        };
        if callbacks.is_empty() {
            return NOTIFLY_NOTIFICATION_NOT_FOUND;
        }
        let count = callbacks.len() as i32;
        let data_usize = data as usize;
        let handle = thread::spawn(move || {
            for cb in callbacks {
                cb(data_usize);
            }
        });
        {
            let mut tasks = self.inner.pending_tasks.lock().unwrap();
            tasks.push(handle);
            tasks.retain(|h| !h.is_finished());
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
        let mut tasks = self.inner.pending_tasks.lock().unwrap();
        for handle in tasks.drain(..) {
            let _ = handle.join();
        }
    }
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

/*
 *  notifly.h
 *  notifly
 *
 *  Originally created by Jonathan Goodman on 11/23/13.
 *  Copyright (c) 2019 Salvatore Rivieccio. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <unordered_map>
#include <sstream>
#include <functional>
#include <list>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <any>
#include <typeindex>
#include <thread>
#include <stack>
#include <memory>
#include <vector>
#include <future>
#include <atomic>
#include <ranges>

// Windows.h defines min and max as macros, which conflicts with std::min and std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "notifly_version.h"

/**
 * @brief   This enum class defines the possible notifly_result that can occur when using the notification center.
 */
enum class notifly_result
{
    success =                    0,
    observer_not_found =        -1,
    notification_not_found =    -2,
    payload_type_not_match =    -3,
    no_more_observer_ids =      -4,
    timeout =                   -5
};

/**
 * @brief   What a notifly::exchange handler decides about one delivery.
 *
 * A handler sees every delivery of the notification it subscribed to and says
 * what it is: something to ignore, one of several pieces of a streamed reply,
 * or the one that ends the wait.
 */
enum class notifly_verdict
{
    skip,   ///< Not what is being waited for. Stay subscribed, record nothing.
    keep,   ///< Part of a streamed reply. Record it and keep waiting.
    done    ///< This delivery completes the exchange.
};

namespace notifly_detail
{
    /**
     * @brief   Helper trait to detect tuple types.
     */
    template<typename>
    struct is_tuple : std::false_type {};

    template<typename... Args>
    struct is_tuple<std::tuple<Args...>> : std::true_type {};
}

/**
 * @brief   This class is an observer that is used to observe notifications.
 */
class notification_observer
{
public:
    /**
     * @brief   Constructor. This constructor initializes the observer with a unique identifier.
     */
    explicit notification_observer(const int a_id, const int a_notification, std::string a_types) :
            m_callback(nullptr),
            m_id(a_id),
            m_types(std::move(a_types)),
            m_notification(a_notification)
    {}

    /**
     * @brief   Get the observer id.
     */
    [[nodiscard]] int get_id() const { return m_id; }

    /**
     * @brief   Get the types of the arguments for the callback function.
     */
    [[nodiscard]] const std::string& get_types() const { return m_types; }

    // Callback function to be invoked when a notification is posted
    std::function<std::any(std::any)> m_callback;

private:
    int m_id;
    std::string m_types;
    int m_notification;
};

/**
 * @brief   This class is a notification center that allows you to post notifications to a set of observers.
 */
class notifly
{
public:
    /**
     * @brief   Constructor.
     */
    notifly() = default;

    /**
     * @brief   Destructor. Makes sure all async tasks are completed.
     */
    ~notifly()
    {
        wait_for_all_async_tasks();
    }

    /**
     * @brief                   This method adds a function callback as an observer to a named notification.
     * @param   a_notification  The name of the notification you wish to observe.
     * @param   a_method        The function callback.
     * @return                  The observer id > 0 if successful or an error code
     */
    template<typename Callable>
    int add_observer(int a_notification, Callable a_method)
    {
        return add_observer(a_notification, std::function(std::move(a_method)));
    }

    /**
     * @brief                   This method adds a function callback as an observer to a named notification.
     * @param   a_notification  The name of the notification you wish to observe.
     * @param   a_method        The function callback.
     * @return                  The observer id > 0 if successful or an error code
     */
    template<typename Return, typename ...Args>
    int add_observer(const int a_notification, std::function<Return(Args ...)> a_method)
    {
        // Generate a unique string for the types signature
        const std::string types = generate_type_signature<Args...>();

        // Lock for thread safety
        std::lock_guard lock(m_mutex);

        // Check for type compatibility if notification already exists
        if(m_observers.contains(a_notification))
        {
            if (const auto& observer_list = m_observers.at(a_notification).observers;
                !observer_list.empty() && observer_list.front().get_types() != types)
            {
                return static_cast<int>(notifly_result::payload_type_not_match);
            }
        }

        // Get a unique ID for the observer
        const int id = get_unique_id();
        if(id == -1) return static_cast<int>(notifly_result::no_more_observer_ids);

        // Create an observer
        notification_observer observer(id, a_notification, types);

        // Create lambda callback that handles the type conversion
        observer.m_callback = [a_method](const std::any& any) -> std::any
        {
            auto message = std::any_cast<std::tuple<Args...>>(any);

            if constexpr (std::is_same_v<Return, void>)
            {
                std::apply(a_method, message);
                return {};
            }
            else
            {
                return std::apply(a_method, message);
            }
        };

        // Add observer to appropriate lists
        auto& obs = m_observers[a_notification].observers;
        obs.push_back(std::move(observer));

        // Store reference to the observer for quick lookup by ID
        m_observer_lookup[id] =
        {
            a_notification,
            --obs.end()
        };

        return id;
    }

    /**
     * @brief               This method removes an observer by id.
     * @param a_observer    The observer id you wish to remove.
     * @return              0 if successful or an error code.
     */
    int remove_observer(const int a_observer)
    {
        std::lock_guard lock(m_mutex);

        // Check if observer exists
        if(!m_observer_lookup.contains(a_observer))
            return static_cast<int>(notifly_result::observer_not_found);

        // Wait for any async tasks related to this observer to complete
        wait_for_observer_tasks(a_observer);

        // Get observer info and remove it
        const auto& [notification_id, observer_iter] = m_observer_lookup[a_observer];

        if(const auto it = m_observers.find(notification_id); it != m_observers.end())
        {
            // Remove the observer from its list
            it->second.observers.erase(observer_iter);

            // If notification has no more observers, remove it completely
            if(it->second.observers.empty())
            {
                m_observers.erase(it);
            }
        }

        // Cleanup
        m_observer_lookup.erase(a_observer);
        release_id(a_observer);

        return static_cast<int>(notifly_result::success);
    }

    /**
     * @brief                   This method removes all observers from a given notification.
     * @param   a_notification  The notification you wish to remove observers from.
     * @return                  The number of observers removed or an error code.
     */
    int remove_all_observers(auto a_notification)
    {
        std::lock_guard lock(m_mutex);

        const auto it = m_observers.find(a_notification);
        if(it == m_observers.end())
            return 0;

        // Get number of observers for return value
        const auto count = it->second.observers.size();

        // Wait for any async tasks related to this notification to complete
        wait_for_notification_tasks(a_notification);

        // Release all observer IDs
        for(const auto& observer: it->second.observers)
        {
            m_observer_lookup.erase(observer.get_id());
            release_id(observer.get_id());
        }

        // Remove the notification entry
        m_observers.erase(it);

        return count;
    }

    /**
     * @brief                   Post a notification to observers synchronously.
     * @param a_notification    The notification you wish to post.
     * @param args              The payload associated with the notification.
     * @return                  Number of observers notified or an error code.
     */
    template<typename ...Args>
    int post_notification(auto a_notification, Args... args)
    {
        return post_notification_impl(a_notification, false, std::forward<Args>(args)...);
    }

    /**
     * @brief                   Post a notification to observers asynchronously.
     * @param a_notification    The notification you wish to post.
     * @param args              The payload associated with the notification.
     * @return                  Number of observers notified or an error code.
     */
    template<typename ...Args>
    int post_notification_async(auto a_notification, Args... args)
    {
        return post_notification_impl(a_notification, true, std::forward<Args>(args)...);
    }

    /**
     * @brief                       Post a notification and wait for a response notification with timeout.
     * @param a_post_notification   The notification you wish to post.
     * @param a_wait_notification   The notification you wish to wait for.
     * @param a_timeout_ms          Timeout in milliseconds.
     * @param a_result              Output parameter to store the result.
     * @param post_args             The payload associated with the post notification.
     * @return                      notifly_result::success if response received, notifly_result::timeout if timeout occurred, or other error code.
     */
    template<typename ResultTuple, typename ...PostArgs>
    notifly_result post_and_wait(
        int a_post_notification,
        int a_wait_notification,
        const int a_timeout_ms,
        ResultTuple& a_result,
        PostArgs... post_args)
    {
        // Subscribing before posting is what makes this safe: a reply that comes
        // back before the post call has even returned is still caught.
        exchange ex(*this);
        ex.capture(a_wait_notification, a_result);
        if (ex.status() != notifly_result::success) return ex.status();

        const int post_result = post_notification(a_post_notification, std::forward<PostArgs>(post_args)...);

        if (post_result < 0) return static_cast<notifly_result>(post_result);
        if (post_result == 0) return notifly_result::notification_not_found;

        return ex.wait(std::chrono::milliseconds(a_timeout_ms)) < 0
            ? notifly_result::timeout
            : notifly_result::success;
    }

    /**
     * @brief   A scoped set of subscriptions that a thread can block on.
     *
     * Where post_and_wait() covers the common shape -- post one notification,
     * wait for one reply, take the first that arrives -- an exchange covers the
     * rest: waiting on several notifications at once and learning which one
     * answered, ignoring deliveries that are not the one being waited for,
     * collecting a reply the sender streams in pieces, posting nothing at all,
     * or treating silence as the successful outcome.
     *
     * Subscribe with on(), then post whatever the reply is expected to answer,
     * then block on wait(), drain() or silent_for(). The destructor
     * unsubscribes, and remove_observer() waits for a dispatch already in
     * flight, so a handler may safely refer to the caller's own locals.
     *
     * Once a handler returns notifly_verdict::done the exchange is complete and
     * later deliveries are ignored, so a sender that repeats itself -- or
     * answers on two of the subscribed notifications -- cannot disturb what the
     * winning handler stored.
     *
     * @warning Never destroy an exchange, or call remove_observer(), from
     *          inside a handler: dispatch runs with the notification centre
     *          locked, and unsubscribing there would wait on the very dispatch
     *          that is running.
     */
    class exchange
    {
    public:
        explicit exchange(notifly& a_center) : m_center(a_center) {}

        ~exchange()
        {
            for (const int observer: m_observers) m_center.remove_observer(observer);
        }

        exchange(const exchange&) = delete;
        exchange& operator=(const exchange&) = delete;

        /**
         * @brief                   Subscribe to a notification, letting a handler judge each delivery.
         * @param a_notification    The notification to observe.
         * @param a_handler         Callable (Args...) -> notifly_verdict.
         * @return                  This exchange, so subscriptions can be chained.
         */
        template<typename ...Args, typename Handler>
        exchange& on(const int a_notification, Handler a_handler)
        {
            const int id = m_center.add_observer(a_notification,
                std::function<void(Args...)>(
                    [this, a_notification, a_handler](Args... args)
                    {
                        offer(a_notification, [&] { return a_handler(args...); });
                    }));

            if (id > 0) m_observers.push_back(id);
            else if (m_status == notifly_result::success) m_status = static_cast<notifly_result>(id);

            return *this;
        }

        /**
         * @brief                   Subscribe to a notification, accepting its first delivery and reading nothing out of it.
         * @param a_notification    The notification to observe.
         * @return                  This exchange, so subscriptions can be chained.
         */
        template<typename ...Args>
        exchange& on(const int a_notification)
        {
            return on<Args...>(a_notification, [](Args...) { return notifly_verdict::done; });
        }

        /**
         * @brief                   Subscribe to a notification and store its first delivery into a tuple.
         * @param a_notification    The notification to observe.
         * @param a_out             Where to store the payload.
         * @return                  This exchange, so subscriptions can be chained.
         */
        template<typename ...Args>
        exchange& capture(const int a_notification, std::tuple<Args...>& a_out)
        {
            return on<Args...>(a_notification, [&a_out](Args... args)
            {
                a_out = std::make_tuple(args...);
                return notifly_verdict::done;
            });
        }

        /**
         * @brief                   Subscribe to a notification and store its first delivery into a single value.
         * @param a_notification    The notification to observe.
         * @param a_out             Where to store the payload.
         * @return                  This exchange, so subscriptions can be chained.
         */
        template<typename T, std::enable_if_t<!notifly_detail::is_tuple<T>::value, int> = 0>
        exchange& capture(const int a_notification, T& a_out)
        {
            return on<T>(a_notification, [&a_out](T a_value)
            {
                a_out = a_value;
                return notifly_verdict::done;
            });
        }

        /**
         * @brief               Block until a handler returns done, or the timeout expires.
         * @param a_timeout     How long to wait.
         * @return              The notification that completed the exchange, or -1 on timeout.
         */
        [[nodiscard]] int wait(const std::chrono::milliseconds a_timeout)
        {
            std::unique_lock lock(m_mutex);
            if(!m_cv.wait_for(lock, a_timeout, [this] { return m_fired >= 0; })) return -1;
            return m_fired;
        }

        /**
         * @brief               Block for the whole window and report whether nothing arrived.
         *
         * For protocols where the sender only answers when a command fails, so
         * silence is the successful outcome.
         *
         * @param a_window      How long the sender is given to object.
         * @return              True if no handler returned done within the window.
         */
        [[nodiscard]] bool silent_for(const std::chrono::milliseconds a_window)
        {
            return wait(a_window) < 0;
        }

        /**
         * @brief               Block while a reply is streamed in pieces, until the sender goes quiet.
         *
         * Ends as soon as a handler returns done, or once nothing has been
         * accepted for a_quiet, or at a_deadline -- whichever comes first. For
         * replies whose length the protocol never states.
         *
         * @param a_quiet       Idle time that marks the end of the stream.
         * @param a_deadline    Upper bound on the whole wait.
         * @return              How many deliveries were accepted (keep or done).
         */
        [[nodiscard]] std::size_t drain(const std::chrono::milliseconds a_quiet,
                                        const std::chrono::milliseconds a_deadline)
        {
            const auto deadline = std::chrono::steady_clock::now() + a_deadline;
            std::unique_lock lock(m_mutex);

            while(m_fired < 0)
            {
                const auto now = std::chrono::steady_clock::now();
                if(now >= deadline) break;
                if(m_accepted > 0 && now - m_last_accept >= a_quiet) break;

                // Wake at whichever comes first: the quiet window closing on the
                // last delivery, or the deadline.
                auto wake = deadline;
                if(m_accepted > 0)
                {
                    if(const auto quiet_at = m_last_accept + a_quiet; quiet_at < wake) wake = quiet_at;
                }
                m_cv.wait_until(lock, wake);
            }

            return m_accepted;
        }

        /**
         * @brief   The first subscription error, or success if every on() call took.
         */
        [[nodiscard]] notifly_result status() const { return m_status; }

        /**
         * @brief   The notification that completed the exchange, or -1 if none has.
         */
        [[nodiscard]] int fired()
        {
            std::lock_guard lock(m_mutex);
            return m_fired;
        }

        /**
         * @brief   How many deliveries have been accepted so far.
         */
        [[nodiscard]] std::size_t accepted()
        {
            std::lock_guard lock(m_mutex);
            return m_accepted;
        }

    private:
        /**
         * @brief   Offer one delivery to its handler and record the verdict.
         *
         * The handler runs under the exchange's own lock so that "the first done
         * wins" is atomic against a second delivery arriving on another thread.
         */
        void offer(const int a_notification, const std::function<notifly_verdict()>& a_evaluate)
        {
            {
                std::lock_guard lock(m_mutex);

                // Already complete: leave whatever the winning handler stored alone.
                if(m_fired >= 0) return;

                const notifly_verdict verdict = a_evaluate();
                if(verdict == notifly_verdict::skip) return;

                ++m_accepted;
                m_last_accept = std::chrono::steady_clock::now();

                if(verdict == notifly_verdict::done) m_fired = a_notification;
            }
            // Wakes wait() on done, and drain() on either verdict so it can
            // re-measure the quiet window.
            m_cv.notify_all();
        }

        notifly& m_center;
        std::vector<int> m_observers;
        notifly_result m_status = notifly_result::success;

        std::mutex m_mutex;
        std::condition_variable m_cv;
        int m_fired = -1;
        std::size_t m_accepted = 0;
        std::chrono::steady_clock::time_point m_last_accept{};
    };

    /**
     * @brief   Get the default global notification center.
     */
    static notifly& default_notifly()
    {
        static notifly instance;
        return instance;
    }

    /**
     * @brief   Get the number of pending async tasks (useful for testing).
     */
    size_t pending_async_task_count() const
    {
        std::lock_guard lock(m_tasks_mutex);
        size_t count = 0;
        for (const auto &tasks: m_async_tasks | std::views::values)
            count += tasks.size();
        return count;
    }

private:
    // Structure to group observer data for a notification
    struct NotificationData
    {
        std::list<notification_observer> observers;
    };

    // Structure to store observer location info for quick lookup
    struct ObserverLocation
    {
        int notification_id{};
        std::list<notification_observer>::iterator iterator;
    };

    // Structure to track an async task and its completion status
    struct AsyncTask
    {
        std::shared_ptr<std::jthread> thread;
        std::shared_ptr<std::atomic<bool>> completed;
    };

    // Helper method to post a notification
    template<typename ...Args>
    int post_notification_impl(auto a_notification, const bool a_async, Args... args)
    {
        // Generate type signature for validation
        std::string types = generate_type_signature<Args...>();

        // Create payload tuple
        auto payload = std::make_any<std::tuple<Args...>>(std::make_tuple(args...));

        // Collect the synchronous callbacks under the lock, then run them without it.
        //
        // Invoking them here, while m_mutex is held, makes one observer's work everybody's
        // problem: nothing else can post a notification until it returns. A caller that posts
        // from several independent sources -- separate devices sharing one notification centre,
        // say -- then has them serialise on this mutex, and a handler that blocks stalls sources
        // it knows nothing about. Worse, a blocking call that waits for a reply delivered through
        // this same centre cannot be satisfied while another observer is running, so it waits out
        // its whole timeout for no reason.
        //
        // The callbacks are copied, so removing an observer while its callback is in flight does
        // not pull the ground out from under it.
        std::vector<std::function<std::any(std::any)>> sync_callbacks;
        std::size_t observer_count = 0;
        {
            std::lock_guard lock(m_mutex);

            auto it = m_observers.find(a_notification);
            if(it == m_observers.end())
                return static_cast<int>(notifly_result::notification_not_found);

            const auto& observer_list = it->second.observers;
            if(observer_list.empty() || observer_list.front().get_types() != types)
                return static_cast<int>(notifly_result::payload_type_not_match);

            observer_count = observer_list.size();

            for(const auto& observer : observer_list)
            {
                if(a_async)
                {
                    auto completed_flag = std::make_shared<std::atomic<bool>>(false);
                    auto task_thread = std::make_shared<std::jthread>(
                        [callback = observer.m_callback, p = payload, completed_flag]()
                        {
                            callback(p);
                            completed_flag->store(true, std::memory_order_release);
                        });
                    std::lock_guard task_lock(m_tasks_mutex);
                    auto& tasks = m_async_tasks[observer.get_id()];
                    // Clean up completed tasks to prevent unbounded growth
                    std::erase_if(tasks, [](const AsyncTask& t) {
                        return t.completed->load(std::memory_order_acquire);
                    });
                    tasks.push_back({task_thread, completed_flag});
                }
                else
                {
                    sync_callbacks.push_back(observer.m_callback);
                }
            }
        }

        for(auto& callback : sync_callbacks)
            callback(payload);

        return static_cast<int>(observer_count);
    }

    // Helper method to wait for async tasks related to a specific observer
    void wait_for_observer_tasks(const int observer_id)
    {
        std::lock_guard task_lock(m_tasks_mutex);
        if (const auto it = m_async_tasks.find(observer_id); it != m_async_tasks.end())
        {
            for (const auto&[thread, completed] : it->second)
            {
                if (thread && thread->joinable())
                    thread->join();
            }
            m_async_tasks.erase(it);
        }
    }

    // Helper method to wait for async tasks related to a specific notification
    void wait_for_notification_tasks(const int notification_id)
    {
        std::vector<int> observer_ids_to_wait;

        // First gather all observer IDs for this notification
        for (const auto& [id, location] : m_observer_lookup)
        {
            if (location.notification_id == notification_id)
            {
                observer_ids_to_wait.push_back(id);
            }
        }

        // Now wait for each observer's tasks
        for (const int observer_id : observer_ids_to_wait)
        {
            wait_for_observer_tasks(observer_id);
        }
    }

    // Wait for all pending async tasks to complete
    void wait_for_all_async_tasks()
    {
        std::lock_guard lock(m_tasks_mutex);
        for (auto &tasks: m_async_tasks | std::views::values)
        {
            for (const auto&[thread, completed] : tasks)
            {
                if (thread && thread->joinable())
                    thread->join();
            }
        }
        m_async_tasks.clear();
    }

    // Helper method to generate type signature
    template <typename... Args>
    std::string generate_type_signature() const
    {
        std::string signature;
        (void)std::initializer_list<int>{(signature += get_type_string<Args>(), 0)...};
        return signature;
    }

    // Get string representation of a type
    template <typename T>
    std::string get_type_string() const
    {
        const std::string name = std::type_index(typeid(T)).name();
        if (std::is_lvalue_reference_v<T>)
            return "ref:" + name;
        if (std::is_rvalue_reference_v<T>)
            return "rval:" + name;
        return "val:" + name;
    }

    // ID management methods
    int get_unique_id()
    {
        if (!m_released_ids.empty())
        {
            const int id = m_released_ids.top();
            m_released_ids.pop();
            return id;
        }
        if(m_next_id == std::numeric_limits<int>::max())
            return -1;

        return m_next_id++;
    }

    void release_id(const int id)
    {
        m_released_ids.push(id);
    }

    // Data members
    std::unordered_map<int, NotificationData> m_observers;
    std::unordered_map<int, ObserverLocation> m_observer_lookup;

    // Async tasks management
    std::unordered_map<int, std::vector<AsyncTask>> m_async_tasks;
    mutable std::mutex m_tasks_mutex;

    // ID management
    std::stack<int> m_released_ids;
    int m_next_id = 1;

    // Thread safety
    mutable std::recursive_mutex m_mutex;

    // Default notification center instance
    static std::shared_ptr<notifly> m_default_center;
};

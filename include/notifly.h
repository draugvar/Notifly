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
#include <any>
#include <typeindex>
#include <thread>
#include <stack>
#include <memory>
#include <vector>
#include <future>
#include <ranges>

// Windows.h defines min and max as macros, which conflicts with std::min and std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define NOTIFLY_VERSION_MAJOR 3
#define NOTIFLY_VERSION_MINOR 4
#define NOTIFLY_VERSION_PATCH 0

#define NOTIFLY_VERSION (NOTIFLY_VERSION_MAJOR << 16 | NOTIFLY_VERSION_MINOR << 8 | NOTIFLY_VERSION_PATCH)

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
        auto&[observers] = m_observers[a_notification];
        observers.push_back(observer);

        // Store reference to the observer for quick lookup by ID
        m_observer_lookup[id] =
        {
            a_notification,
            --observers.end()
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
        int a_timeout_ms,
        ResultTuple& a_result,
        PostArgs... post_args)
    {
        // Create promise/future for synchronization
        auto response_promise = std::make_shared<std::promise<ResultTuple>>();
        std::future<ResultTuple> response_future = response_promise->get_future();

        // Register temporary observer for the response
        int observer_id = add_response_observer(a_wait_notification, response_promise);

        if (observer_id < 0) return static_cast<notifly_result>(observer_id);

        // Post the request
        int post_result = post_notification(a_post_notification, std::forward<PostArgs>(post_args)...);

        if (post_result < 0)
        {
            remove_observer(observer_id);
            return static_cast<notifly_result>(post_result);
        }

        if (post_result == 0)
        {
            remove_observer(observer_id);
            return notifly_result::notification_not_found;
        }

        // Wait for response with timeout
        auto status = response_future.wait_for(std::chrono::milliseconds(a_timeout_ms));

        // Remove the temporary observer
        remove_observer(observer_id);

        if (status == std::future_status::timeout) return notifly_result::timeout;

        a_result = response_future.get();
        return notifly_result::success;
    }

    /**
     * @brief   Get the default global notification center.
     */
    static notifly& default_notifly()
    {
        static notifly instance;
        return instance;
    }

private:
    /**
     * @brief Helper trait to detect tuple types
     */
    template<typename>
    struct is_tuple : std::false_type {};

    template<typename... Args>
    struct is_tuple<std::tuple<Args...>> : std::true_type {};

    /**
     * @brief Add observer for response - specialization for tuple types
     */
    template<typename... Args>
    int add_response_observer(const int a_notification, std::shared_ptr<std::promise<std::tuple<Args...>>> promise)
    {
        // Tuple response
        return add_observer(a_notification, [promise](Args... args)
        {
            promise->set_value(std::make_tuple(args...));
        });
    }

    /**
     * @brief Add observer for response - for single types using SFINAE
     */
    template<typename T>
    std::enable_if_t<!is_tuple<T>::value, int>
    add_response_observer(const int a_notification, std::shared_ptr<std::promise<T>> promise)
    {
        // Single value response
        return add_observer(a_notification, [promise](T value)
        {
            promise->set_value(value);
        });
    }

    // Structure to group observer data for a notification
    struct NotificationData {
        std::list<notification_observer> observers;
    };

    // Structure to store observer location info for quick lookup
    struct ObserverLocation {
        int notification_id{};
        std::list<notification_observer>::iterator iterator;
    };

    // Helper method to post a notification
    template<typename ...Args>
    int post_notification_impl(auto a_notification, const bool a_async, Args... args)
    {
        // Generate type signature for validation
        std::string types = generate_type_signature<Args...>();

        // Create payload tuple
        auto payload = std::make_any<std::tuple<Args...>>(std::make_tuple(args...));

        // Check notification and type compatibility
        std::lock_guard lock(m_mutex);

        auto it = m_observers.find(a_notification);
        if(it == m_observers.end())
            return static_cast<int>(notifly_result::notification_not_found);

        const auto& observer_list = it->second.observers;
        if(observer_list.empty() || observer_list.front().get_types() != types)
            return static_cast<int>(notifly_result::payload_type_not_match);

        // Notify all observers
        for(const auto& observer : observer_list)
        {
            if(a_async)
            {
                auto task_thread = std::make_shared<std::jthread>([callback = observer.m_callback, p = payload]
                {
                    callback(p);
                });

                // Store the thread
                std::lock_guard task_lock(m_tasks_mutex);
                m_async_tasks[observer.get_id()].push_back(task_thread);
            }
            else
            {
                observer.m_callback(payload);
            }
        }
        return static_cast<int>(observer_list.size());
    }

    // Helper method to wait for async tasks related to a specific observer
    void wait_for_observer_tasks(const int observer_id)
    {
        std::lock_guard task_lock(m_tasks_mutex);
        if (const auto it = m_async_tasks.find(observer_id); it != m_async_tasks.end())
        {
            for (const auto& task : it->second)
            {
#ifdef __APPLE__
                if (task && task->joinable())
                    task->join();
#endif
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
            for (const auto& task : tasks)
            {
                if (task && task->joinable()) task->join();
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
    std::unordered_map<int, std::vector<std::shared_ptr<std::jthread>>> m_async_tasks;
    mutable std::mutex m_tasks_mutex;

    // ID management
    std::stack<int> m_released_ids;
    int m_next_id = 1;

    // Thread safety
    mutable std::recursive_mutex m_mutex;

    // Default notification center instance
    static std::shared_ptr<notifly> m_default_center;
};


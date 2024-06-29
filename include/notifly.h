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
#include <utility>
#include <thread>
#include <stack>

#include "threadpool.h"

#define NOTIFLY_VERSION_MAJOR 2
#define NOTIFLY_VERSION_MINOR 0
#define NOTIFLY_VERSION_PATCH 0

#define NOTIFLY_VERSION (NOTIFLY_VERSION_MAJOR << 16 | NOTIFLY_VERSION_MINOR << 8 | NOTIFLY_VERSION_PATCH)

/**
 * @brief   This enum class defines the possible errors that can occur when using the notification center.
 */
enum class errors
{
    success =                    0,
    observer_not_found =        -1,
    notification_not_found =    -2,
    payload_type_not_match =    -3,
    no_more_observer_ids =      -4
};

/**
 * @brief   This struct is an observer that is used to observe notifications.
 */
class notification_observer
{
public:
    /**
     * @brief   Constructor. This constructor initializes the observer with a unique identifier.
     */
    explicit notification_observer(int a_id, int a_notification, std::string a_types) :
        m_id(a_id),
        is_active(false),
        m_notification(a_notification),
        m_types(std::move(a_types))
    {}

    // 'm_id' is a member variable that holds the unique identifier for the observer.
    int m_id;

    // 'm_callback' is a member variable that holds the callback function to be invoked when a notification is posted.
    // The callback function takes a std::any parameter and returns a std::any value.
    std::function<std::any(std::any)> m_callback;

    bool is_active;

    int m_notification;

    std::string m_types;
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
    notifly() : m_thread_pool(5) // Default thread-pool initialization to just 1 thread
    {
        // Initialize the free ids stack with max ull value
        for(auto i = 2000; i >= 1; --i)
        {
            m_free_ids.push(i);
        }
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
    int add_observer(int a_notification, std::function<Return(Args ...)> a_method)
    {
        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += stringType<Args>()));

        // A lock_guard object is created, locking the mutex 'm_mutex' for the duration of the scope.
        // This ensures that the following operations are thread-safe.
        std::lock_guard a_lock(m_mutex);

        if(m_observers.contains(a_notification))
        {
            auto& obs = std::get<0>(m_observers.at(a_notification)).front();
            if (obs.m_types != types)
            {
                return static_cast<int>(errors::payload_type_not_match);
            }
        }

        // A unique id is generated for the observer.
        int id;
        if (!m_free_ids.empty())
        {
            id = m_free_ids.top();
            m_free_ids.pop();
        }
        else
        {
            return static_cast<int>(errors::no_more_observer_ids);
        }

        // A 'notification_observer' object is created with a unique id, which is incremented after the creation.
        notification_observer a_notification_observer(id, a_notification, types);

        // A lambda function is being defined here. This lambda takes a single argument of type std::any and
        // also returns std::any.
        // The lambda captures 'a_method', which is a function passed from the surrounding scope.
        auto lambda = [a_method](std::any any) -> std::any
        {
            // The input std::any is cast to a std::tuple<Args...>. This assumes that the input std::any contains
            // a std::tuple<Args...>.
            auto message = std::any_cast<std::tuple<Args...>>(any);

            // If the return type of the function is void (i.e., the function does not return anything),
            if constexpr (std::is_same_v<Return, void>)
            {
                // The function is invoked with the arguments from the tuple 'message'.
                std::apply(a_method, message);
                return {};
            }
            else
            {
                // If the return type of the function is not void (i.e., the function returns something),
                return std::apply(a_method, message);
            }
        };

        // The callback function 'a_method' is moved into the 'm_callback_' member of the 'notification_observer' object.
        // This is more efficient than copying, especially for large objects.
        a_notification_observer.m_callback = std::move(lambda);

        // The 'notification_observer' object is added to the list of observers for the notification 'a_notification'.
        std::get<0>(m_observers[a_notification]).push_back(a_notification_observer);

        // A tuple is created containing the notification and an iterator pointing to the last element in the list
        // of observers.
        // The '--' operator is used to get the iterator to the last element, as 'end()' returns an iterator to
        // one past the last element.
        auto tuple = std::make_tuple(a_notification, --std::get<0>(m_observers[a_notification]).end());

        // The tuple is added to the map 'm_observers_by_id' with the observer id as the key.
        m_observers_by_id[id] = tuple;

        // The observer id is returned from the function.
        return id;
    }

	/**
	 * @brief               This method removes an observer by iterator.
	 * @param a_observer    The observer you wish to remove.
	 * @return              0 if successful or an error code.
	 */
	int remove_observer(int a_observer)
    {
        // Check if the observer is not in the map of observers by id. If it's not, exit the function.
        if(!m_observers_by_id.contains(a_observer)) return static_cast<int>(errors::observer_not_found);

        // Retrieve the tuple associated with the observer id from the map.
        auto& [observer_id, iterator] = m_observers_by_id.at(a_observer);

        {
            // Lock the mutex to ensure thread safety during the operation.
            std::lock_guard a_lock(m_mutex);

            // Try to find the notification in the map of observers.
            if (auto a_notification_iterator = m_observers.find(observer_id);
                    a_notification_iterator != m_observers.end())
            {
                // If the notification is found, erase the observer from the list of observers for that notification.
                std::get<0>(a_notification_iterator->second).erase(iterator);

                // If there are no more observers for this notification, erase the notification from the map of observers
                // and also erase the notification from the map of payload types.
                if(std::get<0>(a_notification_iterator->second).empty())
                {
                    m_observers.erase(a_notification_iterator);
                }
            }
            // Push the observer id to the queue of free ids.
            m_free_ids.push(a_observer);
        }

        // Finally, erase the observer from the map of observers by id.
        m_observers_by_id.erase(a_observer);

        return static_cast<int>(errors::success);
    }

    /**
     * @brief                       This method removes all observers from a given notification, removing the
     *                              notification from being tracked outright.
     * @param   a_notification      The name of the notification you wish to remove.
     * @return                      The number of observers that were successfully removed or an error code.
     */
    int remove_all_observers(int a_notification)
    {
        // Lock the mutex to ensure thread safety during the operation.
        std::lock_guard a_lock(m_mutex);

        // Check if the notification is not in the map of observers. If it's not, exit the function.
        if(!m_observers.contains(a_notification)) return 0;

        // Get the list of observers for the given notification.
        auto observers_by_notification = std::get<0>(m_observers.at(a_notification));

        // Get the number of observers for the given notification.
        auto ret = observers_by_notification.size();

        // Iterate over all observers for the given notification.
        for(const auto& observer: observers_by_notification)
        {
            // Push the observer id to the queue of free ids.
            m_free_ids.push(observer.m_id);
            // Erase the observer from the map of observers by id.
            m_observers_by_id.erase(observer.m_id);
        }

        // Erase the notification from the map of observers and the map of payload types.
        m_observers.erase(a_notification);

        return static_cast<int>(ret);
    }

    /**
     * @brief                   This method posts a notification to a set of observers. If successful, this function
     *                          calls all callbacks associated with that notification and return true.
     *                          If no such notification exists, this function will print a warning to the console and
     *                          return false.
     *
     * @param a_notification    The name of the notification you wish to post.
     * @param args              The payload associated with the specified notification.
     * @param a_async           If false, this function will run in the same thread as the caller.
     *                          If true, this function will run in a separate thread.
     * @return                  Number of observers that were successfully notified or an error code.
     */
    template<typename ...Args>
    int post_notification(int a_notification, Args... args, bool a_async = false)
    {
        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += stringType<Args>()));
        // This line is using a fold expression to concatenate the names of the types of the arguments (Args...).
        // std::type_index is used to get a std::type_info for each type, and .name() gets the name of the type.
        // The names are appended to the 'types' string.

        // A lock_guard object is created, locking the mutex 'm_mutex' for the duration of the scope.
        // This ensures that the following operations are thread-safe.
        std::lock_guard a_lock(m_mutex);

        // Check if the types string matches the one saved in the map
        if(!m_observers.contains(a_notification))
        {
            return static_cast<int>(errors::notification_not_found);
        }
        else if (std::get<0>(m_observers[a_notification]).front().m_types != types)
        {
            return static_cast<int>(errors::payload_type_not_match);
        }

        // A std::tuple of the arguments is created and wrapped in a std::any.
        auto payload = std::make_any<std::tuple<Args...>>(std::make_tuple(args...));

        // The code attempts to find the notification 'a_notification' in the 'm_observers' map.
        const auto a_notification_iterator = m_observers.find(a_notification);

        // If the notification is found, it retrieves the list of observers for that notification.
        const auto& a_notification_list = std::get<0>(a_notification_iterator->second);

        // It then iterates over each observer in the list.
        for (const auto& callback : a_notification_list)
        {
            // If 'a_async' is true, it pushes the callback function to the thread pool for asynchronous execution.
            // The callback function is invoked with 'a_payload' as its argument.
            if(a_async)
            {
                m_thread_pool.push([=](int id) { return callback.m_callback(payload);});
            }
            // If 'a_async' is false, it directly invokes the callback function with 'a_payload' as its argument.
            else
            {
                callback.m_callback(payload);
            }
        }
        // If the notification is found and the callbacks are successfully invoked, it returns true.
        return static_cast<int>(a_notification_list.size());
    }

	/**
     * @brief               This method is used to resize the threads number in the thread-pool
     * @param   a_threads   The number of threads
     */
	void resize_thread_pool(int a_threads)
    {
        // Push a lambda function to the thread pool that resizes the thread pool to the specified number of threads.
        m_thread_pool.push([this, a_threads](int id) { this->m_thread_pool.resize(a_threads); });
    }

    /**
     * @brief   This method returns the default global notification center. You may alternatively create your
     *          own notification center without using the default notification center.
     */
    static notifly& default_notifly()
    {
        // Guaranteed to be destroyed. Instantiated on first use.
        static notifly a_notification;

        return a_notification;
    }

private:
    /** === Private types === **/
	typedef std::list<notification_observer>::const_iterator observer_const_itr_t;
	typedef std::tuple<int, observer_const_itr_t>  notification_tuple_t;
	typedef std::tuple<std::list<notification_observer>, std::unique_ptr<std::mutex>> notification_info_t;

    /** === Private methods === **/
    /**
     * @brief       This method returns a string representation of the type 'T'.
     * @tparam  T   The type to get the string representation of.
     * @return      A string representation of the type 'T'.
     */
    template <typename T>
    std::string stringType()
    {
        std::string type_name = std::type_index(typeid(T)).name();
        if (std::is_lvalue_reference<T>::value)
        {
            return "(Qxt&)" + type_name;
        }
        else if (std::is_rvalue_reference<T>::value)
        {
            return "(Qxt&&)" + type_name;
        }
        else
        {
            return "(Qxt)" + type_name;
        }
    }

    /** === Private members === **/
    // 'm_default_center' is a static member variable that holds the default notification center.
	static std::shared_ptr<notifly> m_default_center;
    // 'm_free_ids' is a member variable that holds a queue of free observer ids.
    std::stack<int> m_free_ids;
    // 'm_observers' is a member variable that holds a map of notifications and their observers.
    std::unordered_map<int, notification_info_t> m_observers;
    // 'm_observers_by_id' is a member variable that holds a map of observer ids and their associated tuples.
    std::unordered_map<int, notification_tuple_t> m_observers_by_id;

    // 'm_mutex' is a member variable that holds a mutex for thread safety.
	typedef std::recursive_mutex mutex_t;
    mutable mutex_t m_mutex;

    // 'm_thread_pool' is a member variable that holds a thread pool for asynchronous notifications.
	tp::thread_pool m_thread_pool;
};
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

#include "threadpool.h"

/**
 * @brief   This struct is an observer that is used to observe notifications.
 */
struct notification_observer
{
    /**
     * @brief   Constructor. This constructor initializes the observer with a unique identifier.
     */
    explicit notification_observer(uint_fast64_t a_id)
    {
        // The passed identifier is assigned to the member variable 'm_id_'.
        m_id_ = a_id;
    }

    // 'm_id_' is a member variable that holds the unique identifier for the observer.
    uint_fast64_t m_id_;

    // 'm_callback_' is a member variable that holds the callback function to be invoked when a notification is posted.
    // The callback function takes a std::any parameter and returns a std::any value.
    std::function<std::any(std::any)> m_callback_;
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
    notifly() : m_ids_(1), m_thread_pool_(1) // Default thread-pool initialization to just 1 thread
    {}

    /**
     * @brief                   This method adds a function callback as an observer to a named notification.
     * @param a_notification    The name of the notification you wish to observe.
     * @param a_method          The function callback. Accepts unsigned any(any) methods or lambdas.
     * @return                  The observer id. 0 if the payload types do not match the registered types.
    */
    uint64_t add_observer(int a_notification, const std::function<std::any(std::any any)>& a_method)
    {
        // std::type_index is used to get a std::type_info for std::any, and .name() gets the name of the type.
        auto type = std::type_index(typeid(std::any)).name();
        if(!internal_check_observer_type(a_notification, type))
        {
            set_last_error("The payload types do not match the registered types");
            return 0;
        }

        // The type of the payload (in this case std::any) is stored as a string in the 'm_payloads_types_' map.
        // The key for the map is 'a_notification', which is the identifier for the notification.
        m_payloads_types_[a_notification] = type;

        // A lambda function is being defined here. This lambda takes a single argument of type std::any and also
        // returns std::any.
        // The lambda captures 'a_method', which is a function passed from the surrounding scope.
        auto lambda = [a_method](std::any any) -> std::any
        {
            // The input std::any is cast to a std::tuple<std::any>. This assumes that the input std::any contains a
            // std::tuple<std::any>.
            auto message = std::any_cast<std::tuple<std::any>>(any);

            // std::apply is used to call 'a_method' with the elements of 'message' tuple as its arguments.
            // The result of 'a_method' is returned from the lambda.
            return std::apply(a_method, message);
        };

        // The lambda is added as an observer for the notification 'a_notification' using the 'internal_add_observer' method.
        // The id of the observer is returned from the method.
        return internal_add_observer(a_notification, lambda);
    }

    /**
     * @brief                   This method adds a function callback as an observer to a named notification.
     * @param a_notification    The name of the notification you wish to observe.
     * @param a_method          The function callback.
     * @return                  The observer id. 0 if the payload types do not match the registered types.
    */
    template<typename Return, typename ...Args>
    uint64_t add_observer(int a_notification, Return(*a_method)(Args... args))
    {
        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += std::type_index(typeid(Args)).name()));

        // Check if the types string matches the one saved in the map
        if (!internal_check_observer_type(a_notification, types))
        {
            set_last_error("The payload types do not match the registered types");
            return 0;
        }

        // Save the types string in the map
        m_payloads_types_[a_notification] = types;

        // A lambda function is being defined here. This lambda takes a single argument of type std::any and also
        // returns std::any.
        // The lambda captures 'a_method', which is a function passed from the surrounding scope.
        auto lambda = [a_method](std::any any) -> std::any
        {
            // The input std::any is cast to a std::tuple<Args...>. This assumes that the input std::any
            // contains a std::tuple<Args...>.
            auto message = std::any_cast<std::tuple<Args...>>(any);

            // std::apply is used to call 'a_method' with the elements of 'message' tuple as its arguments.
            // The result of 'a_method' is returned from the lambda.
            return std::apply(a_method, message);
        };

        // The lambda is added as an observer for the notification 'a_notification' using the 'internal_add_observer' method.
        // The id of the observer is returned from the method.
        return internal_add_observer(a_notification, lambda);
    }

    /**
     * @brief   This method adds a function callback as an observer to a named notification.
     * @param   a_notification  The name of the notification you wish to observe.
     * @param   a_method        The function callback.
     * @return  The observer id.
     */
    template<typename Return, typename ...Args>
    uint64_t add_observer(int a_notification, std::function<Return(Args ...)> a_method)
    {
        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += std::type_index(typeid(Args)).name()));

        // Check if the types string matches the one saved in the map
        if (!internal_check_observer_type(a_notification, types))
        {
            set_last_error("The payload types do not match the registered types");
            return 0;
        }

        // Save the types string in the map
        m_payloads_types_[a_notification] = types;

        // A lambda function is being defined here. This lambda takes a single argument of type std::any and
        // also returns std::any.
        // The lambda captures 'a_method', which is a function passed from the surrounding scope.
        auto lambda = [a_method](std::any any) -> std::any
        {
            // The input std::any is cast to a std::tuple<Args...>. This assumes that the input std::any contains
            // a std::tuple<Args...>.
            auto message = std::any_cast<std::tuple<Args...>>(any);

            // std::apply is used to call 'a_method' with the elements of 'message' tuple as its arguments.
            // The result of 'a_method' is returned from the lambda.
            return std::apply(a_method, message);
        };

        // The lambda is added as an observer for the notification 'a_notification' using the 'internal_add_observer' method.
        // The id of the observer is returned from the method.
        return internal_add_observer(a_notification, lambda);
    }

	/**
	 * @brief               This method removes an observer by iterator.
	 * @param a_observer    The observer you wish to remove.
	 * @return              Void.
	 */
	void remove_observer(uint64_t a_observer)
    {
        // Check if the observer is not in the map of observers by id. If it's not, exit the function.
        if(!m_observers_by_id_.contains(a_observer)) return;

        // Retrieve the tuple associated with the observer id from the map.
        auto tuple = m_observers_by_id_.at(a_observer);

        {
            // Lock the mutex to ensure thread safety during the operation.
            std::lock_guard a_lock(m_mutex_);

            // Try to find the notification in the map of observers.
            if (auto a_notification_iterator = m_observers_.find(std::get<0>(tuple));
                    a_notification_iterator != m_observers_.end())
            {
                // If the notification is found, erase the observer from the list of observers for that notification.
                a_notification_iterator->second.erase(std::get<1>(tuple));

                // If there are no more observers for this notification, erase the notification from the map of observers
                // and also erase the notification from the map of payload types.
                if(m_observers_.at(std::get<0>(tuple)).empty())
                {
                    m_observers_.erase(std::get<0>(tuple));
                    m_payloads_types_.erase(std::get<0>(tuple));
                }
            }
        }

        // Finally, erase the observer from the map of observers by id.
        m_observers_by_id_.erase(a_observer);
    }

    /**
     * @brief                       This method removes all observers from a given notification, removing the
     *                              notification from being tracked outright.
     * @param   a_notification      The name of the notification you wish to remove.
     */
    void remove_all_observers(int a_notification)
    {
        // Lock the mutex to ensure thread safety during the operation.
        std::lock_guard a_lock(m_mutex_);
        // Erase the notification from the map of observers and the map of payload types.
        m_observers_.erase(a_notification);
        m_payloads_types_.erase(a_notification);
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
     * @return                  True if the notification was successfully posted, false otherwise.
     */
    template<typename ...Args>
    bool post_notification(int a_notification, Args... args, bool a_async = false)
    {
        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += std::type_index(typeid(Args)).name()));
        // This line is using a fold expression to concatenate the names of the types of the arguments (Args...).
        // std::type_index is used to get a std::type_info for each type, and .name() gets the name of the type.
        // The names are appended to the 'types' string.

        // Check if the types string matches the one saved in the map
        if(!m_payloads_types_.contains(a_notification))
        {
            // If the notification does not exist in the 'm_payloads_types_' map, an error message is set and the
            // function returns false.
            set_last_error("The notification does not exist");
            return false;
        }
        else if (m_payloads_types_[a_notification] != types)
        {
            // If the types string does not match the one saved in the 'm_payloads_types_' map for the given notification,
            // an error message is set and the function returns false.
            set_last_error("The payload types do not match the registered types");
            return false;
        }

        // A std::tuple of the arguments is created and wrapped in a std::any.
        auto payload = std::make_any<std::tuple<Args...>>(std::make_tuple(args...));

        // The 'internal_post_notification' function is called with the notification, the payload, and the 'a_async' flag.
        // The result of 'internal_post_notification' is returned from the function.
        return internal_post_notification(a_notification, payload, a_async);
    }

    /**
     * @brief   This method gets the last error message.
     */
    std::string get_last_error()
    {
        std::lock_guard a_lock(m_last_error_mutex_);
        return m_last_error_;
    }

	/**
     * @brief               This method is used to resize the threads number in the thread-pool
     * @param   a_threads   The number of threads
     */
	void resize_thread_pool(int a_threads)
    {
        // Push a lambda function to the thread pool that resizes the thread pool to the specified number of threads.
        m_thread_pool_.push([this, a_threads](int id) { this->m_thread_pool_.resize(a_threads); });
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


    /** === Private methods === **/
    /**
     * @breif                   This method adds a function callback as an observer to a named notification.
     * @param a_notification    The name of the notification you wish to observe.
     * @param a_method          The function callback. Accepts unsigned any(any) methods or lambdas.
     * @return                  The observer id.
    */
    uint64_t internal_add_observer(int a_notification, std::function<std::any(std::any)> a_method)
    {
        // A lock_guard object is created, locking the mutex 'm_mutex_' for the duration of the scope.
        // This ensures that the following operations are thread-safe.
        std::lock_guard a_lock(m_mutex_);

        // A 'notification_observer' object is created with a unique id, which is incremented after the creation.
        notification_observer a_notification_observer(m_ids_++);

        // The callback function 'a_method' is moved into the 'm_callback_' member of the 'notification_observer' object.
        // This is more efficient than copying, especially for large objects.
        a_notification_observer.m_callback_ = std::move(a_method);

        // The 'notification_observer' object is added to the list of observers for the notification 'a_notification'.
        m_observers_[a_notification].push_back(a_notification_observer);

        // A tuple is created containing the notification and an iterator pointing to the last element in the list
        // of observers.
        // The '--' operator is used to get the iterator to the last element, as 'end()' returns an iterator to
        // one past the last element.
        auto tuple = std::make_tuple(a_notification, --m_observers_[a_notification].end());

        // The id of the observer is retrieved from the tuple.
        auto id = std::get<1>(tuple)->m_id_;

        // The tuple is added to the map 'm_observers_by_id_' with the observer id as the key.
        m_observers_by_id_[id] = tuple;

        // The observer id is returned from the function.
        return id;
    }

    /**
     * @brief                       This method posts a notification to a set of observers.
     *                              If successful, this function calls all callbacks associated with that notification
     *                              and return true. If no such notification exists, this function set an error message
     *                              and return false.
     * @param   a_notification      The name of the notification you wish to post.
     * @param   a_payload           The payload associated with the specified notification.
     * @param   a_async             If false, this function will run in the same thread as the caller.
     *                              If true, this function will run in a separate thread.
     * @return                      True if the notification was successfully posted, false otherwise.
     */
    bool internal_post_notification(int a_notification,	const std::any& a_payload = std::any(),	bool a_async = false)
    {
        // A lock_guard object is created, locking the mutex 'm_mutex_' for the duration of the scope.
        // This ensures that the following operations are thread-safe.
        std::lock_guard a_lock(m_mutex_);

        // The code attempts to find the notification 'a_notification' in the 'm_observers_' map.
        if (const auto a_notification_iterator = m_observers_.find(a_notification);
                a_notification_iterator != m_observers_.end())
        {
            // If the notification is found, it retrieves the list of observers for that notification.
            const auto& a_notification_list = a_notification_iterator->second;

            // It then iterates over each observer in the list.
            for (const auto& callback : a_notification_list)
            {
                // If 'a_async' is true, it pushes the callback function to the thread pool for asynchronous execution.
                // The callback function is invoked with 'a_payload' as its argument.
                if(a_async)
                {
                    m_thread_pool_.push([=](int id) { return callback.m_callback_(a_payload);});
                }
                // If 'a_async' is false, it directly invokes the callback function with 'a_payload' as its argument.
                else
                {
                    callback.m_callback_(a_payload);
                }
            }
            // If the notification is found and the callbacks are successfully invoked, it returns true.
            return true;
        }
        else
        {
            // If the notification is not found in the 'm_observers_' map, it sets an error message and returns false.
            std::stringstream a_error;
            a_error << "Notification " << a_notification << " does not exist.";
            set_last_error(a_error.str());
            return false;
        }
    }

    /**
     * @brief                   This method checks if the observer type matches the payload type.
     * @param   a_notification  The notification to check.
     * @param   a_type          The type to check.
     * @return                  True if the observer type matches the payload type, false otherwise.
     */
    bool internal_check_observer_type(int a_notification, const std::string& a_type)
    {
        if(m_payloads_types_.contains(a_notification))
        {
            return m_payloads_types_[a_notification] == a_type;
        }

        return true;
    }

    /**
     * @brief   This method sets the last error message.
     * @param   a_error The error message you wish to set.
     */
    void set_last_error(const std::string& a_error)
    {
        std::lock_guard a_lock(m_last_error_mutex_);
        m_last_error_ = a_error;
    }

    /** === Private members === **/
    // 'm_default_center_' is a static member variable that holds the default notification center.
	static std::shared_ptr<notifly> m_default_center_;
    // 'm_ids_' is a member variable that holds the unique identifier for observers.
    std::atomic_uint_fast64_t m_ids_;
    // 'm_observers_' is a member variable that holds a map of notifications and their observers.
    std::unordered_map<int, std::list<notification_observer> > m_observers_;
    // 'm_observers_by_id_' is a member variable that holds a map of observer ids and their associated tuples.
    std::unordered_map<uint64_t, notification_tuple_t> m_observers_by_id_;
    // 'm_payloads_types_' is a member variable that holds the types of payloads for each notification.
    std::unordered_map<int, std::string> m_payloads_types_;

    // 'm_mutex_' is a member variable that holds a mutex for thread safety.
	typedef std::recursive_mutex mutex_t;
    mutable mutex_t m_mutex_;

    // 'm_thread_pool_' is a member variable that holds a thread pool for asynchronous notifications.
	tp::thread_pool m_thread_pool_;

    // 'm_last_error_mutex_' is a member variable that holds a mutex for thread safety.
    std::mutex m_last_error_mutex_;
    // 'm_last_error_' is a member variable that holds the last error message.
    std::string m_last_error_;
};
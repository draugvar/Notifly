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

#include "threadpool.h"

#include <unordered_map>
#include <functional>
#include <list>
#include <mutex>
#include <any>
#include <typeindex>
#include <utility>
#include <thread>

struct notification_observer
{
	explicit notification_observer(uint_fast64_t a_id)
	{
		m_id = a_id;
	}

	uint_fast64_t m_id;
	std::function<std::any(std::any)> m_callback;
};

class notifly
{
public:
	/**
     * Constructor.
     */
    notifly() : m_ids_(0), m_thread_pool_(1) // Default thread-pool initialization to just 1 thread
    {}

    /**
 * This method adds a function callback as an observer to a named notification.
 */
    uint64_t add_observer
    (
        int a_notification,       				    ///< The name of the notification you wish to observe.
        const std::function<std::any(std::any any)>& a_method	///< The function callback. Accepts unsigned any(any) methods or lambdas.
    )
    {
        auto lambda = [a_method](std::any any) -> std::any
        {
            auto message = std::any_cast<std::tuple<std::any>>(any);
            return std::apply(a_method, message);
        };

        // Save the types string in the map
        m_payloads_types_[a_notification] = std::type_index(typeid(std::any)).name();

        return internal_add_observer(a_notification, lambda);
    }

    /**
    * This method adds a function callback as an observer to a named notification.
    */
    template<typename Return, typename ...Args>
    uint64_t add_observer
    (
        int a_notification,       		    ///< The name of the notification you wish to observe.
        Return(*a_method)(Args... args)     ///< The function callback.
    )
    {
        auto lambda = [a_method](std::any any) -> std::any
        {
            auto message = std::any_cast<std::tuple<Args...>>(any);
            return std::apply(a_method, message);
        };

        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += std::type_index(typeid(Args)).name()));

        // Save the types string in the map
        m_payloads_types_[a_notification] = types;

        return internal_add_observer(a_notification, lambda);
    }

    /**
   * This method adds a function callback as an observer to a named notification.
   */
    template<typename Return, typename ...Args>
    uint64_t add_observer
    (
        int a_notification,       		            ///< The name of the notification you wish to observe.
        std::function<Return(Args ...)> a_method    ///< The function callback.
    )
    {
        auto lambda = [a_method](std::any any) -> std::any
        {
            auto message = std::any_cast<std::tuple<Args...>>(any);
            return std::apply(a_method, message);
        };

        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += std::type_index(typeid(Args)).name()));

        // Save the types string in the map
        m_payloads_types_[a_notification] = types;

        return internal_add_observer(a_notification, lambda);
    }

	/**
	 * This method removes an observer by iterator.
	 */
	void remove_observer
	(
		uint64_t a_observer             	///< The observer you wish to remove.
	)
    {
        if(!m_observers_by_id.contains(a_observer)) return;

        auto tuple = m_observers_by_id.at(a_observer);

        internal_remove_observer(tuple);

        m_observers_by_id.erase(a_observer);
    }

    /**
     * This method removes all observers from a given notification, removing the notification from being tracked outright.
     */
    void remove_all_observers
	(
		int a_notification	///< The name of the notification you wish to remove.
	)
    {
        std::lock_guard a_lock(m_mutex_);
        m_observers_.erase(a_notification);
    }

    /**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
    template<typename ...Args>
    bool post_notification
    (
        int a_notification,		///< The name of the notification you wish to post.
        Args... args,           ///< The payload associated with the specified notification.
        bool a_async = false	///< If false, this function will run in the same thread as the caller.
                                    ///< If true, this function will run in a separate thread.
    )
    {
        // Generate a unique string for the types of Args
        std::string types;
        (..., (types += std::type_index(typeid(Args)).name()));

        // Check if the types string matches the one saved in the map
        if(!m_payloads_types_.contains(a_notification))
        {
            set_last_error("The notification does not exist");
            return false;
        }
        else if (m_payloads_types_[a_notification] != types)
        {
            set_last_error("The payload types do not match the registered types");
            return false;
        }

        auto payload = std::make_any<std::tuple<Args...>>(std::make_tuple(args...));
        return internal_post_notification(a_notification, payload, a_async);
    }
    /**
    * This method posts a notification to a set of observers.
    * If successful, this function calls all callbacks associated with that notification and return true.
    * If no such notification exists, this function will print a warning to the console and return false.
    */

    /**
     * This method gets the last error message.
     */
    std::string get_last_error()
    {
        std::lock_guard a_lock(last_error_mutex_);
        return m_last_error_;
    }

	/**
     * This method is used to resize the threads number in the thread-pool
     */
	void resize_thread_pool
	(
		int a_threads	///< Number of threads
	)
    {
        m_thread_pool_.push([this, a_threads](int id) { this->m_thread_pool_.resize(a_threads); });
    }

    /**
     * This method returns the default global notification center.  You may alternatively create your own notification
     * center without using the default notification center.
     */
    static notifly& default_notifly()
    {
        // Guaranteed to be destroyed. Instantiated on first use.
        // ReSharper disable once CommentTypo
        static notifly a_notification;  // NOLINT(clang-diagnostic-exit-time-destructors)

        return a_notification;
    }

private:
    // Private types
	typedef std::unordered_map<int, std::list<notification_observer> >::iterator notification_itr_t;
	typedef std::list<notification_observer>::const_iterator observer_const_itr_t;
	typedef std::tuple<int, observer_const_itr_t>  notification_tuple_t;

    /**
    * This method adds a function callback as an observer to a named notification.
    */
    uint64_t internal_add_observer
    (
        int a_notification,       				    ///< The name of the notification you wish to observe.
        std::function<std::any(std::any)> a_method	///< The function callback. Accepts unsigned any(any) methods or lambdas.
    )
    {
        std::lock_guard a_lock(m_mutex_);
        notification_observer a_notification_observer(m_ids_++);
        a_notification_observer.m_callback = std::move(a_method);
        m_observers_[a_notification].push_back(a_notification_observer);
        auto tuple = std::make_tuple(a_notification, --m_observers_[a_notification].end());
        auto id = std::get<1>(tuple)->m_id;

        m_observers_by_id[id] = tuple;

        return id;
    }

	/**
	 * This method removes an observer by iterator.
	 */
	void internal_remove_observer
	(
		notification_tuple_t& a_notification             ///< The notification you wish to remove
	)
    {
        std::lock_guard a_lock(m_mutex_);
        if (auto a_notification_iterator = m_observers_.find(std::get<0>(a_notification));
                a_notification_iterator != m_observers_.end())
        {
            a_notification_iterator->second.erase(std::get<1>(a_notification));
        }
    }

    /**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
    bool internal_post_notification
    (
        int a_notification,					///< The name of the notification you wish to post.
        const std::any& a_payload = std::any(),	///< The payload associated with the specified notification.
        bool a_async = false					///< If false, this function will run in the same thread as the caller.
        ///< If false, this function will run in a separate thread.
    )
    {
        std::lock_guard a_lock(m_mutex_);
        if (const auto a_notification_iterator = m_observers_.find(a_notification);
                a_notification_iterator != m_observers_.end())
        {
            const auto& a_notification_list = a_notification_iterator->second;
            for (const auto& callback : a_notification_list)
            {
                if(a_async)
                {
                    m_thread_pool_.push([=](int id) { return callback.m_callback(a_payload);});
                }
                else
                {
                    // ReSharper disable once CppExpressionWithoutSideEffects
                    callback.m_callback(a_payload);
                }
            }
            return true;
        }
        else
        {
            std::string a_error = "Notification \"" + std::to_string(a_notification) + "\" does not exist.";
            set_last_error(a_error);
            return false;
        }
    }

    /**
     * This method sets the last error message.
     */
    void set_last_error
    (
        const std::string& a_error ///< The error message you wish to set.
    )
    {
        std::lock_guard a_lock(last_error_mutex_);
        m_last_error_ = a_error;
    }

    // Private members
	static std::shared_ptr<notifly> m_default_center_;
    std::unordered_map<int, std::list<notification_observer> > m_observers_;
    std::unordered_map<uint64_t, notification_tuple_t> m_observers_by_id;

	typedef std::recursive_mutex mutex_t;
    mutable mutex_t m_mutex_;

	std::atomic_uint_fast64_t m_ids_;

	tp::thread_pool m_thread_pool_;

    std::mutex last_error_mutex_;
    std::string m_last_error_;

    std::unordered_map<int, std::string> m_payloads_types_;
};
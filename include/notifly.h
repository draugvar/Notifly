/*
 *  NotificationCenter.h
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
     notifly();

	/**
     * This method adds a function callback as an observer to a named notification.
     */
	uint64_t add_observer
	(
			int a_name,       				    ///< The name of the notification you wish to observe.
			std::function<std::any(std::any)> a_method	///< The function callback. Accepts unsigned any(any) methods or lambdas.
	);

	/**
	 * This method removes an observer by iterator.
	 */
	void remove_observer
	(
			uint64_t a_observer             	///< The observer you wish to remove.
	);

    /**
     * This method removes all observers from a given notification, removing the notification from being tracked outright.
     */
    void remove_all_observers
	(
		int a_name	///< The name of the notification you wish to remove.
	);

    /**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
    bool post_notification
	(
		int a_notification,					///< The name of the notification you wish to post.
		const std::any& a_payload = std::any(),	///< The payload associated with the specified notification.
		bool a_async = false					///< If false, this function will run in the same thread as the caller.
											///< If false, this function will run in a separate thread.
	);

	/**
     * This method is used to resize the threads number in the thread-pool
     */
	void resize_thread_pool
	(
			int a_threads	///< Number of threads
	);

    /**
     * This method returns the default global notification center.  You may alternatively create your own notification
     * center without using the default notification center.
     */
    static notifly& default_notifly();

private:
	typedef std::unordered_map<int, std::list<notification_observer> >::const_iterator notification_const_itr_t;
	typedef std::unordered_map<int, std::list<notification_observer> >::iterator notification_itr_t;
	typedef std::list<notification_observer>::const_iterator observer_const_itr_t;
	typedef std::list<notification_observer>::iterator observer_itr_t;
	typedef std::tuple<int, observer_const_itr_t>  notification_tuple_t;

	/**
     * This method adds a function callback as an observer to a named notification.
     */
	notification_tuple_t add_observer_ex
	(
			int a_name,       				    ///< The name of the notification you wish to observe.
			std::function<std::any(std::any)> a_method	///< The function callback.  Accepts unsigned any(any) methods or lambdas.
	);

	/**
	 * This method adds a function callback as an observer to a given notification.
	 */
	observer_const_itr_t add_observer
	(
			notification_itr_t& a_notification,				///< The name of the notification you wish to observe.
			std::function<std::any(std::any)> a_method	    ///< The function callback.  Accepts unsigned any(any) methods or lambdas.
	);

	/**
	 * This method removes an observer by iterator.
	 */
	void remove_observer
	(
			notification_tuple_t& a_notification             ///< The notification you wish to remove
	);

	/**
	 * This method removes an observer by iterator.
	 */
	void remove_observer
	(
			notification_itr_t& a_notification,	///< The iterator of the notification you wish to remove a given observer from.
			observer_const_itr_t& a_observer	///< The iterator to the observer you wish to remove.
	);

	/**
     * This method removes all observers from a given notification, removing the notification from being tracked outright.
     */
	void remove_all_observers
	(
		notification_itr_t& a_notification	///< The iterator of the notification you wish to remove.
	);

	/**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
	bool post_notification
	(
			notification_itr_t& a_notification,	///< The name of the notification you wish to post.
			const std::any& a_payload = std::any(),	///< The payload associated with the specified notification.
			bool a_async = false					///< If false, this function will run in the same thread as the caller.
			///< If false, this function will run in a separate thread.
	);

	/**
	 * This method retrieves a notification iterator for a named notification.
	 * The returned iterator may be used with the overloaded variants of postNotification, removeAllObservers,
	 * removeObserver, and addObserver to avoid string lookups.
	 */
	notification_itr_t get_notification_iterator
	(
			int a_notification	///< The name of the notification you wish to post.
	);

	[[maybe_unused]] static std::shared_ptr<notifly> m_default_center_;
    std::unordered_map<int, std::list<notification_observer> > m_observers_;
    std::unordered_map<uint64_t, notification_tuple_t> m_observers_by_id;

	typedef std::recursive_mutex mutex_t;
    mutable mutex_t m_mutex_;

	std::atomic_uint_fast64_t m_ids_;

	tp::thread_pool m_thread_pool_;
};
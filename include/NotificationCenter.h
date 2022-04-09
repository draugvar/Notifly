/*
 *  NotificationCenter.h
 *  Notification Center CPP
 *
 *  Created by Jonathan Goodman on 11/23/13.
 *  Copyright (c) 2013 Jonathan Goodman. All rights reserved.
 *  Edited by Salvatore Rivieccio.
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
#include <functional>
#include <list>
#include <mutex>
#include <any>

template <typename ...ARGS>
struct notification_observer
{
    std::function<std::any(ARGS... args)> m_callback{};
};

template <typename ...ARGS>
class notification_center
{
public:
    typedef typename std::unordered_map<int, std::list<notification_observer<ARGS...>> >::const_iterator notification_const_itr_t;
    typedef typename std::unordered_map<int, std::list<notification_observer<ARGS...>> >::iterator notification_itr_t;
    typedef typename std::list<notification_observer<ARGS...>>::const_iterator observer_const_itr_t;
    typedef typename std::list<notification_observer<ARGS...>>::iterator observer_itr_t;
	typedef std::tuple<int, observer_const_itr_t>  notification_tuple_t;

    /**
     * This method adds a function callback as an observer to a named notification.
     */
	notification_tuple_t add_observer
	(
		int a_name,       				    ///< The name of the notification you wish to observe.
		std::function<std::any(ARGS... args)> a_method	///< The function callback.  Accepts unsigned int(std::any) methods or lambdas.
	);

    /**
     * This method adds a function callback as an observer to a given notification.
     */
    observer_const_itr_t add_observer
	(
		notification_itr_t& a_notification,				///< The name of the notification you wish to observe.
		std::function<std::any(ARGS... args)> a_method	    ///< The function callback.  Accepts unsigned int(std::any) methods or lambdas.
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
		observer_const_itr_t& a_observer		///< The iterator to the observer you wish to remove.
	);

    /**
     * This method removes all observers from a given notification, removing the notification from being tracked outright.
     */
    void remove_all_observers
	(
		int a_name	///< The name of the notification you wish to remove.
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
		int a_notification,			///< The name of the notification you wish to post.
		ARGS... args			///< The payload associated with the specified notification. nullptr by default.
	) const;

	/**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
	bool post_notification
	(
		int a_notification			///< The name of the notification you wish to post.
	) const;

    /**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
    bool post_notification
	(
		notification_const_itr_t& a_notification,	///< The name of the notification you wish to post.
		ARGS... args	///< The payload associated with the specified notification. nullptr by default.
	) const;

	/**
     * This method posts a notification to a set of observers.
     * If successful, this function calls all callbacks associated with that notification and return true.
	 * If no such notification exists, this function will print a warning to the console and return false.
     */
	bool post_notification
	(
		notification_const_itr_t& a_notification	///< The name of the notification you wish to post.
	) const;

    /**
     * This method retrieves a notification iterator for a named notification.
     * The returned iterator may be used with the overloaded variants of postNotification, removeAllObservers, removeObserver, and addObserver to avoid string lookups.
     */
    notification_itr_t get_notification_iterator
	(
		int a_notification	///< The name of the notification you wish to post.
	);

    /**
     * This method returns the default global notification center.  You may alternatively create your own notification center without using the default notification center.
     */
    static notification_center& default_notification_center();

private:
	static std::shared_ptr<notification_center> m_default_center_;
    std::unordered_map<int, std::list<notification_observer<ARGS...>> > m_observers_{};
	typedef std::recursive_mutex mutex_t;
    mutable mutex_t m_mutex_;
};
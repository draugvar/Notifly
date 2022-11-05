/*
 *  NotificationCenter.cpp
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

#include <utility>
#include <thread>

#include "notifly.h"

notifly::notifly() : m_ids_(0)
{
	m_thread_pool_.resize(static_cast<int>(std::thread::hardware_concurrency()/10));
}

uint64_t notifly::add_observer(int a_name, std::function<std::any(std::any)> a_method)
{
	auto tuple = add_observer_ex(a_name, std::move(a_method));
	auto id = std::get<1>(tuple)->m_id;

	m_observers_by_id[id] = tuple;

	return id;
}

void notifly::remove_observer(uint64_t a_observer)
{
	auto tuple = m_observers_by_id.at(a_observer);

	remove_observer(tuple);

	m_observers_by_id.erase(a_observer);
}

notifly::notification_tuple_t notifly::add_observer_ex(
	const int a_name, std::function<std::any(std::any)> a_method)
{
	std::lock_guard a_lock(m_mutex_);
	notification_observer a_notification_observer(m_ids_++);
	a_notification_observer.m_callback = std::move(a_method);
	m_observers_[a_name].push_back(a_notification_observer);
	return std::make_tuple(a_name, --m_observers_[a_name].end());
}

notifly::observer_const_itr_t notifly::add_observer(
	notification_itr_t& a_notification, std::function<std::any(std::any)> a_method)
{
	std::lock_guard a_lock(m_mutex_);
	auto a_return_value = a_notification->second.end();
	if (a_notification != m_observers_.end())
	{
		notification_observer a_notification_observer(m_ids_++);
		a_notification_observer.m_callback = std::move(a_method);
		a_notification->second.push_back(a_notification_observer);
		a_return_value = --a_notification->second.end();
	}
	return a_return_value;
}

void notifly::remove_observer(notification_tuple_t& a_notification)
{
	std::lock_guard a_lock(m_mutex_);
	if (auto a_notification_iterator = m_observers_.find(std::get<0>(a_notification));
		a_notification_iterator != m_observers_.end())
	{
		a_notification_iterator->second.erase(std::get<1>(a_notification));
	}
}

void notifly::remove_observer(notification_itr_t& a_notification, observer_const_itr_t& a_observer)
{
	std::lock_guard a_lock(m_mutex_);
	if (a_notification != m_observers_.end())
	{
		a_notification->second.erase(a_observer);
	}
}

void notifly::remove_all_observers(const int a_name)
{
	std::lock_guard a_lock(m_mutex_);
	m_observers_.erase(a_name);
}

void notifly::remove_all_observers(notification_itr_t& a_notification)
{
	std::lock_guard a_lock(m_mutex_);
	if (a_notification != m_observers_.end())
	{
		m_observers_.erase(a_notification);
	}
}

bool notifly::post_notification(const int a_notification, const std::any& a_payload, bool a_async)
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
		printf("WARNING: Notification \"%d\" does not exist.\n", a_notification);
		return false;
	}
}

bool notifly::post_notification(notification_itr_t& a_notification, const std::any& a_payload, bool a_async)
{
	std::lock_guard a_lock(m_mutex_);
	if (a_notification != m_observers_.end())
	{
		const auto& a_notification_list = a_notification->second;
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
		printf("WARNING: Notification \"%d\" does not exist.\n", a_notification->first);
		return false;
	}
}

notifly::notification_itr_t notifly::get_notification_iterator(const int a_notification)
{
	notification_itr_t a_return_value;
	if (m_observers_.find(a_notification) != m_observers_.end())
	{
		a_return_value = m_observers_.find(a_notification);
	}

	return a_return_value;
}

notifly& notifly::default_notifly()
{
	// Guaranteed to be destroyed. Instantiated on first use.
	// ReSharper disable once CommentTypo
	static notifly a_notification;  // NOLINT(clang-diagnostic-exit-time-destructors)

	return a_notification;
}


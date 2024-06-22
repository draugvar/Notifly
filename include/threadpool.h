/*
 *  threadpool.h
 *  threadpool
 *
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

#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <exception>
#include <future>
#include <mutex>
#include <queue>

// thread pool to run user's functors with signature
//      ret func(int id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type

namespace tp
{
	namespace detail
	{
		template <typename T>
		class Queue
		{
		public:
			bool push(T const & value)
			{
				std::unique_lock<std::mutex> lock(this->mutex);
				this->q.push(value);
				return true;
			}
			// deletes the retrieved element, do not use for non integral types
			bool pop(T & v)
			{
				std::unique_lock<std::mutex> lock(this->mutex);
				if (this->q.empty())
					return false;
				v = this->q.front();
				this->q.pop();
				return true;
			}
			bool empty()
			{
				std::unique_lock<std::mutex> lock(this->mutex);
				return this->q.empty();
			}
		private:
			std::queue<T> q;
			std::mutex mutex;
		};
	}

	class thread_pool
	{
	public:

		thread_pool() { this->init(); }
		explicit thread_pool(int nThreads) { this->init(); this->resize(nThreads); }

		// the destructor waits for all the functions in the queue to be finished
		~thread_pool()
		{
			this->stop(true);
		}

		// get the number of running threads in the pool
		int size() { return static_cast<int>(this->threads.size()); }

		// number of idle threads
		int n_idle() { return this->nWaiting; }
		std::thread & get_thread(int i) { return *this->threads[i]; }

		// change the number of threads in the pool
		// should be called from one thread, otherwise be careful to not interleave, also with this->stop()
		// nThreads must be >= 0
		void resize(int nThreads)
		{
			if (!this->isStop && !this->isDone)
			{
				int oldNThreads = static_cast<int>(this->threads.size());
				if (oldNThreads <= nThreads)
				{  // if the number of threads is increased
					this->threads.resize(nThreads);
					this->flags.resize(nThreads);

					for (int i = oldNThreads; i < nThreads; ++i)
					{
						this->flags[i] = std::make_shared<std::atomic<bool>>(false);
						this->set_thread(i);
					}
				}
				else
				{  // the number of threads is decreased
					for (int i = oldNThreads - 1; i >= nThreads; --i)
					{
						*this->flags[i] = true;  // this thread will finish
						this->threads[i]->detach();
					}
					{
						// stop the detached threads that were waiting
						std::unique_lock<std::mutex> lock(this->mutex);
						this->cv.notify_all();
					}
					this->threads.resize(nThreads);  // safe to delete because the threads are detached
					this->flags.resize(nThreads);  // safe to delete because the threads have copies of shared_ptr of the flags, not originals
				}
			}
		}

		// empty the queue
		void clear_queue()
		{
			std::function<void(int id)> * _f;
			while (this->q.pop(_f))
				delete _f; // empty the queue
		}

		// pops a functional wrapper to the original function
		std::function<void(int)> pop()
		{
			std::function<void(int id)> * _f = nullptr;
			this->q.pop(_f);
			std::unique_ptr<std::function<void(int id)>> func(_f); // at return, delete the function even if an exception occurred
			std::function<void(int)> f;
			if (_f)
				f = *_f;
			return f;
		}

		// wait for all computing threads to finish and stop all threads
		// may be called asynchronously to not pause the calling thread while waiting
		// if isWait == true, all the functions in the queue are run, otherwise the queue is cleared without running the functions
		void stop(bool isWait = false)
		{
			if (!isWait)
			{
				if (this->isStop)
					return;
				this->isStop = true;
				for (int i = 0, n = this->size(); i < n; ++i)
				{
					*this->flags[i] = true;  // command the threads to stop
				}
				this->clear_queue();  // empty the queue
			}
			else
			{
				if (this->isDone || this->isStop)
					return;
				this->isDone = true;  // give the waiting threads a command to finish
			}
			{
				std::unique_lock<std::mutex> lock(this->mutex);
				this->cv.notify_all();  // stop all waiting threads
			}
			for (auto & thread : this->threads)
			{  // wait for the computing threads to finish
				if (thread->joinable())
					thread->join();
			}
			// if there were no threads in the pool but some functors in the queue, the functors are not deleted by the threads
			// therefore delete them here
			this->clear_queue();
			this->threads.clear();
			this->flags.clear();
		}

		template<typename F, typename... Rest>
		auto push(F && f, Rest&&... rest) ->std::future<decltype(f(0, rest...))>
		{
			auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
					std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...)
			);
			auto _f = new std::function<void(int id)>([pck](int id)
			{
				(*pck)(id);
			});
			this->q.push(_f);
			std::unique_lock<std::mutex> lock(this->mutex);
			this->cv.notify_one();
			return pck->get_future();
		}

		// run the user's function that excepts argument int - id of the running thread. returned value is templatized
		// operator returns std::future, where the user can get the result and rethrow the caught exceptions
		template<typename F>
		auto push(F && f) ->std::future<decltype(f(0))>
		{
			auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(std::forward<F>(f));
			auto _f = new std::function<void(int id)>([pck](int id)
			{
				(*pck)(id);
			});
			this->q.push(_f);
			std::unique_lock<std::mutex> lock(this->mutex);
			this->cv.notify_one();
			return pck->get_future();
		}

// deleted
		thread_pool(const thread_pool &) = delete;
		thread_pool(thread_pool &&) = delete;
		thread_pool & operator=(const thread_pool &) = delete;
		thread_pool & operator=(thread_pool &&) = delete;
	private:
		void set_thread(int i)
		{
			std::shared_ptr<std::atomic<bool>> flag(this->flags[i]); // a copy of the shared ptr to the flag
			auto f = [this, i, flag/* a copy of the shared ptr to the flag */]() {
				std::atomic<bool> & _flag = *flag;
				std::function<void(int id)> * _f;
				bool isPop = this->q.pop(_f);
				while (true)
				{
					while (isPop)
					{  // if there is anything in the queue
						std::unique_ptr<std::function<void(int id)>> func(_f); // at return, delete the function even if an exception occurred
						(*_f)(i);
						if (_flag)
							return;  // the thread is wanted to stop, return even if the queue is not empty yet
						else
							isPop = this->q.pop(_f);
					}
					// the queue is empty here, wait for the next command
					std::unique_lock<std::mutex> lock(this->mutex);
					++this->nWaiting;
					this->cv.wait(lock, [this, &_f, &isPop, &_flag](){ isPop = this->q.pop(_f); return isPop || this->isDone || _flag; });
					--this->nWaiting;
					if (!isPop)
						return;  // if the queue is empty and this->isDone == true or *flag then return
				}
			};
			this->threads[i] = std::make_unique<std::thread>(f); // compiler may not support std::make_unique()
		}

		void init() { this->nWaiting = 0; this->isStop = false; this->isDone = false; }

		std::vector<std::unique_ptr<std::thread>> threads;
		std::vector<std::shared_ptr<std::atomic<bool>>> flags;
		detail::Queue<std::function<void(int id)> *> q;
		std::atomic<bool> isDone;
		std::atomic<bool> isStop;
		std::atomic<int> nWaiting;  // how many threads are waiting

		std::mutex mutex;
		std::condition_variable cv;
	};

}

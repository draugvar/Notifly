/*
 *  PartyThreads.h
 *  PartyThreads
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
#include <thread>
#include <atomic>
#include <vector>
#include <future>
#include <queue>

namespace PartyThreads
{
	constexpr int DEFAULT_STARTING_THREADS = 20;

	namespace detail
	{
		template <typename T>
		/**
         * @brief A thread safe queue
         */
		class Queue
		{
		public:
			/**
			 * @brief           Push an element to the queue
			 * @param aValue    The value to push
			 * @return          True if the value was pushed, false otherwise
			 */
			bool push(T const & aValue)
			{
				// Lock the mutex
				std::unique_lock lock(this->mMutex);
				// Push the value to the queue and return true
				this->mQueue.push(aValue);
				return true;
			}

			/**
			 * @brief			Pop an element from the queue
			 * @param aValue	The value to pop
			 * @return          True if the value was popped, false otherwise
			 */
			bool pop(T & aValue)
			{
				// Lock the mutex
				std::unique_lock lock(this->mMutex);
				// If the queue is empty, return false
				if (this->mQueue.empty())
					return false;
				// Otherwise, pop the front element and return true
				aValue = this->mQueue.front();
				this->mQueue.pop();
				return true;
			}

			/**
			 * @brief           Is the queue empty?
			 * @return          True if the queue is empty, false otherwise
			 */
			bool empty()
			{
				std::unique_lock lock(this->mMutex);
				return this->mQueue.empty();
			}
		private:
			// The queue
			std::queue<T> mQueue;
			// The mutex
			std::mutex mMutex;
		};
	}

	/**
	 * @brief A thread pool
	 */
	class Pool
	{
	public:
		/**
		 * @brief				Constructor
		 * @param aNumThreads	The number of threads
		 */
		explicit Pool(const int aNumThreads = DEFAULT_STARTING_THREADS)
		{
			this->init();
			this->mThreads.resize(aNumThreads);
			for( int i = 0; i < aNumThreads; ++i)
			{
				this->setThread(i);
			}
		}

		/**
         * @brief Destructor
         */
		~Pool()
		{
			// Stop the thread pool
			this->stop(true);
		}

		/**
         * @brief Get the number of threads
         * @return The number of threads
         */
		int size() const { return static_cast<int>(this->mThreads.size()); }

		/**
         * @brief Get the number of idle threads
         * @return The number of idle threads
         */
		int nIdle() { return this->mWaiting; }
		
		/**
         * @brief           Clear the queue
         */
		void clearQueue()
		{
			std::function<void()>* func;
			while (this->mQueue.pop(func))
				delete func; // empty the queue
		}

		/**
         * @brief           Stop the thread pool
         * @details         Wait for the threads to finish
         * @param aWait		Should we wait for the threads to finish?
         */
		void stop(const bool aWait = false)
		{
			// set the stopping flag
			this->mStopping.test_and_set();
			{
				std::unique_lock lock(this->mMutex);
				this->mCv.notify_all();  // notify all waiting threads
			}
			if (aWait)
			{
				// wait for the threads to finish
				for (auto & [ thread, promise ] : this->mThreads)
				{
					// Check if the thread has started
					promise.get_future().get();
					// Wait for the thread to finish
					thread.wait();
				}
				// clear the threads
				this->mThreads.clear();
			}
			
			// if there were no threads in the pool but some functors in the queue,
			// the functors are not deleted by the threads therefore delete them here
			this->clearQueue();
		}

		/**
		 * @brief           Push a function to the queue
		 * @param aFunc     The function
		 * @param aArgs     The arguments
		 * @return          The future
		 */
		template<typename F, typename... Args>
		auto push(F&& aFunc, Args&&... aArgs) -> std::future<decltype(aFunc(aArgs...))>
		{
			auto task = std::make_shared<std::packaged_task<decltype(aFunc(aArgs...))()>>(
					std::bind(std::forward<F>(aFunc), std::forward<Args>(aArgs)...)
			);

			// wrap the function with the arguments into a std::function
			auto func = new std::function<void()>([task]{ (*task)(); });

			// push the function to the queue
			mQueue.push(func);
			std::unique_lock lock(this->mMutex);
			mCv.notify_one();

			return task->get_future();
		}

		/// 
		/// Deleted functions
		/// 
		Pool(const Pool &) = delete;
		Pool(Pool &&) = delete;
		Pool & operator=(const Pool &) = delete;
		Pool & operator=(Pool &&) = delete;
	private:
		
		/**
		 * @brief           Set a thread
		 * @param aIndex    The index of the thread
		 */
		void setThread(const int aIndex)
		{
			// The promise is used to signal the thread has started
			this->mThreads[aIndex].second = std::promise<void>();
			// Threads are started with a lambda function
			this->mThreads[aIndex].first = std::async([this, aIndex]
			{
				// Signal the thread has started
				this->mThreads[aIndex].second.set_value();
				std::function<void()>* func = nullptr;
				while (!this->mStopping.test())
				{
					// Pop the function from the queue
					if (const bool isPop = this->mQueue.pop(func); !isPop)
					{
						// If there is nothing in the queue, wait
						std::unique_lock lock(this->mMutex);
						++this->mWaiting;
						this->mCv.wait(lock, [this, &func]
						{
							// Check if the thread should stop or if there is a function in the queue
							return this->mStopping.test() || this->mQueue.pop(func);
						});
						--this->mWaiting;
					}
					// Execute the function
					if (func) (*func)();
				}
			});
		}

		/**
		 * @brief Initialize the thread pool
		 */
		void init() { this->mWaiting = 0; }

		// threads
		std::vector<std::pair<std::future<void>, std::promise<void>>> mThreads;
		std::atomic_flag mStopping;
		
		// the task queue
		detail::Queue<std::function<void()>*> mQueue;
		// how many threads are waiting
		std::atomic<int> mWaiting;

		// mutex and conditional variable
		std::mutex mMutex;
		std::condition_variable mCv;
	};
}
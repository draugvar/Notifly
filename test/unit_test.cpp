#include <gtest/gtest.h>
#include <future>
#include <atomic>
#include <chrono>
#include <thread>
#include "notifly.h"
#include "unit_test.h"

using namespace std::chrono_literals;

TEST(notifly, version)
{
    printf("Version: %d.%d.%d\n", NOTIFLY_VERSION_MAJOR, NOTIFLY_VERSION_MINOR, NOTIFLY_VERSION_PATCH);
    // print hexadecimal version
    printf("Version hex: 0x%.6x\n", NOTIFLY_VERSION);
}

TEST(notifly, func_add_observer)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);

    const auto ret = notifly::default_notifly().post_notification(poster, 5, 0x100000000);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::payload_type_not_match));
}

TEST(notifly, add_observer_struct)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, print_struct);

    constexpr point a_point = {0, 0};
    // We are passing a struct by value when we should pass it by reference as the observer is expecting a pointer,
    // so it will fail.
    const auto ret = notifly::default_notifly().post_notification(poster, a_point);
    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::payload_type_not_match));
}

TEST(notifly, struct_add_observer_and_post_message)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, print_struct);

    constexpr point p = {10, 20};

    const auto ret = notifly::default_notifly().post_notification_async(poster, &p);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    notifly::default_notifly().remove_observer(i1);
    ASSERT_GE(ret, 0);
}

TEST(notifly, lambda_and_post_message)
{
    const auto lambda = std::function([](const int a, const int b) -> int
    {
        printf("Sum is %d\n", a + b);
        return a + b;
    });

    const auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    const auto ret = notifly::default_notifly().post_notification(poster, 5, 10);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_GE(ret, 0);
}

TEST(notifly, nothing_to_lambda)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, []
    {
        printf("No payload!\n");
        return 1;
    });

    const auto ret = notifly::default_notifly().post_notification(poster);
    notifly::default_notifly().remove_observer(i1);

    ASSERT_GE(ret, 0);
}

TEST(notifly, int_to_nothing)
{
    const auto ret = notifly::default_notifly().post_notification(poster, 5);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::notification_not_found));
}

TEST(notifly, add_different_observers)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);
    auto i2 = notifly::default_notifly().add_observer(poster, print_struct);

    const auto ret1 = notifly::default_notifly().post_notification(poster, i1, i2);
    const auto ret2 = notifly::default_notifly().post_notification(poster, &i2);

    notifly::default_notifly().remove_observer(i1);
    notifly::default_notifly().remove_observer(i2);

    ASSERT_EQ(i2, static_cast<int>(notifly_result::payload_type_not_match));
    ASSERT_GE(ret1, 0);
    ASSERT_EQ(ret2, static_cast<int>(notifly_result::payload_type_not_match));
}

TEST(notifly, critical_section)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;

    const auto i1 = notifly::default_notifly().add_observer(poster, critical_section);

    const auto ret = notifly::default_notifly().post_notification_async
    (
            poster,
            &cv,
            &mutex,
            &ready
    );

    ASSERT_GE(ret, 0);

    // Notify the observer that it can proceed
    {
        std::unique_lock lock(mutex);
        ready = true;
    }
    cv.notify_one();

    // Wait for the observer to finish
    {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&ready] { return !ready; });
    }

    notifly::default_notifly().remove_observer(i1);
    ASSERT_GE(ret, 0);
    ASSERT_EQ(ready, false);
}

TEST(notifly, different_notifly_instances)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);
    notifly another_notifly;
    const auto i2 = another_notifly.add_observer(poster, sum_callback);

    const auto ret1 = notifly::default_notifly().post_notification_async(poster, i1, i2);
    const auto ret2 = another_notifly.post_notification(poster, i1, i2);

    notifly::default_notifly().remove_observer(i1);
    another_notifly.remove_observer(i2);

    ASSERT_GE(i1, 0);
    ASSERT_EQ(i2, 1);
    ASSERT_GE(ret1, 0);
    ASSERT_GE(ret2, 0);
}

TEST(notifly, multi_threads)
{
    const auto ret = notifly::default_notifly().add_observer(poster, just_increment_and_print);

    // 100 threads will increment the value 10 times
    std::atomic_int a_value = 0;
    for(int i = 0; i < 100; i++)
    {
        notifly::default_notifly().post_notification_async(poster, &a_value);
    }
    while(a_value < 1000)
    {
        std::this_thread::yield();
    }

    notifly::default_notifly().remove_observer(ret);
    ASSERT_GE(ret, 0);
}

TEST(notifly, check_ids)
{
    auto id_1 = notifly::default_notifly().add_observer(poster, sum_callback);
    const auto id_2 = notifly::default_notifly().add_observer(poster, sum_callback);

    notifly::default_notifly().remove_observer(id_1);

    id_1 = notifly::default_notifly().add_observer(poster, sum_callback);

    notifly::default_notifly().remove_observer(id_2);
    notifly::default_notifly().remove_observer(id_1);

    ASSERT_EQ(id_1, 1);
    ASSERT_EQ(id_2, 2);
    ASSERT_EQ(id_1, 1);
}

TEST(notifly, remove_id_0)
{
    notifly::default_notifly().remove_observer(0);
    const auto id_1 = notifly::default_notifly().add_observer(poster, print_struct);

    notifly::default_notifly().remove_observer(id_1);
    ASSERT_EQ(id_1, 1);
}

TEST(notifly, remove_id_not_found)
{
    const auto id_1 = notifly::default_notifly().add_observer(poster, sum_callback);
    const auto id_2 = notifly::default_notifly().add_observer(poster, divide_callback);

    const auto ret = notifly::default_notifly().post_notification(poster, 5, 3);

    notifly::default_notifly().remove_observer(id_1);
    notifly::default_notifly().remove_observer(id_2);

    ASSERT_GE(id_1, 1);
    ASSERT_GE(id_2, 1);
    ASSERT_EQ(ret, 2);
}

TEST(notifly, test_wrong_reference)
{
    const auto lambda = std::function([](const int& a) -> int
    {
        printf("The reference is %d\n", a);
        return 0;
    });
    const auto id_1 = notifly::default_notifly().add_observer(poster, lambda);

    const auto ret = notifly::default_notifly().post_notification(poster, 5);

    notifly::default_notifly().remove_observer(id_1);

    ASSERT_GE(id_1, 1);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::payload_type_not_match));
}

TEST(notifly, multiple_observers)
{
    std::vector<int> observers;
    observers.reserve(100);
    for(auto i = 0; i < 100; ++i)
    {
        auto id = notifly::default_notifly().add_observer(poster, sum_callback);
        observers.push_back(id);
    }

    const auto ret_sync = notifly::default_notifly().post_notification(poster, 9, 9);
    const auto ret_async = notifly::default_notifly().post_notification_async(poster, 9, 9);
    for(const auto& observer: observers)
    {
        notifly::default_notifly().remove_observer(observer);
    }
    ASSERT_EQ(ret_sync, 100);
    ASSERT_EQ(ret_async, 100);
}

TEST(notifly, no_params)
{
    const auto id = notifly::default_notifly().add_observer(poster, no_params);
    const auto ret = notifly::default_notifly().post_notification(poster);

    ASSERT_GE(id, 0);
    ASSERT_GE(ret, 0);

    notifly::default_notifly().remove_observer(id);
}

TEST(notifly, lambda_no_params)
{
    auto lambda = []() -> int
    {
        printf("No params\n");
        return 0;
    };

    const auto id = notifly::default_notifly().add_observer(poster, lambda);
    const auto ret = notifly::default_notifly().post_notification(poster);

    ASSERT_GE(id, 0);
    ASSERT_GE(ret, 0);

    notifly::default_notifly().remove_observer(id);
}

TEST(notifly, lambda_no_params_return_void)
{
    auto lambda = []
    {
        printf("No params\n");
    };

    const auto id = notifly::default_notifly().add_observer(poster, lambda);
    const auto ret = notifly::default_notifly().post_notification(poster);

    ASSERT_GE(id, 0);
    ASSERT_GE(ret, 0);

    notifly::default_notifly().remove_observer(id);
}

TEST(notifly, void_no_params)
{
    const auto id = notifly::default_notifly().add_observer(poster, void_no_params);
    const auto ret = notifly::default_notifly().post_notification(poster);

    ASSERT_GE(id, 0);
    ASSERT_GE(ret, 0);

    notifly::default_notifly().remove_observer(id);
}

TEST(notifly, remove_observers)
{
    const auto id = notifly::default_notifly().add_observer(poster, sum_callback);
    const auto ret = notifly::default_notifly().remove_observer(id);

    ASSERT_GE(id, 0);
    ASSERT_EQ(ret, 0);
}

TEST(notifly, fail_remove_observers)
{
    const auto ret = notifly::default_notifly().remove_observer(0xFF);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::observer_not_found));
}

TEST(notifly, remove_all_observers)
{
    const auto ret = notifly::default_notifly().remove_all_observers(poster);

    notifly::default_notifly().add_observer(poster, sum_callback);
    notifly::default_notifly().add_observer(poster, sum_callback);
    notifly::default_notifly().add_observer(poster, sum_callback);
    notifly::default_notifly().add_observer(poster, sum_callback);
    notifly::default_notifly().add_observer(poster, sum_callback);

    const auto ret_all = notifly::default_notifly().remove_all_observers(poster);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(ret_all, 5);
}

TEST(notifly, post_notification_with_deleted_payload)
{
    std::promise<void> promise;
    {
        int a = 5;
        auto lambda = [&a, &promise](const int* a_ptr)
        {
            printf("a = %d\n", a);
            printf("a_ptr = %p\n", a_ptr);
            printf("*a_ptr = %d\n", *a_ptr);
            promise.set_value();
        };

        const auto id = notifly::default_notifly().add_observer(poster, lambda);
        ASSERT_GE(id, 0);

    }
    {
        constexpr int a = 10;
        const auto ret = notifly::default_notifly().post_notification_async(poster, &a);
        ASSERT_GE(ret, 0);  
    }
    promise.get_future().get();
}

TEST(notifly, delete_notifly)
{
    auto lambda = []() -> int
    {
        printf("No params\n");
        return 0;
    };

    const auto notifly_ptr = new notifly();
    notifly_ptr->add_observer(poster, lambda);
    notifly_ptr->post_notification(poster);

    delete notifly_ptr;
}

TEST(notifly, delete_no_notification)
{
    const auto notifly_ptr = new notifly();
    delete notifly_ptr;
}

TEST(notifly, post_and_wait_success)
{
    // Clean up any existing observers to avoid type conflicts
    notifly::default_notifly().remove_all_observers(poster);
    notifly::default_notifly().remove_all_observers(second_poster);

    // Setup: register responder observer that listens for the request
    const auto responder_id = notifly::default_notifly().add_observer(poster,
        [](int, int)
    {
        // Simulate some processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Send response on second_poster
        notifly::default_notifly().post_notification(second_poster, 42, 100);
    });

    // Test: post request and wait for response
    std::tuple<int, int> result;
    const auto ret = notifly::default_notifly().post_and_wait(
        poster,          // Send request on this notification
        second_poster,   // Wait for response on this notification
        500,             // 500ms timeout
        result,          // Output parameter
        1, 2             // Request payload
    );

    ASSERT_EQ(ret, notifly_result::success);
    ASSERT_EQ(std::get<0>(result), 42);
    ASSERT_EQ(std::get<1>(result), 100);

    // Cleanup
    notifly::default_notifly().remove_observer(responder_id);
    notifly::default_notifly().remove_all_observers(poster);
    notifly::default_notifly().remove_all_observers(second_poster);
}

TEST(notifly, post_and_wait_timeout)
{
    // Clean up any existing observers to avoid type conflicts
    notifly::default_notifly().remove_all_observers(third_poster);
    notifly::default_notifly().remove_all_observers(fourth_poster);

    // Setup: add a dummy observer on third_poster that receives the request but doesn't respond
    const auto dummy_observer_id = notifly::default_notifly().add_observer(third_poster,
        [](const int a, const int b)
        {
            // Receive the request but deliberately don't send any response
            printf("Request received (%d, %d) but not responding\n", a, b);
        });

    // Test: post request but no one responds, should timeout
    std::tuple<int, int> result;
    const auto ret = notifly::default_notifly().post_and_wait(
        third_poster,    // Send request on this notification
        fourth_poster,   // Wait for response on this notification (no one listening)
        100,             // 100ms timeout
        result,          // Output parameter
        1, 2             // Request payload
    );

    ASSERT_EQ(ret, notifly_result::timeout);

    // Cleanup
    notifly::default_notifly().remove_observer(dummy_observer_id);
    notifly::default_notifly().remove_all_observers(third_poster);
    notifly::default_notifly().remove_all_observers(fourth_poster);
}

TEST(notifly, post_and_wait_with_observer)
{
    // Clean up any existing observers to avoid type conflicts
    notifly::default_notifly().remove_all_observers(poster);
    notifly::default_notifly().remove_all_observers(second_poster);

    // Setup: add observer that will respond
    const auto observer_id = notifly::default_notifly().add_observer(poster,
        [](const int a, const int b)
        {
            // Simulate some processing
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // Send response
            notifly::default_notifly().post_notification(second_poster, a + b, a * b);
        });

    // Test: post and wait
    std::tuple<int, int> result;
    const auto ret = notifly::default_notifly().post_and_wait(
        poster,
        second_poster,
        200,
        result,
        5, 10
    );

    ASSERT_EQ(ret, notifly_result::success);
    ASSERT_EQ(std::get<0>(result), 15);  // 5 + 10
    ASSERT_EQ(std::get<1>(result), 50);  // 5 * 10

    notifly::default_notifly().remove_observer(observer_id);
}

TEST(notifly, post_and_wait_single_param)
{
    // Clean up any existing observers to avoid type conflicts
    notifly::default_notifly().remove_all_observers(third_poster);
    notifly::default_notifly().remove_all_observers(fourth_poster);

    // Setup: register responder observer that listens for the request
    const auto responder_id = notifly::default_notifly().add_observer(third_poster, []
    {
        // Simulate some processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Send response on fourth_poster
        notifly::default_notifly().post_notification(fourth_poster, std::string("Hello World"));
    });

    // Test: post and wait for single string parameter
    std::string result;
    const auto ret = notifly::default_notifly().post_and_wait(
        third_poster,
        fourth_poster,
        200,
        result
    );

    ASSERT_EQ(ret, notifly_result::success);
    ASSERT_EQ(result, "Hello World");

    // Cleanup
    notifly::default_notifly().remove_observer(responder_id);
    notifly::default_notifly().remove_all_observers(third_poster);
    notifly::default_notifly().remove_all_observers(fourth_poster);
}

enum TestNotification : int
{
    kTestNotification = 1000,
    kTestNotification2 = 1001,
};

// Test 1: Completed async tasks are cleaned up on next post
TEST(NotiflyMemoryLeak, AsyncTaskCleanup)
{
    notifly nf;
    std::atomic call_count{0};

    nf.add_observer(kTestNotification, [&call_count](int) {
        call_count.fetch_add(1, std::memory_order_relaxed);
    });

    // Post 100 async notifications
    for (int i = 0; i < 100; ++i)
    {
        nf.post_notification_async(kTestNotification, i);
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(call_count.load(), 100);

    // Post one more to trigger cleanup
    nf.post_notification_async(kTestNotification, 999);
    std::this_thread::sleep_for(50ms);

    // After cleanup, pending count should be very small (just the last one, or 0 if it completed)
    EXPECT_LE(nf.pending_async_task_count(), 1u);
}

// Test 2: Async tasks do not grow unbounded under sustained load
TEST(NotiflyMemoryLeak, AsyncTasksDoNotGrowUnbounded)
{
    notifly nf;
    std::atomic call_count{0};

    nf.add_observer(kTestNotification, [&call_count](int) {
        call_count.fetch_add(1, std::memory_order_relaxed);
    });

    // Post 1000 async notifications with small delays
    for (int i = 0; i < 1000; ++i)
    {
        nf.post_notification_async(kTestNotification, i);
        if (i % 10 == 0)
            std::this_thread::sleep_for(1ms);
    }

    // The task count should stay bounded - completed tasks are cleaned on each post
    // Allow generous upper bound, but it should not be anywhere near 1000
    EXPECT_LT(nf.pending_async_task_count(), 100u);
}

// Test 3: Destructor cleans up all tasks without crashing or hanging
TEST(NotiflyMemoryLeak, DestructorCleansUpAllTasks)
{
    auto done = std::make_shared<std::atomic<bool>>(false);

    std::thread test_thread([done]() {
        auto nf = std::make_unique<notifly>();
        std::atomic call_count{0};

        nf->add_observer(kTestNotification, [&call_count](int) {
            std::this_thread::sleep_for(10ms);
            call_count.fetch_add(1, std::memory_order_relaxed);
        });

        for (int i = 0; i < 20; ++i)
        {
            nf->post_notification_async(kTestNotification, i);
        }

        // Destroy while tasks may still be running - should join cleanly
        nf.reset();
        done->store(true, std::memory_order_release);
    });

    // Guard against hang: wait up to 10 seconds
    const auto start = std::chrono::steady_clock::now();
    while (!done->load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() - start > 10s)
        {
            FAIL() << "Destructor appears to have hung";
        }
        std::this_thread::sleep_for(50ms);
    }

    test_thread.join();
}

// Test 4: Synchronous notifications still work correctly
TEST(NotiflyMemoryLeak, SyncNotificationsStillWork)
{
    notifly nf;
    int received_value = 0;

    nf.add_observer(kTestNotification, [&received_value](const int value) {
        received_value = value;
    });

    nf.post_notification(kTestNotification, 42);
    EXPECT_EQ(received_value, 42);

    nf.post_notification(kTestNotification, 100);
    EXPECT_EQ(received_value, 100);
}

// Test 5: Removing an observer with pending async tasks doesn't crash
TEST(NotiflyMemoryLeak, RemoveObserverWithPendingAsyncTasks)
{
    notifly nf;
    std::atomic call_count{0};

    const int observer_id = nf.add_observer(kTestNotification, [&call_count](int) {
        std::this_thread::sleep_for(50ms);
        call_count.fetch_add(1, std::memory_order_relaxed);
    });

    // Post async notifications to a slow observer
    for (int i = 0; i < 5; ++i)
    {
        nf.post_notification_async(kTestNotification, i);
    }

    // Remove observer while tasks are still running - should wait and clean up
    const int result = nf.remove_observer(observer_id);
    EXPECT_EQ(result, static_cast<int>(notifly_result::success));

    // All tasks should have completed since remove_observer waits
    EXPECT_EQ(call_count.load(), 5);
}

// Test 6: Multiple observers on the same notification both get called
TEST(NotiflyMemoryLeak, MultipleObserversSameNotification)
{
    notifly nf;
    std::atomic count_a{0};
    std::atomic count_b{0};

    nf.add_observer(kTestNotification, [&count_a](int) {
        count_a.fetch_add(1, std::memory_order_relaxed);
    });

    nf.add_observer(kTestNotification, [&count_b](int) {
        count_b.fetch_add(1, std::memory_order_relaxed);
    });

    for (int i = 0; i < 10; ++i)
    {
        nf.post_notification_async(kTestNotification, i);
    }

    // Wait for completion
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(count_a.load(), 10);
    EXPECT_EQ(count_b.load(), 10);
}

// Test 7: High-frequency async dispatch stays bounded (customer regression scenario)
TEST(NotiflyMemoryLeak, HighFrequencyAsyncDispatch)
{
    notifly nf;
    std::atomic call_count{0};

    nf.add_observer(kTestNotification, [&call_count](int) {
        call_count.fetch_add(1, std::memory_order_relaxed);
    });

    // Simulate the customer scenario: tight loop posting async notifications
    constexpr int total_posts = 5000;
    size_t max_pending = 0;

    for (int i = 0; i < total_posts; ++i)
    {
        nf.post_notification_async(kTestNotification, i);

        // Periodically check the pending count
        if (i % 100 == 0)
        {
            size_t current = nf.pending_async_task_count();
            max_pending = std::max(max_pending, current);
        }
    }

    // The max pending should stay bounded - should never approach total_posts
    EXPECT_LT(max_pending, 500u)
        << "Pending task count reached " << max_pending
        << " which suggests completed tasks are not being reclaimed";

    // Wait for everything to finish
    std::this_thread::sleep_for(1s);
    EXPECT_EQ(call_count.load(), total_posts);
}

// Test 8: Completed tasks are reclaimed after triggering cleanup
TEST(NotiflyMemoryLeak, CompletedTasksAreReclaimed)
{
    notifly nf;
    std::atomic call_count{0};

    nf.add_observer(kTestNotification, [&call_count](int) {
        call_count.fetch_add(1, std::memory_order_relaxed);
    });

    // Post N async tasks
    constexpr int N = 50;
    for (int i = 0; i < N; ++i)
    {
        nf.post_notification_async(kTestNotification, i);
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(call_count.load(), N);

    // Post one more to trigger cleanup of completed tasks
    nf.post_notification_async(kTestNotification, 999);
    std::this_thread::sleep_for(50ms);

    // Should only have ~1 task (the most recent one, possibly already completed)
    const size_t count = nf.pending_async_task_count();
    EXPECT_LE(count, 1u)
        << "Expected at most 1 pending task after cleanup, got " << count;
}


//
// Created by Salvatore Rivieccio
//
#include <gtest/gtest.h>

#include "notifly.h"
#include "unit_test.h"

TEST(notifly, version)
{
    printf("Version: %d.%d.%d\n", NOTIFLY_VERSION_MAJOR, NOTIFLY_VERSION_MINOR, NOTIFLY_VERSION_PATCH);
    // print hexadecimal version
    printf("Version hex: 0x%.6x\n", NOTIFLY_VERSION);
}

TEST(notifly, func_add_observer)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);

    const auto ret = notifly::default_notifly().post_notification<int, long>(poster, 5, 10);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::payload_type_not_match));
}

TEST(notifly, add_observer_struct)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, print_struct);

    const point a_point = {0, 0};
    // We are passing a struct by value when we should pass it by reference as the observer is expecting a pointer,
    // so it will fail.
    const auto ret = notifly::default_notifly().post_notification<point>(poster, a_point);
    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::payload_type_not_match));
}

TEST(notifly, struct_add_observer_and_post_message)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, print_struct);

    point p = {0, 0};
    p.x = 10;
    p.y = 20;

    const auto ret = notifly::default_notifly().post_notification<point*>(poster, &p, true);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    notifly::default_notifly().remove_observer(i1);
    ASSERT_GE(ret, 0);
}

TEST(notifly, lambda_and_post_message)
{
    const auto lambda = std::function<int(int, int)>([](int a, int b) -> int
    {
        printf("Sum is %d\n", a + b);
        return a + b;
    });

    const auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    const auto ret = notifly::default_notifly().post_notification<int, int>(poster, 5, 10);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_GE(ret, 0);
}

TEST(notifly, nothing_to_lambda)
{
    const auto lambda = std::function<std::any()>([]() -> std::any
    {
        printf("No payload!\n");
        return 1;
    });

    const auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    const auto ret = notifly::default_notifly().post_notification(poster);
    notifly::default_notifly().remove_observer(i1);

    ASSERT_GE(ret, 0);
}

TEST(notifly, int_to_nothing)
{
    const auto ret = notifly::default_notifly().post_notification<int>(poster, 5);
    ASSERT_EQ(ret, static_cast<int>(notifly_result::notification_not_found));
}

TEST(notifly, add_different_observers)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);
    auto i2 = notifly::default_notifly().add_observer(poster, print_struct);

    const auto ret1 = notifly::default_notifly().post_notification<int, int>(poster, (int)i1, (int)i2);
    const auto ret2 = notifly::default_notifly().post_notification<int*>(poster, &i2);

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
    bool notify = false;

    const auto i1 = notifly::default_notifly().add_observer(poster, critical_section);

    const auto ret = notifly::default_notifly().post_notification<std::condition_variable*, std::mutex*, const bool*, bool*>
            (
                    poster,
                    &cv,
                    &mutex,
                    &ready,
                    &notify,
                    true
            );

    // Notify the observer that it can proceed
    {
        std::unique_lock<std::mutex> lock(mutex);
        ready = true;
    }
    cv.notify_one();

    // Wait for the observer to finish
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&notify] { return notify; });
    }

    notifly::default_notifly().remove_observer(i1);
    ASSERT_GE(ret, 0);
    ASSERT_EQ(notify, true);
}

TEST(notifly, different_notifly_instances)
{
    const auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);
    notifly another_notifly;
    const auto i2 = another_notifly.add_observer(poster, sum_callback);

    const auto ret1 = notifly::default_notifly().post_notification<int, int>(poster, (int) i1, (int) i2, true);
    const auto ret2 = another_notifly.post_notification<int, int>(poster, (int) i1, (int) i2);

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
        notifly::default_notifly().post_notification<std::atomic_int*>(poster, &a_value, true);
    }
    while(a_value < 1000)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

    const auto ret = notifly::default_notifly().post_notification<int, int>(poster, 5, 3);

    notifly::default_notifly().remove_observer(id_1);
    notifly::default_notifly().remove_observer(id_2);

    ASSERT_GE(id_1, 1);
    ASSERT_GE(id_2, 1);
    ASSERT_EQ(ret, 2);
}

TEST(notifly, test_wrong_reference)
{
    const auto lambda = std::function<int(int&)>([](int& a) -> int
    {
        printf("The reference is %d\n", a);
        return 0;
    });
    const auto id_1 = notifly::default_notifly().add_observer(poster, lambda);

    const auto ret = notifly::default_notifly().post_notification<int>(poster, 5);

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

    const auto ret_sync = notifly::default_notifly().post_notification<int, int>(poster, 9, 9);
    const auto ret_async = notifly::default_notifly().post_notification<int, int>(poster, 9, 9, true);
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
    auto lambda = []()
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
        const auto ret = notifly::default_notifly().post_notification<const int*>(poster, &a, true);
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

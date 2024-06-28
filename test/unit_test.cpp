//
// Created by Salvatore Rivieccio
//
#include <gtest/gtest.h>
#include <unordered_map>

#include "notifly.h"
#include "unit_test.h"

TEST(notifly, func_add_observer)
{
    auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);

    auto ret = notifly::default_notifly().post_notification<int, long>(poster, 5, 10);
    if(!ret)
    {
        printf("Failed to post notification: %s\n", notifly::default_notifly().get_last_error().c_str());
    }

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, false);
}

TEST(notifly, add_observer_struct)
{
    auto i1 = notifly::default_notifly().add_observer(poster, print_struct);

    point a_point = {0, 0};
    // We are passing a struct by value when we should pass it by reference as the observer is expecting a pointer,
    // so it will fail.
    auto ret = notifly::default_notifly().post_notification<point>(poster, a_point);
    if(!ret)
    {
        printf("Failed to post notification: %s\n", notifly::default_notifly().get_last_error().c_str());
    }

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, false);
}

TEST(notifly, struct_add_observer_and_post_message)
{
    auto i1 = notifly::default_notifly().add_observer(poster, print_struct);

    point p = {0, 0};
    p.x = 10;
    p.y = 20;

    auto ret = notifly::default_notifly().post_notification<point*>(poster, &p, true);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, true);
}

TEST(notifly, lambda_and_post_message)
{
    auto lambda = std::function<int(int, int)>([](int a, int b) -> int
    {
        printf("Sum is %d\n", a + b);
        return a + b;
    });

    auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    auto ret = notifly::default_notifly().post_notification<int, int>(poster, 5, 10);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, true);
}

TEST(notifly, nothing_to_lambda)
{
    auto lambda = std::function<std::any()>([]() -> std::any
    {
        printf("No payload!\n");
        return 1;
    });

    auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    auto ret = notifly::default_notifly().post_notification(poster);
    ASSERT_EQ(ret, true);

    notifly::default_notifly().remove_observer(i1);
}

TEST(notifly, int_to_nothing)
{
    auto ret = notifly::default_notifly().post_notification<int>(poster, 5);
    if(!ret)
    {
        printf("Failed to post notification: %s\n", notifly::default_notifly().get_last_error().c_str());
    }
    ASSERT_EQ(ret, false);
}

TEST(notifly, add_different_observers)
{
    auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);
    auto i2 = notifly::default_notifly().add_observer(poster, print_struct);
    ASSERT_EQ(i2, 0);
    if(i2 == 0)
    {
        printf("Failed to add observer: %s\n", notifly::default_notifly().get_last_error().c_str());
    }

    auto ret = notifly::default_notifly().post_notification<int, int>(poster, (int)i1, (int)i2);
    auto ret2 = notifly::default_notifly().post_notification<int*>(poster, &i2);

    notifly::default_notifly().remove_observer(i1);
    notifly::default_notifly().remove_observer(i2);

    ASSERT_EQ(ret, true);
    ASSERT_EQ(ret2, false);
}

TEST(notifly, critical_section)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
    bool notify = false;

    auto i1 = notifly::default_notifly().add_observer(poster, critical_section);

    auto ret = notifly::default_notifly().post_notification<std::condition_variable*, std::mutex*, const bool*, bool*>
            (
                    poster,
                    &cv,
                    &mutex,
                    &ready,
                    &notify,
                    true
            );
    ASSERT_EQ(ret, true);

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
    ASSERT_EQ(notify, true);
}

TEST(notifly, different_notifly_instances)
{
    auto i1 = notifly::default_notifly().add_observer(poster, sum_callback);
    notifly another_notifly;
    auto i2 = another_notifly.add_observer(poster, sum_callback);
    ASSERT_GT(i1, 0);
    ASSERT_EQ(i2, 1);

    auto ret = notifly::default_notifly().post_notification<int, int>(poster, (int) i1, (int) i2, true);
    ASSERT_EQ(ret, true);
    ret = another_notifly.post_notification<int, int>(poster, (int) i1, (int) i2);
    ASSERT_EQ(ret, true);

    notifly::default_notifly().remove_observer(i1);
    another_notifly.remove_observer(i2);
}

TEST(notifly, multi_threads)
{
    notifly::default_notifly().resize_thread_pool(100);

    auto ret = notifly::default_notifly().add_observer(poster, just_increment_and_print);
    ASSERT_GE(ret, 0);

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
}

TEST(notifly, check_ids)
{
    auto id_1 = notifly::default_notifly().add_observer(poster, sum_callback);
    ASSERT_EQ(id_1, 1);
    auto id_2 = notifly::default_notifly().add_observer(poster, sum_callback);
    ASSERT_EQ(id_2, 2);
    notifly::default_notifly().remove_observer(id_1);

    id_1 = notifly::default_notifly().add_observer(poster, sum_callback);
    ASSERT_EQ(id_1, 1);

    notifly::default_notifly().remove_observer(id_2);
    notifly::default_notifly().remove_observer(id_1);
}

TEST(notifly, remove_id_0)
{
    notifly::default_notifly().remove_observer(0);
    auto id_1 = notifly::default_notifly().add_observer(poster, print_struct);
    ASSERT_EQ(id_1, 1);
    notifly::default_notifly().remove_observer(id_1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
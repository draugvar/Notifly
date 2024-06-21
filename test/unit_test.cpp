//
// Created by Salvatore Rivieccio on 17/06/24.
//
#include <gtest/gtest.h>
#include <unordered_map>

#include "notifly.h"

typedef struct point_
{
    int x, y;
} point;

class foo
{
public:
    // ReSharper disable once CppMemberFunctionMayBeStatic
    static unsigned int func(point* a_point, int a_value)
    {
        printf("Hello std::bind!\n");
        a_point->x = 11;
        a_point->y = 23;
        printf("Hello value %d\n", a_value);
        return 0;
    }
};

enum message
{
    poster,
    second_poster,
    third_poster,
    fourth_poster
};

int sum_callback(int a, int b)
{
    printf("Sum is %d\n", a + b);
    return a + b;
}

int print_struct(point* a_point)
{
    printf("Point x: %d, y: %d\n", a_point->x, a_point->y);
    return 0;
}

TEST(notifly, lambda_add_observer)
{
    auto lambda = [](std::any any) -> std::any
    {
        if(any.has_value())
        {
            auto message = std::any_cast<int*>(any);
            printf("Received notification %d!\n", (*message)++);
            return 0;
        }
        else
        {
            printf("No payload!\n");
            return 1;
        }
    };

    auto i1 = notifly::default_notifly().add_observer(poster,lambda);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(0, 0);
}

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
    // We are passing a struct by value when we should pass it by reference as the observer is expecting a pointer
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

    std::this_thread::sleep_for(std::chrono::seconds(1));

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, true);
}

TEST(notifly, lamda_and_post_message)
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

TEST(notifly, any_to_any)
{
    auto lambda = std::function<std::any(std::any)>([](std::any any) -> std::any
    {
        if(any.has_value())
        {
            auto message = std::any_cast<int*>(any);
            printf("Received notification %d!\n", (*message)++);
            printf("Incremented message to %d\n", *message);
            return 0;
        }
        else
        {
            printf("No payload!\n");
            return 1;
        }
    });

    auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    int message = 0;
    auto ret = notifly::default_notifly().post_notification<std::any>(poster, &message);
    printf("Message: %d\n", message);
    ASSERT_EQ(message, 1);

    notifly::default_notifly().remove_observer(i1);
    ASSERT_EQ(ret, true);
}

TEST(notifly, nothing_to_wrong_lambda)
{
    auto lambda = std::function<std::any(std::any)>([](const std::any& any) -> std::any
    {
        if(any.has_value())
        {
            return 0;
        }
        else
        {
            printf("No payload!\n");
            return 1;
        }
    });

    auto i1 = notifly::default_notifly().add_observer(poster, lambda);

    auto ret = notifly::default_notifly().post_notification(poster);
    ASSERT_EQ(ret, false);

    notifly::default_notifly().remove_observer(i1);
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

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
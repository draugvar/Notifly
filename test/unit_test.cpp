//
// Created by Salvatore Rivieccio on 17/06/24.
//
#include <gtest/gtest.h>
#include <unordered_map>

#include "notifly.h"

struct point
{
    int x, y;
};

class foo
{
public:
    // ReSharper disable once CppMemberFunctionMayBeStatic
    static unsigned int func(struct point* a_point, int a_value)
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

int sum(int a, int b)
{
    printf("Sum is %d\n", a + b);
    return a + b;
}

TEST(notifly, add_observer)
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

    auto i1 = notifly::default_notifly().add_observer(
            poster,
            lambda);

    notifly::default_notifly().remove_observer(i1);
}

TEST(notifly, add_observer_temp)
{
    auto i1 = notifly::default_notifly().add_observer_temp(
            poster,
            sum);

    notifly::default_notifly().remove_observer(i1);
}

TEST(notifly, add_observer_and_post_message)
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

    auto i1 = notifly::default_notifly().add_observer(
            poster,
            lambda);

    int message = 0;
    notifly::default_notifly().post_notification(poster, &message);

    notifly::default_notifly().remove_observer(i1);
}

TEST(notifly, lamda_and_post_message)
{
    auto lambda = [](int a, int b) -> int
    {
        return a + b;
    };

    auto i1 = notifly::default_notifly().add_observer_temp(
            poster,
            lambda);

    notifly::default_notifly().post_notification_temp<int, int>(poster, 5, 10);

    notifly::default_notifly().remove_observer(i1);

}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
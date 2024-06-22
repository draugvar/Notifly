//
// Created by Salvatore Rivieccio
//
#pragma once

#include <iostream>

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

int critical_section(std::condition_variable* a_cv, std::mutex* a_mutex, const bool* a_ready, bool* a_notify)
{
    if(a_cv == nullptr || a_mutex == nullptr || a_notify == nullptr)
    {
        return -1;
    }

    std::unique_lock<std::mutex> lock(*a_mutex);
    a_cv->wait(lock, [a_ready] { return *a_ready; });
    printf("Hello critical section\n");
    *a_notify = true;
    a_cv->notify_one();
    return 0;
}
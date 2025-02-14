//
// Created by Salvatore Rivieccio
//
#pragma once

typedef struct point_
{
    int x, y;
} point;

enum message
{
    poster,
    second_poster,
    third_poster,
    fourth_poster
};

inline int sum_callback(int a, int b)
{
    printf("Sum is %d\n", a + b);
    return a + b;
}

inline float divide_callback(int a, int b)
{
    printf("Division is %f\n", (float)a / (float)b);
    return (float)a / (float)b;
}

inline int print_struct(point* a_point)
{
    printf("Point x: %d, y: %d\n", a_point->x, a_point->y);
    return 0;
}

inline int critical_section(std::condition_variable* a_cv, std::mutex* a_mutex, const bool* a_ready, bool* a_notify)
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

inline int just_increment_and_print(std::atomic_int* a_value)
{
    if(a_value == nullptr)
    {
        return -1;
    }

    // increment 10 times
    for(int i = 0; i < 10; i++)
    {
        const auto n = a_value->fetch_add(1);
        printf("Value is %d\n", n);
    }

    return 0;
}

inline int no_params()
{
    printf("No params\n");
    return 0;
}

inline void void_no_params()
{
    printf("No params\n");
}
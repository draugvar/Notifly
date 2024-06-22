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


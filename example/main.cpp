/*
 *  main.cpp
 *  Notification Center CPP
 *
 *  Created by Jonathan Goodman on 11/23/13.
 *  Copyright (c) 2013 Jonathan Goodman. All rights reserved.
 *  Edited by Salvatore Rivieccio.
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

// ReSharper disable CppExpressionWithoutSideEffects
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

void run_notification()
{
	auto lambda = std::function<std::any(int*)>([](int* message) -> std::any
	{
        printf("Received notification %d!\n", (*message)++);
        return 0;
	});

	auto i1 = notifly::default_notifly().add_observer(
		poster,
		lambda);

	notifly::default_notifly().remove_observer(i1);

	auto i2 = notifly::default_notifly().add_observer(
		poster,
		lambda);

	auto i3 = notifly::default_notifly().add_observer(
		poster,
		lambda);

	auto i4 = notifly::default_notifly().add_observer(
		poster,
		lambda);

	auto i5 = notifly::default_notifly().add_observer(
		poster,
		lambda);

	notifly::default_notifly().add_observer(
		poster,
		lambda);

	notifly::default_notifly().add_observer(
		poster,
		lambda);

	notifly::default_notifly().add_observer(
		poster,
		std::function<unsigned int(const std::any&)>([](const std::any&) -> unsigned int
		{
			printf("Received notification, but idc of payload...\n");
			return 0;
		}));

	auto value = 1;
	printf("I'm sending an int that has value %d\n", value);
	auto payload = std::make_any<int*>(&value);
	notifly::default_notifly().post_notification<std::any>(poster, payload);
	printf("After post value is %d\n", value);
	printf("============\n");

	notifly::default_notifly().add_observer(
		third_poster,
		std::function<unsigned int(const int)>([](const int) -> unsigned int
		{
			printf("Received ASYNC notification, but idc of payload...\n");
			return 0;
		}));
	
	notifly::default_notifly().post_notification<std::any>(third_poster, std::any(), true);

	notifly::default_notifly().remove_observer(i1);

	notifly::default_notifly().post_notification(poster);

	printf("============\n");

	notifly::default_notifly().remove_observer(i2);

	notifly::default_notifly().post_notification(poster);

	printf("============\n");

	notifly::default_notifly().remove_observer(i3);

	notifly::default_notifly().post_notification(poster);

	printf("============\n");

	notifly::default_notifly().remove_observer(i4);

	notifly::default_notifly().post_notification(poster);

	printf("============\n");

	notifly::default_notifly().remove_observer(i5);

	notifly::default_notifly().post_notification(poster);

	printf("============\n");

	notifly::default_notifly().remove_all_observers(poster);

	notifly::default_notifly().post_notification(poster);

	printf("============\n");

	notifly::default_notifly().add_observer(
		second_poster,
		std::function<unsigned int(const std::any&)>([=](const std::any&) -> unsigned int
		{
			printf("Called!\n");
			return 0;
		}));

	struct point a_point { 1, 1 };
	printf("Point x.value = %d\n", a_point.x);
	printf("Point y.value = %d\n", a_point.y);
	notifly::default_notifly().add_observer(
            second_poster,
            std::function<int()>([capture0 = &a_point]() -> int
            {
                foo::func(capture0, 1);
                return 0;
            }));

	notifly::default_notifly().post_notification(second_poster);
	printf("Point x.value = %d\n", a_point.x);
	printf("Point y.value = %d\n", a_point.y);

    notifly::default_notifly().add_observer(fourth_poster, sum);
    auto ret = notifly::default_notifly().post_notification<int, std::string>(fourth_poster, 5, "ciao", false);
    if(ret < 0)
    {
        printf("Error: %d\n", ret);
    }
    notifly::default_notifly().post_notification<int, int>(fourth_poster, 5, 7, true);

    ret = notifly::default_notifly().post_notification<int, long>(fourth_poster, 5, 7000, true);
    if(ret < 0)
    {
        printf("Error: %d\n", ret);
    }

    notifly::default_notifly().remove_all_observers(fourth_poster);
}

int main()
{
	run_notification();

	return 0;
}
# Notifly - A simple notification center in pure C++ - NOW ASYNCHRONOUS TOO!

This project was originally forked from https://github.com/Geenz/CPP-NotificationCenter which is not maintained anymore.

A C++ API inspired by Cocoa's NSNotificationCenter API.

## Usage

Using `Notifly` is simple. In order to use the default center, simply use the static
method `Notifly::default_notifly()` like so:

```C++
Notifly::default_notifly().add_observer(1, [=]{printf("Hello world!\n");});
```

NotificationCenter is intended to be included directly in your projects, as such no library (dynamic or static) is
provided.

### Supported Compilers

NotificationCenter requires a compiler that supports the following C++17 APIs:

```C++
std::mutex
std::function
std::shared_ptr
std::any
std::unordered_map
```

### Adding Observers

Adding observers is a simple process. Simply invoke the method `Notifly::add_observer` on your
NotificationCenter passing in a function pointer and integer ID for the notification that this observer should respond to. A
couple of examples of how to do this are:

```C++
#define MY_NOTIFICATION_ID 1
Notifly::default_notifly().add_observer(MY_NOTIFICATION_ID, [=]{printf("Hello world!\n");});
Notifly::default_notifly().add_observer(MY_NOTIFICATION_ID, helloWorldFunc);
Foo myFoo;
Notifly::default_notifly().add_observer(MY_NOTIFICATION_ID, std::bind(&Foo::func, myFoo));
```

Currently, only `std::any(std::any)` function signatures are supported.

### Posting Notifications

Posting notifications can be done with `Notifly::post_notification`, like so:

```C++
Notifly::default_notifly().post_notification(MY_NOTIFICATION_ID);
```
Using the third parameter `a_async`, you can set the function to be called inside the same thread or in a separate one. It 
is set to `false` by default.
### Avoiding Unnecessary Lookups

Notifications can be posted and modified by either integer or iterator:

```C++
auto notiItr = Notifly::default_notifly().get_notification_iterator(MY_NOTIFICATION_ID);
Notifly::default_notifly().add_observer(notiItr, [=]{printf("I'm being posted by an iterator!\n");});
Notifly::default_notifly().post_notification(notiItr);
```

`Notifly::add_observer`, `Notifly::remove_observer`, `Notifly::remove_all_observers`,
and `Notifly::post_notification` all support notification iterators in overloaded methods.

### Multiple NotificationCenters

You can also use more than one instance of NotificationCenter. Although a default notification center is provided, you
can also create your own notification centers for whatever purpose you may require them for.

### Example Program

The included example program shows you the basics of how to use NotificationCenter. It's not intended to be
sophisticated by any means, just to showcase the basics.

### Bugs

I don't expect this to work flawlessly for all applications, and thread safety isn't something that I've tested
particularly well. If you find issues, feel free to file a bug. If you wish to contribute, simply fork this repository
and make a pull request.

## License

Notifly is licensed under the MIT license.

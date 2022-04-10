# Notifly - A simple notification center in pure C++ - NOW ASYNCHRONOUS TOO!

This project was originally forked from https://github.com/Geenz/CPP-NotificationCenter which is not maintained anymore.

A C++ API inspired by Cocoa's NSNotificationCenter API.

## Usage

Using NotificationCenter is simple. In order to use the default center, simply use the static
method `notification_center::default_notification_center()` like so:

```C++
notification_center::default_notification_center().add_observer([=]{printf("Hello world!\n");}, "My Observer");
```

NotificationCenter is intended to be included directly in your projects, as such no library (dynamic or static) is
provided.

### Supported Compilers

NotificationCenter requires a compiler that supports the following C++17 APIs:

```C++
std::mutex
std::function
std::bind
std::shared_ptr
std::any
```

### Adding Observers

Adding observers is a simple porcess. Simply invoke the method `notification_center::add_observer` on your
NotificationCenter passing in a function pointer and string for the notification that this observer should respond to. A
couple of examples of how to do this are:

```C++
notification_center::default_notification_center().add_observer([=]{printf("Hello world!\n");}, "My Observer");
notification_center::default_notification_center().add_observer(helloWorldFunc, "My Observer");
Foo myFoo;
notification_center::default_notification_center().add_observer(std::bind(&Foo::func, myFoo), "My Observer");
```

Currently, only `std::any(std::any)` function signatures are supported.

### Posting Notifications

Posting notifications can be done with `notification_center::post_notification`, like so:

```C++
notification_center::default_notification_center().post_notification("My Observer");
```
Using the third parameter `a_sync`, you can set the function to be called inside the same thread or in a separate one. It 
is set to `true` by default.
### Avoiding Unnecessary Lookups

Notifications can be posted and modified by either string or iterator. Posting or modifying by string incurs a string
lookup, which depending on the application may not be ideal. For these situations, the best option when using
NotificationCenter is to post and modify by iterator. An example of how to do this is:

```C++
auto notiItr = notification_center::default_notification_center().get_notification_iterator("My Observer");
notification_center::default_notification_center().add_observer([=]{printf("I'm being posted by an iterator!\n");}, notiItr);
notification_center::default_notification_center().post_notification(notiItr);
```

`notification_center::add_observer`, `notification_center::remove_observer`, `notification_center::remove_all_observers`,
and `notification_center::post_notification` all support notification iterators in overloaded methods.

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

NotificationCenter is licensed under the MIT license.

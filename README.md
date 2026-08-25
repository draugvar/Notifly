# Notifly - A simple notification center in pure C++ -

This project was originally forked from https://github.com/Geenz/CPP-NotificationCenter which is not maintained anymore.

A C++ API inspired by Cocoa's NSNotificationCenter API.

## C Interface

Notifly now includes a **C interface** that provides access to the notification center functionality from C programs through a shared library (DLL/SO). This allows you to use Notifly from C projects while maintaining the performance and features of the C++ implementation.

**Key features of the C interface:**
- Shared library (`libnotifly_c.so`/`notifly_c.dll`) for easy integration
- Handle-based API for type safety
- Function pointer callbacks
- Synchronous and asynchronous notification posting
- Full compatibility with the C++ API functionality

See [docs/C_INTERFACE.md](docs/C_INTERFACE.md) for complete documentation and examples.

**Quick C example:**
```c
#include "notifly_c.h"

void my_callback(int notification_id, void* data, void* user_data) {
    printf("Received notification %d\n", notification_id);
}

int main() {
    notifly_handle notifly = notifly_default();
    int observer_id = notifly_add_observer(notifly, 1001, my_callback, NULL);
    notifly_post_notification(notifly, 1001, NULL);
    notifly_remove_observer(notifly, observer_id);
    return 0;
}
```

## C++ API Usage

Using `notifly` is simple. In order to use the default center, simply use the static
method `notifly::default_notifly()` like so:

```C++
notifly::default_notifly().add_observer(1, [=](){printf("Hello world!\n");});
```

Notifly is intended to be included directly in your projects, as such no library (dynamic or static) is
provided.

### Supported Compilers

Notifly requires a compiler that supports the following C++17 APIs:

```C++
std::mutex
std::function
std::shared_ptr
std::any
std::unordered_map
```

### Adding Observers

Adding observers is a simple process. Simply invoke the method `notifly::add_observer` passing in a function pointer 
and integer ID for the notification that this observer should respond to. 

A couple of examples of how to do this are:

```C++
#define MY_NOTIFICATION_ID 1
notifly::default_notifly().add_observer(MY_NOTIFICATION_ID, [=]{printf("Hello world!\n");});
notifly::default_notifly().add_observer(MY_NOTIFICATION_ID, helloWorldFunc);
```

### Posting Notifications

You can post notifications both synchronously and asynchronously:

#### Synchronous Notification

```C++
notifly::default_notifly().post_notification(MY_NOTIFICATION_ID);
```

#### Asynchronous Notification

```C++
notifly::default_notifly().post_notification_async(MY_NOTIFICATION_ID);
```

Asynchronous notifications are executed in separate threads, allowing your application to continue processing without waiting for observers to complete their work.

### Waiting For A Reply

`post_notification` is one-way. When something is expected to answer, `post_and_wait` posts a request and blocks until
the reply arrives or the timeout expires. It subscribes before posting, so a reply that comes back before the post call
returns is still caught:

```C++
int reply = 0;
const auto result = notifly::default_notifly().post_and_wait(
        REQUEST_ID,     // post this
        REPLY_ID,       // wait for this
        500,            // timeout, milliseconds
        reply,          // where the payload lands (a value, or a std::tuple)
        arg1, arg2);    // request payload

if (result == notifly_result::timeout) { /* nobody answered */ }
```

For anything more involved, `notifly::exchange` is the same machinery with the pieces exposed. Each handler returns a
`notifly_verdict` saying what the delivery was — `skip` to ignore it and stay subscribed, `keep` for one piece of a
streamed reply, `done` to end the wait:

```C++
notifly::exchange ex(notifly::default_notifly());

// Ignore the state the sender is leaving; only the one asked for ends the wait.
ex.on<int>(STATUS_ID, [](const int state)
{
    return state == 1 ? notifly_verdict::done : notifly_verdict::skip;
});

notifly::default_notifly().post_notification(ENABLE_ID, 0);

if (ex.wait(std::chrono::milliseconds(500)) < 0) { /* timed out */ }
```

That covers the shapes a single request/reply pair cannot:

| Shape | How |
|---|---|
| Ignore deliveries that are not the awaited one | return `notifly_verdict::skip` |
| Several possible answers, and which one arrived matters | chain `on()` calls; `wait()` returns the notification that fired |
| A reply streamed in pieces of unstated length | return `notifly_verdict::keep`, end with `drain(quiet, deadline)` |
| Silence is the successful outcome | `silent_for(window)` |
| Nothing to post — waiting on an external event | subscribe and `wait()`, post nothing |

Once a handler returns `done` the exchange is complete and later deliveries are ignored, so a sender that repeats itself
cannot disturb what the winning handler stored. The destructor unsubscribes; never destroy an exchange from inside a
handler.

Note that a notification's payload shape must match exactly, references included: `post_notification` deduces its
arguments by value, so an observer declared as `[](const std::string&)` registers a different shape than one declared
as `[](std::string)` and will not be called.

### Avoiding Unnecessary Lookups

Notifications can be posted and modified by using the unique identifier returned when add observer is called:

```C++
auto observerId = notifly::default_notifly().add_observer([Notification ID], [=]{printf("I'm being posted by an iterator!\n");});
notifly::default_notifly().remove_observer(observerId);
```

### Multiple NotificationCenters

You can also use more than one instance of NotificationCenter. Although a default notification center is provided, you
can also create your own notification centers for whatever purpose you may require them for.

### Example Programs

The included example program shows you the basics of how to use NotificationCenter. It's not intended to be
sophisticated by any means, just to showcase the basics.

`example/exchange_example.cpp` covers the request/reply side: it drives a simulated device through each of the shapes in
the table above, one per numbered section.

### Bugs

I don't expect this to work flawlessly for all applications, and thread safety isn't something that I've tested
particularly well. If you find issues, feel free to file a bug. If you wish to contribute, simply fork this repository
and make a pull request.

## License

Notifly is licensed under the MIT license.

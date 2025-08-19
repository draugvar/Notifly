# Notifly C Interface Documentation

This document describes the C interface for the Notifly notification center library.

## Overview

The C interface provides access to Notifly functionality from C programs through a shared library (`libnotifly_c.so` on Linux, `notifly_c.dll` on Windows). The interface wraps the C++ API with a C-compatible API using:

- Opaque handle-based design for type safety
- Function pointer callbacks instead of C++ std::function
- Simple void pointer data payloads
- Standard C error codes

## API Reference

### Types

```c
typedef struct notifly_instance* notifly_handle;
typedef void (*notifly_callback)(int notification_id, void* data, void* user_data);

typedef enum {
    NOTIFLY_SUCCESS = 0,
    NOTIFLY_OBSERVER_NOT_FOUND = -1,
    NOTIFLY_NOTIFICATION_NOT_FOUND = -2,
    NOTIFLY_PAYLOAD_TYPE_NOT_MATCH = -3,
    NOTIFLY_NO_MORE_OBSERVER_IDS = -4,
    NOTIFLY_INVALID_HANDLE = -5
} notifly_result_t;
```

### Instance Management

```c
// Create a new notifly instance
notifly_handle notifly_create(void);

// Destroy a notifly instance (only for instances created with notifly_create)
void notifly_destroy(notifly_handle handle);

// Get the default global notifly instance
notifly_handle notifly_default(void);
```

### Observer Management

```c
// Add an observer for a specific notification
int notifly_add_observer(notifly_handle handle, int notification_id, 
                        notifly_callback callback, void* user_data);

// Remove a specific observer by ID
int notifly_remove_observer(notifly_handle handle, int observer_id);

// Remove all observers for a notification
int notifly_remove_all_observers(notifly_handle handle, int notification_id);
```

### Notification Posting

```c
// Post a notification synchronously
int notifly_post_notification(notifly_handle handle, int notification_id, void* data);

// Post a notification asynchronously  
int notifly_post_notification_async(notifly_handle handle, int notification_id, void* data);
```

### Utility Functions

```c
// Convert error code to human-readable string
const char* notifly_result_to_string(int result);
```

## Usage Example

```c
#include "notifly_c.h"
#include <stdio.h>

// Callback function
void my_callback(int notification_id, void* data, void* user_data) {
    printf("Received notification %d\n", notification_id);
    if (data) {
        int* value = (int*)data;
        printf("Data: %d\n", *value);
    }
}

int main() {
    // Get default instance
    notifly_handle notifly = notifly_default();
    
    // Add observer
    int observer_id = notifly_add_observer(notifly, 1001, my_callback, NULL);
    if (observer_id <= 0) {
        printf("Failed to add observer: %s\n", notifly_result_to_string(observer_id));
        return 1;
    }
    
    // Post notification
    int data = 42;
    int result = notifly_post_notification(notifly, 1001, &data);
    if (result <= 0) {
        printf("Failed to post notification: %s\n", notifly_result_to_string(result));
        return 1;
    }
    
    printf("Notification sent to %d observers\n", result);
    
    // Cleanup
    notifly_remove_observer(notifly, observer_id);
    
    return 0;
}
```

## Building

The shared library is built using CMake:

```bash
mkdir build && cd build
cmake ..
make
```

This creates:
- `libnotifly_c.so` (Linux) / `notifly_c.dll` (Windows) - the shared library
- `notifly_c_test` - C interface unit tests
- `notifly_c_example` - C interface usage example

## Linking

To use the C interface in your project:

### CMake
```cmake
find_library(NOTIFLY_C_LIBRARY notifly_c)
target_link_libraries(your_target ${NOTIFLY_C_LIBRARY})
target_include_directories(your_target PRIVATE /path/to/notifly/include)
```

### Direct compilation
```bash
gcc -o my_program my_program.c -lnotifly_c -I/path/to/notifly/include
```

## Memory Management

- **Handles**: The default handle (`notifly_default()`) should never be destroyed. Only destroy handles created with `notifly_create()`.
- **Data**: The library does not take ownership of data passed to `notifly_post_notification()`. Ensure data remains valid during synchronous calls.
- **User data**: User data passed to `notifly_add_observer()` must remain valid until the observer is removed.

## Thread Safety

The C interface inherits the thread safety characteristics of the underlying C++ implementation:
- Multiple threads can safely add/remove observers and post notifications
- Callbacks may be invoked from different threads when using async notifications
- No additional locking is required in user code

## Error Handling

All functions return integer results:
- Positive values: Success (usually count of affected observers)
- Zero: Success with no side effects
- Negative values: Error codes (see `notifly_result_t` enum)

Use `notifly_result_to_string()` to get human-readable error descriptions.

## Limitations

1. **Type Safety**: Unlike the C++ API, the C interface uses void pointers for data, sacrificing compile-time type checking for simplicity.
2. **Templates**: The C++ template-based type validation is not available in C.
3. **Complex Data**: Only simple data structures should be passed through the void pointer interface.

## Migration from C++ API

| C++ API | C API |
|---------|-------|
| `notifly::default_notifly()` | `notifly_default()` |
| `add_observer(id, callback)` | `notifly_add_observer(handle, id, callback, user_data)` |
| `remove_observer(id)` | `notifly_remove_observer(handle, id)` |
| `post_notification(id, args...)` | `notifly_post_notification(handle, id, &data)` |
| `post_notification_async(id, args...)` | `notifly_post_notification_async(handle, id, &data)` |
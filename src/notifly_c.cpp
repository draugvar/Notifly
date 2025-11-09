/*
 *  notifly_c.cpp
 *  notifly C interface implementation
 *
 *  Copyright (c) 2024 Salvatore Rivieccio. All rights reserved.
 */

#include "notifly_c.h"
#include "notifly.h"
#include <unordered_map>
#include <memory>
#include <mutex>

// Internal structure to wrap notifly instances
struct notifly_instance {
    std::unique_ptr<notifly> instance;
    // Maps observer ID to callback info for proper cleanup
    std::unordered_map<int, std::pair<notifly_callback, void*>> callbacks;
    std::mutex callback_mutex;
    
    explicit notifly_instance(std::unique_ptr<notifly> inst) 
        : instance(std::move(inst)) {}
};

// Wrapper callback that adapts C++ callback to C callback
class CallbackWrapper {
public:
    CallbackWrapper(const notifly_callback cb, void* user_data)
        : callback(cb), user_data(user_data) {}
    
    void operator()(void* data) const {
        callback(0, data, user_data); // notification_id will be set by the caller
    }
    
private:
    notifly_callback callback;
    void* user_data;
};

// Global default instance pointer
static notifly_handle g_default_handle = nullptr;
static std::mutex g_default_mutex;

extern "C" {

notifly_handle notifly_create(void) {
    try {
        auto cpp_instance = std::make_unique<notifly>();
        return new notifly_instance(std::move(cpp_instance));
    } catch (...) {
        return nullptr;
    }
}

void notifly_destroy(const notifly_handle handle)
{
        delete handle;
}

notifly_handle notifly_default(void) {
    std::lock_guard lock(g_default_mutex);
    if (g_default_handle == nullptr) {
        // Create a wrapper around the default C++ instance
        // Note: we don't own the C++ default instance, so we create a special wrapper
        g_default_handle = new notifly_instance(nullptr); // nullptr indicates default instance
    }
    return g_default_handle;
}

int notifly_add_observer(notifly_handle handle, int notification_id, notifly_callback callback, void* user_data) {
    if (!handle || !callback) {
        return NOTIFLY_INVALID_HANDLE;
    }
    
    try {
        // Get the actual notifly instance
        notifly* instance = nullptr;
        if (handle->instance) {
            instance = handle->instance.get();
        } else {
            // This is the default instance
            instance = &notifly::default_notifly();
        }
        
        // Create a lambda that captures the notification_id and calls our C callback
        auto cpp_callback = [notification_id, callback, user_data](void* data) {
            callback(notification_id, data, user_data);
        };
        
        // Add observer to the C++ instance
        const int observer_id = instance->add_observer(notification_id, cpp_callback);
        
        if (observer_id > 0) {
            // Store callback info for cleanup
            std::lock_guard lock(handle->callback_mutex);
            handle->callbacks[observer_id] = std::make_pair(callback, user_data);
        }
        
        return observer_id;
    } catch (...) {
        return NOTIFLY_INVALID_HANDLE;
    }
}

int notifly_remove_observer(notifly_handle handle, const int observer_id) {
    if (!handle) {
        return NOTIFLY_INVALID_HANDLE;
    }
    
    try {
        // Get the actual notifly instance
        notifly* instance = nullptr;
        if (handle->instance) {
            instance = handle->instance.get();
        } else {
            // This is the default instance
            instance = &notifly::default_notifly();
        }
        
        // Remove from C++ instance
        const int result = instance->remove_observer(observer_id);
        
        // Clean up our callback info
        if (result == static_cast<int>(notifly_result::success)) {
            std::lock_guard lock(handle->callback_mutex);
            handle->callbacks.erase(observer_id);
        }
        
        return result;
    } catch (...) {
        return NOTIFLY_INVALID_HANDLE;
    }
}

int notifly_remove_all_observers(notifly_handle handle, const int notification_id) {
    if (!handle) {
        return NOTIFLY_INVALID_HANDLE;
    }
    
    try {
        // Get the actual notifly instance
        notifly* instance = nullptr;
        if (handle->instance) {
            instance = handle->instance.get();
        } else {
            // This is the default instance
            instance = &notifly::default_notifly();
        }

        const int result = instance->remove_all_observers(notification_id);
        
        // Clean up our callback info - remove all callbacks for this notification
        // Note: This is a simplified cleanup. In practice, we'd need to track which
        // observers belong to which notification, but for now this clears all.
        if (result > 0) {
            std::lock_guard lock(handle->callback_mutex);
            handle->callbacks.clear();
        }
        
        return result;
    } catch (...) {
        return NOTIFLY_INVALID_HANDLE;
    }
}

int notifly_post_notification(notifly_handle handle, const int notification_id, void* data) {
    if (!handle) {
        return NOTIFLY_INVALID_HANDLE;
    }
    
    try {
        // Get the actual notifly instance
        notifly* instance = nullptr;
        if (handle->instance) {
            instance = handle->instance.get();
        } else {
            // This is the default instance
            instance = &notifly::default_notifly();
        }
        
        // Post notification with void* data
        return instance->post_notification(notification_id, data);
    } catch (...) {
        return NOTIFLY_INVALID_HANDLE;
    }
}

int notifly_post_notification_async(notifly_handle handle, const int notification_id, void* data) {
    if (!handle) {
        return NOTIFLY_INVALID_HANDLE;
    }
    
    try {
        // Get the actual notifly instance
        notifly* instance = nullptr;
        if (handle->instance) {
            instance = handle->instance.get();
        } else {
            // This is the default instance
            instance = &notifly::default_notifly();
        }
        
        // Post notification asynchronously with void* data
        return instance->post_notification_async(notification_id, data);
    } catch (...) {
        return NOTIFLY_INVALID_HANDLE;
    }
}

int notifly_post_and_wait(notifly_handle handle,
                         const int post_notification_id,
                         const int wait_notification_id,
                         const int timeout_ms,
                         void* post_data,
                         void** response_data) {
    if (!handle || !response_data) {
        return NOTIFLY_INVALID_HANDLE;
    }

    try {
        // Get the actual notifly instance
        notifly* instance = nullptr;
        if (handle->instance) {
            instance = handle->instance.get();
        } else {
            // This is the default instance
            instance = &notifly::default_notifly();
        }

        // Use the C++ post_and_wait with void* type
        void* result = nullptr;
        auto ret = instance->post_and_wait(
            post_notification_id,
            wait_notification_id,
            timeout_ms,
            result,
            post_data
        );

        if (ret == notifly_result::success) {
            *response_data = result;
        } else {
            *response_data = nullptr;
        }

        return static_cast<int>(ret);
    } catch (...) {
        return NOTIFLY_INVALID_HANDLE;
    }
}

const char* notifly_result_to_string(const int result) {
    switch (result) {
        case NOTIFLY_SUCCESS:
            return "Success";
        case NOTIFLY_OBSERVER_NOT_FOUND:
            return "Observer not found";
        case NOTIFLY_NOTIFICATION_NOT_FOUND:
            return "Notification not found";
        case NOTIFLY_PAYLOAD_TYPE_NOT_MATCH:
            return "Payload type mismatch";
        case NOTIFLY_NO_MORE_OBSERVER_IDS:
            return "No more observer IDs available";
        case NOTIFLY_TIMEOUT:
            return "Timeout";
        case NOTIFLY_INVALID_HANDLE:
            return "Invalid handle";
        default:
            return "Unknown error";
    }
}

} // extern "C"
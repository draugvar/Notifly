/**
 * @file c_notifly.cpp
 * @brief C interface implementation for the Notifly notification system.
 * 
 * This file provides a C-compatible API wrapper around the C++ Notifly library,
 * allowing C applications to use the notification system. It handles the translation
 * between C and C++ types and manages memory appropriately.
 */
#include "c_notifly.h"
#include "notifly.h"

/**
 * @struct notifly_handler
 * @brief Opaque structure that encapsulates a notifly C++ instance.
 * 
 * This structure serves as a handle for C code to reference a C++ notifly object
 * without exposing C++ implementation details.
 */
typedef struct notifly_handler
{
    notifly* _;
} notifly_handler_t;

/**
 * @struct c_payload
 * @brief Wrapper structure to bridge C void pointers with the C++ type system.
 * 
 * Encapsulates arbitrary C data to be passed through the C++ notification system
 * and back to C callback functions.
 */
struct c_payload
{
    void* data;
};

/**
 * @brief Creates a new Notifly instance or returns the default one.
 * 
 * @param get_default If non-zero, returns a handler to the singleton default instance.
 *                   If zero, creates a new Notifly instance.
 * @return A pointer to a notifly handler which must be destroyed with notifly_destroy
 *         when no longer needed (unless it's the default instance).
 */
notifly_handler_t* notifly_create(const unsigned char get_default)
{
    // Allocate memory for the handler structure
    auto* handler = new notifly_handler_t();
    if (get_default)
    {
        // Use the default singleton instance
        handler->_ = &notifly::default_notifly();
    }
    else
    {
        // Create a new notifly instance
        handler->_ = new notifly();
    }
    return handler;
}

/**
 * @brief Destroys a Notifly instance and frees associated memory.
 * 
 * @param handler Pointer to the handler to be destroyed. If this handler references
 *               the default Notifly instance, the instance itself isn't destroyed.
 */
void notifly_destroy(notifly_handler_t *handler)
{
    // Only delete if it's not the default instance
    if (handler->_ != &notifly::default_notifly())
    {
        delete handler->_;
    }
    handler->_ = nullptr;
}

/**
 * @brief Registers a callback function to be invoked when a specified notification is posted.
 * 
 * @param handler The Notifly instance handler.
 * @param notification The notification identifier to observe.
 * @param callback The function to be called when the notification is posted.
 * @return The observer ID (positive integer) on success, or -1 on failure.
 */
int notifly_add_observer(const notifly_handler_t* handler, const int notification, notifly_callback_t callback)
{
    // Validate parameters
    if (handler == nullptr || handler->_ == nullptr)
    {
        return -1;
    }
    if (callback == nullptr)
    {
        return -1;
    }

    // Create a C++ wrapper around the C callback
    auto cpp_callback = [callback](const c_payload payload) -> void {
        callback(payload.data);
    };

    // Add the observer using notifly's template function
    const int observer_id = handler->_->add_observer(notification, cpp_callback);
    
    return observer_id;
}

/**
 * @brief Posts a notification synchronously, immediately invoking all registered callbacks.
 * 
 * @param handler The Notifly instance handler.
 * @param notification The notification identifier to post.
 * @param payload Pointer to user data that will be passed to all callbacks.
 * @return The number of observers notified, or -1 on failure.
 */
int notifly_post_notification(const notifly_handler_t* handler, const int notification, void* payload)
{
    // Validate parameters
    if (handler == nullptr || handler->_ == nullptr)
    {
        return -1;
    }
    
    // Create a structure to encapsulate the C payload
    const c_payload cpp_payload = {payload};
    
    // Send the notification
    return handler->_->post_notification(notification, cpp_payload);
}

/**
 * @brief Posts a notification asynchronously, scheduling callbacks for future execution.
 * 
 * This function returns immediately after queuing the notification.
 * Callbacks will be executed on a background thread.
 * 
 * @param handler The Notifly instance handler.
 * @param notification The notification identifier to post.
 * @param payload Pointer to user data that will be passed to all callbacks.
 * @return 1 on successful queuing, or -1 on failure.
 */
int notifly_post_notification_async(const notifly_handler_t* handler, const int notification, void* payload)
{
    // Validate parameters
    if (handler == nullptr || handler->_ == nullptr)
    {
        return -1;
    }
    
    // Create a structure to encapsulate the C payload
    c_payload cpp_payload = {payload};
    
    // Send the notification asynchronously
    return handler->_->post_notification_async(notification, cpp_payload);
}

/**
 * @brief Removes a specific observer from the notification system.
 * 
 * @param handler The Notifly instance handler.
 * @param observer_id The ID of the observer to remove (returned by notifly_add_observer).
 * @return 1 if the observer was successfully removed, 0 if the observer wasn't found,
 *         or -1 on failure.
 */
int notifly_remove_observer(const notifly_handler_t* handler, const int observer_id)
{
    // Validate parameters
    if (handler == nullptr || handler->_ == nullptr)
    {
        return -1;
    }

    const int result = handler->_->remove_observer(observer_id);
    
    return result;
}

/**
 * @brief Removes all observers for a specific notification.
 * 
 * @param handler The Notifly instance handler.
 * @param notification The notification identifier whose observers should be removed.
 * @return The number of observers removed, or -1 on failure.
 */
int notifly_remove_all_observers(const notifly_handler_t* handler, const int notification)
{
    // Validate parameters
    if (handler == nullptr || handler->_ == nullptr)
    {
        return -1;
    }

    const int result = handler->_->remove_all_observers(notification);

    return result;
}

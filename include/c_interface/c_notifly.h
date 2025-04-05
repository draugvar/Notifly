#pragma once

/**
 * @file c_notifly.h
 * @brief C interface for the Notifly library - a notification system.
 * 
 * This header provides a C-compatible interface to the Notifly library,
 * enabling language interoperability through a simple observer pattern implementation.
 */

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

#ifdef WIN32
#define NOTIFLY_API EXTERN_C __declspec(dllexport)
#else
#define NOTIFLY_API EXTERN_C
#endif

/**
 * @brief Opaque type for the Notifly handler.
 * Implementation details are hidden from the user.
 */
typedef struct notifly_handler notifly_handler_t;

/**
 * @brief Standard C callback function type for notification observers.
 * @param payload User data passed when the notification is triggered.
 */
typedef void (*notifly_callback_t)(void* payload);

/**
 * @brief Creates a new Notifly handler instance.
 * @param get_default If non-zero, returns the default global handler instance.
 * @return Pointer to the newly created handler or the default handler.
 */
NOTIFLY_API notifly_handler_t* notifly_create(unsigned char get_default);

/**
 * @brief Destroys a Notifly handler and frees its resources.
 * @param handler The handler to destroy.
 */
NOTIFLY_API void notifly_destroy(notifly_handler_t* handler);

/**
 * @brief Registers an observer for a specific notification type.
 * @param handler The Notifly handler.
 * @param notification The notification ID to observe.
 * @param callback The function to call when the notification is triggered.
 * @return Observer ID on success, negative value on failure.
 */
NOTIFLY_API int notifly_add_observer(const notifly_handler_t* handler, int notification, notifly_callback_t callback);

/**
 * @brief Triggers a notification synchronously.
 * @param handler The Notifly handler.
 * @param notification The notification ID to trigger.
 * @param payload User data to pass to the observers.
 * @return Number of observers notified or negative value on failure.
 */
NOTIFLY_API int notifly_post_notification(const notifly_handler_t* handler, int notification, void* payload);

/**
 * @brief Triggers a notification asynchronously.
 * @param handler The Notifly handler.
 * @param notification The notification ID to trigger.
 * @param payload User data to pass to the observers.
 * @return 1 on successful queuing of the notification, negative value on failure.
 */
NOTIFLY_API int notifly_post_notification_async(const notifly_handler_t* handler, int notification, void* payload);

/**
 * @brief Removes a specific observer.
 * @param handler The Notifly handler.
 * @param observer_id The ID of the observer to remove.
 * @return 1 on success, 0 if observer not found, negative value on failure.
 */
NOTIFLY_API int notifly_remove_observer(const notifly_handler_t* handler, int observer_id);

/**
 * @brief Removes all observers for a specific notification type.
 * @param handler The Notifly handler.
 * @param notification The notification ID to clear observers for.
 * @return Number of observers removed or negative value on failure.
 */
NOTIFLY_API int notifly_remove_all_observers(const notifly_handler_t* handler, int notification);

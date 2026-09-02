/*
 *  notifly_c.h
 *  notifly C interface
 *
 *  Copyright (c) 2024 Salvatore Rivieccio. All rights reserved.
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

#ifndef NOTIFLY_C_H
#define NOTIFLY_C_H

#ifdef _WIN32
    #ifdef NOTIFLY_C_EXPORTS
        #define NOTIFLY_C_API __declspec(dllexport)
    #else
        #define NOTIFLY_C_API __declspec(dllimport)
    #endif
#else
    #define NOTIFLY_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Handle for notifly instance (opaque pointer) */
typedef struct notifly_instance* notifly_handle;

/* Callback function type for observers */
typedef void (*notifly_callback)(int notification_id, void* data, void* user_data);

/* Result codes (matching notifly_result enum from C++ API) */
typedef enum {
    NOTIFLY_SUCCESS = 0,
    NOTIFLY_OBSERVER_NOT_FOUND = -1,
    NOTIFLY_NOTIFICATION_NOT_FOUND = -2,
    NOTIFLY_PAYLOAD_TYPE_NOT_MATCH = -3,
    NOTIFLY_NO_MORE_OBSERVER_IDS = -4,
    NOTIFLY_TIMEOUT = -5,
    NOTIFLY_INVALID_HANDLE = -6
} notifly_result_t;

/* Library version */
#define NOTIFLY_C_VERSION_MAJOR 1
#define NOTIFLY_C_VERSION_MINOR 2
#define NOTIFLY_C_VERSION_PATCH 0

/* Instance management */
NOTIFLY_C_API notifly_handle notifly_create(void);
NOTIFLY_C_API void notifly_destroy(notifly_handle handle);
NOTIFLY_C_API notifly_handle notifly_default(void);
NOTIFLY_C_API void notifly_cleanup_default(void);  /* Cleanup default instance (for tests/shutdown) */

/* Observer management */
NOTIFLY_C_API int notifly_add_observer(notifly_handle handle, int notification_id, notifly_callback callback, void* user_data);
NOTIFLY_C_API int notifly_remove_observer(notifly_handle handle, int observer_id);
NOTIFLY_C_API int notifly_remove_all_observers(notifly_handle handle, int notification_id);

/* Notification posting */
NOTIFLY_C_API int notifly_post_notification(notifly_handle handle, int notification_id, void* data);
NOTIFLY_C_API int notifly_post_notification_async(notifly_handle handle, int notification_id, void* data);

/* Synchronous request-response pattern */
NOTIFLY_C_API int notifly_post_and_wait(notifly_handle handle,
                                        int post_notification_id,
                                        int wait_notification_id,
                                        int timeout_ms,
                                        void* post_data,
                                        void** response_data);

/*
 * Exchange: a scoped set of subscriptions a thread can block on.
 *
 * Where notifly_post_and_wait() covers one request/reply shape -- post one
 * notification, wait for one reply, take the first that arrives -- an exchange
 * covers the rest: waiting on several notifications at once and learning which
 * one answered, ignoring deliveries that are not the one being waited for,
 * collecting a reply streamed in pieces, posting nothing at all, or treating
 * silence as the successful outcome. Mirrors notifly::exchange in notifly.h.
 *
 * Usage: notifly_exchange_create(), subscribe with one or more calls to
 * notifly_exchange_on() / notifly_exchange_capture(), post whatever the reply
 * is expected to answer, then block on notifly_exchange_wait(),
 * notifly_exchange_drain() or notifly_exchange_silent_for(). Always finish with
 * notifly_exchange_destroy(), which unsubscribes every handler.
 *
 * Never destroy an exchange, or call notifly_remove_observer() on one of its
 * observer ids, from inside a handler.
 */
typedef struct notifly_exchange* notifly_exchange_handle;

/* What a handler decides about one delivery. */
typedef enum {
    NOTIFLY_VERDICT_SKIP = 0,  /* Not what is being waited for. Stay subscribed, record nothing. */
    NOTIFLY_VERDICT_KEEP = 1,  /* Part of a streamed reply. Record it and keep waiting. */
    NOTIFLY_VERDICT_DONE = 2   /* This delivery completes the exchange. */
} notifly_verdict_t;

/* Handler invoked for each delivery of a notification an exchange subscribed to. */
typedef notifly_verdict_t (*notifly_exchange_handler)(int notification_id, void* data, void* user_data);

/* Create an exchange bound to the given centre (a custom instance, or notifly_default()). */
NOTIFLY_C_API notifly_exchange_handle notifly_exchange_create(notifly_handle handle);

/* Unsubscribe every handler and destroy the exchange. */
NOTIFLY_C_API void notifly_exchange_destroy(notifly_exchange_handle exchange);

/* Subscribe to a notification, letting handler judge each delivery. Returns
 * notifly_exchange_status() after the call: NOTIFLY_SUCCESS, or the first
 * subscription error encountered by this exchange so far. */
NOTIFLY_C_API int notifly_exchange_on(notifly_exchange_handle exchange,
                                      int notification_id,
                                      notifly_exchange_handler handler,
                                      void* user_data);

/* Subscribe to a notification and store its first delivery's payload into
 * *out_data (set to NULL until then). Same return convention as
 * notifly_exchange_on(). */
NOTIFLY_C_API int notifly_exchange_capture(notifly_exchange_handle exchange,
                                           int notification_id,
                                           void** out_data);

/* Block until a handler returns NOTIFLY_VERDICT_DONE, or the timeout expires.
 * Returns the notification that completed the exchange, or -1 on timeout. */
NOTIFLY_C_API int notifly_exchange_wait(notifly_exchange_handle exchange, int timeout_ms);

/* Block for the whole window and report whether nothing arrived (1) or a
 * handler returned NOTIFLY_VERDICT_DONE within it (0). For protocols where the
 * sender only answers to object, so silence is the successful outcome. */
NOTIFLY_C_API int notifly_exchange_silent_for(notifly_exchange_handle exchange, int window_ms);

/* Block while a reply is streamed in pieces, until quiet_ms of silence follows
 * the last accepted delivery, deadline_ms elapses, or a handler returns
 * NOTIFLY_VERDICT_DONE -- whichever comes first. Returns how many deliveries
 * were accepted (NOTIFLY_VERDICT_KEEP or NOTIFLY_VERDICT_DONE). */
NOTIFLY_C_API int notifly_exchange_drain(notifly_exchange_handle exchange, int quiet_ms, int deadline_ms);

/* The first subscription error, or NOTIFLY_SUCCESS if every on()/capture() call took. */
NOTIFLY_C_API int notifly_exchange_status(notifly_exchange_handle exchange);

/* The notification that completed the exchange, or -1 if none has yet. */
NOTIFLY_C_API int notifly_exchange_fired(notifly_exchange_handle exchange);

/* How many deliveries have been accepted so far. */
NOTIFLY_C_API int notifly_exchange_accepted(notifly_exchange_handle exchange);

/* Utility functions */
NOTIFLY_C_API const char* notifly_result_to_string(int result);
NOTIFLY_C_API const char* notifly_verdict_to_string(int verdict);

#ifdef __cplusplus
}
#endif

#endif /* NOTIFLY_C_H */

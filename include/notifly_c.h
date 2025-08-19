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
    NOTIFLY_INVALID_HANDLE = -5
} notifly_result_t;

/* Library version */
#define NOTIFLY_C_VERSION_MAJOR 1
#define NOTIFLY_C_VERSION_MINOR 0  
#define NOTIFLY_C_VERSION_PATCH 0

/* Instance management */
notifly_handle notifly_create(void);
void notifly_destroy(notifly_handle handle);
notifly_handle notifly_default(void);

/* Observer management */
int notifly_add_observer(notifly_handle handle, int notification_id, notifly_callback callback, void* user_data);
int notifly_remove_observer(notifly_handle handle, int observer_id);
int notifly_remove_all_observers(notifly_handle handle, int notification_id);

/* Notification posting */
int notifly_post_notification(notifly_handle handle, int notification_id, void* data);
int notifly_post_notification_async(notifly_handle handle, int notification_id, void* data);

/* Utility functions */
const char* notifly_result_to_string(int result);

#ifdef __cplusplus
}
#endif

#endif /* NOTIFLY_C_H */
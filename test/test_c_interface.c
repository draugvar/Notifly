/*
 * test_c_interface.c
 * Simple C test for notifly C interface
 */

#include "notifly_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for sleep

// Test data structure
typedef struct {
    int value;
    char message[100];
} test_data_t;

// Global variables to track callback invocations
static int callback_count = 0;
static int last_notification_id = -1;
static test_data_t last_received_data = {0, ""};

// Test callback function
void test_callback(int notification_id, void* data, void* user_data) {
    callback_count++;
    last_notification_id = notification_id;
    
    printf("C Callback called: notification_id=%d, callback_count=%d\n", 
           notification_id, callback_count);
    
    if (data) {
        test_data_t* test_data = (test_data_t*)data;
        last_received_data = *test_data;
        printf("  Received data: value=%d, message='%s'\n", 
               test_data->value, test_data->message);
    }
    
    if (user_data) {
        const char* user_msg = (const char*)user_data;
        printf("  User data: '%s'\n", user_msg);
    }
}

void simple_callback(int notification_id, void* data, void* user_data) {
    printf("Simple callback: notification_id=%d\n", notification_id);
    callback_count++;
}

int test_basic_functionality() {
    printf("\n=== Test Basic Functionality ===\n");
    
    // Reset globals
    callback_count = 0;
    last_notification_id = -1;
    
    // Get default handle
    notifly_handle handle = notifly_default();
    if (!handle) {
        printf("FAIL: Could not get default handle\n");
        return 1;
    }
    
    // Add observer
    const char* user_data = "test user data";
    int observer_id = notifly_add_observer(handle, 1001, test_callback, (void*)user_data);
    if (observer_id <= 0) {
        printf("FAIL: Could not add observer, result=%d (%s)\n", 
               observer_id, notifly_result_to_string(observer_id));
        return 1;
    }
    printf("Added observer with ID: %d\n", observer_id);
    
    // Post notification with data
    test_data_t test_data = {42, "Hello from C!"};
    int result = notifly_post_notification(handle, 1001, &test_data);
    if (result <= 0) {
        printf("FAIL: Could not post notification, result=%d (%s)\n", 
               result, notifly_result_to_string(result));
        return 1;
    }
    printf("Posted notification, %d observers notified\n", result);
    
    // Verify callback was called
    if (callback_count != 1) {
        printf("FAIL: Expected callback_count=1, got %d\n", callback_count);
        return 1;
    }
    if (last_notification_id != 1001) {
        printf("FAIL: Expected notification_id=1001, got %d\n", last_notification_id);
        return 1;
    }
    if (last_received_data.value != 42 || strcmp(last_received_data.message, "Hello from C!") != 0) {
        printf("FAIL: Data not received correctly\n");
        return 1;
    }
    
    // Remove observer
    result = notifly_remove_observer(handle, observer_id);
    if (result != NOTIFLY_SUCCESS) {
        printf("FAIL: Could not remove observer, result=%d (%s)\n", 
               result, notifly_result_to_string(result));
        return 1;
    }
    printf("Removed observer successfully\n");
    
    printf("PASS: Basic functionality test\n");
    return 0;
}

int test_multiple_observers() {
    printf("\n=== Test Multiple Observers ===\n");
    
    // Reset globals
    callback_count = 0;
    
    notifly_handle handle = notifly_default();
    
    // Add multiple observers
    int observer1 = notifly_add_observer(handle, 1002, simple_callback, NULL);
    int observer2 = notifly_add_observer(handle, 1002, simple_callback, NULL);
    int observer3 = notifly_add_observer(handle, 1002, simple_callback, NULL);
    
    if (observer1 <= 0 || observer2 <= 0 || observer3 <= 0) {
        printf("FAIL: Could not add observers\n");
        return 1;
    }
    printf("Added 3 observers: %d, %d, %d\n", observer1, observer2, observer3);
    
    // Post notification
    int result = notifly_post_notification(handle, 1002, NULL);
    if (result != 3) {
        printf("FAIL: Expected 3 observers notified, got %d\n", result);
        return 1;
    }
    printf("Posted notification, %d observers notified\n", result);
    
    if (callback_count != 3) {
        printf("FAIL: Expected 3 callbacks, got %d\n", callback_count);
        return 1;
    }
    
    // Remove all observers
    result = notifly_remove_all_observers(handle, 1002);
    if (result != 3) {
        printf("FAIL: Expected 3 observers removed, got %d\n", result);
        return 1;
    }
    printf("Removed all observers: %d\n", result);
    
    printf("PASS: Multiple observers test\n");
    return 0;
}

int test_async_notification() {
    printf("\n=== Test Async Notification ===\n");
    
    // Reset globals
    callback_count = 0;
    
    notifly_handle handle = notifly_default();
    
    int observer_id = notifly_add_observer(handle, 1003, simple_callback, NULL);
    if (observer_id <= 0) {
        printf("FAIL: Could not add observer\n");
        return 1;
    }
    
    // Post async notification
    int result = notifly_post_notification_async(handle, 1003, NULL);
    if (result != 1) {
        printf("FAIL: Expected 1 observer notified async, got %d\n", result);
        return 1;
    }
    printf("Posted async notification\n");
    
    // Wait a bit for async callback
    usleep(100000); // 100ms
    
    if (callback_count != 1) {
        printf("FAIL: Expected 1 async callback, got %d\n", callback_count);
        return 1;
    }
    
    // Cleanup
    notifly_remove_observer(handle, observer_id);
    
    printf("PASS: Async notification test\n");
    return 0;
}

int test_instance_creation() {
    printf("\n=== Test Instance Creation ===\n");
    
    // Create our own instance
    notifly_handle handle = notifly_create();
    if (!handle) {
        printf("FAIL: Could not create instance\n");
        return 1;
    }
    printf("Created notifly instance\n");
    
    // Reset globals
    callback_count = 0;
    
    int observer_id = notifly_add_observer(handle, 1004, simple_callback, NULL);
    if (observer_id <= 0) {
        printf("FAIL: Could not add observer to custom instance\n");
        notifly_destroy(handle);
        return 1;
    }
    
    int result = notifly_post_notification(handle, 1004, NULL);
    if (result != 1) {
        printf("FAIL: Expected 1 observer notified, got %d\n", result);
        notifly_destroy(handle);
        return 1;
    }
    
    if (callback_count != 1) {
        printf("FAIL: Expected 1 callback, got %d\n", callback_count);
        notifly_destroy(handle);
        return 1;
    }
    
    // Destroy instance
    notifly_destroy(handle);
    printf("Destroyed notifly instance\n");
    
    printf("PASS: Instance creation test\n");
    return 0;
}

int test_error_handling() {
    printf("\n=== Test Error Handling ===\n");
    
    // Test invalid handle
    int result = notifly_add_observer(NULL, 1005, simple_callback, NULL);
    if (result != NOTIFLY_INVALID_HANDLE) {
        printf("FAIL: Expected NOTIFLY_INVALID_HANDLE for NULL handle, got %d\n", result);
        return 1;
    }
    printf("NULL handle correctly rejected\n");
    
    // Test NULL callback
    notifly_handle handle = notifly_default();
    result = notifly_add_observer(handle, 1005, NULL, NULL);
    if (result != NOTIFLY_INVALID_HANDLE) {
        printf("FAIL: Expected NOTIFLY_INVALID_HANDLE for NULL callback, got %d\n", result);
        return 1;
    }
    printf("NULL callback correctly rejected\n");
    
    // Test removing non-existent observer
    result = notifly_remove_observer(handle, 99999);
    if (result != NOTIFLY_OBSERVER_NOT_FOUND) {
        printf("FAIL: Expected NOTIFLY_OBSERVER_NOT_FOUND, got %d\n", result);
        return 1;
    }
    printf("Non-existent observer correctly rejected\n");
    
    // Test posting to non-existent notification
    result = notifly_post_notification(handle, 99999, NULL);
    if (result != NOTIFLY_NOTIFICATION_NOT_FOUND) {
        printf("FAIL: Expected NOTIFLY_NOTIFICATION_NOT_FOUND, got %d\n", result);
        return 1;
    }
    printf("Non-existent notification correctly rejected\n");
    
    printf("PASS: Error handling test\n");
    return 0;
}

int main() {
    printf("Starting notifly C interface tests...\n");
    
    int failed = 0;
    
    failed += test_basic_functionality();
    failed += test_multiple_observers();
    failed += test_async_notification();
    failed += test_instance_creation();
    failed += test_error_handling();
    
    if (failed == 0) {
        printf("\n✓ All tests passed!\n");
        return 0;
    } else {
        printf("\n✗ %d test(s) failed!\n", failed);
        return 1;
    }
}
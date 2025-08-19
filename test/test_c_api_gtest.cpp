/*
 * test_c_api_gtest.cpp
 * Google Test based tests for the notifly C interface
 * 
 * This file tests the C API using Google Test framework to provide
 * structured test reporting and better integration with CI/CD systems.
 */

#include <gtest/gtest.h>
extern "C" {
#include "notifly_c.h"
}
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif

// Test data structure
typedef struct {
    int value;
    char message[100];
} test_data_t;

// Global variables to track callback invocations
static int g_callback_count = 0;
static int g_last_notification_id = -1;
static test_data_t g_last_received_data = {0, ""};
static void* g_last_user_data = nullptr;

// Test callback function
void test_callback(int notification_id, void* data, void* user_data) {
    g_callback_count++;
    g_last_notification_id = notification_id;
    g_last_user_data = user_data;
    
    if (data) {
        test_data_t* test_data = (test_data_t*)data;
        g_last_received_data = *test_data;
    }
}

void simple_callback(int notification_id, void* data, void* user_data) {
    g_callback_count++;
    g_last_notification_id = notification_id;
    g_last_user_data = user_data;
}

// Test fixture class for C API tests
class NotiflyCAI : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset global state before each test
        g_callback_count = 0;
        g_last_notification_id = -1;
        g_last_received_data = {0, ""};
        g_last_user_data = nullptr;
    }

    void TearDown() override {
        // Clean up any remaining observers after each test
        // This ensures tests don't interfere with each other
    }
};

// Test version information
TEST_F(NotiflyCAI, VersionConstants) {
    // Test that version constants are defined
    EXPECT_EQ(NOTIFLY_C_VERSION_MAJOR, 1);
    EXPECT_EQ(NOTIFLY_C_VERSION_MINOR, 0);
    EXPECT_EQ(NOTIFLY_C_VERSION_PATCH, 0);
}

// Test basic functionality
TEST_F(NotiflyCAI, BasicFunctionality) {
    // Get default handle
    notifly_handle handle = notifly_default();
    ASSERT_NE(handle, nullptr) << "Could not get default handle";
    
    // Add observer
    const char* user_data = "test user data";
    int observer_id = notifly_add_observer(handle, 1001, test_callback, (void*)user_data);
    ASSERT_GT(observer_id, 0) << "Could not add observer, result=" << observer_id 
                              << " (" << notifly_result_to_string(observer_id) << ")";
    
    // Post notification with data
    test_data_t test_data = {42, "Hello from Google Test!"};
    int result = notifly_post_notification(handle, 1001, &test_data);
    ASSERT_GT(result, 0) << "Could not post notification, result=" << result 
                         << " (" << notifly_result_to_string(result) << ")";
    EXPECT_EQ(result, 1) << "Expected 1 observer notified";
    
    // Verify callback was called correctly
    EXPECT_EQ(g_callback_count, 1) << "Callback should have been called once";
    EXPECT_EQ(g_last_notification_id, 1001) << "Wrong notification ID in callback";
    EXPECT_EQ(g_last_received_data.value, 42) << "Wrong data value received";
    EXPECT_STREQ(g_last_received_data.message, "Hello from Google Test!") << "Wrong message received";
    EXPECT_EQ(g_last_user_data, (void*)user_data) << "Wrong user data received";
    
    // Remove observer
    result = notifly_remove_observer(handle, observer_id);
    EXPECT_EQ(result, NOTIFLY_SUCCESS) << "Could not remove observer, result=" << result 
                                       << " (" << notifly_result_to_string(result) << ")";
}

// Test multiple observers
TEST_F(NotiflyCAI, MultipleObservers) {
    notifly_handle handle = notifly_default();
    ASSERT_NE(handle, nullptr);
    
    // Add multiple observers
    int observer1 = notifly_add_observer(handle, 1002, simple_callback, nullptr);
    int observer2 = notifly_add_observer(handle, 1002, simple_callback, nullptr);
    int observer3 = notifly_add_observer(handle, 1002, simple_callback, nullptr);
    
    ASSERT_GT(observer1, 0) << "Could not add observer1";
    ASSERT_GT(observer2, 0) << "Could not add observer2";
    ASSERT_GT(observer3, 0) << "Could not add observer3";
    
    // Verify observers have different IDs
    EXPECT_NE(observer1, observer2);
    EXPECT_NE(observer1, observer3);
    EXPECT_NE(observer2, observer3);
    
    // Post notification
    int result = notifly_post_notification(handle, 1002, nullptr);
    EXPECT_EQ(result, 3) << "Expected 3 observers notified";
    EXPECT_EQ(g_callback_count, 3) << "Expected 3 callbacks";
    
    // Remove all observers
    result = notifly_remove_all_observers(handle, 1002);
    EXPECT_EQ(result, 3) << "Expected 3 observers removed";
}

// Test asynchronous notification
TEST_F(NotiflyCAI, AsyncNotification) {
    notifly_handle handle = notifly_default();
    ASSERT_NE(handle, nullptr);
    
    int observer_id = notifly_add_observer(handle, 1003, simple_callback, nullptr);
    ASSERT_GT(observer_id, 0) << "Could not add observer";
    
    // Post async notification
    int result = notifly_post_notification_async(handle, 1003, nullptr);
    EXPECT_EQ(result, 1) << "Expected 1 observer notified async";
    
    // Wait for async callback
    usleep(100000); // 100ms
    
    EXPECT_EQ(g_callback_count, 1) << "Expected 1 async callback";
    EXPECT_EQ(g_last_notification_id, 1003) << "Wrong notification ID in async callback";
    
    // Cleanup
    notifly_remove_observer(handle, observer_id);
}

// Test instance creation and destruction
TEST_F(NotiflyCAI, InstanceCreation) {
    // Create custom instance
    notifly_handle handle = notifly_create();
    ASSERT_NE(handle, nullptr) << "Could not create instance";
    
    int observer_id = notifly_add_observer(handle, 1004, simple_callback, nullptr);
    ASSERT_GT(observer_id, 0) << "Could not add observer to custom instance";
    
    int result = notifly_post_notification(handle, 1004, nullptr);
    EXPECT_EQ(result, 1) << "Expected 1 observer notified";
    EXPECT_EQ(g_callback_count, 1) << "Expected 1 callback";
    
    // Destroy instance (this should clean up observers automatically)
    notifly_destroy(handle);
}

// Test error handling
TEST_F(NotiflyCAI, ErrorHandling) {
    // Test invalid handle
    int result = notifly_add_observer(nullptr, 1005, simple_callback, nullptr);
    EXPECT_EQ(result, NOTIFLY_INVALID_HANDLE) << "NULL handle should be rejected";
    
    // Test NULL callback
    notifly_handle handle = notifly_default();
    result = notifly_add_observer(handle, 1005, nullptr, nullptr);
    EXPECT_EQ(result, NOTIFLY_INVALID_HANDLE) << "NULL callback should be rejected";
    
    // Test removing non-existent observer
    result = notifly_remove_observer(handle, 99999);
    EXPECT_EQ(result, NOTIFLY_OBSERVER_NOT_FOUND) << "Non-existent observer should return error";
    
    // Test posting to non-existent notification
    result = notifly_post_notification(handle, 99999, nullptr);
    EXPECT_EQ(result, NOTIFLY_NOTIFICATION_NOT_FOUND) << "Non-existent notification should return error";
    
    // Test other invalid handle cases
    result = notifly_remove_observer(nullptr, 1);
    EXPECT_EQ(result, NOTIFLY_INVALID_HANDLE) << "NULL handle should be rejected for remove_observer";
    
    result = notifly_post_notification(nullptr, 1005, nullptr);
    EXPECT_EQ(result, NOTIFLY_INVALID_HANDLE) << "NULL handle should be rejected for post_notification";
    
    result = notifly_post_notification_async(nullptr, 1005, nullptr);
    EXPECT_EQ(result, NOTIFLY_INVALID_HANDLE) << "NULL handle should be rejected for post_notification_async";
    
    result = notifly_remove_all_observers(nullptr, 1005);
    EXPECT_EQ(result, NOTIFLY_INVALID_HANDLE) << "NULL handle should be rejected for remove_all_observers";
}

// Test result string conversion
TEST_F(NotiflyCAI, ResultToString) {
    EXPECT_STREQ(notifly_result_to_string(NOTIFLY_SUCCESS), "Success");
    EXPECT_STREQ(notifly_result_to_string(NOTIFLY_OBSERVER_NOT_FOUND), "Observer not found");
    EXPECT_STREQ(notifly_result_to_string(NOTIFLY_NOTIFICATION_NOT_FOUND), "Notification not found");
    EXPECT_STREQ(notifly_result_to_string(NOTIFLY_PAYLOAD_TYPE_NOT_MATCH), "Payload type mismatch");
    EXPECT_STREQ(notifly_result_to_string(NOTIFLY_NO_MORE_OBSERVER_IDS), "No more observer IDs available");
    EXPECT_STREQ(notifly_result_to_string(NOTIFLY_INVALID_HANDLE), "Invalid handle");
    EXPECT_STREQ(notifly_result_to_string(999), "Unknown error");
}

// Test data passing with different structures
TEST_F(NotiflyCAI, DataPassingVariousTypes) {
    notifly_handle handle = notifly_default();
    ASSERT_NE(handle, nullptr);
    
    // Test with integer
    int observer_id = notifly_add_observer(handle, 2001, test_callback, nullptr);
    ASSERT_GT(observer_id, 0);
    
    int int_data = 123;
    int result = notifly_post_notification(handle, 2001, &int_data);
    EXPECT_EQ(result, 1);
    EXPECT_EQ(g_callback_count, 1);
    
    notifly_remove_observer(handle, observer_id);
    
    // Test with string
    SetUp(); // Reset callback counters
    observer_id = notifly_add_observer(handle, 2002, test_callback, nullptr);
    ASSERT_GT(observer_id, 0);
    
    const char* str_data = "Test String";
    result = notifly_post_notification(handle, 2002, (void*)str_data);
    EXPECT_EQ(result, 1);
    EXPECT_EQ(g_callback_count, 1);
    
    notifly_remove_observer(handle, observer_id);
}

// Test observer ID reuse
TEST_F(NotiflyCAI, ObserverIDReuse) {
    notifly_handle handle = notifly_default();
    ASSERT_NE(handle, nullptr);
    
    // Add and remove observer multiple times
    for (int i = 0; i < 5; i++) {
        int observer_id = notifly_add_observer(handle, 3000 + i, simple_callback, nullptr);
        ASSERT_GT(observer_id, 0) << "Iteration " << i;
        
        int result = notifly_remove_observer(handle, observer_id);
        EXPECT_EQ(result, NOTIFLY_SUCCESS) << "Iteration " << i;
    }
}

// Test concurrent notifications (basic test)
TEST_F(NotiflyCAI, MultipleNotifications) {
    notifly_handle handle = notifly_default();
    ASSERT_NE(handle, nullptr);
    
    int observer1 = notifly_add_observer(handle, 4001, simple_callback, nullptr);
    int observer2 = notifly_add_observer(handle, 4002, simple_callback, nullptr);
    
    ASSERT_GT(observer1, 0);
    ASSERT_GT(observer2, 0);
    
    // Post to different notifications
    int result1 = notifly_post_notification(handle, 4001, nullptr);
    int result2 = notifly_post_notification(handle, 4002, nullptr);
    
    EXPECT_EQ(result1, 1);
    EXPECT_EQ(result2, 1);
    EXPECT_EQ(g_callback_count, 2);
    
    // Cleanup
    notifly_remove_observer(handle, observer1);
    notifly_remove_observer(handle, observer2);
}

// Test default instance behavior
TEST_F(NotiflyCAI, DefaultInstanceBehavior) {
    notifly_handle handle1 = notifly_default();
    notifly_handle handle2 = notifly_default();
    
    ASSERT_NE(handle1, nullptr);
    ASSERT_NE(handle2, nullptr);
    // Both should return the same default instance
    EXPECT_EQ(handle1, handle2) << "Default instance should be singleton";
    
    // Test that observers added to one handle are visible to the other
    int observer_id = notifly_add_observer(handle1, 5001, simple_callback, nullptr);
    ASSERT_GT(observer_id, 0);
    
    int result = notifly_post_notification(handle2, 5001, nullptr);
    EXPECT_EQ(result, 1) << "Notification should work across default handles";
    
    // Cleanup
    notifly_remove_observer(handle1, observer_id);
}
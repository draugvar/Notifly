/*
 * c_api_test.c
 * Simple C-based tests for the notifly C interface
 *
 * This file tests the C API using simple assertion-based testing
 * without external framework dependencies.
 */

#include "notifly_c.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(x) Sleep(x)
#else
#include <unistd.h>
#define sleep_ms(x) usleep((x)*1000)
#endif

/* Test tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test assertions */
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ✗ FAILED: %s\n    at %s:%d\n", message, __FILE__, __LINE__); \
            tests_failed++; \
            return 0; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, message) \
    ASSERT((a) == (b), message)

#define ASSERT_NE(a, b, message) \
    ASSERT((a) != (b), message)

#define ASSERT_NULL(ptr, message) \
    ASSERT((ptr) == NULL, message)

#define ASSERT_NOT_NULL(ptr, message) \
    ASSERT((ptr) != NULL, message)

#define RUN_TEST(test_func) \
    do { \
        printf("\nRunning: %s\n", #test_func); \
        tests_run++; \
        if (test_func()) { \
            printf("  ✓ PASSED\n"); \
            tests_passed++; \
        } \
    } while(0)

/* Test data structure */
typedef struct {
    int value;
    char message[100];
} test_data_t;

/* Global variables to track callback invocations */
static int callback_count = 0;
static int last_notification_id = -1;
static test_data_t last_received_data = {0, ""};
static void* last_user_data = NULL;
static void* response_data_to_send = NULL;
static int response_notification_id = -1;

/* Reset test state */
static void reset_test_state(void) {
    callback_count = 0;
    last_notification_id = -1;
    last_received_data.value = 0;
    memset(last_received_data.message, 0, sizeof(last_received_data.message));
    last_user_data = NULL;
}

/* Test callback functions */
void test_callback(const int notification_id, void* data, void* user_data) {
    callback_count++;
    last_notification_id = notification_id;
    last_user_data = user_data;

    if (data) {
        const test_data_t* test_data = (test_data_t*)data;
        last_received_data = *test_data;
    }
}

void simple_callback(const int notification_id, void* data, void* user_data) {
    callback_count++;
    last_notification_id = notification_id;
    last_user_data = user_data;
}

void responder_callback(int notification_id, void* data, void* user_data) {
    sleep_ms(50);
    notifly_handle handle = notifly_default();
    notifly_post_notification(handle, response_notification_id, response_data_to_send);
}

void non_responder_callback(int notification_id, void* data, void* user_data) {
    callback_count++;
}

/* Test functions */

int test_basic_functionality(void) {
    reset_test_state();

    notifly_handle handle = notifly_default();
    ASSERT_NOT_NULL(handle, "Could not get default handle");

    const char* user_data = "test user data";
    const int observer_id = notifly_add_observer(handle, 1001, test_callback, (void*)user_data);
    ASSERT(observer_id > 0, "Could not add observer");

    test_data_t test_data = {42, "Hello from C test!"};
    int result = notifly_post_notification(handle, 1001, &test_data);
    ASSERT(result > 0, "Could not post notification");
    ASSERT_EQ(result, 1, "Expected 1 observer notified");

    ASSERT_EQ(callback_count, 1, "Callback should have been called once");
    ASSERT_EQ(last_notification_id, 1001, "Wrong notification ID");
    ASSERT_EQ(last_received_data.value, 42, "Wrong data value");
    ASSERT(strcmp(last_received_data.message, "Hello from C test!") == 0, "Wrong message");
    ASSERT(last_user_data == (void*)user_data, "Wrong user data");

    result = notifly_remove_observer(handle, observer_id);
    ASSERT_EQ(result, NOTIFLY_SUCCESS, "Could not remove observer");

    return 1;
}

int test_multiple_observers(void) {
    reset_test_state();

    notifly_handle handle = notifly_default();
    ASSERT_NOT_NULL(handle, "Could not get default handle");

    const int observer1 = notifly_add_observer(handle, 1002, simple_callback, NULL);
    const int observer2 = notifly_add_observer(handle, 1002, simple_callback, NULL);
    const int observer3 = notifly_add_observer(handle, 1002, simple_callback, NULL);

    ASSERT(observer1 > 0, "Could not add observer1");
    ASSERT(observer2 > 0, "Could not add observer2");
    ASSERT(observer3 > 0, "Could not add observer3");
    ASSERT_NE(observer1, observer2, "Observer IDs should be different");

    int result = notifly_post_notification(handle, 1002, NULL);
    ASSERT_EQ(result, 3, "Expected 3 observers notified");
    ASSERT_EQ(callback_count, 3, "Expected 3 callbacks");

    result = notifly_remove_all_observers(handle, 1002);
    ASSERT_EQ(result, 3, "Expected 3 observers removed");

    return 1;
}

int test_error_handling(void) {
    reset_test_state();

    int result = notifly_add_observer(NULL, 1005, simple_callback, NULL);
    ASSERT_EQ(result, NOTIFLY_INVALID_HANDLE, "NULL handle should be rejected");

    notifly_handle handle = notifly_default();
    result = notifly_add_observer(handle, 1005, NULL, NULL);
    ASSERT_EQ(result, NOTIFLY_INVALID_HANDLE, "NULL callback should be rejected");

    result = notifly_remove_observer(handle, 99999);
    ASSERT_EQ(result, NOTIFLY_OBSERVER_NOT_FOUND, "Non-existent observer should return error");

    result = notifly_post_notification(handle, 99999, NULL);
    ASSERT_EQ(result, NOTIFLY_NOTIFICATION_NOT_FOUND, "Non-existent notification should return error");

    return 1;
}

int test_post_and_wait_success(void) {
    reset_test_state();

    notifly_handle handle = notifly_default();
    ASSERT_NOT_NULL(handle, "Could not get default handle");

    notifly_remove_all_observers(handle, 6001);
    notifly_remove_all_observers(handle, 6002);

    test_data_t response = {99, "Response data"};
    response_data_to_send = &response;
    response_notification_id = 6002;

    const int responder_id = notifly_add_observer(handle, 6001, responder_callback, NULL);
    ASSERT(responder_id > 0, "Could not add responder");

    test_data_t request = {42, "Request data"};
    void* received_data = NULL;
    const int result = notifly_post_and_wait(handle, 6001, 6002, 500, &request, &received_data);

    ASSERT_EQ(result, NOTIFLY_SUCCESS, "post_and_wait should succeed");
    ASSERT_NOT_NULL(received_data, "Should receive response data");

    const test_data_t* response_ptr = (test_data_t*)received_data;
    ASSERT_EQ(response_ptr->value, 99, "Wrong response value");
    ASSERT(strcmp(response_ptr->message, "Response data") == 0, "Wrong response message");

    notifly_remove_observer(handle, responder_id);
    notifly_remove_all_observers(handle, 6001);
    notifly_remove_all_observers(handle, 6002);

    return 1;
}

int test_post_and_wait_timeout(void) {
    reset_test_state();

    notifly_handle handle = notifly_default();
    ASSERT_NOT_NULL(handle, "Could not get default handle");

    notifly_remove_all_observers(handle, 6003);
    notifly_remove_all_observers(handle, 6004);

    const int observer_id = notifly_add_observer(handle, 6003, non_responder_callback, NULL);
    ASSERT(observer_id > 0, "Could not add observer");

    test_data_t request = {42, "Request data"};
    void* received_data = NULL;
    const int result = notifly_post_and_wait(handle, 6003, 6004, 100, &request, &received_data);

    ASSERT_EQ(result, NOTIFLY_TIMEOUT, "Should timeout waiting for response");
    ASSERT_NULL(received_data, "Should not receive any data on timeout");
    ASSERT(callback_count > 0, "Request callback should have been called");

    notifly_remove_observer(handle, observer_id);
    notifly_remove_all_observers(handle, 6003);
    notifly_remove_all_observers(handle, 6004);

    return 1;
}

int test_post_and_wait_invalid_params(void) {
    reset_test_state();

    notifly_handle handle = notifly_default();
    test_data_t request = {42, "Request data"};
    void* received_data = NULL;

    int result = notifly_post_and_wait(NULL, 6007, 6008, 100, &request, &received_data);
    ASSERT_EQ(result, NOTIFLY_INVALID_HANDLE, "Should fail with NULL handle");

    result = notifly_post_and_wait(handle, 6005, 6006, 100, &request, NULL);
    ASSERT_EQ(result, NOTIFLY_INVALID_HANDLE, "Should fail with NULL response pointer");

    return 1;
}

/* Main test runner */
int main(void) {
    printf("==========================================\n");
    printf("  Notifly C API Test Suite\n");
    printf("==========================================\n");

    RUN_TEST(test_basic_functionality);
    RUN_TEST(test_multiple_observers);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_post_and_wait_success);
    RUN_TEST(test_post_and_wait_timeout);
    RUN_TEST(test_post_and_wait_invalid_params);

    printf("\n==========================================\n");
    printf("  Test Results\n");
    printf("==========================================\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("==========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}

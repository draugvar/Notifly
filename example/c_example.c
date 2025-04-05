/**
 * @file c_example.c
 * @author Salvatore Rivieccio
 * @date 2025-04-05
 * @brief Example demonstrating the usage of the C Notifly API
 *
 * This example showcases how to use the Notifly notification system
 * with different types of observers and payload structures.
 */

#include "c_notifly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * @brief Simple numeric payload demonstrating basic data passing
 */
typedef struct numeric_payload {
    int a;
    int b;
    const char* operation;
} numeric_payload_t;

/**
 * @brief Sensor data payload demonstrating a more practical use case
 */
typedef struct sensor_data {
    double temperature;
    double humidity;
    const char* sensor_id;
    unsigned long timestamp;
} sensor_data_t;

/**
 * @brief Callback function for handling numeric operations
 * @param payload Pointer to the numeric_payload structure
 */
void numeric_callback(const numeric_payload_t* payload)
{
    assert(payload != NULL);
    
    if (strcmp(payload->operation, "sum") == 0) {
        printf("Sum operation: %d + %d = %d\n", payload->a, payload->b, payload->a + payload->b);
    } else if (strcmp(payload->operation, "multiply") == 0) {
        printf("Multiply operation: %d * %d = %d\n", payload->a, payload->b, payload->a * payload->b);
    } else {
        printf("Unknown operation '%s' on values %d and %d\n", payload->operation, payload->a, payload->b);
    }
}

/**
 * @brief Callback function for handling temperature alerts
 * @param data Pointer to the sensor_data structure
 */
void temperature_alert_callback(const sensor_data_t* data)
{
    assert(data != NULL);
    
    if (data->temperature > 30.0) {
        printf("⚠️ HIGH TEMPERATURE ALERT: %.1f°C from sensor %s at timestamp %lu\n", 
               data->temperature, data->sensor_id, data->timestamp);
    } else {
        printf("Temperature reading: %.1f°C from sensor %s\n", data->temperature, data->sensor_id);
    }
}

/**
 * @brief Callback function for logging all sensor data
 * @param data Pointer to the sensor_data structure
 */
void sensor_logger_callback(const sensor_data_t* data)
{
    assert(data != NULL);
    
    printf("Sensor Logger: ID=%s, Temp=%.1f°C, Humidity=%.1f%%, Timestamp=%lu\n",
           data->sensor_id, data->temperature, data->humidity, data->timestamp);
}

/**
 * @brief Example entry point
 */
int main(void)
{
    // Create notification handler with default configuration
    notifly_handler_t* handler = notifly_create(0);
    if (handler == NULL) {
        fprintf(stderr, "Failed to create notifly handler\n");
        return EXIT_FAILURE;
    }
    
    printf("=== Notifly Example Application ===\n\n");
    
    // === Numeric Operations Example ===
    printf("-- Numeric Operations Demo --\n");
    
    // Register observer for numeric operations (notification type 1)
    if (notifly_add_observer(handler, 1, (notifly_callback_t)numeric_callback) < 0) {
        fprintf(stderr, "Failed to add numeric callback\n");
        notifly_destroy(handler);
        return EXIT_FAILURE;
    }
    
    // Send synchronous notification with sum operation
    numeric_payload_t num_payload;
    num_payload.a = 5;
    num_payload.b = 10;
    num_payload.operation = "sum";
    notifly_post_notification(handler, 1, &num_payload);
    
    // Send synchronous notification with multiply operation
    num_payload.a = 7;
    num_payload.b = 6;
    num_payload.operation = "multiply";
    notifly_post_notification(handler, 1, &num_payload);
    
    // === Sensor Data Example ===
    printf("\n-- Sensor Monitoring Demo --\n");
    
    // Register observers for sensor data (notification type 2)
    if (notifly_add_observer(handler, 2, (notifly_callback_t)temperature_alert_callback) < 0 ||
        notifly_add_observer(handler, 2, (notifly_callback_t)sensor_logger_callback) < 0) {
        fprintf(stderr, "Failed to add sensor data observers\n");
        notifly_destroy(handler);
        return EXIT_FAILURE;
    }
    
    // Send normal temperature reading (synchronous)
    sensor_data_t sensor_payload = {
        .temperature = 25.5,
        .humidity = 60.0,
        .sensor_id = "living_room_01",
        .timestamp = 1712345678
    };
    notifly_post_notification(handler, 2, &sensor_payload);
    
    // Send high temperature alert (asynchronous)
    printf("\nSubmitting high temperature notification asynchronously...\n");
    sensor_payload.temperature = 32.7;
    sensor_payload.humidity = 45.0;
    sensor_payload.sensor_id = "kitchen_02";
    sensor_payload.timestamp = 1712345800;
    notifly_post_notification_async(handler, 2, &sensor_payload);
    
    // Small delay to allow async notifications to be processed
    printf("Waiting for async notifications to complete...\n");
    
    // In a real application, you might use a proper synchronization mechanism
    // For this example, we're relying on notifly_destroy to wait for completion
    printf("\nCleaning up resources...\n");
    notifly_destroy(handler);
    
    printf("\n=== Example completed successfully ===\n");
    return EXIT_SUCCESS;
}

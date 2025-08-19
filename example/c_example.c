/*
 * c_example.c
 * Simple example demonstrating the notifly C interface
 */

#include "notifly_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif

// Message IDs
#define MSG_STARTUP 1001
#define MSG_DATA_RECEIVED 1002  
#define MSG_SHUTDOWN 1003

// Example data structure
typedef struct {
    int sensor_id;
    float temperature;
    char location[50];
} sensor_data_t;

// Application context
typedef struct {
    const char* app_name;
    int message_count;
} app_context_t;

// Callback for startup notifications
void on_startup(int notification_id, void* data, void* user_data) {
    app_context_t* ctx = (app_context_t*)user_data;
    printf("[%s] System startup notification received\n", ctx->app_name);
    ctx->message_count++;
}

// Callback for sensor data notifications
void on_sensor_data(int notification_id, void* data, void* user_data) {
    app_context_t* ctx = (app_context_t*)user_data;
    sensor_data_t* sensor = (sensor_data_t*)data;
    
    if (sensor) {
        printf("[%s] Sensor data received:\n", ctx->app_name);
        printf("  Sensor ID: %d\n", sensor->sensor_id);
        printf("  Temperature: %.1f°C\n", sensor->temperature);
        printf("  Location: %s\n", sensor->location);
    }
    
    ctx->message_count++;
}

// Callback for shutdown notifications  
void on_shutdown(int notification_id, void* data, void* user_data) {
    app_context_t* ctx = (app_context_t*)user_data;
    printf("[%s] Shutdown notification received\n", ctx->app_name);
    ctx->message_count++;
}

// Generic callback for logging all notifications
void on_any_message(int notification_id, void* data, void* user_data) {
    const char* observer_name = (const char*)user_data;
    printf("[%s] Notification %d received\n", observer_name, notification_id);
}

int main() {
    printf("=== Notifly C Interface Example ===\n\n");
    
    // Create application context
    app_context_t app_ctx = {
        .app_name = "SensorApp",
        .message_count = 0
    };
    
    // Get the default notification center
    notifly_handle notifly = notifly_default();
    if (!notifly) {
        printf("ERROR: Failed to get default notification center\n");
        return 1;
    }
    
    printf("1. Setting up observers...\n");
    
    // Add observers for different message types
    int startup_observer = notifly_add_observer(notifly, MSG_STARTUP, on_startup, &app_ctx);
    int data_observer = notifly_add_observer(notifly, MSG_DATA_RECEIVED, on_sensor_data, &app_ctx);
    int shutdown_observer = notifly_add_observer(notifly, MSG_SHUTDOWN, on_shutdown, &app_ctx);
    
    // Add a logger that observes all message types
    int logger1 = notifly_add_observer(notifly, MSG_STARTUP, on_any_message, (void*)"Logger");
    int logger2 = notifly_add_observer(notifly, MSG_DATA_RECEIVED, on_any_message, (void*)"Logger");
    int logger3 = notifly_add_observer(notifly, MSG_SHUTDOWN, on_any_message, (void*)"Logger");
    
    if (startup_observer <= 0 || data_observer <= 0 || shutdown_observer <= 0 ||
        logger1 <= 0 || logger2 <= 0 || logger3 <= 0) {
        printf("ERROR: Failed to add observers\n");
        return 1;
    }
    
    printf("   Added %d observers successfully\n", 6);
    printf("\n2. Sending notifications...\n\n");
    
    // Send startup notification
    int result = notifly_post_notification(notifly, MSG_STARTUP, NULL);
    printf("   Startup notification sent to %d observers\n\n", result);
    
    // Send sensor data notifications
    sensor_data_t sensors[] = {
        {101, 23.5, "Living Room"},
        {102, 19.8, "Bedroom"},
        {103, 25.1, "Kitchen"}
    };
    
    for (int i = 0; i < 3; i++) {
        result = notifly_post_notification(notifly, MSG_DATA_RECEIVED, &sensors[i]);
        printf("   Sensor data notification sent to %d observers\n\n", result);
    }
    
    // Send some async notifications
    printf("3. Sending async notifications...\n\n");
    
    sensor_data_t outdoor_sensor = {201, 15.3, "Outdoor"};
    result = notifly_post_notification_async(notifly, MSG_DATA_RECEIVED, &outdoor_sensor);
    printf("   Async sensor data notification sent to %d observers\n", result);
    
    // Wait a bit for async notifications to complete
    usleep(100000); // 100ms
    printf("\n");
    
    // Send shutdown notification
    result = notifly_post_notification(notifly, MSG_SHUTDOWN, NULL);
    printf("   Shutdown notification sent to %d observers\n\n", result);
    
    printf("4. Summary:\n");
    printf("   Application processed %d notifications\n", app_ctx.message_count);
    
    // Clean up observers
    printf("\n5. Cleaning up...\n");
    
    notifly_remove_observer(notifly, startup_observer);
    notifly_remove_observer(notifly, data_observer);  
    notifly_remove_observer(notifly, shutdown_observer);
    
    // Remove all logger observers for MSG_DATA_RECEIVED
    int removed = notifly_remove_all_observers(notifly, MSG_DATA_RECEIVED);
    printf("   Removed %d remaining observers for data notifications\n", removed);
    
    // Remove remaining logger observers
    notifly_remove_observer(notifly, logger1);
    notifly_remove_observer(notifly, logger3);
    
    printf("   Cleanup complete\n\n");
    
    // Test creating our own instance
    printf("6. Testing custom instance...\n");
    
    notifly_handle custom_notifly = notifly_create();
    if (!custom_notifly) {
        printf("ERROR: Failed to create custom instance\n");
        return 1;
    }
    
    int custom_observer = notifly_add_observer(custom_notifly, 999, on_any_message, (void*)"Custom");
    result = notifly_post_notification(custom_notifly, 999, NULL);
    printf("   Custom instance notification sent to %d observers\n", result);
    
    notifly_destroy(custom_notifly);
    printf("   Custom instance destroyed\n");
    
    printf("\n=== Example completed successfully! ===\n");
    return 0;
}
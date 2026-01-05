/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "driver/gpio.h"
#include "can_autodetect.h"
#include "slcan_protocol.h"

static const char *TAG = "can_bridge";

// Default GPIO configuration (can be changed in menuconfig)
#ifndef CONFIG_CAN_TX_GPIO
#define CONFIG_CAN_TX_GPIO 4
#endif

#ifndef CONFIG_CAN_RX_GPIO
#define CONFIG_CAN_RX_GPIO 5
#endif

// Auto-detection timeout per bitrate attempt (ms)
#define AUTODETECT_TIMEOUT_MS 2000

// Bridge state
static twai_node_handle_t g_node_handle = NULL;
static QueueHandle_t g_rx_queue = NULL;
static bool g_bridge_running = false;

// Frame structure for queue
typedef struct {
    twai_frame_t frame;
    uint8_t data_buffer[64];
} queued_frame_t;

/**
 * @brief CAN RX callback - called from ISR when frame received
 */
static IRAM_ATTR bool can_rx_callback(twai_node_handle_t handle, 
                                       const twai_rx_done_event_data_t *event_data, 
                                       void *user_ctx)
{
    (void)event_data;
    
    QueueHandle_t rx_queue = (QueueHandle_t)user_ctx;
    BaseType_t higher_priority_task_woken = pdFALSE;
    
    // Receive frame directly in ISR
    queued_frame_t queued_frame = {0};
    queued_frame.frame.buffer = queued_frame.data_buffer;
    queued_frame.frame.buffer_len = sizeof(queued_frame.data_buffer);
    
    if (twai_node_receive_from_isr(handle, &queued_frame.frame) == ESP_OK) {
        // Send frame to queue
        xQueueSendFromISR(rx_queue, &queued_frame, &higher_priority_task_woken);
    }
    
    return (higher_priority_task_woken == pdTRUE);
}

/**
 * @brief Task to handle CAN RX and forward to USB
 */
static void can_rx_task(void *arg)
{
    queued_frame_t queued_frame;
    
    ESP_LOGI(TAG, "CAN RX task started");
    
    while (g_bridge_running) {
        // Wait for frame from queue
        if (xQueueReceive(g_rx_queue, &queued_frame, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Forward to PC via SLCAN (logging disabled to avoid interfering with SavvyCAN)
            slcan_send_frame(&queued_frame.frame);
        }
    }
    
    ESP_LOGI(TAG, "CAN RX task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Task to handle USB RX and process SLCAN commands
 */
static void usb_rx_task(void *arg)
{
    char buffer[128];
    int pos = 0;
    
    ESP_LOGI(TAG, "USB RX task started");
    
    while (g_bridge_running) {
        // Read from stdin (USB CDC)
        int c = fgetc(stdin);
        
        if (c == EOF || c < 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        // Check for command terminator
        if (c == '\r' || c == '\n') {
            if (pos > 0) {
                buffer[pos] = '\0';
                
                // Process SLCAN command
                slcan_process_command((uint8_t *)buffer, pos);
                
                pos = 0;
            }
        } else {
            // Add to buffer
            if (pos < sizeof(buffer) - 1) {
                buffer[pos++] = c;
            } else {
                // Buffer overflow - reset
                ESP_LOGW(TAG, "Command buffer overflow");
                pos = 0;
            }
        }
    }
    
    ESP_LOGI(TAG, "USB RX task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Initialize CAN bridge with auto-detection
 */
static esp_err_t init_can_bridge(void)
{
    esp_err_t ret;
    uint32_t detected_bitrate = 0;
    
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "CAN Bridge for SavvyCAN");
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "TX GPIO: %d", CONFIG_CAN_TX_GPIO);
    ESP_LOGI(TAG, "RX GPIO: %d", CONFIG_CAN_RX_GPIO);
    ESP_LOGI(TAG, "");
    
    // Auto-detect bitrate
    ESP_LOGI(TAG, "Starting CAN bitrate auto-detection...");
    ESP_LOGI(TAG, "This may take several seconds...");
    
    ret = can_autodetect_bitrate(CONFIG_CAN_TX_GPIO, CONFIG_CAN_RX_GPIO, 
                                  &detected_bitrate, AUTODETECT_TIMEOUT_MS);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to auto-detect bitrate!");
        ESP_LOGE(TAG, "Please check:");
        ESP_LOGE(TAG, "  - CAN transceiver connections");
        ESP_LOGE(TAG, "  - CAN bus has active traffic");
        ESP_LOGE(TAG, "  - GPIO configuration (TX:%d, RX:%d)", 
                 CONFIG_CAN_TX_GPIO, CONFIG_CAN_RX_GPIO);
        return ret;
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "✓ CAN bitrate detected: %lu bps", detected_bitrate);
    ESP_LOGI(TAG, "");
    
    // Initialize CAN bridge
    ret = can_bridge_init(CONFIG_CAN_TX_GPIO, CONFIG_CAN_RX_GPIO, 
                          detected_bitrate, &g_node_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CAN bridge");
        return ret;
    }
    
    // Create RX queue for ISR communication (must hold full frame data)
    g_rx_queue = xQueueCreate(50, sizeof(queued_frame_t));
    if (g_rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        can_bridge_deinit(g_node_handle);
        return ESP_ERR_NO_MEM;
    }
    
    // Register RX callback
    twai_event_callbacks_t callbacks = {
        .on_rx_done = can_rx_callback,
    };
    ret = twai_node_register_event_callbacks(g_node_handle, &callbacks, g_rx_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register callbacks");
        vQueueDelete(g_rx_queue);
        can_bridge_deinit(g_node_handle);
        return ret;
    }
    
    // Enable the TWAI node to start receiving
    ret = twai_node_enable(g_node_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TWAI node");
        vQueueDelete(g_rx_queue);
        can_bridge_deinit(g_node_handle);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ CAN bridge initialized successfully");
    ESP_LOGI(TAG, "✓ TWAI node enabled and ready to receive");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Bridge is now running!");
    ESP_LOGI(TAG, "Connect SavvyCAN to this USB port.");
    ESP_LOGI(TAG, "SLCAN protocol ready.");
    ESP_LOGI(TAG, "===================================");
    
    return ESP_OK;
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    // Initialize SLCAN protocol
    slcan_init();
    
    // Initialize CAN bridge with auto-detection
    esp_err_t ret = init_can_bridge();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CAN bridge initialization failed, halting...");
        return;
    }
    
    // Start bridge
    g_bridge_running = true;
    
    // Create tasks
    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 10, NULL);
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 10, NULL);
    
    // Main loop - keep running (logging disabled to prevent SLCAN interference)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

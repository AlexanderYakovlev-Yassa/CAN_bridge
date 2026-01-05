/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "can_autodetect.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "can_autodetect";

// Prioritized list of bitrates to try (125k first as most probable)
static const uint32_t bitrate_list[] = {
    CAN_BITRATE_125K,
    CAN_BITRATE_250K,
    CAN_BITRATE_500K,
    CAN_BITRATE_1M,
    CAN_BITRATE_100K,
    CAN_BITRATE_50K,
};

#define BITRATE_COUNT (sizeof(bitrate_list) / sizeof(bitrate_list[0]))

// Frame queue for detection
static volatile bool frame_received = false;

/**
 * @brief RX callback for auto-detection
 */
static IRAM_ATTR bool detection_rx_callback(twai_node_handle_t handle, 
                                             const twai_rx_done_event_data_t *event_data, 
                                             void *user_ctx)
{
    (void)user_ctx;
    
    // Receive frame directly in ISR
    twai_frame_t frame = {0};
    uint8_t data_buffer[8];
    frame.buffer = data_buffer;
    frame.buffer_len = sizeof(data_buffer);
    
    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        frame_received = true;
        // Log frame details (safe since we're just setting a flag)
        if (event_data) {
            // Store frame ID for later logging
        }
    }
    
    return false;
}

/**
 * @brief Try to receive frames at a specific bitrate
 */
static esp_err_t try_bitrate(int tx_gpio, int rx_gpio, uint32_t bitrate, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Testing bitrate: %lu bps (timeout: %lu ms)", bitrate, timeout_ms);
    
    // Configure TWAI driver
    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = tx_gpio,
            .rx = rx_gpio,
            .quanta_clk_out = -1,
            .bus_off_indicator = -1,
        },
        .bit_timing = {
            .bitrate = bitrate,
        },
        .tx_queue_depth = 1,  // Minimum required for listen-only mode
        .flags = {
            .enable_listen_only = 1,  // Listen-only for detection
        },
    };
    
    twai_node_handle_t node_handle;
    esp_err_t ret = twai_new_node_onchip(&node_config, &node_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create node at %lu bps: %s", bitrate, esp_err_to_name(ret));
        return ret;
    }
    
    // Register RX callback
    frame_received = false;
    twai_event_callbacks_t callbacks = {
        .on_rx_done = detection_rx_callback,
    };
    ret = twai_node_register_event_callbacks(node_handle, &callbacks, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register callbacks: %s", esp_err_to_name(ret));
        twai_node_delete(node_handle);
        return ret;
    }
    
    // Enable node
    ret = twai_node_enable(node_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable node: %s", esp_err_to_name(ret));
        twai_node_delete(node_handle);
        return ret;
    }
    
    // Wait for frames
    int64_t start_time = esp_timer_get_time();
    int64_t timeout_us = timeout_ms * 1000;
    int checks = 0;
    
    while ((esp_timer_get_time() - start_time) < timeout_us) {
        if (frame_received) {
            ESP_LOGI(TAG, "Valid frame detected at %lu bps!", bitrate);
            twai_node_disable(node_handle);
            twai_node_delete(node_handle);
            return ESP_OK;
        }
        
        checks++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Cleanup
    twai_node_disable(node_handle);
    twai_node_delete(node_handle);
    
    ESP_LOGI(TAG, "No frames detected at %lu bps after %d checks (%lld ms)", 
             bitrate, checks, (esp_timer_get_time() - start_time) / 1000);
    return ESP_ERR_TIMEOUT;
}

esp_err_t can_autodetect_bitrate(int tx_gpio, int rx_gpio, uint32_t *detected_bitrate, uint32_t timeout_per_rate_ms)
{
    ESP_LOGI(TAG, "Starting CAN bitrate auto-detection...");
    ESP_LOGI(TAG, "TX GPIO: %d, RX GPIO: %d", tx_gpio, rx_gpio);
    
    for (int i = 0; i < BITRATE_COUNT; i++) {
        uint32_t bitrate = bitrate_list[i];
        
        esp_err_t ret = try_bitrate(tx_gpio, rx_gpio, bitrate, timeout_per_rate_ms);
        if (ret == ESP_OK) {
            *detected_bitrate = bitrate;
            ESP_LOGI(TAG, "Auto-detection successful! Bitrate: %lu bps", bitrate);
            return ESP_OK;
        }
        
        // Small delay between attempts
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGW(TAG, "Auto-detection failed - no valid CAN traffic detected on any bitrate");
    return ESP_ERR_TIMEOUT;
}

esp_err_t can_bridge_init(int tx_gpio, int rx_gpio, uint32_t bitrate, twai_node_handle_t *node_handle)
{
    ESP_LOGI(TAG, "Initializing CAN bridge at %lu bps", bitrate);
    
    // Configure TWAI driver for normal operation
    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = tx_gpio,
            .rx = rx_gpio,
            .quanta_clk_out = -1,
            .bus_off_indicator = -1,
        },
        .bit_timing = {
            .bitrate = bitrate,
        },
        .tx_queue_depth = 10,  // Required: minimum 1, use 10 for bridge operation
    };
    
    esp_err_t ret = twai_new_node_onchip(&node_config, node_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CAN node: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "CAN bridge initialized successfully");
    return ESP_OK;
}

esp_err_t can_bridge_deinit(twai_node_handle_t node_handle)
{
    ESP_LOGI(TAG, "Deinitializing CAN bridge");
    
    twai_node_disable(node_handle);
    esp_err_t ret = twai_node_delete(node_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize CAN bridge: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "CAN bridge deinitialized");
    return ESP_OK;
}

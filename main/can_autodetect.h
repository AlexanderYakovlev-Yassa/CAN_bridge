/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"
#include "esp_twai.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CAN bitrate auto-detection
 */

// Common CAN bitrates to try (prioritized list)
#define CAN_BITRATE_125K    125000
#define CAN_BITRATE_250K    250000
#define CAN_BITRATE_500K    500000
#define CAN_BITRATE_1M      1000000
#define CAN_BITRATE_100K    100000
#define CAN_BITRATE_50K     50000

/**
 * @brief Auto-detect CAN bus bitrate
 * 
 * Tries different bitrates starting with 125kbps and detects valid traffic
 * 
 * @param tx_gpio TX GPIO pin number
 * @param rx_gpio RX GPIO pin number
 * @param detected_bitrate Output: detected bitrate in bps
 * @param timeout_per_rate_ms Timeout for each bitrate attempt (ms)
 * 
 * @return ESP_OK if bitrate detected, ESP_ERR_TIMEOUT if no valid traffic found
 */
esp_err_t can_autodetect_bitrate(int tx_gpio, int rx_gpio, uint32_t *detected_bitrate, uint32_t timeout_per_rate_ms);

/**
 * @brief Initialize CAN bridge
 * 
 * @param tx_gpio TX GPIO pin number
 * @param rx_gpio RX GPIO pin number
 * @param bitrate Bitrate in bps
 * @param node_handle Output: TWAI node handle
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t can_bridge_init(int tx_gpio, int rx_gpio, uint32_t bitrate, twai_node_handle_t *node_handle);

/**
 * @brief Deinitialize CAN bridge
 * 
 * @param node_handle TWAI node handle
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t can_bridge_deinit(twai_node_handle_t node_handle);

#ifdef __cplusplus
}
#endif

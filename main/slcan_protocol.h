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
 * @brief SLCAN protocol handler
 * 
 * Implements Serial Line CAN protocol for communication with PC tools like SavvyCAN
 */

/**
 * @brief Initialize SLCAN protocol handler
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t slcan_init(void);

/**
 * @brief Process incoming SLCAN commands from USB
 * 
 * @param data Incoming data buffer
 * @param len Length of data
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t slcan_process_command(const uint8_t *data, size_t len);

/**
 * @brief Send CAN frame to PC in SLCAN format
 * 
 * @param frame CAN frame to send
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t slcan_send_frame(const twai_frame_t *frame);

/**
 * @brief Get current SLCAN bitrate setting
 * 
 * @return Bitrate in bps, or 0 if not set
 */
uint32_t slcan_get_bitrate(void);

/**
 * @brief Check if SLCAN channel is open
 * 
 * @return true if open, false otherwise
 */
bool slcan_is_open(void);

#ifdef __cplusplus
}
#endif

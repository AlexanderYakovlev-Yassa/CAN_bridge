/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "slcan_protocol.h"
#include "esp_log.h"

static const char *TAG = "slcan";

// SLCAN state
static struct {
    bool is_open;
    uint32_t bitrate;
    uint8_t timestamp_enabled;
} slcan_state = {
    .is_open = false,
    .bitrate = 0,
    .timestamp_enabled = 0
};

// Standard SLCAN bitrate codes
static const uint32_t slcan_bitrates[] = {
    [0] = 10000,    // S0
    [1] = 20000,    // S1
    [2] = 50000,    // S2
    [3] = 100000,   // S3
    [4] = 125000,   // S4
    [5] = 250000,   // S5
    [6] = 500000,   // S6
    [7] = 800000,   // S7
    [8] = 1000000,  // S8
};

/**
 * @brief Send response to PC via USB CDC
 */
static void slcan_send_response(const char *response)
{
    // For USB CDC, we write directly to stdout
    printf("%s", response);
    fflush(stdout);
}

/**
 * @brief Convert byte to hex string
 */
static void byte_to_hex(uint8_t byte, char *hex)
{
    const char hex_chars[] = "0123456789ABCDEF";
    hex[0] = hex_chars[(byte >> 4) & 0x0F];
    hex[1] = hex_chars[byte & 0x0F];
}

/**
 * @brief Convert hex char to nibble
 */
static int hex_to_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/**
 * @brief Convert hex string to byte
 */
static int hex_to_byte(const char *hex)
{
    int high = hex_to_nibble(hex[0]);
    int low = hex_to_nibble(hex[1]);
    if (high < 0 || low < 0) return -1;
    return (high << 4) | low;
}

esp_err_t slcan_init(void)
{
    slcan_state.is_open = false;
    slcan_state.bitrate = 0;
    slcan_state.timestamp_enabled = 0;
    
    ESP_LOGI(TAG, "SLCAN protocol initialized");
    return ESP_OK;
}

esp_err_t slcan_process_command(const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    
    char cmd = data[0];
    
    switch (cmd) {
        case 'S': // Set bitrate with standard codes (S0-S8)
            if (len >= 2) {
                int rate_code = data[1] - '0';
                if (rate_code >= 0 && rate_code < sizeof(slcan_bitrates)/sizeof(slcan_bitrates[0])) {
                    slcan_state.bitrate = slcan_bitrates[rate_code];
                    ESP_LOGI(TAG, "Bitrate set to %lu bps (code S%d)", slcan_state.bitrate, rate_code);
                    slcan_send_response("\r");
                } else {
                    slcan_send_response("\x07"); // Bell (error)
                }
            }
            break;
            
        case 's': // Set bitrate with BTR registers (not implemented, use S command)
            ESP_LOGW(TAG, "BTR register setting not supported, use S command");
            slcan_send_response("\x07");
            break;
            
        case 'O': // Open channel
            slcan_state.is_open = true;
            ESP_LOGI(TAG, "Channel opened");
            slcan_send_response("\r");
            break;
            
        case 'C': // Close channel
            slcan_state.is_open = false;
            ESP_LOGI(TAG, "Channel closed");
            slcan_send_response("\r");
            break;
            
        case 'V': // Get hardware version
            slcan_send_response("V1234\r"); // Version string
            break;
            
        case 'v': // Get firmware version
            slcan_send_response("v1234\r"); // Version string
            break;
            
        case 'N': // Get serial number
            slcan_send_response("NESP32\r");
            break;
            
        case 'Z': // Set timestamp on/off
            if (len >= 2) {
                slcan_state.timestamp_enabled = (data[1] == '1') ? 1 : 0;
                slcan_send_response("\r");
            }
            break;
            
        case 'F': // Read status flags
            slcan_send_response("F00\r"); // No errors
            break;
            
        case 't': // Transmit standard frame (11-bit ID)
        case 'T': // Transmit extended frame (29-bit ID)
        case 'r': // Transmit standard RTR frame
        case 'R': // Transmit extended RTR frame
            // These will be handled by the main bridge task
            ESP_LOGD(TAG, "TX frame command received (handled elsewhere)");
            slcan_send_response("z\r"); // TX buffer access (success)
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown SLCAN command: 0x%02X", cmd);
            slcan_send_response("\x07"); // Bell (error)
            break;
    }
    
    return ESP_OK;
}

esp_err_t slcan_send_frame(const twai_frame_t *frame)
{
    if (!slcan_state.is_open) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char buffer[64];
    int pos = 0;
    
    // Determine frame type and format ID
    bool is_extended = (frame->header.id > 0x7FF);  // Extended if ID > 11-bit max
    bool is_rtr = frame->header.rtr;
    
    if (is_extended) {
        // Extended frame: T or R
        buffer[pos++] = is_rtr ? 'R' : 'T';
        // 8 hex digits for 29-bit ID
        snprintf(&buffer[pos], 9, "%08lX", frame->header.id & 0x1FFFFFFF);
        pos += 8;
    } else {
        // Standard frame: t or r
        buffer[pos++] = is_rtr ? 'r' : 't';
        // 3 hex digits for 11-bit ID
        snprintf(&buffer[pos], 4, "%03lX", frame->header.id & 0x7FF);
        pos += 3;
    }
    
    // DLC
    uint8_t dlc = frame->header.dlc;
    if (dlc > 8) dlc = 8; // Limit to 8 for classic CAN
    buffer[pos++] = '0' + dlc;
    
    // Data bytes (if not RTR)
    if (!is_rtr) {
        for (int i = 0; i < dlc; i++) {
            byte_to_hex(frame->buffer[i], &buffer[pos]);
            pos += 2;
        }
    }
    
    // Timestamp (if enabled) - 4 hex digits
    if (slcan_state.timestamp_enabled) {
        uint16_t timestamp = 0; // TODO: Add real timestamp
        snprintf(&buffer[pos], 5, "%04X", timestamp);
        pos += 4;
    }
    
    // Carriage return
    buffer[pos++] = '\r';
    buffer[pos] = '\0';
    
    // Send to PC
    slcan_send_response(buffer);
    
    return ESP_OK;
}

uint32_t slcan_get_bitrate(void)
{
    return slcan_state.bitrate;
}

bool slcan_is_open(void)
{
    return slcan_state.is_open;
}

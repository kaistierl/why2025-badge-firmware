/* Copyright 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "esp_loader.h"
#include "why_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIN_HEADER_SIZE     0x8
#define BIN_HEADER_EXT_SIZE 0x18
// Maximum block sized for RAM and Flash writes, respectively.
#define ESP_RAM_BLOCK       0x1800

typedef struct {
    FILE          *fp;
    uint32_t       size;
    uint32_t       addr;
    uint8_t const *md5;
} partition_attr_t;

typedef struct {
    partition_attr_t boot;
    partition_attr_t part;
    partition_attr_t app;
} why2025_binaries_t;

typedef struct {
    partition_attr_t ram_app;
} why2025_ram_app_binary_t;

/**
 * @brief esptool portable bin header format
 */
typedef struct why2025_bin_header {
    uint8_t  magic;
    uint8_t  segments;
    uint8_t  flash_mode;
    uint8_t  flash_size_freq;
    uint32_t entrypoint;
} why2025_bin_header_t;

/**
 * @brief esptool portable bin segment format
 */
typedef struct why2025_bin_segment {
    uint32_t addr;
    uint32_t size;
    uint8_t *data;
} why2025_bin_segment_t;


bool get_why2025_binaries(why2025_binaries_t *binaries);
bool get_why2025_network_adapter_binary(why2025_binaries_t *bins);
void free_why2025_binaries(why2025_binaries_t *bins);

esp_loader_error_t connect_to_target(uint32_t higher_transmission_rate);
esp_loader_error_t connect_to_target_with_stub(uint32_t current_transmission_rate, uint32_t higher_transmission_rate);
esp_loader_error_t flash_binary(FILE *bin, size_t size, size_t address);
esp_loader_error_t load_ram_binary(uint8_t const *bin);


#ifdef __cplusplus
}
#endif

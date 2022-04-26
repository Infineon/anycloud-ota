/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 /*******************************************************************************/

#ifndef FLASH_MAP_BACKEND_H
#define FLASH_MAP_BACKEND_H

#include "cy_flash.h"
#define FLASH_DEVICE_INDEX_MASK                 (0x7Fu)
#define FLASH_DEVICE_GET_EXT_INDEX(n)           ((n) & FLASH_DEVICE_INDEX_MASK)
#define FLASH_DEVICE_UNDEFINED                  (0x00u)
#define FLASH_DEVICE_EXTERNAL_FLAG              (0x80u)
#define FLASH_DEVICE_INTERNAL_FLASH             (0x7Fu)
#define FLASH_DEVICE_EXTERNAL_FLASH(index)      (FLASH_DEVICE_EXTERNAL_FLAG | index)

#ifndef CY_BOOT_EXTERNAL_DEVICE_INDEX
/* assume first(one) SMIF device is used */
#define CY_BOOT_EXTERNAL_DEVICE_INDEX            (0)
#endif

/**
 *
 * Provides abstraction of flash regions for type of use.
 * I.e. dude where's my image?
 *
 * System will contain a map which contains flash areas. Every
 * region will contain flash identifier, offset within flash and length.
 *
 * 1. This system map could be in a file within filesystem (Initializer
 * must know/figure out where the filesystem is at).
 * 2. Map could be at fixed location for project (compiled to code)
 * 3. Map could be at specific place in flash (put in place at mfg time).
 *
 * Note that the map you use must be valid for BSP it's for,
 * match the linker scripts when platform executes from flash,
 * and match the target offset specified in download script.
 */
#include <inttypes.h>

/**
 * @brief Structure describing an area on a flash device.
 *
 * Multiple flash devices may be available in the system, each of
 * which may have its own areas. For this reason, flash areas track
 * which flash device they are part of.
 */
struct flash_area {
    /**
     * This flash area's ID; unique in the system.
     */
    uint8_t fa_id;

    /**
     * ID of the flash device this area is a part of.
     */
    uint8_t fa_device_id;

    uint16_t pad16;

    /**
     * This area's offset, relative to the beginning of its flash
     * device's storage.
     */
    uint32_t fa_off;

    /**
     * This area's size, in bytes.
     */
    uint32_t fa_size;
};

/**
 * @brief Structure describing a sector within a flash area.
 *
 * Each sector has an offset relative to the start of its flash area
 * (NOT relative to the start of its flash device), and a size. A
 * flash area may contain sectors with different sizes.
 */
struct flash_sector {
    /**
     * Offset of this sector, from the start of its flash area (not device).
     */
    uint32_t fs_off;

    /**
     * Size of this sector, in bytes.
     */
    uint32_t fs_size;
};

struct flash_map_entry {
    uint32_t magic;
    struct flash_area area;
    unsigned int ref_count;
};

/*
 * Retrieve a memory-mapped flash device's base address.
 * On success, the address will be stored in the value pointed to by
 * ret.
 * Returns 0 on success, or an error code on failure.
 */
int flash_device_base(uint8_t fd_id, uintptr_t *ret);

/*< Opens the area for use. id is one of the `fa_id`s */
int flash_area_open(uint8_t id, const struct flash_area **fa);
void flash_area_close(const struct flash_area *);
/*< Reads `len` bytes of flash memory at `off` to the buffer at `dst` */
int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
                     uint32_t len);
/*< Writes `len` bytes of flash memory at `off` from the buffer at `src` */
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len);
/*< Erases `len` bytes of flash memory at `off` */
int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len);
/*< Returns this `flash_area`s alignment */
size_t flash_area_align(const struct flash_area *fa);
/*< Initializes an array of flash_area elements for the slot's sectors */
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa);
/*< Returns the `fa_id` for slot, where slot is 0 (primary) or 1 (secondary) */
int flash_area_id_from_image_slot(int slot);
/*< Returns the slot, for the `fa_id` supplied */
int flash_area_id_to_image_slot(int area_id);

int flash_area_id_from_multi_image_slot(int image_index, int slot);
int flash_area_id_to_multi_image_slot(int image_index, int area_id);
#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
int flash_area_get_sectors(int idx, uint32_t *cnt, struct flash_sector *ret);
#endif
/*
 * Returns the value expected to be read when accesing any erased
 * flash byte.
 */
uint8_t flash_area_erased_val(const struct flash_area *fa);

// *****************************************************************************
#ifdef MCUBOOT_ENC_IMAGES_XIP

#include "bootutil/image.h"
#include "bootutil/enc_key.h"

int bootutil_img_encrypt(struct enc_key_data *enc_state, int image_index,
        struct image_header *hdr, const struct flash_area *fap, uint32_t off, uint32_t sz,
        uint32_t blk_off, uint8_t *buf);
#endif /* MCUBOOT_ENC_IMAGES_XIP */


#endif /* FLASH_MAP_BACKEND_H */

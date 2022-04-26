/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Modifications are Copyright (c) 2019 Arm Limited.
 */

#ifndef H_BOOTUTIL_PRIV_
#define H_BOOTUTIL_PRIV_

#include "sysflash/sysflash.h"

#include "flash_map_backend/flash_map_backend.h"

#include "bootutil/bootutil.h"
#include "mcuboot_config/mcuboot_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT assert

struct flash_area;

#define BOOT_EFLASH      1
#define BOOT_EFILE       2
#define BOOT_EBADIMAGE   3
#define BOOT_EBADVECT    4
#define BOOT_EBADSTATUS  5
#define BOOT_ENOMEM      6
#define BOOT_EBADARGS    7
#define BOOT_EBADVERSION 8

#define BOOT_TMPBUF_SZ  256

/** Number of image slots in flash; currently limited to two. */
#define BOOT_NUM_SLOTS                  2

/*
 * Maintain state of copy progress.
 */
struct boot_status {
    uint32_t idx;         /* Which area we're operating on */
    uint8_t state;        /* Which part of the swapping process are we at */
    uint8_t use_scratch;  /* Are status bytes ever written to scratch? */
    uint8_t swap_type;    /* The type of swap in effect */
    uint32_t swap_size;   /* Total size of swapped image */
};

#define BOOT_MAGIC_GOOD     1
#define BOOT_MAGIC_BAD      2
#define BOOT_MAGIC_UNSET    3
#define BOOT_MAGIC_ANY      4  /* NOTE: control only, not dependent on sector */
#define BOOT_MAGIC_NOTGOOD  5  /* NOTE: control only, not dependent on sector */

/*
 * NOTE: leave BOOT_FLAG_SET equal to one, this is written to flash!
 */
#define BOOT_FLAG_SET       1
#define BOOT_FLAG_BAD       2
#define BOOT_FLAG_UNSET     3
#define BOOT_FLAG_ANY       4  /* NOTE: control only, not dependent on sector */

#define BOOT_STATUS_IDX_0   1

#define BOOT_STATUS_STATE_0 1
#define BOOT_STATUS_STATE_1 2
#define BOOT_STATUS_STATE_2 3

/**
 * End-of-image slot structure.
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  ~                                                               ~
 *  ~    Swap status (BOOT_MAX_IMG_SECTORS * min-write-size * 3)    ~
 *  ~                                                               ~
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                 Encryption key 0 (16 octets) [*]              |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                 Encryption key 1 (16 octets) [*]              |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                      Swap size (4 octets)                     |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Swap info   |           0xff padding (7 octets)             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Copy done   |           0xff padding (7 octets)             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Image OK    |           0xff padding (7 octets)             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                       MAGIC (16 octets)                       |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * [*]: Only present if the encryption option is enabled
 *      (`MCUBOOT_ENC_IMAGES`).
 */

extern uint32_t boot_img_magic[4];

struct boot_swap_state {
    uint8_t magic;      /* One of the BOOT_MAGIC_[...] values. */
    uint8_t swap_type;  /* One of the BOOT_SWAP_TYPE_[...] values. */
    uint8_t copy_done;  /* One of the BOOT_FLAG_[...] values. */
    uint8_t image_ok;   /* One of the BOOT_FLAG_[...] values. */
    uint8_t image_num;  /* Boot status belongs to this image */
};

#ifdef MCUBOOT_IMAGE_NUMBER
#define BOOT_IMAGE_NUMBER          MCUBOOT_IMAGE_NUMBER
#else
#define BOOT_IMAGE_NUMBER          1
#endif

_Static_assert(BOOT_IMAGE_NUMBER > 0, "Invalid value for BOOT_IMAGE_NUMBER");

#define BOOT_MAX_IMG_SECTORS       MCUBOOT_MAX_IMG_SECTORS

/*
 * Extract the swap type and image number from image trailers's swap_info
 * filed.
 */
#define BOOT_GET_SWAP_TYPE(swap_info)    ((swap_info) & 0x0F)
#define BOOT_GET_IMAGE_NUM(swap_info)    ((swap_info) >> 4)

/* Construct the swap_info field from swap type and image number */
#define BOOT_SET_SWAP_INFO(swap_info, image, type)  {                          \
                                                    assert((image) < 0xF);     \
                                                    assert((type)  < 0xF);     \
                                                    (swap_info) = (image) << 4 \
                                                                | (type);      \
                                                    }

/*
 * The current flashmap API does not check the amount of space allocated when
 * loading sector data from the flash device, allowing for smaller counts here
 * would most surely incur in overruns.
 *
 * TODO: make flashmap API receive the current sector array size.
 */
#if BOOT_MAX_IMG_SECTORS < 32
#error "Too few sectors, please increase BOOT_MAX_IMG_SECTORS to at least 32"
#endif

/** Maximum number of image sectors supported by the bootloader. */
#define BOOT_STATUS_STATE_COUNT         3
#define BOOT_STATUS_MAX_ENTRIES         BOOT_MAX_IMG_SECTORS

#define BOOT_PRIMARY_SLOT               0
#define BOOT_SECONDARY_SLOT             1

#define BOOT_STATUS_SOURCE_NONE         0
#define BOOT_STATUS_SOURCE_SCRATCH      1
#define BOOT_STATUS_SOURCE_PRIMARY_SLOT 2

#define BOOT_MAGIC_SZ (sizeof boot_img_magic)

int boot_magic_compatible_check(uint8_t tbl_val, uint8_t val);
uint32_t boot_trailer_sz(uint32_t min_write_sz);
uint32_t boot_swap_info_off(const struct flash_area *fap);
int boot_read_swap_state(const struct flash_area *fap,
                         struct boot_swap_state *state);
int boot_read_swap_state_by_id(int flash_area_id,
                               struct boot_swap_state *state);
int boot_write_magic(const struct flash_area *fap);
int boot_write_image_ok(const struct flash_area *fap);
int boot_write_swap_info(const struct flash_area *fap, uint8_t swap_type,
                         uint8_t image_num);

#ifdef __cplusplus
}
#endif

#endif

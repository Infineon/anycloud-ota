/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
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
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 /*******************************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mcuboot_config/mcuboot_config.h"
#include "flash_map_backend/flash_map_backend.h"
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"

#include "cy_pdl.h"
#include "cy_flash.h"

#include "ota_serial_flash.h"

#ifdef MCUBOOT_SWAP_USING_STATUS
#include "swap_status.h"
#endif

/*
 * For now, we only support one flash device.
 *
 * Pick a random device ID for it that's unlikely to collide with
 * anything "real".
 */
#define FLASH_DEVICE_ID                     111
#define FLASH_MAP_ENTRY_MAGIC               (0xD00DBEEFu)

#define FLASH_AREA_IMAGE_SECTOR_SIZE        FLASH_AREA_IMAGE_SCRATCH_SIZE

#ifndef CY_BOOTLOADER_START_OFFSET
#define CY_BOOTLOADER_START_OFFSET          (0x0u)
#endif

#ifndef CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE
/* This is the value of external flash bytes after an erase */
#define CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE  (0xFFu)
#endif

#ifdef CY_FLASH_MAP_EXT_DESC
/* Use external Flash Map Descriptors */
extern struct flash_area *boot_area_descs[];
#else /* CY_FLASH_MAP_EXT_DESC */
/* Declare the flash partitions structures */
struct flash_area bootloader;
struct flash_area primary_1;
struct flash_area secondary_1;

#if (MCUBOOT_IMAGE_NUMBER == 2)
struct flash_area primary_2;
struct flash_area secondary_2;
#endif
#ifdef MCUBOOT_SWAP_USING_SCRATCH
struct flash_area scratch;
#endif
#ifdef MCUBOOT_SWAP_USING_STATUS
struct flash_area status;
#endif

struct flash_area bootloader =
{
    .fa_id = FLASH_AREA_BOOTLOADER,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = CY_BOOTLOADER_START_OFFSET,
    .fa_size = CY_BOOT_BOOTLOADER_SIZE
};

struct flash_area primary_1 =
{
    .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = CY_BOOTLOADER_START_OFFSET + CY_BOOT_BOOTLOADER_SIZE,
    .fa_size = CY_BOOT_PRIMARY_1_SIZE
};

struct flash_area secondary_1 =
{
    .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = CY_BOOT_SECONDARY_1_EXT_MEM_OFFSET,
    .fa_size = CY_BOOT_SECONDARY_1_SIZE
};

#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
struct flash_area primary_2 =
{
    .fa_id = FLASH_AREA_IMAGE_PRIMARY(1),
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = CY_BOOTLOADER_START_OFFSET +
                CY_BOOT_BOOTLOADER_SIZE +
                CY_BOOT_PRIMARY_1_SIZE,
    .fa_size = CY_BOOT_PRIMARY_2_SIZE
};

struct flash_area secondary_2 =
{
    .fa_id = FLASH_AREA_IMAGE_SECONDARY(1),
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = CY_BOOT_SECONDARY_2_EXT_MEM_OFFSET,
    .fa_size = CY_BOOT_SECONDARY_2_SIZE
};
#endif /* MCUBOOT_IMAGE_NUMBER == 2 */

#ifdef MCUBOOT_SWAP_USING_SCRATCH
struct flash_area scratch =
{
    .fa_id = FLASH_AREA_IMAGE_SCRATCH,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = CY_BOOTLOADER_START_OFFSET + CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET,
    .fa_size = CY_BOOT_SCRATCH_SIZE
};
#endif /* MCUBOOT_SWAP_USING_SCRATCH */

#ifdef MCUBOOT_SWAP_USING_STATUS
#ifndef SWAP_STATUS_PARTITION_OFF /* can be defined externally */
#if (MCUBOOT_IMAGE_NUMBER == 1) /* if single-image, internal flash */
#define SWAP_STATUS_PARTITION_OFF   (CY_BOOTLOADER_START_OFFSET + \
                                     CY_BOOT_BOOTLOADER_SIZE + \
                                     CY_BOOT_PRIMARY_1_SIZE + \
                                     CY_BOOT_SECONDARY_1_SIZE)
#elif (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image, internal flash */
#define SWAP_STATUS_PARTITION_OFF   (CY_BOOTLOADER_START_OFFSET + \
                                     CY_BOOT_BOOTLOADER_SIZE + \
                                     CY_BOOT_PRIMARY_1_SIZE + \
                                     CY_BOOT_SECONDARY_1_SIZE + \
                                     CY_BOOT_PRIMARY_2_SIZE + \
                                     CY_BOOT_SECONDARY_2_SIZE)
#endif /* MCUBOOT_IMAGE_NUMBER */
#endif /* SWAP_STATUS_PARTITION_OFF */

struct flash_area status =
{
    .fa_id = FLASH_AREA_IMAGE_SWAP_STATUS,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off = SWAP_STATUS_PARTITION_OFF,
#if (MCUBOOT_IMAGE_NUMBER == 1) /* if single-image */
    .fa_size = (BOOT_SWAP_STATUS_SZ_PRIM + BOOT_SWAP_STATUS_SZ_SEC + BOOT_SWAP_STATUS_SZ_SCRATCH)
#elif (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
    .fa_size = (BOOT_SWAP_STATUS_SZ_PRIM + BOOT_SWAP_STATUS_SZ_SEC) * BOOT_IMAGE_NUMBER + BOOT_SWAP_STATUS_SZ_SCRATCH;
#endif /* MCUBOOT_IMAGE_NUMBER */

};
#endif /* MCUBOOT_SWAP_USING_STATUS */

struct flash_area *boot_area_descs[] =
{
    &bootloader,
    &primary_1,
    &secondary_1,
#if (MCUBOOT_IMAGE_NUMBER == 2)
    &primary_2,
    &secondary_2,
#endif /* MCUBOOT_IMAGE_NUMBER */
#ifdef MCUBOOT_SWAP_USING_SCRATCH
    &scratch,
#endif
#ifdef MCUBOOT_SWAP_USING_STATUS
    &status,
#endif
    NULL
};
#endif  /* CY_FLASH_MAP_EXT_DESC */

#if 0
void print_area_descs(void)
{
    int i;

    printf("\r\nMCUBoot flash areas:\r\n");
    for (i = 0; boot_area_descs[i] != NULL; i++)
    {
        printf("      fa_id:  %d\r\n", boot_area_descs[i]->fa_id);
        printf("  device_id:  %d\r\n", boot_area_descs[i]->fa_device_id);
        printf("     fa_off:  0x%08lx\r\n", boot_area_descs[i]->fa_off);
        printf("    fa_size:  0x%08lx\r\n", boot_area_descs[i]->fa_size);
        printf("  erase_val:  0x%02x\r\n", flash_area_erased_val(boot_area_descs[i]));
        printf("\r\n");
    }
}
#endif

/*
* Returns device flash start based on supported fa_id
*/
int flash_device_base(uint8_t fa_device_id, uintptr_t *ret)
{
   int rc = -1;

    if (NULL != ret) {

        if (fa_device_id == (uint8_t)FLASH_DEVICE_INTERNAL_FLASH) {
            *ret = CY_FLASH_BASE;
            rc = 0;
        }
        else if ((fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
            *ret = CY_FLASH_BASE;
            rc = 0;
        }
        else {
            BOOT_LOG_ERR("invalid flash ID %u; expected %u or %u",
                         fa_device_id, FLASH_DEVICE_INTERNAL_FLASH, FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX));
        }
    }

    return rc;
}

/*
* Opens the area for use. id is one of the `fa_id`s
*/
int flash_area_open(uint8_t id, const struct flash_area **fa)
{
    int ret = -1;
    uint32_t i = 0u;

    if (NULL != fa) {
        while (NULL != boot_area_descs[i]) {
            if (id == boot_area_descs[i]->fa_id) {
                *fa = boot_area_descs[i];
                ret = 0;
                break;
            }
            i++;
        }
    }

    return ret;
}

/*
* Clear pointer to flash area fa
*/
void flash_area_close(const struct flash_area *fa)
{
    (void)fa; /* Nothing to do there */
}

/*
* Reads `len` bytes of flash memory at `off` to the buffer at `dst`
*/
int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
                     uint32_t len)
{
    int rc = -1;

    if ( (NULL != fa) && (NULL != dst) ) {

        if (off > fa->fa_size ||
            len > fa->fa_size ||
            off + len > fa->fa_size) {

            return BOOT_EBADARGS;
        }

        if (ota_smif_read(fa->fa_off + off, dst, len) == CY_RSLT_SUCCESS)
        {
            rc = 0;
        }
    }

    return rc;
}

/*
* Writes `len` bytes of flash memory at `off` from the buffer at `src`
 */
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len)
{
    int rc = -1;

    if ( (NULL != fa) && (NULL != src) )
    {
        if (off > fa->fa_size ||
            len > fa->fa_size ||
            off + len > fa->fa_size)
        {
            return BOOT_EBADARGS;
        }

        if (ota_smif_write(fa->fa_off + off, src, len) == CY_RSLT_SUCCESS)
        {
            rc = 0;
        }
    }

    return (int) rc;
}

/*< Erases `len` bytes of flash memory at `off` */
int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    int rc = -1;

    if (NULL != fa)
    {
        if (off > fa->fa_size ||
            len > fa->fa_size ||
            off + len > fa->fa_size)
        {
            return BOOT_EBADARGS;
        }

        if (ota_smif_erase(fa->fa_off + off, len) == CY_RSLT_SUCCESS)
        {
            rc = 0;
        }
    }

    return rc;
}

/*< Returns this `flash_area`s alignment */
size_t flash_area_align(const struct flash_area *fa)
{
    size_t rc = 0u; /* error code (alignment cannot be zero) */

    if (NULL != fa)
    {
        if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG)
        {
            rc = ota_smif_get_erase_size(fa->fa_off);
        }
    }

    return rc;
}

#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
/*< Initializes an array of flash_area elements for the slot's sectors */
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa)
{
    int rc = -1;

    if (fa != NULL && cnt != NULL) {
        if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
            (void)idx;
            (void)cnt;
            rc = 0;
        }
    }

    return rc;
}
#endif

/*
 * This depends on the mappings defined in sysflash.h.
 * MCUBoot uses continuous numbering for the primary slot, the secondary slot,
 * and the scratch while zephyr might number it differently.
 */
int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    int rc;
    switch (slot) {
        case 0:
            rc = (int)FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index);
            break;
        case 1:
            rc = (int)FLASH_AREA_IMAGE_SECONDARY((uint32_t)image_index);
            break;
        case 2:
            rc = (int)FLASH_AREA_IMAGE_SCRATCH;
            break;
        default:
            rc = -1; /* flash_area_open will fail on that */
            break;
    }
    return rc;
}

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (area_id == (int) FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index)) {
        return 0;
    }
    if (area_id == (int) FLASH_AREA_IMAGE_SECONDARY((uint32_t)image_index)) {
        return 1;
    }

    return -1;
}

int flash_area_id_to_image_slot(int area_id)
{
    return flash_area_id_to_multi_image_slot(0, area_id);
}

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    uint8_t rc = 0;

    if (NULL != fa) {
        if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
            rc = (uint8_t) CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE;
        }
    }

    return rc;
}

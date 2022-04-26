/*
 * Copyright (c) 2021 Infineon Technologies AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_512K MCUBOOT_LOG_LEVEL=MCUBOOT_LOG_LEVEL_DEBUG USE_EXTERNAL_FLASH=1 MCUBOOT_IMAGE_NUMBER=2 USE_CUSTOM_MEMORY_MAP=1 PLATFORM_EXTERNAL_FLASH_SECONDARY_1_OFFSET=0x128000u PLATFORM_EXTERNAL_FLASH_SECONDARY_2_OFFSET=0x1FE200u PLATFORM_STATUS_PARTITION_OFFSET=0x070200u PLATFORM_EXTERNAL_FLASH_SCRATCH_OFFSET=0x440000u PLATFORM_SCRATCH_SIZE=0x80000u PLATFORM_IMAGE_1_SLOT_SIZE=0x00058200 PLATFORM_IMAGE_2_SLOT_SIZE=0x00082000 */

/*
 *             FLASH MAP
 *
 * Address    (Slot)Size Area
 * ---------- ---------- -----------
 * 0x10000000 0x00018000 Bootloader
 * 0x10018000 0x00058200 Primary_1
 * 0x10070200 0x00008C00 Status
 * 0x1803E200 0x00082000 Primary_2
 * 0x18128000 0x00058200 Secondary_1
 * 0x181FE200 0x00082000 Secondary_2
 * 0x18440000 0x00080000 Scratch
 *
 */

#ifdef CY_FLASH_MAP_EXT_DESC
#ifdef CY_RUN_CODE_FROM_XIP

#include "mcuboot_config/mcuboot_config.h"
#include "flash_map_backend/flash_map_backend.h"
#include <sysflash/sysflash.h>


#define CY_BOOTLOADER_START_OFFSET              (0x000000u)
//#define CY_BOOT_PRIMARY_1_OFFSET                (0x003fe00u)
#define SWAP_STATUS_PARTITION_SIZE              (0x008C00u)

#ifndef SWAP_STATUS_PARTITION_OFF
#define SWAP_STATUS_PARTITION_OFF   CY_BOOT_STATUS_START
#endif

//#ifndef CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET
//#define CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET CY_BOOT_SECONDARY_1_START
//#endif
//
//#ifndef CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET
//#define CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET CY_BOOT_SECONDARY_2_START
//#endif

#ifndef CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET   (0x00500000) //CY_BOOT_SCRATCH_START
#endif

static struct flash_area bootloader =
{
    .fa_id        = FLASH_AREA_BOOTLOADER,
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off       = CY_BOOTLOADER_START_OFFSET,
    .fa_size      = CY_BOOT_BOOTLOADER_SIZE
};

static struct flash_area primary_1 =
{
    .fa_id        = FLASH_AREA_IMAGE_0,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off       = CY_BOOT_PRIMARY_1_START,    /* 0x1803fe00 */
    .fa_size      = CY_BOOT_PRIMARY_1_SIZE      /* 0x00100400  */
};

#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
static struct flash_area primary_2 =
{
    .fa_id        = FLASH_AREA_IMAGE_2,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off       = CY_BOOT_PRIMARY_2_START,    /*  */
    .fa_size      = CY_BOOT_PRIMARY_1_SIZE      /*   */
};
#endif

static struct flash_area secondary_1 =
{
    .fa_id        = FLASH_AREA_IMAGE_1,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off       = CY_BOOT_SECONDARY_1_START,  /* 0x181bfe00 */
    .fa_size      = CY_BOOT_SECONDARY_1_SIZE    /*  0x00100400 */
};

#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
static struct flash_area secondary_2 =
{
    .fa_id        = FLASH_AREA_IMAGE_3,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off       = CY_BOOT_SECONDARY_2_START,  /*  */
    .fa_size      = CY_BOOT_PRIMARY_1_SIZE      /*   */
};
#endif

static struct flash_area status =
{
    .fa_id        = FLASH_AREA_IMAGE_SWAP_STATUS,
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off       = SWAP_STATUS_PARTITION_OFF,
    .fa_size      = SWAP_STATUS_PARTITION_SIZE
};

static struct flash_area scratch =
{
    .fa_id        = FLASH_AREA_IMAGE_SCRATCH,
    .fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX),
    .fa_off       = CY_SMIF_BASE_MEM_OFFSET + CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET,
    .fa_size      = CY_BOOT_SCRATCH_SIZE
};

struct flash_area *boot_area_descs[] =
{
    &bootloader,
    &primary_1,
    &secondary_1,
#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
    &primary_2,
    &secondary_2,
#endif
    &scratch,
    &status,
    NULL
};

#endif /*CY_RUN_CODE_FROM_XIP */
#endif /* CY_FLASH_MAP_EXT_DESC */

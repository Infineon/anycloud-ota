/* Copyright 2019 Cypress Semiconductor Corporation
 *
 * Copyright (c) 2018 Open Source Foundries Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MCUBOOT_CONFIG_H
#define MCUBOOT_CONFIG_H

/*
 * Template configuration file for MCUboot.
 *
 * When porting MCUboot to a new target, copy it somewhere that your
 * include path can find it as mcuboot_config/mcuboot_config.h, and
 * make adjustments to suit your platform.
 *
 * For examples, see:
 *
 * boot/zephyr/include/mcuboot_config/mcuboot_config.h
 * boot/mynewt/mcuboot_config/include/mcuboot_config/mcuboot_config.h
 */
/* Default maximum number of flash sectors per image slot; change
 * as desirable. */
#ifndef MCUBOOT_MAX_IMG_SECTORS
#define MCUBOOT_MAX_IMG_SECTORS 14848 /* the sector is temporary 128 bytes now */
#endif

/*
 * Flash abstraction
 */

/* Default number of separately updateable images; change in case of
 * multiple images. */
#ifndef MCUBOOT_IMAGE_NUMBER
#define MCUBOOT_IMAGE_NUMBER 1
#endif

/*
 * Logging
 */

/*
 * If logging is enabled the following functions must be defined by the
 * platform:
 *
 *    MCUBOOT_LOG_ERR(...)
 *    MCUBOOT_LOG_WRN(...)
 *    MCUBOOT_LOG_INF(...)
 *    MCUBOOT_LOG_DBG(...)
 *
 * The following global logging level configuration macros must also be
 * defined, each with a unique value. Those will be used to define a global
 * configuration and will allow any source files to override the global
 * configuration:
 *
 *    MCUBOOT_LOG_LEVEL_OFF
 *    MCUBOOT_LOG_LEVEL_ERROR
 *    MCUBOOT_LOG_LEVEL_WARNING
 *    MCUBOOT_LOG_LEVEL_INFO
 *    MCUBOOT_LOG_LEVEL_DEBUG
 *
 * The global logging level must be defined, with one of the previously defined
 * logging levels:
 *
 *    #define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_(OFF|ERROR|WARNING|INFO|DEBUG)
 *
 * MCUBOOT_LOG_LEVEL sets the minimum level that will be logged. The function
 * priority is:
 *
 *    MCUBOOT_LOG_ERR > MCUBOOT_LOG_WRN > MCUBOOT_LOG_INF > MCUBOOT_LOG_DBG
 *
 * NOTE: Each source file is still able to request its own logging level by
 * defining BOOT_LOG_LEVEL before #including `bootutil_log.h`
 */
#ifndef MCUBOOT_HAVE_LOGGING
//#define MCUBOOT_HAVE_LOGGING 1
#endif

#endif /* MCUBOOT_CONFIG_H */

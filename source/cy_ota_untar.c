/*
 * Copyright 2020 Cypress Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
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

/*
 *  Cypress OTA Agent network abstraction un-tar archive
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <FreeRTOS.h>

/* lwIP header files */
#include <lwip/tcpip.h>
#include <lwip/api.h>

#include "cy_ota_api.h"
#include "cy_ota_internal.h"
#include "ip4_addr.h"

#include "cyabs_rtos.h"
#include "cy_log.h"

#include "untar.h"
#include "sysflash.h"
#include "flash_map_backend.h"
#include "bootutil.h"

/* define CY_TEST_APP_VERSION_IN_TAR to test the application version in the
 * TAR archive at start of OTA image download.
 *
 * NOTE: This requires that the user set the version number in the Makefile
 *          APP_VERSION_MAJOR
 *          APP_VERSION_MINOR
 *          APP_VERSION_BUILD
 */

/***********************************************************************
 *
 * OTA wrappers and callbacks for untar.c
 *
 **********************************************************************/

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/

/**
 * @brief Tarball support file types recognized in components.json file
 *
 * The file type in the tarball
 */
#define CY_FILE_TYPE_SPE        "SPE"       /**< Secure Programming Environment (TFM) code type               */
#define CY_FILE_TYPE_NSPE       "NSPE"      /**< Non-Secure Programming Environment (application) code type   */

/***********************************************************************
 *
 * Macros
 *
 **********************************************************************/

/***********************************************************************
 *
 * Structures
 *
 **********************************************************************/

/***********************************************************************
 *
 * Data & Variables
 *
 **********************************************************************/

/**
 * @brief Context structure for parsing the tar file
 */
cy_untar_context_t  ota_untar_context;

/**
 * @brief Flag to denote this is a tar file
 *
 * We use this flag on subsequent chunks of file to know how to handle the data
 */
int ota_is_tar_archive;

/**
 * @brief FLASH write block
 * This is used if a block is < Block size to satisfy requirements
 * of flash_area_write()
 */
static uint8_t block_buffer[CY_FLASH_SIZEOF_ROW];

/***********************************************************************
 *
 * Forward declarations
 *
 **********************************************************************/

/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/

/**
 * @brief Write different sized chunks in 512 byte blocks
 *
 * @param fap           currently open Flash Area
 * @param offset        offset into the Flash Area to write data
 * @param source        data to use
 * @param size          amount of data in source to write
 *
 * @return  CY_UNTAR_SUCCESS
 *          CY_UNTAR_ERROR
 */
static cy_untar_result_t write_data_to_flash( const struct flash_area *fap,
        uint32_t offset,
        uint8_t * const source,
        uint32_t size)
{
    uint32_t bytes_to_write;
    uint32_t curr_offset;
    uint8_t *curr_src;

    bytes_to_write = size;
    curr_offset = offset;
    curr_src = source;

    while (bytes_to_write > 0)
    {
        uint32_t chunk_size = bytes_to_write;
        if (chunk_size > CY_FLASH_SIZEOF_ROW)
        {
            chunk_size = CY_FLASH_SIZEOF_ROW;
        }

        /* this should only happen on last part of last 4k chunk */
        if (chunk_size % CY_FLASH_SIZEOF_ROW)
        {
            /* we will read a 512 byte block, write out data into the block, then write the whole block */
            if (flash_area_read(fap, curr_offset, block_buffer, sizeof(block_buffer)) != 0)
            {
                cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_read() failed\n", __func__);
                return CY_UNTAR_ERROR;
            }
            memcpy (block_buffer, curr_src, chunk_size);

            if (flash_area_write(fap, curr_offset, block_buffer, sizeof(block_buffer)) != 0)
            {
                cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%d:%s() flash_area_write() failed\n", __LINE__, __func__);
                return CY_UNTAR_ERROR;
            }

        }
        else
        {
            if (flash_area_write(fap, curr_offset, curr_src, chunk_size) != 0)
            {
                cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%d:%s() flash_area_write() failed\n", __LINE__, __func__);
                return CY_UNTAR_ERROR;
            }
        }

        curr_offset += chunk_size;
        curr_src += chunk_size;
        bytes_to_write -= chunk_size;
    }

    return CY_UNTAR_SUCCESS;
}

/**
 * @brief callback to handle tar data
 *
 * @param ctxt          untar context
 * @param file_index    index into ctxt->files for the data
 * @param buffer        data to use
 * @param file_offset   offset into the file to store data
 * @param chunk_size    amount of data in buffer to use
 * @param cb_arg        argument passed into initialization
 *
 * return   CY_UNTAR_SUCCESS
 *          CY_UNTAR_ERROR
 */
cy_untar_result_t ota_untar_write_callback(cy_untar_context_ptr ctxt,
                                   uint16_t file_index,
                                   uint8_t *buffer,
                                   uint32_t file_offset,
                                   uint32_t chunk_size,
                                   void *cb_arg)
{
    int image = 0;
    const struct flash_area *fap;

    if ( (ctxt == NULL) || (buffer == NULL) )
    {
        return CY_UNTAR_ERROR;
    }

    if ( strncmp(ctxt->files[file_index].type, CY_FILE_TYPE_SPE, strlen(CY_FILE_TYPE_SPE)) == 0)
    {
        image = 1;  /* The TFM code, cm0 */
    }
    else if ( strncmp(ctxt->files[file_index].type, CY_FILE_TYPE_NSPE, strlen(CY_FILE_TYPE_NSPE)) == 0)
    {
        image = 0;  /* The application code, cm4 */
    }
    else
    {
        /* unknown file type */
//        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%d:%s BAD FILE TYPE : >%s<\n", __LINE__, __func__, ctxt->files[file_index].type);
        return CY_UNTAR_ERROR;
    }

    if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(image), &fap) != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_open(%d) failed\n", __func__, image);
        return CY_UNTAR_ERROR;
    }

    if (write_data_to_flash(fap, file_offset, buffer, chunk_size) != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() write_data_to_flash() failed\n", __func__);
        flash_area_close(fap);
        return CY_UNTAR_ERROR;
    }

    flash_area_close(fap);

    return CY_UNTAR_SUCCESS;
}

/**
 * @brief Initialization routine for handling tarball OTA file
 *
 * @param ctxt[in,out]  pointer to untar context to be initialized
 *
 * return   CY_UNTAR_SUCCESS
 *          CY_UNTAR_ERROR
 */
cy_untar_result_t cy_ota_untar_init_context( cy_untar_context_t* ctx )
{
    if (cy_untar_init( ctx, ota_untar_write_callback, NULL ) == CY_RSLT_SUCCESS)
    {
        ota_is_tar_archive  = 1;
        return CY_UNTAR_SUCCESS;
    }
    return CY_UNTAR_ERROR;
}

/**
 * @brief - set pending for the files contained in the TAR archive we just validated.
 *
 * return   CY_UNTAR_SUCCESS
 */
cy_untar_result_t cy_ota_untar_set_pending(void)
{
    int i;
    int image = 0;
    for (i = 0; i < ota_untar_context.num_files_in_json; i++ )
    {
        if ( strncmp(ota_untar_context.files[i].type, CY_FILE_TYPE_SPE, strlen(CY_FILE_TYPE_SPE)) == 0)
        {
            image = 1;  /* The TFM code, cm0 */
        }
        else if ( strncmp(ota_untar_context.files[i].type, CY_FILE_TYPE_NSPE, strlen(CY_FILE_TYPE_NSPE)) == 0)
        {
            image = 0;  /* The application code, cm4 */
        }
        else
        {
            /* unknown file type */
            cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%d:%s BAD FILE TYPE %d: >%s<\n", __LINE__, __func__, i, ota_untar_context.files[i].type);
            continue;
        }
        boot_set_image_pending(image, 0);
    }

    return CY_UNTAR_SUCCESS;

}

/**
 * @brief Determine if tar or non-tar and call correct write function
 *
 * @param[in]   ctx_ptr     - pointer to OTA agent context @ref cy_ota_context_ptr
 * @param[in]   chunk_info  - pointer to chunk information
 *
 * @return  CY_UNTAR_SUCCESS
 *          CY_UNTAR_ERROR
 */
cy_rslt_t cy_ota_write_incoming_data_block(cy_ota_context_ptr ctx_ptr, cy_ota_storage_write_info_t *chunk_info)
{

    if (chunk_info == NULL)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() chunk_info is NULL ! \n", __func__);
        return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
    }

    /* we need to check some things when we receive the first block */
    if (chunk_info->offset == 0UL)
    {
        /*
         * Check for incoming tarball (as opposed to a single file OTA)
         */
        if (cy_is_tar_header( chunk_info->buffer, chunk_info->size) == CY_UNTAR_SUCCESS)
        {
            if (cy_ota_untar_init_context(&ota_untar_context) != CY_RSLT_SUCCESS)
            {
                cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() cy_ota_untar_init_context() FAILED! \n", __func__);
                return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
            }
        }
    }

    /* treat a tar file differently from a "normal" OTA */
    if (ota_is_tar_archive != 0)
    {
        uint32_t consumed = 0;

        while( consumed < chunk_info->size )
        {
            cy_untar_result_t result;
            result = cy_untar_parse(&ota_untar_context, (chunk_info->offset + consumed), &chunk_info->buffer[consumed],
                                    (chunk_info->size - consumed), &consumed);
            if ( (result == CY_UNTAR_ERROR) || (result == CY_UNTAR_INVALID))
            {
                cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() cy_untar_parse() FAIL consumed: %ld sz:%ld result:%ld)!\n", __func__, consumed, chunk_info->size, result);
                return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
            }
            /* Yield for a bit */
            vTaskDelay(1);
        }

        /* with the tarball we get a version - check if it is > current so we can bail early */
#ifdef CY_TEST_APP_VERSION_IN_TAR
        if (ota_untar_context.version[0] != 0)
        {
            /* example version string "<major>.<minor>.<build>" */
            uint16_t major = 0;
            uint16_t minor = 0;
            uint16_t build = 0;
            char *dot;
            major = atoi(ota_untar_context.version);
            dot = strstr(ota_untar_context.version, ".");
            if (dot != NULL)
            {
                dot++;
                minor = atoi(dot);
                dot = strstr(dot, ".");
                if (dot != NULL)
                {
                    dot++;
                    build = atoi(dot);

                    if ( (major < APP_VERSION_MAJOR) ||
                          ( (major == APP_VERSION_MAJOR) &&
                            (minor < APP_VERSION_MINOR)) ||
                          ( (major == APP_VERSION_MAJOR) &&
                            (minor == APP_VERSION_MINOR) &&
                            (build <= APP_VERSION_BUILD)))
                     {
                         cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() OTA image version %d.%d.%d <= current %d.%d.%d-- bail!\r\n", __func__,
                                 major, minor, build,
                                 APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);

                         return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
                     }
                }
            }
        }
#endif  /* CY_TEST_APP_VERSION_IN_TAR */
    }
    else
    {
        /* non-tarball OTA here, always image 0x00 */
        const struct flash_area *fap;
        if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap) != 0)
        {
            cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_pointer is NULL\n", __func__);
            return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
        }

        if (write_data_to_flash( fap, chunk_info->offset, chunk_info->buffer, chunk_info->size) == -1)
        {
            return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
        }

        flash_area_close(fap);
    }

    return CY_RSLT_SUCCESS;
}

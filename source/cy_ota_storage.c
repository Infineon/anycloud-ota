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
 * Cypress OTA Download Storage abstraction
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "cy_ota_api.h"
#include "cy_ota_internal.h"

#include "bootutil/bootutil.h"
#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "cy_log.h"

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/


/***********************************************************************
 *
 * define tests
 *
 **********************************************************************/

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
 * Variables
 *
 **********************************************************************/

/***********************************************************************
 *
 * functions
 *
 **********************************************************************/

/**
 * @brief erase the second slot to prepare for writing OTA'ed application
 *
 * @param   N/A
 *
 * @return   0 on success
 *          -1 on error
 */
static int eraseSlotTwo(void)
{
    const struct flash_area *fap;

    if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap) != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0) ) failed\n", __func__);
        return -1;
    }
    if (flash_area_erase(fap, 0, fap->fa_size) != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_erase(fap, 0) failed\r\n", __func__);
        return -1;
    }

    flash_area_close(fap);

    return 0;
}

/**
 * @brief Open Storage area for download
 *
 * NOTE: Typically, this erases Secondary Slot
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_OPEN_STORAGE
 */
cy_rslt_t cy_ota_storage_open(cy_ota_context_ptr ctx_ptr)
{
    const struct flash_area *fap;
    cy_ota_context_t *ctx = (cy_ota_context_t *)ctx_ptr;
    CY_OTA_CONTEXT_ASSERT(ctx);
    cy_log_msg(CYLF_OTA, CY_LOG_DEBUG2, "%s()\n", __func__);

    /* clear out the stats */
    ctx->total_image_size    = 0;
    ctx->total_bytes_written = 0;
    ctx->last_offset         = 0;
    ctx->last_size           = 0;
    ctx->storage_loc         = NULL;

    /* erase secondary slot */
    cy_log_msg(CYLF_OTA, CY_LOG_NOTICE, "Erasing Secondary Slot...\n");
    eraseSlotTwo();
    cy_log_msg(CYLF_OTA, CY_LOG_NOTICE, "Erasing Secondary Slot Done.\n");

    if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap) != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0) ) failed\r\n", __func__);
        return CY_RSLT_OTA_ERROR_OPEN_STORAGE;
    }
    ctx->storage_loc = (void *)fap;

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Write to storage area
 *
 * @param[in]   ctx         - pointer to OTA agent context @ref cy_ota_context_t
 * @param[in]   chunk_info  - pointer to chunk information
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_WRITE_STORAGE
 */
cy_rslt_t cy_ota_storage_write(cy_ota_context_ptr ctx_ptr, cy_ota_storage_write_info_t *chunk_info)
{
    int rc;
    const struct flash_area *fap;
    cy_ota_context_t *ctx = (cy_ota_context_t *)ctx_ptr;
    CY_OTA_CONTEXT_ASSERT(ctx);

    cy_log_msg(CYLF_OTA, CY_LOG_DEBUG2, "%s() buf:%p len:%ld off: 0x%lx (%ld)\n", __func__,
                             chunk_info->buffer, chunk_info->size,
                             chunk_info->offset, chunk_info->offset);

    /* write to secondary slot */
    fap = (const struct flash_area *)ctx->storage_loc;
    if (fap == NULL)
    {
        return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
    }

    rc = flash_area_write(fap, chunk_info->offset, chunk_info->buffer, chunk_info->size);
    if (rc != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_ERR, "%s() flash_area_write() failed result:%d\n", __func__, rc);
        return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Close Storage area for download
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_CLOSE_STORAGE
 */
cy_rslt_t cy_ota_storage_close(cy_ota_context_ptr ctx_ptr)
{
    const struct flash_area *fap;
    cy_ota_context_t *ctx = (cy_ota_context_t *)ctx_ptr;
    CY_OTA_CONTEXT_ASSERT(ctx);
    cy_log_msg(CYLF_OTA, CY_LOG_DEBUG2, "%s()\n", __func__);

    /* close secondary slot */
    fap = (const struct flash_area *)ctx->storage_loc;
    if (fap == NULL)
    {
        return CY_RSLT_OTA_ERROR_CLOSE_STORAGE;
    }
    flash_area_close(fap);

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Verify download signature
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_storage_verify(cy_ota_context_ptr ctx_ptr)
{
    int boot_ret;
    int boot_pending;
    cy_ota_context_t *ctx = (cy_ota_context_t *)ctx_ptr;
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* We do not verify the slot - we expect that the Application
     * will do verification in the callback.
     */

    boot_pending = 1;
    if (ctx->agent_params.validate_after_reboot == 1)
    {
        boot_pending = 0;
    }
    /*
     * boot_set_pending(x)
     *
     * 0 - We want the New Application to call cy_ota_storage_validated() on boot.
     * 1 - We believe this to be good and ready to move to Primary Slot
     *     without having to test on re-boot
     * */
    boot_ret = boot_set_pending(boot_pending);
    if (boot_ret != 0)
    {
        cy_log_msg(CYLF_OTA, CY_LOG_INFO, "%s() boot_set_pending() Failed ret:%d\n", __func__, boot_ret);
#if (OTA_USE_EXTERNAL_FLASH != 0)
        /* [stde] Internal FLASH always returns failure due to internal flash alignment issues in boot_write_trailer().
         * We will re-visit after MW-3630. The call sets the proper "magic" value and mcuboot
         * does do the update as expected.
         */
        return CY_RSLT_OTA_ERROR_VERIFY;
#endif
    }

    return CY_RSLT_SUCCESS;
}

/**
 * @brief App has validated the new OTA Image
 *
 * This call needs to be after reboot and MCUBoot has copied the new Application
 *      to the Primary Slot.
 *
 * @param   N/A
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_storage_validated(void)
{
    int16_t boot_result;
    /* Mark Image in Primary Slot as valid
     * For AWS, there is a separate callback after a re-boot "self-test"
     */
    boot_result = boot_set_confirmed();
    if (boot_result != 0)
    {
        return CY_RSLT_OTA_ERROR_GENERAL;
    }

    return CY_RSLT_SUCCESS;
}

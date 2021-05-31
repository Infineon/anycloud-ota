/***************************************************************************//**
* \file cy_smif_psoc6.c
* \version 1.0
*
* \brief
*  This is the source file of external flash driver adoption layer between PSoC6
*  and standard MCUBoot code.
*
******************************************************************************/
/*
 * Copyright 2021, Cypress Semiconductor Corporation (an Infineon company)
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "flash_map_backend/flash_map_backend.h"
#include <sysflash.h>

#include "cy_device_headers.h"
#include "cy_smif_psoc6.h"
#include "cy_flash.h"
#include "cy_syspm.h"

#include "cy_serial_flash_qspi.h"
#include "cybsp_types.h"

/* QSPI bus frequency set to 50 Mhz */
#define QSPI_BUS_FREQUENCY_HZ   (50000000lu)

static cy_stc_smif_mem_cmd_t sfdpcmd =
{
    .command = 0x5A,
    .cmdWidth = CY_SMIF_WIDTH_SINGLE,
    .addrWidth = CY_SMIF_WIDTH_SINGLE,
    .mode = 0xFFFFFFFFU,
    .dummyCycles = 8,
    .dataWidth = CY_SMIF_WIDTH_SINGLE,
};

static cy_stc_smif_mem_cmd_t rdcmd0;
static cy_stc_smif_mem_cmd_t wrencmd0;
static cy_stc_smif_mem_cmd_t wrdiscmd0;
static cy_stc_smif_mem_cmd_t erasecmd0;
static cy_stc_smif_mem_cmd_t chiperasecmd0;
static cy_stc_smif_mem_cmd_t pgmcmd0;
static cy_stc_smif_mem_cmd_t readsts0;
static cy_stc_smif_mem_cmd_t readstsqecmd0;
static cy_stc_smif_mem_cmd_t writestseqcmd0;

static cy_stc_smif_mem_device_cfg_t dev_sfdp_0 =
{
    .numOfAddrBytes = 4,
    .readSfdpCmd = &sfdpcmd,
    .readCmd = &rdcmd0,
    .writeEnCmd = &wrencmd0,
    .writeDisCmd = &wrdiscmd0,
    .programCmd = &pgmcmd0,
    .eraseCmd = &erasecmd0,
    .chipEraseCmd = &chiperasecmd0,
    .readStsRegWipCmd = &readsts0,
    .readStsRegQeCmd = &readstsqecmd0,
    .writeStsRegQeCmd = &writestseqcmd0,
};

static cy_stc_smif_mem_config_t mem_sfdp_0 =
{
    /* The base address the memory slave is mapped to in the PSoC memory map.
    Valid when the memory-mapped mode is enabled. */
    .baseAddress = 0x18000000U,
    /* The size allocated in the PSoC memory map, for the memory slave device.
    The size is allocated from the base address. Valid when the memory mapped mode is enabled. */
    .flags = CY_SMIF_FLAG_DETECT_SFDP,
    .slaveSelect = CY_SMIF_SLAVE_SELECT_0,
    .dataSelect = CY_SMIF_DATA_SEL0,
    .deviceCfg = &dev_sfdp_0
};


int psoc6_qspi_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    result = cy_serial_flash_qspi_init(&mem_sfdp_0, CYBSP_QSPI_D0, CYBSP_QSPI_D1, CYBSP_QSPI_D2, CYBSP_QSPI_D3, NC, NC, NC, NC, CYBSP_QSPI_SCK, CYBSP_QSPI_SS, QSPI_BUS_FREQUENCY_HZ);

    if ( result == CY_RSLT_SUCCESS) {
        return (0);
    } else {
        return (-1);
    }
}

int psoc6_smif_read(const struct flash_area *fap,
                                        off_t addr,
                                        void *data,
                                        size_t len)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    addr = addr - CY_SMIF_BASE_MEM_OFFSET;

    result = cy_serial_flash_qspi_read(addr, len, data);

    if ( result == CY_RSLT_SUCCESS) {
        return (0);
    } else {
        return (-1);
    }
}

int psoc6_smif_write(const struct flash_area *fap,
                                        off_t addr,
                                        const void *data,
                                        size_t len)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    addr = addr - CY_SMIF_BASE_MEM_OFFSET;

    result = cy_serial_flash_qspi_write(addr, len, data);

    if ( result == CY_RSLT_SUCCESS) {
        return (0);
    } else {
        return (-1);
    }
}

int psoc6_smif_erase(off_t addr, size_t size)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    size_t min_erase_size;
    uint32_t address;
    uint32_t length;

    min_erase_size = cy_serial_flash_qspi_get_erase_size(addr - CY_SMIF_BASE_MEM_OFFSET);

    address = ((addr - CY_SMIF_BASE_MEM_OFFSET ) & ~(min_erase_size - 1u));

    if (( (addr + size) % min_erase_size) == 0) {
       length = size;
    } else {
       length = ((((addr - CY_SMIF_BASE_MEM_OFFSET ) + size)  + min_erase_size) & (~(min_erase_size - 1)));
    }

    result = cy_serial_flash_qspi_erase(address, length);

    if ( result == CY_RSLT_SUCCESS) {
        return (0);
    } else {
        return (-1);
    }
}


uint32_t psoc6_smif_get_prog_size(off_t addr)
{
    return cy_serial_flash_qspi_get_prog_size(addr);
}

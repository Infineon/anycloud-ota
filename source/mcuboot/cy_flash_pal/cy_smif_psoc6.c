/***************************************************************************//**
* \file cy_smif_psoc6.c
* \version 1.0
*
* \brief
*  This is the source file of external flash driver adoption layer between PSoC6
*  and standard MCUBoot code.
*
********************************************************************************
* \copyright
*
* (c) 2020, Cypress Semiconductor Corporation
* or a subsidiary of Cypress Semiconductor Corporation. All rights
* reserved.
*
* This software, including source code, documentation and related
* materials ("Software"), is owned by Cypress Semiconductor
* Corporation or one of its subsidiaries ("Cypress") and is protected by
* and subject to worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-
* exclusive, non-transferable license to copy, modify, and compile the
* Software source code solely for use in connection with Cypress?s
* integrated circuit products. Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO
* WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING,
* BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE. Cypress reserves the right to make
* changes to the Software without notice. Cypress does not assume any
* liability arising out of the application or use of the Software or any
* product or circuit described in the Software. Cypress does not
* authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*
******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sysflash.h"
#include "cy_smif_psoc6.h"

#include "mem_config_sfdp.h"
#include "cy_serial_flash_qspi.h"
#include "cybsp_types.h"


#define MEM_SLOT_NUM            (0u)      /* Slot number of the memory to use */
#define QSPI_BUS_FREQUENCY_HZ   (50000000lu)


int psoc6_qspi_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    result = cy_serial_flash_qspi_init(smifMemConfigsSfdp[MEM_SLOT_NUM], CYBSP_QSPI_D0,
              CYBSP_QSPI_D1, CYBSP_QSPI_D2, CYBSP_QSPI_D3, NC, NC, NC, NC,
              CYBSP_QSPI_SCK, CYBSP_QSPI_SS, QSPI_BUS_FREQUENCY_HZ);

    if (result == CY_RSLT_SUCCESS) {
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

    if (result == CY_RSLT_SUCCESS) {
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

    if (result == CY_RSLT_SUCCESS) {
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

    if ((size % min_erase_size) == 0) {
       length = size;
    } else {
       length = ((size + min_erase_size) & (~(min_erase_size - 1)));
    }

    result = cy_serial_flash_qspi_erase(address, length);

    if (result == CY_RSLT_SUCCESS) {
        return (0);
    } else {
        return (-1);
    }
}

uint32_t psoc6_smif_get_prog_size(off_t addr)
{
    return cy_serial_flash_qspi_get_prog_size(addr);
}

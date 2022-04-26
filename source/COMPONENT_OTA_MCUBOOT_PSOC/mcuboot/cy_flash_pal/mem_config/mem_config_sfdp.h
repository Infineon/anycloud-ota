/******************************************************************************
* File Name: mem_config_sfdp.h
*
* Description: This file contains the extern definitions for the memory
*              configuration structures to use with the QSPI MemSlot driver
*              (cy_smif_memslot.c/.h). This file is generated first using the
*              QSPI Configurator tool and then some structures have been renamed
*              to avoid collision with the memory configuration present in the
*              BSP.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2019-2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

#ifndef MEM_CONFIG_SFDP_H
#define MEM_CONFIG_SFDP_H
#include "cy_smif_memslot.h"

#define CY_SMIF_CFG_TOOL_VERSION           (210)

/* Supported QSPI Driver version */
#define CY_SMIF_DRV_VERSION_REQUIRED       (100)

#if !defined(CY_SMIF_DRV_VERSION)
    #define CY_SMIF_DRV_VERSION            (100)
#endif

/* Check the used Driver version */
#if (CY_SMIF_DRV_VERSION_REQUIRED > CY_SMIF_DRV_VERSION)
   #error The QSPI Configurator requires a newer version of the PDL. Update the PDL in your project.
#endif

#define CY_SMIF_DEVICE_NUM 1

extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_readCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_writeEnCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_writeDisCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_eraseCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_chipEraseCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_programCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_readStsRegQeCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_readStsRegWipCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_writeStsRegQeCmd;
extern cy_stc_smif_mem_cmd_t Auto_detect_SFDP_SlaveSlot_0_readSfdpCmd;

extern cy_stc_smif_mem_device_cfg_t deviceCfg_Auto_detect_SFDP_SlaveSlot_0;

extern cy_stc_smif_mem_config_t Auto_detect_SFDP_SlaveSlot_0;
extern cy_stc_smif_mem_config_t* smifMemConfigsSfdp[CY_SMIF_DEVICE_NUM];

extern cy_stc_smif_block_config_t smifBlockConfigSfdp;


#endif /* MEM_CONFIG_SFDP_H */

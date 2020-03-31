# Over The Air update middleware library for AnyCloud

***This library is still in development. This will be available when this comment is removed.***

This code library provides support for OTA updating a PSoC® 6 MCU and CYW4343W or 43012 connectivity device. In this example, device establishes connection with designated MQTT Broker. Once the connection completes successfully, the device subscribes to a topic. When an update is available, the customer will publish the update and the device saves the update to FLASH. On the next reboot, MCUBoot will copy the new application over to the Primary slot, and run the application.

## Requirements

- [ModusToolbox™ IDE](https://www.cypress.com/products/modustoolbox-software-environment) v2.0
- Programming Language: C
- Associated Parts:

## Supported Kits

- [PSoC 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W) - Default target
- [PSoC 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)

## Hardware Setup

This example uses the board's default configuration. See the kit user guide to ensure the board is configured correctly.

**Note**: Before using this code example, make sure that the board is upgraded to KitProg3. The tool and instructions are available in the [Firmware Loader](https://github.com/cypresssemiconductorco/Firmware-loader) GitHub repository. If you do not upgrade, you will see an error like "unable to find CMSIS-DAP device" or "KitProg firmware is out of date".

## Software Setup

- Install a terminal emulator if you don't have one. Instructions in this document use [Tera Term](https://ttssh2.osdn.jp/index.html.en).
- Python Interpreter. This code example is tested with [Python 3.8.1](https://www.python.org/downloads/release/python-381/).

## Using the Code Example

## Prepare MCUbootApp

OTA provides a way to update the system software. The OTA mechanism stores the new software to a "staging" area called the Secondary Slot.  MCUboot to do the actual update from the Secondary Slot to the Primary Slot. In order for the OTA software and MCUBoot to have the same information on where the two Slots are in FLASH, we need to tell MCUBoot where the Slots are located. We also use some of the same code (exact .h and .c files) in the OTA code so that they are both in agreement.

Preparing MCUBootApp requires a few steps

### In Command-line Interface (CLI):

1. Download and unzip the Example Program repository onto your local machine, or clone the repository.

2. Open a CLI terminal and navigate to the application folder.

3. Import required libraries by executing the `make getlibs` command.

4. We need to pull in mcuboot sub-modules to build mcuboot

   1. `cd libs/mcuboot`
   2. `git submodule update --init --recursive`
   7. `cd ../..`

5. We need to adjust MCUBootApp FLASH locations

   1. Edit libs/mcuboot/boot/cypress/MCUBootApp/MCUBootApp.mk

      `Add at line 48`

      `DEFINES_APP +=-DMCUBOOT_MAX_IMG_SECTORS=2000`

      `DEFINES_APP +=-DCY_BOOT_BOOTLOADER_SIZE=0x12000`

      `DEFINES_APP +=-DCY_BOOT_SCRATCH_SIZE=0x10000`

      `DEFINES_APP +=-DCY_BOOT_PRIMARY_1_SIZE=0x0EE000`

      `DEFINES_APP +=-DCY_BOOT_SECONDARY_1_SIZE=0x0EE000`

6. Change MCUBoot to ignore Primary Slot verify. Note that This is where the application is programmed, the OTA download is stores in Secondary slot, and it is verified before copying to Primary slot.

   1. Edit libs/mcuboot/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h:

   `line 77`

   `//#define MCUBOOT_VALIDATE_PRIMARY_SLOT`

7. Adjust signing type for MCUBoot as the default has changed from previous versions. The side effect of not doing this change is that the OTA will complete the download, reboot, and MCUBoot will not find the magic_number and fail to copy Secondary slot to Primary slot.

   1. Edit libs/mcuboot/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h

      `line 36 comment out these two lines`

      `/* Uncomment for ECDSA signatures using curve P-256. */`

      `//#define MCUBOOT_SIGN_EC256`

      `//#define NUM_ECC_BYTES (256 / 8) 	// P-256 curve size in bytes, rnok: to make compilable`


#### Building and programming MCUBootApp

1. `cd libs/mcuboot/cypress`

2. Make the application

   `make app APP_NAME=MCUBootApp TARGET=CY8CPROTO-062-4343W`

3. Use Cypress Programmer to program mcuboot. Remember to close Cypress Programmer before trying to program the application using the CLI as described below. The .elf file is here:

   `libs/mcuboot/boot/cypress/MCUBootApp/out/PSOC_062_2M/Debug/MCUBootApp.elf`

## Operation

1. Connect the board to your PC using the provided USB cable through the USB connector.

2. Modify WIFI_SSID and WIFI_PASSWORD macros to match with that of the Wi-Fi network credentials that you want to connect. These macros are defined in the `main.c` file.

3. Consult the README.md file for configuration of the Example Application.

4. Open a terminal program and select the KitProg3 COM port. Set the serial port parameters to 8N1 and 115200 baud.

5. Program the board.

   ### Using CLI

   #### Building and Programming mtb-example-ota

   1. From the terminal, execute the `make program` command to build and program the application using the default toolchain to the default target. You can specify a target and toolchain manually:
        ```
       make program TARGET=<BSP> TOOLCHAIN=<toolchain>
       ```
       Example:

        ```
        make program TARGET=CY8CKIT-062S2-43012 TOOLCHAIN=GCC_ARM
        ```
        **Note**:  Before building the application, ensure that the *libs* folder contains the BSP file (*TARGET_xxx.lib*) corresponding to the TARGET. Execute the `make getlibs` command to fetch the BSP contents before building the application.

   After programming, the application starts automatically.




## Related Resources

| Application Notes                                            |                                                              |
| :----------------------------------------------------------- | :----------------------------------------------------------- |
| [AN228571](https://www.cypress.com/AN228571) – Getting Started with PSoC 6 MCU on ModusToolbox | Describes PSoC 6 MCU devices and how to build your first application with ModusToolbox |
| [AN221774](https://www.cypress.com/AN221774) – Getting Started with PSoC 6 MCU on PSoC Creator | Describes PSoC 6 MCU devices and how to build your first application with PSoC Creator |
| [AN210781](https://www.cypress.com/AN210781) – Getting Started with PSoC 6 MCU with Bluetooth Low Energy (BLE) Connectivity on PSoC Creator | Describes PSoC 6 MCU with BLE Connectivity devices and how to build your first application with PSoC Creator |
| [AN215656](https://www.cypress.com/AN215656) – PSoC 6 MCU: Dual-CPU System Design | Describes the dual-CPU architecture in PSoC 6 MCU, and shows how to build a simple dual-CPU design |
| **Code Examples**                                            |                                                              |
| [Using ModusToolbox IDE](https://github.com/cypresssemiconductorco/Code-Examples-for-ModusToolbox-Software) | [Using PSoC Creator](https://www.cypress.com/documentation/code-examples/psoc-6-mcu-code-examples) |
| **Device Documentation**                                     |                                                              |
| [PSoC 6 MCU Datasheets](https://www.cypress.com/search/all?f[0]=meta_type%3Atechnical_documents&f[1]=resource_meta_type%3A575&f[2]=field_related_products%3A114026) | [PSoC 6 Technical Reference Manuals](https://www.cypress.com/search/all/PSoC%206%20Technical%20Reference%20Manual?f[0]=meta_type%3Atechnical_documents&f[1]=resource_meta_type%3A583) |
| **Development Kits**                                         | Buy at Cypress.com                                           |
| [CY8CPROTO-062-4343W](https://www.cypress.com/cy8cproto-062-4343w) PSoC 6 Wi-Fi BT Prototyping Kit | [CY8CKIT-062S2-43012](https://www.cypress.com/CY8CKIT-062S2-43012) PSoC 62S2 Wi-Fi BT Pioneer Kit |
| **Libraries**                                                |                                                              |
| Cypress Hardware Abstraction Layer Library and docs          | [psoc6hal](https://github.com/cypresssemiconductorco/psoc6hal) on GitHub |
| RetargetIO - Library for redirecting low level IO commands to allow sending messages via standard printf/scanf functions over a UART connection | [retarget-io](https://github.com/cypresssemiconductorco/retarget-io) on GitHub |
| **Middleware**                                               |                                                              |
| Links to all PSoC 6 Middleware                               | [psoc6-middleware](https://github.com/cypresssemiconductorco/psoc6-middleware) on GitHub |
| Wi-Fi Middleware core                                        | [wifi-mw-core](https://github.com/cypresssemiconductorco/wifi-mw-core) on GitHub |
| **Tools**                                                    |                                                              |
| [ModusToolbox IDE](https://www.cypress.com/modustoolbox)     | The Cypress IDE for PSoC 6 and IoT designers                 |
| [PSoC Creator](https://www.cypress.com/products/psoc-creator-integrated-design-environment-ide) | The Cypress IDE for PSoC and FM0+ development                |

## Other Resources

Cypress provides a wealth of data at www.cypress.com to help you select the right device, and quickly and effectively integrate it into your design.

For PSoC 6 MCU devices, see [How to Design with PSoC 6 MCU - KBA223067](https://community.cypress.com/docs/DOC-14644) in the Cypress community.

## Document History

Document Title: Over The Air update middleware library for AnyCloud

| Version | Description of Change |
| ------- | --------------------- |
| 1.0.0   | New middleware code   |

------

All other trademarks or registered trademarks referenced herein are the property of their respective owners.

![Banner](images/Banner.png)

-------------------------------------------------------------------------------

© Cypress Semiconductor Corporation, 2020. This document is the property of Cypress Semiconductor Corporation and its subsidiaries ("Cypress"). This document, including any software or firmware included or referenced in this document ("Software"), is owned by Cypress under the intellectual property laws and treaties of the United States and other countries worldwide. Cypress reserves all rights under such laws and treaties and does not, except as specifically stated in this paragraph, grant any license under its patents, copyrights, trademarks, or other intellectual property rights. If the Software is not accompanied by a license agreement and you do not otherwise have a written agreement with Cypress governing the use of the Software, then Cypress hereby grants you a personal, non-exclusive, nontransferable license (without the right to sublicense) (1) under its copyright rights in the Software (a) for Software provided in source code form, to modify and reproduce the Software solely for use with Cypress hardware products, only internally within your organization, and (b) to distribute the Software in binary code form externally to end users (either directly or indirectly through resellers and distributors), solely for use on Cypress hardware product units, and (2) under those claims of Cypress's patents that are infringed by the Software (as provided by Cypress, unmodified) to make, use, distribute, and import the Software solely for use with Cypress hardware products. Any other use, reproduction, modification, translation, or compilation of the Software is prohibited.<br/>
TO THE EXTENT PERMITTED BY APPLICABLE LAW, CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS DOCUMENT OR ANY SOFTWARE OR ACCOMPANYING HARDWARE, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. No computing device can be absolutely secure. Therefore, despite security measures implemented in Cypress hardware or software products, Cypress shall have no liability arising out of any security breach, such as unauthorized access to or use of a Cypress product. CYPRESS DOES NOT REPRESENT, WARRANT, OR GUARANTEE THAT CYPRESS PRODUCTS, OR SYSTEMS CREATED USING CYPRESS PRODUCTS, WILL BE FREE FROM CORRUPTION, ATTACK, VIRUSES, INTERFERENCE, HACKING, DATA LOSS OR THEFT, OR OTHER SECURITY INTRUSION (collectively, "Security Breach"). Cypress disclaims any liability relating to any Security Breach, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any Security Breach. In addition, the products described in these materials may contain design defects or errors known as errata which may cause the product to deviate from published specifications. To the extent permitted by applicable law, Cypress reserves the right to make changes to this document without further notice. Cypress does not assume any liability arising out of the application or use of any product or circuit described in this document. Any information provided in this document, including any sample design information or programming code, is provided only for reference purposes. It is the responsibility of the user of this document to properly design, program, and test the functionality and safety of any application made of this information and any resulting product. "High-Risk Device" means any device or system whose failure could cause personal injury, death, or property damage. Examples of High-Risk Devices are weapons, nuclear installations, surgical implants, and other medical devices. "Critical Component" means any component of a High-Risk Device whose failure to perform can be reasonably expected to cause, directly or indirectly, the failure of the High-Risk Device, or to affect its safety or effectiveness. Cypress is not liable, in whole or in part, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any use of a Cypress product as a Critical Component in a High-Risk Device. You shall indemnify and hold Cypress, its directors, officers, employees, agents, affiliates, distributors, and assigns harmless from and against all claims, costs, damages, and expenses, arising out of any claim, including claims for product liability, personal injury or death, or property damage arising from any use of a Cypress product as a Critical Component in a High-Risk Device. Cypress products are not intended or authorized for use as a Critical Component in any High-Risk Device except to the limited extent that (i) Cypress's published data sheet for the product explicitly states Cypress has qualified the product for use in a specific High-Risk Device, or (ii) Cypress has given you advance written authorization to use the product as a Critical Component in the specific High-Risk Device and you have signed a separate indemnification agreement.<br/>
Cypress, the Cypress logo, Spansion, the Spansion logo, and combinations thereof, WICED, PSoC, CapSense, EZ-USB, F-RAM, and Traveo are trademarks or registered trademarks of Cypress in the United States and other countries. For a more complete list of Cypress trademarks, visit cypress.com. Other names and brands may be claimed as property of their respective owners.

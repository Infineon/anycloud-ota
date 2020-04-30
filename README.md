# Over-the-Air update middleware library

This library provides support for OTA updating using WiFi of the application code running on a PSoC® 6 MCU with CYW4343W or CYW43012 connectivity device.

In this example, device establishes connection with designated MQTT Broker. Once the connection completes successfully, the device subscribes to a topic. The topic is defined by the user and passed as a parameter when starting the OTA Agent.

When an update is available, the customer will publish the update to the MQTT broker. The device receives and saves the update to FLASH Secondary slot (slot1). On the next reboot, MCUBoot bootloader will copy the new version of the application over to the Primary slot (slot 0) and run the application.

The ModusToolbox OTA code examples import this library automatically.

## Features and functionality

This library utilizes MQTT and TLS to securely connect to an MQTT Broker and download an update for the users application.

Other features:

- Configuration to adjust multiple timing values to customize how often to check for updates, and other parameters for the MQTT Broker connection.
- Runs in a separate background thread, only connecting to MQTT broker based on the application configuration.
- Run-time parameters for MQTT broker, credentials, and other parameters.
- Provides a callback mechanism to report stages of connect, download %, and errors.

Once the application starts the OTA agent, the OTA agent will contact the MQTT broker at the defined intervals to see if there is an update available. If an update is available, it will be downloaded. If the "reset_after_complete" flag was set in the agent parameters, the OTA agent will automatically reset the device. If not set, on the next manual reset, MCUBoot will perform the update.

## Requirements

- [ModusToolbox™ IDE](https://www.cypress.com/products/modustoolbox-software-environment) v2.1
- [MCUBoot](https://juullabs-oss.github.io/mcuboot/)
- Programming Language: C

## Supported Toolchains

- GCC
- IAR
- ARM6

## Supported OS

- FreeRTOS

## Supported Kits

- [PSoC 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W)
- [PSoC 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)

Only kits with 2M of Internal FLASH are supported at this time. The kit-specific things that need to be changed:

- FLASH locations and sizes for Bootloader, Primary Slot (slot 0) and Secondary Slot (slot 1).
  - See further information in the Prepare MCUBoot section below.
- Linker script for the new kit needs to be created.

## Hardware Setup

This example uses the board's default configuration. See the kit user guide to ensure the board is configured correctly.

**Note**: Before using this code example, make sure that the board is upgraded to KitProg3. The tool and instructions are available in the [Firmware Loader](https://github.com/cypresssemiconductorco/Firmware-loader) GitHub repository. If you do not upgrade, you will see an error like "unable to find CMSIS-DAP device" or "KitProg firmware is out of date".

## Software Setup

- Install a terminal emulator if you don't have one. Instructions in this document use [Tera Term](https://ttssh2.osdn.jp/index.html.en).
- Python Interpreter. This code example is tested with [Python 3.8.1](https://www.python.org/downloads/release/python-381/).

## Enabling Debug Output

- define LIBRARY_LOG_LEVEL to one of these defines, in order from no debug output to maximum debug output.
  - IOT_LOG_NONE
  - IOT_LOG_ERROR
  - IOT_LOG_WARN
  - IOT_LOG_INFO
  - IOT_LOG_DEBUG

## Prepare MCUbootApp

OTA provides a way to update the system software. The OTA mechanism stores the new software to a "staging" area called the Secondary Slot.  MCUboot will do the actual update from the Secondary Slot to the Primary Slot on the next reboot of the device. In order for the OTA software and MCUBoot to have the same information on where the two Slots are in FLASH, we need to tell MCUBoot where the Slots are located. We also use some of the same code (exact .h and .c files) in the OTA code so that they are both in agreement.

Preparing MCUBootApp requires a few steps.

## Flash Partitioning

Currently only internal flash is supported.

Primary Slot      (Slot 0): start: 0x012000, size: 0xEE000

Secondary Slot (Slot 1): start: 0x100000, size: 0xEE000

### Building MCUBootApp using Cygwin Command-line Interface (CLI):

Clone the MCUBoot repository onto your local machine, **outside of your application's directory.**

1. git clone https://github.com/JuulLabs-OSS/mcuboot.git

Open a CLI terminal and navigate to the mcuboot folder.

Change the branch to get the Cypress version.

1. `cd mcuboot`
2. `git checkout v1.5.0-cypress`

We need to pull in mcuboot sub-modules to build mcuboot.

1. `git submodule update --init --recursive`

Install the required python packages mentioned in `mcuboot\scripts\requirements.txt`.

1.  `cd mcuboot/scripts`
2. `pip install -r requirements.txt`

#### Adjust MCUBootApp FLASH locations.

It is important for both MCUBoot and the application to have the exact same understanding of the memory layout. Otherwise, the bootloader may consider an authentic image as invalid. To learn more about the bootloader refer to the [MCUBoot](https://github.com/JuulLabs-OSS/mcuboot/blob/cypress/docs/design.md) documentation.

NOTE: These values are used when the Primary and Secondary Slots are both in **internal** FLASH. To adjust the settings for your device/application, please read your application's notes.

1. Edit libs/mcuboot/boot/cypress/MCUBootApp/MCUBootApp.mk

   `Add at line 48`

   `DEFINES_APP +=-DMCUBOOT_MAX_IMG_SECTORS=2000`

   `DEFINES_APP +=-DCY_BOOT_BOOTLOADER_SIZE=0x12000`

   `DEFINES_APP +=-DCY_BOOT_SCRATCH_SIZE=0x10000`

   `DEFINES_APP +=-DCY_BOOT_PRIMARY_1_SIZE=0x0EE000`

   `DEFINES_APP +=-DCY_BOOT_SECONDARY_1_SIZE=0x0EE000`

#### Change MCUBoot to ignore Primary Slot verify.

​	Notes:

- The Primary Slot is where the application is run from.
- The OTA download stores in Secondary Slot.
- MCUBoot verifies the signature in the Secondary Slot before copying to Primary slot.
- Signature verify takes some time. Removing the verify allows for a faster bring up of your application.

1. Adjust signing type for MCUBoot as the default has changed from previous versions. The side effect of not doing this change is that the OTA will complete the download, reboot, and MCUBoot will not find the magic_number and fail to copy Secondary slot to Primary slot.

   1. Edit mcuboot/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h

      `line 38 & 39 comment out these two lines`

      `/* Uncomment for ECDSA signatures using curve P-256. */`

      `//#define MCUBOOT_SIGN_EC256`

      `//#define NUM_ECC_BYTES (256 / 8) 	// P-256 curve size in bytes, rnok: to make compilable`

   2. Edit mcuboot/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h:

      `line 77`

      `//#define MCUBOOT_VALIDATE_PRIMARY_SLOT`

#### Building MCUBootApp

Ensure that the toolchain path is set for the compiler. Check that the path is correct for your installed version of ModusToolbox.

​		`export TOOLCHAIN_PATH=<path>/ModusToolbox/tools_2.1/gcc-7.2.1`

Build the Application

​		`cd mcuboot/boot/cypress`

​		`make app APP_NAME=MCUBootApp TARGET=CY8CPROTO-062-4343W`

Use Cypress Programmer to program MCUBoot. Remember to close Cypress Programmer before trying to program the application using ModusToolbox or from the CLI. The MCUBOOT .elf file is here:

​		`mcuboot/boot/cypress/MCUBootApp/out/PSOC_062_2M/Debug/MCUBootApp.elf`

#### Program MCUBootApp

1. Connect the board to your PC using the provided USB cable through the USB connector.
2. Program the board using the instructions in your Customer Example Application notes.

### Prepare for Building your Application

Copy libs/anycloud-ota/configs/cy_ota_config.h to your application's top level directory, and adjust for your application needs.

Consult the README.md file for configuration of the Example Application and other information.

## Limitations
1. If the device is not subscribed to the topic on the MQTT Broker, it will miss the update messages published by the broker.
2. Currently only internal flash is supported. This currently limits OTA functionality to platforms that have 2M of internal FLASH.
3. Be sure to have a reliable network connection before starting an OTA update. If your Network connection is poor, OTA update may fail due to lost packets or lost connection.
4. Be sure to have a fully charged device before starting an OTA update. If you device's battery is low, OTA may fail.

## Unsupported Features

1. HTTP OTA Updates are not implemented at this time.
2. Application supplying MQTT connection information to OTA Agent not supported at this time.

## Additional Information

- [OTA RELEASE.md]()
- [OTA API reference guide](https://cypresssemiconductorco.github.io/anycloud-ota/api_reference_manual/html/index.html)
- [Cypress OTA Example](https://github.com/cypresssemiconductorco/mtb-example-anycloud-ota-mqtt )
- [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)
-  [MCUBoot](https://github.com/JuulLabs-OSS/mcuboot/blob/cypress/docs/design.md) documentation

Cypress also provides a wealth of data at www.cypress.com to help you select the right device, and quickly and effectively integrate it into your design.

For PSoC 6 MCU devices, see [How to Design with PSoC 6 MCU - KBA223067](https://community.cypress.com/docs/DOC-14644) in the Cypress community.

## Document History

| Version | Description of Change  |
| ------- | ---------------------- |
| 1.0.1   | Documentation updates  |
| 1.0.0   | New middleware library |

------

All other trademarks or registered trademarks referenced herein are the property of their respective owners.

![Banner](images/Banner.png)

-------------------------------------------------------------------------------

© Cypress Semiconductor Corporation, 2020. This document is the property of Cypress Semiconductor Corporation and its subsidiaries ("Cypress"). This document, including any software or firmware included or referenced in this document ("Software"), is owned by Cypress under the intellectual property laws and treaties of the United States and other countries worldwide. Cypress reserves all rights under such laws and treaties and does not, except as specifically stated in this paragraph, grant any license under its patents, copyrights, trademarks, or other intellectual property rights. If the Software is not accompanied by a license agreement and you do not otherwise have a written agreement with Cypress governing the use of the Software, then Cypress hereby grants you a personal, non-exclusive, nontransferable license (without the right to sublicense) (1) under its copyright rights in the Software (a) for Software provided in source code form, to modify and reproduce the Software solely for use with Cypress hardware products, only internally within your organization, and (b) to distribute the Software in binary code form externally to end users (either directly or indirectly through resellers and distributors), solely for use on Cypress hardware product units, and (2) under those claims of Cypress's patents that are infringed by the Software (as provided by Cypress, unmodified) to make, use, distribute, and import the Software solely for use with Cypress hardware products. Any other use, reproduction, modification, translation, or compilation of the Software is prohibited.<br/>
TO THE EXTENT PERMITTED BY APPLICABLE LAW, CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS DOCUMENT OR ANY SOFTWARE OR ACCOMPANYING HARDWARE, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. No computing device can be absolutely secure. Therefore, despite security measures implemented in Cypress hardware or software products, Cypress shall have no liability arising out of any security breach, such as unauthorized access to or use of a Cypress product. CYPRESS DOES NOT REPRESENT, WARRANT, OR GUARANTEE THAT CYPRESS PRODUCTS, OR SYSTEMS CREATED USING CYPRESS PRODUCTS, WILL BE FREE FROM CORRUPTION, ATTACK, VIRUSES, INTERFERENCE, HACKING, DATA LOSS OR THEFT, OR OTHER SECURITY INTRUSION (collectively, "Security Breach"). Cypress disclaims any liability relating to any Security Breach, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any Security Breach. In addition, the products described in these materials may contain design defects or errors known as errata which may cause the product to deviate from published specifications. To the extent permitted by applicable law, Cypress reserves the right to make changes to this document without further notice. Cypress does not assume any liability arising out of the application or use of any product or circuit described in this document. Any information provided in this document, including any sample design information or programming code, is provided only for reference purposes. It is the responsibility of the user of this document to properly design, program, and test the functionality and safety of any application made of this information and any resulting product. "High-Risk Device" means any device or system whose failure could cause personal injury, death, or property damage. Examples of High-Risk Devices are weapons, nuclear installations, surgical implants, and other medical devices. "Critical Component" means any component of a High-Risk Device whose failure to perform can be reasonably expected to cause, directly or indirectly, the failure of the High-Risk Device, or to affect its safety or effectiveness. Cypress is not liable, in whole or in part, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any use of a Cypress product as a Critical Component in a High-Risk Device. You shall indemnify and hold Cypress, its directors, officers, employees, agents, affiliates, distributors, and assigns harmless from and against all claims, costs, damages, and expenses, arising out of any claim, including claims for product liability, personal injury or death, or property damage arising from any use of a Cypress product as a Critical Component in a High-Risk Device. Cypress products are not intended or authorized for use as a Critical Component in any High-Risk Device except to the limited extent that (i) Cypress's published data sheet for the product explicitly states Cypress has qualified the product for use in a specific High-Risk Device, or (ii) Cypress has given you advance written authorization to use the product as a Critical Component in the specific High-Risk Device and you have signed a separate indemnification agreement.<br/>
Cypress, the Cypress logo, Spansion, the Spansion logo, and combinations thereof, WICED, PSoC, CapSense, EZ-USB, F-RAM, and Traveo are trademarks or registered trademarks of Cypress in the United States and other countries. For a more complete list of Cypress trademarks, visit cypress.com. Other names and brands may be claimed as property of their respective owners.

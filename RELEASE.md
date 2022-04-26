# Cypress Over The Air (OTA) Library

## What's Included?

See the [README.md](./README.md) for a complete description of the OTA library.

## Changelog

### v5.0.0

 - Add support for CYW920829M2EVB-01 with GCC compiler, BT Only.
 - Add support for CY8CPROTO-062S3-4343W, BT Only, internal flash.
 - Updated library use to MCUBoot v1.8.1-cypress
   - Legacy PSoC6 OTA targets
   - CY8CPROTO-062S3-4343W
   - CYW920829M2EVB-01


### v4.2.0

 - Add support for CY8CEVAL-062S2-MUR-43439M2

### v4.1.0

 - Fixed Bluetooth build issues with ARM and IAR compilers.
 - Add support for CY8CEVAL-062S2-LAI-4373M2.

### v4.0.0
 - Added Bluetooth transport support. This is a Push model where the Host pushes the changes to the IoT Device.
 - Updated for use with MCUboot v1.7.2.

### v3.0.0

- Updated for new MQTT and HTTP-CLIENT libraries. New APIs and data flow changes.
- Updated for use with MCUboot v1.7.0, which includes SWAP and REVERT functionality.
- Updated documentation to describe API changes and new functionality.
- Added OTA support for CY8CKIT-064B0S2-4343W.


### v2.0.1

- Separated OTA logging from the IotLogXXXX() facility. The application must call `cy_log_init()` before starting OTA to see the logs.


### v2.0.0

- Added support for HTTP and HTTPS transfer of the OTA image.

- Added support for OTA to use a "Job" file that gives information about an available update, including version available, and MQTT Broker or HTTP server where the update is located.

- Device and *publisher.py* script have more message interaction, including the device sending the result of the download back to the Publisher.


### v1.2.1

- Updated cyignore.

- Basic HTTP one file OTA image updating is working.


### v1.2.0

- Initial public release of the OTA library.

- Added MQTT connection context to parameters for `cy_ota_agent_start()`

- Fixed OTA per-packet timer.

- Fixed `CY_OTA_MQTT_TIMEOUT_MS`.

- Always check for duplicate packets / missed packets.

- Added config `USE_NEWLIB_REENTRANT` to *FreeRTOSConfig.h* for the test app.


### v1.1

- Added MQTT "clean" flag to parameters for `cy_ota_agent_start()`.

- Updated online documentation.

- Made OTA data flow more efficient.


### v1.0.0

- Initial release of the OTA library.


## Additional Information

- [ModusToolbox® Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)

_______________________________________________________________________________________________________

© 2021, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
This software, associated documentation and materials ("Software") is owned by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and is protected by and subject to worldwide patent protection (United States and foreign), United States copyright laws and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software ("EULA"). If no EULA applies, then any reproduction, modification, translation, compilation, or representation of this Software is prohibited without the express written permission of Cypress.
Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the Software without notice. Cypress does not assume any liability arising out of the application or use of the Software or any product or circuit described in the Software. Cypress does not authorize its products for use in any products where a malfunction or failure of the Cypress product may reasonably be expected to result in significant property damage, injury or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the manufacturer of such system or application assumes all risk of such use and in doing so agrees to indemnify Cypress against all liability.

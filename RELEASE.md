# Cypress Over The Air (OTA) Library

## What's Included?

See the [README.md](./README.md) for a complete description of the OTA library.

## Known Issues

- None.

## Changelog

### v3.0.0

- Updated for new MQTT and HTTP-CLIENT libraries. New APIs and data flow changes.
- Updated for use with MCUboot v1.7.0, which includes SWAP and REVERT functionality.
- Updated documentation to describe API changes and new functionality.
- Added OTA support for CY8CKIT_064B0S2_4343W.


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

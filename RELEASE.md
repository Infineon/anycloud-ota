# Cypress Over The Air (OTA) library

## What's Included?
Refer to the [README.md](./README.md) for a complete description of the OTA library.

## Known Issues
* None

## Changelog

### v2.0.0

- Add support for HTTP and HTTPS transfer of OTA Image.
- Add support for OTA to use a "job" file that gives information about an available update, including version available, and MQTT Broker or HTTP Server where update is located.
- Device and publisher.py script have more message interaction, including Device sending the result of the download back to the publisher.

### v1.2.1

* update cyignore
* Basic HTTP one file OTA Image updating is working


### v1.2.0
* Initial public release of OTA library
* Add MQTT connection context to paramaters for cy_ota_agent_start()
* Fix OTA per-packet timer
* Fix CY_OTA_MQTT_TIMEOUT_MS
* Always check for duplicate packets / missed packets
* Added configUSE_NEWLIB_REENTRANT to FreeRTOSConfig.h for test app


### v1.1
* Add MQTT "clean" flag to paramaters for cy_ota_agent_start()
* Update on-line docs
* Made OTA data flow more efficient.

### v1.0.0
* Initial release of OTA library

## Additional Information
* [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)

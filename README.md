# Over-the-Air update middleware library

This library provides support for OTA updating using WiFi of the application code running on a PSoC® 6 MCU with CYW4343W or CYW43012 connectivity device.

There are two "update flows" that the user can designate.

- Get OTA Image directly
  - This is the only flow that was available in the initial public release. It is still supported in this release. Be aware there are some API changes that require modification of the network parameters.

  - This is the simplest means of getting an OTA Image. The Device needs to know the location of the OTA Image.

    - For an MQTT broker, the Device will subscribe to a known topic and wait for the OTA Image download.
    - For an HTTP server, the Device will use "GET" to download the OTA Image


- Get Job document, and use the information within the Job document to locate the OTA Image
  - This is a new flow added in release 2.0.0. The API changes in the network parameters were required to have both MQTT and HTTP credentials present when calling cy_ota_agent_start(). There is one additional parameter "use_job_flow" which designates that the OTA Agent use this new flow. The default is 0 (do not use Job flow).
  - For MQTT this means that the Publisher will subscribe to a known topic on a designated MQTT broker and wait for a message. A Device will connect to the MQTT Broker and publish an "Update Availability" message to the Publisher.  The Publisher will send a Job document, which has information about what update is available and where it is located. The Device will then connect to the MQTT/HTTP location and download the OTA Image.
  - For HTTP this means that the HTTP Server will contain the Job document, provided in the network parameters when calling cy_ota_gent_start() (default: "ota_upgrade.json"). The Device gets the Job document, and decides if the update is needed.

The Device then stores the update in the Secondary Slot (slot 1) in the FLASH.

Upon reboot, MCUBoot will verify the update in the Secondary Slot (slot 1) and copy it to the Primary Slot (slot 0), and runs the new application.

The ModusToolbox OTA code examples import this library automatically.

## Features and functionality

This library utilizes MQTT or HTTP and TLS to securely connect to an MQTT Broker / HTTP Server and download an update for the users application.

Other features:

- Configuration to adjust multiple timing values to customize how often to check for updates, and other parameters for the MQTT Broker / HTP Server connection. Copy configs/ota_config.h to the directory where your application Makefile is, and adjust as needed.
- The OTA Agent runs in a separate background thread, only connecting to MQTT Broker / HTTP Server based on the timing configuration.
- Parameters for MQTT Broker / HTTP Server, credentials, etc. are passed into cy_ota_agent_start().
- Provides a callback mechanism to report stages of connect, download percentage, and errors.

Once the application starts the OTA agent, the OTA agent will contact the MQTT Broker / HTTP Server at the defined intervals to see if there is an update available. If an update is available, it will be downloaded. If the "reset_after_complete" flag was set in the agent parameters, the OTA agent will automatically reset the device. If not set, on the next manual reset, MCUBoot will perform the update.

### New Feature In v2.0.0:

### Job Document

We have added a message exchange between the Device and the Publisher. The JSON formatted "Job document" contains information where the OTA Image is located. This allows for Job document to reside in one place, and the OTA Image to reside in another place.

For MQTT transport, this allows a Publisher python script to be subscribed to a known topic (ex: "anycloud-CY8CPROTO_062_4343W/notify_publisher") to listen for a Device request. This allows for an asynchronous update sequence.

For HTTP transport, the Device will GET the job document (ex:"ota_update.json") which contains the same information as if it was served from an MQTT Broker.

#### Example Update Sequence Using MQTT

- The Publisher python script (libs/anycloud-ota/scripts/publisher.py) subscribes to a known topic (ex: "anycloud/CY8CPROTO_062_4343W/publish_notify") on the specified MQTT Broker.
- The Device publishes the message "Update Availability" to the known topic. The message includes specifics about the Device (manufacturer, product, serial number, application version, etc.), and a "Unique Topic Name" for the Publisher to send messages to the Device.
- The Publisher receives the "Update Availability" message and sends the appropriate Job document on the "Unique Topic Name". The Publisher can make a decision if there is an appropriate update available for the Device. The Publisher sends an "Update Available" (or "No Update Available") message, which repeats Device specific info and includes the location of the OTA Image file for the Device to download.
- Device receives the job document and connects to the Broker/Server given in the Job document to obtain the OTA Image.
  - If the OTA Image is located on an MQTT Broker, the Device connects and sends a "Request Update" message to the Publisher, which includes the Device info again. The Publisher then splits the OTA Image into 4k chunks, adds a header to each chunk, and sends it to the Device on the "Unique Topic Name".
  - If the OTA Image is accessible on an HTTP Server, the Device will connect to the HTTP Server and download the OTA Image using GET.

#### Example Update Sequence Using HTTP

- Device connects to the HTTP server and uses "GET" to obtain the Job document (ex: "ota_update.json").
- Device decides if the OTA Image version is newer than the currently executing application, and the board name is the same, etc.
  - If the OTA Image is accessible through an MQTT Broker, the Device creates and subscribes to a unique topic name and sends a "Request Download" message with the "Unique Topic Name" to the Publisher on "anycloud-CY8CPROTO_062_4343W/notify_publisher". The Publisher then splits to the OTA Image into chunks and publishes them on the unique topic.
  - If the OTA Image is accessible on an HTTP Server, the Device will connect to the HTTP Server and download the OTA Image using GET.

#### The Job Document

The job document is a JSON formatted file that contains fields used to communicate between the Publisher script and the OTA Agent running on the Device.

```
Manufacturer:	Manufacturer Name.
ManufacturerId: Manufacturer Identifier
Product: 		Product Name.
SerialNumber:   The Devices unique Serial Number
Version:		Application version Major.Minor.Build (ex: "12.15.0")
Board:			Name of board used for the product. (ex: "CY8CPROTO_062_4343W")
Connection:		The type of Connection to access the OTA Image. ("MQTT", "HTTP", "HTTPS")
Port:			Port number for accessing the OTA Image. (ex: MQTT:1883, HTTP:80)
```

For MQTT Transport Type:

```
Broker:			 MQTT Broker to access the OTA Image (ex: "mqtt.my_broker.com")
UniqueTopicName: The "replace" value is just a placeholder. It is replaced with
                 the Unique Topic Name for the OTA data transfer.
```

For HTTP Transport Type:

```
Server:			Server URL (ex: "http://ota_images.my_server.com")
File:			Name of the OTA Image file (ex: "<product>_<board>_v5.6.0.bin")
```

Example job document for an OTA Image that is available on an MQTT Broker:

```
{
  "Manufacturer":"Express Widgits Corporation",
  "ManufacturerId":"EWCO",
  "Product":"Easy Widgit",
  "SerialNumber":"ABC213450001",
  "Version":"1.2.28",
  "Board":"CY8CPROTO_062_4343W",
  "Connection":"MQTT",
  "Broker":"mqtt.widgitcorp.com",
  "Port":"1883"
}
```



Example job document for an OTA Image that is available on an HTTP Server:

```
{
  "Manufacturer":"Express Widgits Corporation",
  "ManufacturerId":"EWCO",
  "Product":"Easy Widgit",
  "SerialNumber":"ABC213450001",
  "Version":"1.76.0",
  "Board":"CY8CPROTO_062_4343W",
  "Connection":"HTTP",
  "Server":"<server_URL>",
  "Port":"80",
  "File":"/<OTA_image_file_name>"
}
```

#### Setting Up The Supplied Publisher Python script

There are a number of configurable values  in the libs/anycloud-ota/scripts/publisher.py script. The values are described in the publisher.py script. There are many items that need to match the Application values. Please be careful when changing the values, and see the example application you are using for any special needs.

##### Running Publisher Python Script

```
cd libs/anycloud-ota/scripts
python publisher.py [tls] [-l] [-f <filepath>] [-b <broker>] [-k <kit>]
```

Usage: 'python publisher.py [tls] [-f filepath]'

- tls - add this argument to run publisher with TLS (secure sockets). You must also supply appropriate certificates and keys

- -l - output more logging information to debug connection issues.

- -f filepath - use this argument to designate the name of the downloaded file.

- -b broker - use this argument to override the default broker.  Note that you will need to add certificates for TLS usage. The files need to reside in the same directory as subscriber.py.

  - amazon

    - You need to change the AMAZON_BROKER_ADDRESS endpoint.

         ```
         AMAZON_BROKER_ADDRESS = "<endpoint>.iot.us-west-1.amazonaws.com"
         ```

    - Add these certificate files:

              ca_certs = "amazon_ca.crt"
              certfile = "amazon_client.crt"
              keyfile  = "amazon_private_key.pem"

  - eclipse

    - Add these certificate files:

              ca_certs = "eclipse_ca.crt"
              certfile = "eclipse_client.crt"
              keyfile  = "eclipse_client.pem"

  - mosquitto

    - Add these certificate files:

      ```
      ca_certs = "mosquitto.org.crt"
      certfile = "mosquitto_client.crt"
      keyfile  = "mosquitto_client.key"
      ```

- -k kit - use this argument to override the default kit (CY8CPROTO_062_4343W).

  - The kit name is used as part of the topic name, and matches the KIT that the application was built with.

#### Use Of The Supplied Subscriber Python Script

The script subscriber.py is provided as a verification script that acts the same as a Device. It can be used to verify that the publisher is working as expected. Be sure that the BROKER_ADDRESS matches the publisher.py.

##### Running Subscriber Python Script

```
cd libs/anycloud-ota/scripts
python subscriber.py [tls] [-l] [-b broker] [-k kit] [-f filepath]
```

Usage: 'python subscriber.py [tls] [-f filepath]'

- tls - add this argument to run publisher with TLS (secure sockets). You must also supply appropriate certificates and keys.

- -l - output more logging information to debug connection issues.

- -b broker - use this argument to override the default broker.  Note that you will need to add certificates for TLS usage. The files need to reside in the same directory as publisher.py.

  - amazon

    - You need to change the AMAZON_BROKER_ADDRESS endpoint.

      ```
      AMAZON_BROKER_ADDRESS = "<endpoint>.iot.us-west-1.amazonaws.com"
      ```

    - Add these certificate files:

          ca_certs = "amazon_ca.crt"
          certfile = "amazon_client.crt"
          keyfile  = "amazon_private_key.pem"

  - eclipse

    - Add these certificate files:

          ca_certs = "eclipse_ca.crt"
          certfile = "eclipse_client.crt"
          keyfile  = "eclipse_client.pem"

  - mosquitto

    - Add these certificate files:

      ```
      ca_certs = "mosquitto.org.crt"
      certfile = "mosquitto_client.crt"
      keyfile  = "mosquitto_client.key"
      ```

- -k kit - use this argument to override the default kit (CY8CPROTO_062_4343W).

  - The kit name is used as part of the topic name, and matches the KIT that the application was built with.

- -f filepath - use this argument to designate the output file for the download.

## Migrating from OTA 1.X to OTA 2.X

There are a few items that need to be changed when migrating from v1.X to v2.X:

- OTA 2.X no longer uses mcuboot as a library.
  - Verify that "mcuboot.lib" does not appear in the "deps" folder for your project.
  - delete the "libs/mcuboot" folder
- The structure used when calling cy_ota_agent_start () is the same but some content has changed.
  - cy_ota_network_params_t
    - We now accept connection and credentials for both MQTT and HTTP connections so that if a job document redirects from MQTT to HTTP, the OTA Agent will have the TLS credentials for both servers.
    - Deprecated :
      - transport - This directed the OTA Agent which transport (MQTT or HTTP) was to be used for the entire OTA transfer.
      - u - This was a union of the MQTT and HTTP server and credential information. As we now accept both, the union was dissolved.
      - server - This has been moved to the individual MQTT and HTTP information structures.
      - credentials - This has been moved to the individual MQTT and HTTP information structures.
    - New:
      - initial_connection - This directs the OTA Agent as to the first transport to use when getting the Job document. The Job document may re-direct to a different type of transport for downloading the data.
      - mqtt and http - These are now separated and provide information about the connections.
      - use_get_job_flow - This allows the application to decide if a Direct download is required or if the new Job Document Flow is preferred.
- The Application Callback has been expanded to provide more information to the application and allow for the application to modify some of the fields of the information.
  - The new structure contains the similar "reason" for the callback, and more. Please see the documentation for more in-depth inormation.

## Requirements

- [ModusToolbox™ IDE](https://www.cypress.com/products/modustoolbox-software-environment) v2.1
- [MCUBoot](https://juullabs-oss.github.io/mcuboot/)
- Programming Language: C
- [Python 3.7 or higher](https://www.python.org/downloads//) (for signing and Publisher script)
  - Run pip to install the modules.

```
pip install -r <repo root>/vendors/cypress/MTB/port_support/ota/scripts/requirements.txt
```

## Supported Toolchains

- GCC
- IAR

## Supported OS

- FreeRTOS

## Supported Kits

- [PSoC 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W)
- [PSoC 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)

Only kits with 2M of Internal FLASH and External FLASH are supported at this time. The kit-specific things that need to be changed:

- FLASH locations and sizes for Bootloader, Primary Slot (slot 0) and Secondary Slot (slot 1).
  - See further information in the Prepare MCUBoot section below.

## Hardware Setup

This example uses the board's default configuration. See the kit user guide to ensure the board is configured correctly.

**Note**: Before using this code example, make sure that the board is upgraded to KitProg3. The tool and instructions are available in the [Firmware Loader](https://github.com/cypresssemiconductorco/Firmware-loader) GitHub repository. If you do not upgrade, you will see an error like "unable to find CMSIS-DAP device" or "KitProg firmware is out of date".

## Software Setup

- Install a terminal emulator if you don't have one. Instructions in this document use [Tera Term](https://ttssh2.osdn.jp/index.html.en).
- Python Interpreter. The supplied publisher.py is tested with [Python 3.8.1](https://www.python.org/downloads/release/python-381/).

## Enabling Debug Output

- In libs/anycloud-ota/include/cy_ota_api.h, define LIBRARY_LOG_LEVEL to one of these defines, in order, from no debug output to maximum debug output.
  - IOT_LOG_NONE
  - IOT_LOG_ERROR
  - IOT_LOG_WARN
  - IOT_LOG_INFO
  - IOT_LOG_DEBUG

## Clone MCUBoot

We need to first build and program the bootloader app *MCUBootApp* that is available in the MCUBoot GitHub repo, before programming this OTA app. The bootloader app is run by CM0+ while this OTA app is run by CM4. Clone the MCUBoot repository onto your local machine, **outside of your application directory.**

1. git clone https://github.com/JuulLabs-OSS/mcuboot.git

2. Open a CLI terminal and navigate to the cloned mcuboot folder: `cd mcuboot`

3. Change the branch to get the Cypress version: `git checkout v1.6.1-cypress`

4. We need to pull in mcuboot sub-modules to build mcuboot: `git submodule update --init --recursive`

5. Install the required python packages mentioned in *mcuboot\scripts\requirements.txt*.

   i. cd `mcuboot/scripts`

   ii. `pip install -r requirements.txt`

## Configure MCUBootApp

#### Adjust MCUBootApp RAM start in linker script

The default RAM location needs to be modified for use with OTA.

1. Edit *mcuboot/boot/cypress/MCUBootApp/MCUBootApp.ld*

   Change line 66 from:

   ```
   ram               (rwx)   : ORIGIN = 0x08020000, LENGTH = 0x20000
   ```

   To:

   ```
   ram               (rwx)   : ORIGIN = 0x08000000, LENGTH = 0x20000
   ```



#### Adjust MCUBootApp FLASH locations.

It is important for both MCUBoot and the application to have the exact same understanding of the memory layout. Otherwise, the bootloader may consider an authentic image as invalid. To learn more about the bootloader refer to the [MCUBoot](https://github.com/JuulLabs-OSS/mcuboot/blob/cypress/docs/design.md) documentation.

#### Internal Flash for Secondary Slot

These values are used when the Secondary Slot is located in **internal** flash. Since internal flash is 2 MB, we need to have the bootloader and the Primary and Secondary slots fit. The Primary and Secondary slots are always the same size.

1. Edit *mcuboot/boot/cypress/MCUBootApp/MCUBootApp.mk*

   Change at line 31:

   ```
   USE_EXTERNAL_FLASH ?= 0
   ```

   Replace line 55 with:

   ```
   DEFINES_APP +=-DMCUBOOT_MAX_IMG_SECTORS=2000
   ```

   Add at line 56:

   ```
   DEFINES_APP +=-DCY_BOOT_PRIMARY_1_SIZE=0x0EE000
   DEFINES_APP +=-DCY_BOOT_SECONDARY_1_SIZE=0x0EE000
   ```



#### External Flash for Secondary Slot

These values are used when the Secondary Slot is located in **external** flash. This allows for a larger Primary Slot and hence, a larger application size. The Primary and Secondary slots are always the same size.

1. Edit *mcuboot/boot/cypress/MCUBootApp/MCUBootApp.mk*

   Change at line 31:

   ```
   USE_EXTERNAL_FLASH ?= 1
   ```

   Replace line 55 with:

   ```
   DEFINES_APP +=-DMCUBOOT_MAX_IMG_SECTORS=3584
   ```

   Add at line 56:

   ```
   DEFINES_APP +=-DCY_BOOT_PRIMARY_1_SIZE=0x01c0000
   DEFINES_APP +=-DCY_BOOT_SECONDARY_1_SIZE=0x01c00=00
   ```



### Change MCUBoot to ignore Primary Slot verify.

​	Notes:

- The Primary Slot is where the application is run from.
- The OTA download stores the new application in Secondary Slot.
- MCUBoot verifies the signature in the Secondary Slot before copying to Primary slot.
- Signature verify takes some time. Removing the verify before running from the Primary Slot allows for a faster boot up of your application. Note that MCUBoot checks the signature on the Secondary Slot before copying the application.

1. Adjust signing type for MCUBoot as the default has changed from previous versions. The side effect of not doing this change is that the OTA will complete the download, reboot, and MCUBoot will not find the magic_number and fail to copy Secondary slot to Primary slot.

   1. Edit mcuboot/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h

      On lines 38 & 39 comment out these two lines:

      ```
      /* Uncomment for ECDSA signatures using curve P-256. */
      //#define MCUBOOT_SIGN_EC256
      //#define NUM_ECC_BYTES (256 / 8) 	// P-256 curve size in bytes, rnok: to make compilable
      ```

      Edit mcuboot/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h:

      On line 78:

      ```
      //#define MCUBOOT_VALIDATE_PRIMARY_SLOT
      ```



#### Building MCUBootApp


Ensure that the toolchain path is set for the compiler. Check that the path is correct for your installed version of ModusToolbox.

This is for Modustoolbox v2.1.x:

​		`export TOOLCHAIN_PATH=<path>/ModusToolbox/tools_2.1/gcc-7.2.1`

This is for Modustoolbox v2.2.x:

​		`export TOOLCHAIN_PATH=<path>/ModusToolbox/tools_2.2/gcc`

Build the Application

​		`cd mcuboot/boot/cypress`

​		`make app APP_NAME=MCUBootApp TARGET=CY8CPROTO-062-4343W`

Use Cypress Programmer to program MCUBoot. Remember to close Cypress Programmer before trying to program the application using ModusToolbox or from the CLI. The MCUBoot application .elf file is here:

​		`mcuboot/boot/cypress/MCUBootApp/out/PSOC_062_2M/Debug/MCUBootApp.elf`

#### Program MCUBootApp

1. Connect the board to your PC using the provided USB cable through the USB connector.
2. Program the board using the instructions in your Customer Example Application notes.

## Prepare for Building your OTA Application

### Prepare for Building your Application

Copy libs/anycloud-ota/configs/cy_ota_config.h to your application's top level directory, and adjust for your application needs.

Consult the README.md file for configuration of the Example Application and other information.

The Makefile uses this compile variable to determine which FLASH to use:

`OTA_USE_EXTERNAL_FLASH=1`

The default for the build is to use external. The build system will use the values described above for the slot sizes automatically.

To use internal FLASH only, set the Makefile variable to zero:

`OTA_USE_EXTERNAL_FLASH=0`

## Flash Partitioning

OTA provides a way to update the system software. The OTA mechanism stores the new software to a "staging" area called the Secondary Slot.  MCUboot will do the actual update from the Secondary Slot to the Primary Slot on the next reboot of the device. In order for the OTA software and MCUBoot to have the same information on where the two Slots are in FLASH, we need to tell MCUBoot where the Slots are located.

Secondary slot can be placed on either Internal or external flash. The start address when internal flash is used is an offset from the starting address of the internal flash 0x10000000.

Internal flash:

Primary Slot      (Slot 0): start: 0x018000, size: 0xEE000

Secondary Slot (Slot 1): start: 0x106000, size: 0xEE000

External flash:

Primary Slot      (Slot 0): start: 0x018000, size: 0x01c0000

Secondary Slot (Slot 1): start: 0x18000000, size: 0x01c0000

## Limitations
1. If not using a Job document with MQTT, and the Device is not subscribed to the topic on the MQTT Broker when the Publisher sends the update, it will miss the update.

   ***The solution is to use a Job document, which has information about where the OTA Image is located.***

2. Internal and External FLASH is supported. Using Internal FLASH can only be used with platforms that have 2M of internal FLASH.

3. Be sure to have a reliable network connection before starting an OTA update. If your Network connection is poor, OTA update may fail due to lost packets or lost connection.

4. Be sure to have a fully charged device before starting an OTA update. If you device's battery is low, OTA may fail.

## Additional Information

- [OTA RELEASE.md]()
- [OTA API reference guide](https://cypresssemiconductorco.github.io/anycloud-ota/api_reference_manual/html/index.html)
- [Cypress OTA Example](https://github.com/cypresssemiconductorco/mtb-example-anycloud-ota-mqtt )
- [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)
-  [MCUBoot](https://github.com/JuulLabs-OSS/mcuboot/blob/cypress/docs/design.md) documentation

Cypress also provides a wealth of data at www.cypress.com to help you select the right device, and quickly and effectively integrate it into your design.

For PSoC 6 MCU devices, see [How to Design with PSoC 6 MCU - KBA223067](https://community.cypress.com/docs/DOC-14644) in the Cypress community.

## Document History

| Document Version | Description of Change              |
| ---------------- | ---------------------------------- |
| 1.2.0            | Updated for version 2.0.0 features |
| 1.1.0            | Updated for HTTP support           |
| 1.0.1            | Documentation updates              |
| 1.0.0            | New middleware library             |

------

All other trademarks or registered trademarks referenced herein are the property of their respective owners.

------

![Banner](images/Banner.png)

-------------------------------------------------------------------------------

© Cypress Semiconductor Corporation, 2020. This document is the property of Cypress Semiconductor Corporation and its subsidiaries ("Cypress"). This document, including any software or firmware included or referenced in this document ("Software"), is owned by Cypress under the intellectual property laws and treaties of the United States and other countries worldwide. Cypress reserves all rights under such laws and treaties and does not, except as specifically stated in this paragraph, grant any license under its patents, copyrights, trademarks, or other intellectual property rights. If the Software is not accompanied by a license agreement and you do not otherwise have a written agreement with Cypress governing the use of the Software, then Cypress hereby grants you a personal, non-exclusive, nontransferable license (without the right to sublicense) (1) under its copyright rights in the Software (a) for Software provided in source code form, to modify and reproduce the Software solely for use with Cypress hardware products, only internally within your organization, and (b) to distribute the Software in binary code form externally to end users (either directly or indirectly through resellers and distributors), solely for use on Cypress hardware product units, and (2) under those claims of Cypress's patents that are infringed by the Software (as provided by Cypress, unmodified) to make, use, distribute, and import the Software solely for use with Cypress hardware products. Any other use, reproduction, modification, translation, or compilation of the Software is prohibited.<br/>
TO THE EXTENT PERMITTED BY APPLICABLE LAW, CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS DOCUMENT OR ANY SOFTWARE OR ACCOMPANYING HARDWARE, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. No computing device can be absolutely secure. Therefore, despite security measures implemented in Cypress hardware or software products, Cypress shall have no liability arising out of any security breach, such as unauthorized access to or use of a Cypress product. CYPRESS DOES NOT REPRESENT, WARRANT, OR GUARANTEE THAT CYPRESS PRODUCTS, OR SYSTEMS CREATED USING CYPRESS PRODUCTS, WILL BE FREE FROM CORRUPTION, ATTACK, VIRUSES, INTERFERENCE, HACKING, DATA LOSS OR THEFT, OR OTHER SECURITY INTRUSION (collectively, "Security Breach"). Cypress disclaims any liability relating to any Security Breach, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any Security Breach. In addition, the products described in these materials may contain design defects or errors known as errata which may cause the product to deviate from published specifications. To the extent permitted by applicable law, Cypress reserves the right to make changes to this document without further notice. Cypress does not assume any liability arising out of the application or use of any product or circuit described in this document. Any information provided in this document, including any sample design information or programming code, is provided only for reference purposes. It is the responsibility of the user of this document to properly design, program, and test the functionality and safety of any application made of this information and any resulting product. "High-Risk Device" means any device or system whose failure could cause personal injury, death, or property damage. Examples of High-Risk Devices are weapons, nuclear installations, surgical implants, and other medical devices. "Critical Component" means any component of a High-Risk Device whose failure to perform can be reasonably expected to cause, directly or indirectly, the failure of the High-Risk Device, or to affect its safety or effectiveness. Cypress is not liable, in whole or in part, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any use of a Cypress product as a Critical Component in a High-Risk Device. You shall indemnify and hold Cypress, its directors, officers, employees, agents, affiliates, distributors, and assigns harmless from and against all claims, costs, damages, and expenses, arising out of any claim, including claims for product liability, personal injury or death, or property damage arising from any use of a Cypress product as a Critical Component in a High-Risk Device. Cypress products are not intended or authorized for use as a Critical Component in any High-Risk Device except to the limited extent that (i) Cypress's published data sheet for the product explicitly states Cypress has qualified the product for use in a specific High-Risk Device, or (ii) Cypress has given you advance written authorization to use the product as a Critical Component in the specific High-Risk Device and you have signed a separate indemnification agreement.<br/>
Cypress, the Cypress logo, Spansion, the Spansion logo, and combinations thereof, WICED, PSoC, CapSense, EZ-USB, F-RAM, and Traveo are trademarks or registered trademarks of Cypress in the United States and other countries. For a more complete list of Cypress trademarks, visit cypress.com. Other names and brands may be claimed as property of their respective owners.

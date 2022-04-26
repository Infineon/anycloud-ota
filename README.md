﻿﻿﻿﻿﻿﻿﻿﻿# Over-the-Air (OTA) Update Middleware Library

The OTA library provides support for Over-The-Air update of the application code running on a PSoC™ 6 MCU with AIROC™ CYW4343W or CYW43012 Wi-Fi & Bluetooth® combo chip, or the CYW920829M2EVB-01 evaluation board. Device communication using Wi-Fi or Bluetooth®.

## 1. WiFi Update Flows

There are two "update flows" that you can designate. Update Flows use a Pull Model and only apply to WiFi connectivity.

#### 1.1 Get the OTA Update Image Directly

This is the only flow that was available in the initial public release. It is still supported in this release.

This is the simplest means of getting an OTA update image.

For an MQTT broker, the Publisher will subscribe to a known topic on a designated MQTT broker and wait for a message. The device will subscribe to a unique topic, publish a request on the known topic with a document that includes the unique topic name, and wait for the OTA update image download.

For an HTTP server, the device will use `GET` to download the OTA update image.


#### 1.2 Get the Job document, and Locate the OTA Update Image

This in the new flow added in release v2.0.0. Here, the information within the Job document is used to locate the OTA update image. API changes in the network parameters are required to have both MQTT and HTTP credentials present when calling `cy_ota_agent_start()`. The additional parameter `use_job_flow` designates that the OTA Agent use this new flow. The default is 0 (do not use Job flow).

For MQTT, this means that the Publisher will subscribe to a known topic on a designated MQTT broker and wait for a message. A device will connect to the MQTT Broker and publish an "Update Availability" message to the Publisher. The Publisher will send a Job document, which has information about the update that is available and its location (the location can be an MQTT Broker or an HTTP server). The device will then connect to the MQTT/HTTP location and download the OTA update image.

For HTTP, this means that the HTTP server will contain the Job document. The device will use the HTTP `GET` request to get the document from the known location, which has information about the update that is available, and its location (the location can be an MQTT Broker or an HTTP server). The device will then connect to the MQTT/HTTP location and download the OTA update image.

The device then stores the update in the secondary slot (Slot 1) in the flash.

When rebooted, MCUboot will verify the update in the secondary slot (Slot 1) and copy or swap it to the primary slot (Slot 0), and run the new application.

#### 1.3 Inclusion of the anycloud-ota library in ModusToolbox applications.

The ModusToolbox OTA code examples import the anycloud-ota library automatically.

## 2. Features and Functionality

This library utilizes MQTT or HTTP and TLS to securely connect to an MQTT Broker/HTTP server and download an update for the user application.

### 2.1 Features

- Provides configuration options to adjust multiple timing values to customize how often to check for updates, and other parameters for the MQTT Broker/HTTP Server connection. Copy the configurations or the *cy_ota_config.h* file to the directory where your application Makefile is, and adjust as needed.

- The OTA Agent runs in a separate background thread, only connecting to MQTT Broker/HTTP server based on the timing configuration.

- Parameters such as for MQTT Broker/HTTP server and credentials are passed into `cy_ota_agent_start()`.

- Provides a callback mechanism to report stages of connect, download percentage, and errors. The application can override the default OTA Agent behavior, or stop the current download during the callback.

Once the application starts the OTA agent, the OTA agent will contact the MQTT Broker/HTTP server at the defined intervals to check whether an update is available. If available, the update it will be downloaded. If the `reset_after_complete` flag was set in the agent parameters, the OTA Agent will automatically reset the device. If not set, on the next manual reset, MCUboot will perform the update.

### 2.2 New Features in v5.0.0

#### 2.2.1 New Target Platforms

We have added support for 2 new platforms in limited configurations.

- CYW920829M2EVB-01
- [CY8CPROTO-062S3-4343W](https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062s3-4343w/)

#### 2.2.2 Cypress Bootloader  v1.8.1

See "MCUBoot Instructions" below.

### 2.3 New Features in v4.0.0

#### 2.3.1 Bluetooth® Support

Support for Bluetooth® transport in the Application has been added to the AnyCloud-OTA Library. The application is expected to initialize the Bluetooth® connection, including setting up the GATT database correctly in order to accept a Bluetooth® connection and send the OTA Image data to the anycloud-ota library for saving to FLASH, and setting the proper flags for MCUBoot to update the executable on the next system reset.

Note that all of the COPY / SWAP  / REVERT functionality applies equally to Bluetooth® Support.

#### 2.3.2 Bluetooth® Host Support Application

For testing, there is an example Host-side tool that can be obtained here:

https://github.com/cypresssemiconductorco/btsdk-peer-apps-ota

Clone into a separate directory. There are sub-directories with Windows, Android, and iOS sources.

```
git clone https://github.com/cypresssemiconductorco/btsdk-peer-apps-ota.git
```

For usage information, please see the README.md in the btsdk-peer-apps-ota repo.

### 2.4 New Features in v3.0.0

#### 2.4.1 SWAP/REVERT Support with MCUboot v1.7.0-cypress

The updated version of MCUboot supports a SWAP mode (default). Rather than copy the new OTA application from the secondary slot to the primary slot, the two applications are swapped. That is, the downloaded application that was stored in the secondary slot is moved to the primary slot; the image in the primary slot (original application) is moved to the secondary slot. This allows for a REVERT to the previous version if there is an issue with the newly downloaded application. This provides a restoration of a "last known good" application.

If SWAP and REVERT is not desired, you can build mcuboot with COPY so that the OTA application is copied from the secondary slot to the primary slot, which leaves no "last known good" application. This process is much quicker than SWAP.

#### 2.4.2 Enhanced Callback Functionality

The OTA Agent calls back to the application with status updates on the OTA download session. The callback now has the ability to change some aspects of the OTA process, and can stop the OTA session if required.

#### 2.4.3 HTTP OTA Update Image Transfer

The OTA Agent now requests a "range" for each `GET` request when transferring the OTA update image over HTTP, rather than a single `GET` request for the whole file.

### 2.5 New Features in v2.0.0

#### 2.5.1 Job Document

This release adds a message exchange between the device and the Publisher. The JSON-formatted "Job document" contains information where the OTA update image is located. This allows for the Job document to reside in one place, while the OTA update image can reside in another.

For MQTT transport, this allows a Publisher Python script to be subscribed to a known topic (for example, `anycloud/CY8CPROTO_062_4343W/notify_publisher`) to listen for a device request. This allows for an asynchronous update sequence.

For HTTP transport, the device will `GET` the Job document (for example:*ota_update.json*) which contains the same information as if it was served from an MQTT Broker.

#### 2.5.2 Example Update Sequences

Example Using BLE

1. Build the library with one of the following defines:

   1. CY_OTA_BLE_SUPPORT=1
      1. This define will add BLE support to the existing WiFi based transports.
   2. CY_OTA_BLE_ONLY=1
      1. This define will compile out support for WiFi based transports, and only provide support for BLE. This will save 250+k bytes of code space.

2. Use the example Host Application (Windows, Mac, or Linux) from this repo:

   https://github.com/cypresssemiconductorco/btsdk-peer-apps-ota

   Please see README.md in the peer apps repo.

3. Compile and program the device with your test program

4. Start the Peer Application

5. Connect Peer to Device from Peer Application

6. Start OTA transfer

Please check README.md for you application for more information.

#### 2.5.3 Example Update Sequence Using MQTT

1. The Publisher Python script (*libs/anycloud-ota/scripts/publisher.py*) subscribes to a known topic (for example: *anycloud/CY8CPROTO_062_4343W/publish_notify*) on the specified MQTT Broker.

2. The device publishes the message "Update Availability" to the known topic. The message includes information about the device (manufacturer, product, serial number, application version, etc.), and a "Unique Topic Name" for the Publisher to send messages to the device.

3. The Publisher receives the "Update Availability" message and publishes the appropriate Job document on the "Unique Topic Name". The Publisher can make a decision if there is an appropriate update available for the device. The Publisher sends an "Update Available" (or "No Update Available") message, which repeats the device-specific information.

4. The device receives the Job document and connects to the Broker/server given in the Job document to obtain the OTA update image.

5. If the OTA update image is located on an MQTT Broker, the device connects and sends a "Request Update" message to the Publisher, which includes the device information again. The Publisher then splits the OTA update image into 4-kB chunks, adds a header to each chunk, and sends it to the device on the "Unique Topic Name".

6. If the OTA update image is located on an HTTP server, the device will connect to the HTTP server and download the OTA update image using an HTTP `GET` request, for a range of data sequentially until all data is transferred.

#### 2.5.4 Example Update Sequence Using HTTP

1. The device connects to the HTTP server and uses the HTTP `GET` request to obtain the Job document (for example: *ota_update.json*).

2. The device determines whether the OTA update image version is newer than the currently executing application, and the board name is the same, etc.

   - If the OTA update image is accessible through an MQTT Broker, the device creates and subscribes to a unique topic name and sends a "Request Download" message with the "Unique Topic Name" to the Publisher on the known topic `anycloud/CY8CPROTO_062_4343W/notify_publisher`. The Publisher then splits the OTA update image into chunks and publishes them on the unique topic.

   - If the OTA update image is accessible on an HTTP Server, the device connects to the HTTP Server and downloads the OTA update image using an HTTP `GET` request, asking for a range of data sequentially until all data is transferred.

### 3. The Job Document

The Job document is a JSON-formatted file that contains fields used to communicate between the Publisher script and the OTA Agent running on the device.

```
Manufacturer:	Manufacturer Name.
ManufacturerId: Manufacturer Identifier
Product: 		Product Name.
SerialNumber:   The Devices unique Serial Number
Version:		Application version Major.Minor.Build (ex: "12.15.0")
Board:			Name of board used for the product. (ex: "CY8CPROTO_062_4343W")
Connection:		The type of Connection to access the OTA Image. ("MQTT", "HTTP", "HTTPS")
Port:			Port number for accessing the OTA Image. (ex: `MQTT:1883`, HTTP:80`)
```

For MQTT Transport Type:

```
Broker:			 MQTT Broker to access the OTA update image (for example: `mqtt.my_broker.com`)
UniqueTopicName: The "replace" value is just a placeholder. It is replaced with the Unique Topic Name for the OTA data transfer.
```

For HTTP Transport Type:

```
Server:			Server URL (ex: "http://ota_images.my_server.com")
File:			Name of the OTA Image file (ex: "<product>_<board>_v5.6.0.bin")
Offset:			Offset in bytes for the start of data to transfer.
Size:			The size of the chunk of data to send, in bytes.
```

The following is an example Job document for an OTA update image that is available on an MQTT Broker:

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
The following is an example Job document for an OTA update image that is available on an HTTP server:

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

## 4. Setting Up the Supplied Publisher Python Script for MQTT Updates

The *libs/anycloud-ota/scripts/publisher.py* script contains several configurable values. Ensure that the parameters in the script match the application values. In addition, check the example application you are using for any special needs.

The Publisher and Subscriber scripts require an additional Python module.

```
pip install paho-mqtt
```



### 4.1 Running the Publisher Python Script for MQTT Updates

```
cd libs/anycloud-ota/scripts
python publisher.py [tls] [-l] [-f <filepath>] [-b <broker>] [-k <kit>]
                    [-c <company_topic]
```

Usage:

- `tls` - Add this argument to run the Publisher with TLS (secure sockets). You must also supply the appropriate certificates and keys.

- `-l` - Output more logging information to debug connection issues.

- `-f <filepath>` -Use this argument to designate the name of the downloaded file.

- `-b <broker>` - Use this argument to override the default broker.  Note that you will need to add the certificates for TLS usage. The files must reside in the same directory as the *subscriber.py* script.

  - `amazon`

    - Change the `AMAZON_BROKER_ADDRESS` endpoint in the *publisher.py* script.

         ```
         AMAZON_BROKER_ADDRESS = "<endpoint>.iot.us-west-1.amazonaws.com"
         ```

    - Add the following certificate files:

      ```
      ca_certs = "amazon_ca.crt"
      certfile = "amazon_client.crt"
      keyfile  = "amazon_private_key.pem"
      ```

  - `eclipse` ( broker: `mqtt.eclipseprojects.io`)

    - Add the following certificate files:

      ```
      ca_certs = "eclipse_ca.crt"
      certfile = "eclipse_client.crt"
      keyfile  = "eclipse_client.pem"
      ```
  - `mosquitto` (broker: `test.mosquitto.org`)

    - Add the following certificate files:

      ```
      ca_certs = "mosquitto.org.crt"
      certfile = "mosquitto_client.crt"
      keyfile  = "mosquitto_client.key"
      ```

- `-k <kit>` - Use this argument to override the default kit (CY8CPROTO_062_4343W).

  The kit name is used as part of the topic name; it must match the kit you are using.

- `-c <company_topic>` - Add this to the first part of the topic path names.

  - default is "anycloud"

  This must also be mirrored in the application for the topic name. This allows for multiple devices being tested to simultaneously connect to different instances of the  Publisher running on different systems so that they do not interfere with each other.

### 4.2 Subscriber Python Script for MQTT Updates

The *subscriber.py* script is provided as a verification script that acts the same as a device. It can be used to verify that the Publisher is working as expected. Ensure that the `BROKER_ADDRESS` matches the Broker used in *publisher.py*.

#### 4.2.1 Running the Subscriber Python Script for MQTT Updates

```
cd libs/anycloud-ota/scripts
python subscriber.py [tls] [-l] [-b broker] [-k kit] [-f filepath] [-c <chunk_size>]
                     [-e <company_topic]
```

Usage:

- `tls` - Add this argument to run the Publisher with TLS (secure sockets). You must also supply appropriate certificates and keys.

- `-l` - Output more logging information to debug connection issues.

- `-b <broker>` - Use this argument to override the default Broker. Note that you will need to add certificates for TLS usage. The files need to reside in the same directory as *publisher.py*.

  - `amazon`

    - Change the `AMAZON_BROKER_ADDRESS` endpoint in the *subscriber.py* script.

      ```
      AMAZON_BROKER_ADDRESS = "<endpoint>.iot.us-west-1.amazonaws.com"
      ```

    - Add the following certificate files:

      ```
      ca_certs = "amazon_ca.crt"
      certfile = "amazon_client.crt"
      keyfile  = "amazon_private_key.pem"
      ```

  - `eclipse`  (Broker: `mqtt.eclipseprojects.io`)

    - Add the following certificate files:
      ```
      ca_certs = "eclipse_ca.crt"
      certfile = "eclipse_client.crt"
      keyfile  = "eclipse_client.pem"
      ```

  - `mosquitto`  (Broker: `test.mosquitto.org`)

    - Add the following certificate files:

      ```
      ca_certs = "mosquitto.org.crt"
      certfile = "mosquitto_client.crt"
      keyfile  = "mosquitto_client.key"
      ```

- `-k kit` - Use this argument to override the default kit (CY8CPROTO_062_4343W).

  The kit name is used as part of the topic name; it must match the kit that the application was built with.

- `-f filepath` - Use this argument to designate the output file for download.

- `-c <chunk_size>` - The size in bytes of the chunk to request (used for testing).

- `-e <company_topic>` - Add this to the first part of the topic path names.

  - default is "anycloud"

  This must also be mirrored in the application for the topic name. This allows for multiple devices being tested to simultaneously connect to different instances of the Publisher running on different systems so that they do not interfere with each other.

## 5. Migrating OTA Version

### 5.1 Migrating from OTA 4.x to OTA 5.x

The OTA 5.x release has support for 2 new platforms. Please refer to section `16. Prepare for Building Your OTA Application` after following this section.

#### 5.1.1 General Changes

- Use of MCUBoot v1.8.1-cypress
  - No longer using MCUBoot v1.7.2-cypress
  - Please see section **14 MCUBoot Instructions** below for access and migration changes.
- Support for 2 new platforms
  - CYW920829M2EVB-01
    - BT Only, GCC only support
    - Uses external flash only
  - CY8CPROTO-062S3-4343W
    - 1M internal flash part
    - BT Only
    - Uses internal flash only
- [OTA API reference guide](https://cypresssemiconductorco.github.io/anycloud-ota/api_reference_manual/html/index.html)

#### 5.1.2 Changes due to underlying library changes

- Coding Changes

  - If you use the command console utility, make sure you add the thread_priority field in the configuration structure.

        + console_cfg.thread_priority    = CY_RTOS_PRIORITY_LOW;
        cy_command_console_init(&console_cfg);

#### 5.1.3 Required Application Changes

- Serial Flash Interface changes (external flash)

  To support new targets, we have combined serial flash access. This required a new interface.

  - Initialization

    - replace

      ```
      - #include "cy_smif_psoc6.h"

      + #include "ota_serial_flash.h"
      ```



    - and

      ```
      - if (psoc6_qspi_init() != 0)

      + if (ota_smif_initialize() != CY_RSLT_SUCCESS)
      ```



- UART changes for CYW920829M2EVB-01

  - UART Rx and Tx pins are different for CYW920829M2EVB-01

    ```
    #if defined (CYW20829A0LKML)
    #if !defined (CYBSP_DEBUG_UART_CTS)
    #define CYBSP_DEBUG_UART_CTS  (P4_0)
    #endif

    #if !defined (CYBSP_DEBUG_UART_RTS)
    #define CYBSP_DEBUG_UART_RTS  (P3_1)
    #endif
    #endif
    ```

  - After cy_retarget_io_init() call

    ```
    #if defined (CYW20829A0LKML)
        {
            /*
             * Workaround for 20829 UART RX issue. See PROGTOOLS-3415.
             */

            cyhal_gpio_init((cyhal_gpio_t)CYBSP_DEBUG_UART_RTS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
        }

    #endif
    ```

#### 5.1.4 Makefile changes

##### 5.1.4.1 Linker Script Argument changes

Now using CY_BOOT_PRIMARY_1_START instead of MCUBOOT_BOOTLOADER_SIZE for GCC_ARM builds. For IAR and ARM, we need both for this release.

​	From:

```
ifeq ($(TOOLCHAIN),GCC_ARM)
    LDFLAGS+="-Wl,--defsym,MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE),--defsym,MCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE),--defsym,CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)"
else
ifeq ($(TOOLCHAIN),IAR)
    LDFLAGS+=--config_def MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) --config_def MCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) --config_def CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)
else
ifeq ($(TOOLCHAIN),ARM)
    LDFLAGS+=--pd=-DMCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) --pd=-DMCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) --pd=-DCY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)
endif #ARM
endif #IAR
endif #GCC_ARM
```

To:

    ifeq ($(TOOLCHAIN),GCC_ARM)
        LDFLAGS+="-Wl,--defsym,MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE),--defsym,CY_BOOT_PRIMARY_1_START=$(CY_BOOT_PRIMARY_1_START),--defsym,CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)"
    else
    ifeq ($(TOOLCHAIN),IAR)
        LDFLAGS+=--config_def MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) --config_def MCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) --config_def CY_BOOT_PRIMARY_1_START=$(CY_BOOT_PRIMARY_1_START) --config_def CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)
    else
    ifeq ($(TOOLCHAIN),ARM)
        LDFLAGS+=--pd=-DMCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) --pd=-DMCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) --pd=-DCY_BOOT_PRIMARY_1_START=$(CY_BOOT_PRIMARY_1_START) --pd=-DCY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)
    endif #ARM
    endif #IAR
    endif #GCC_ARM


##### 5.1.4.2 Linker Script Location Addition

We have a different linker script for the CYW920829M2EVB-01 target, which will also be used for other target builds in the future.

From:

```
LINKER_SCRIPT_WILDCARD:=libs/anycloud-ota/$(TARGET_UNDERSCORE)/COMPONENT_$(CORE)/TOOLCHAIN_$(TOOLCHAIN)/ota/*_ota_int.$(CY_TOOLCHAIN_LS_EXT)

```

To:

```
ifeq ($(TARGET), CYW920829M2EVB-01)
    LINKER_SCRIPT_WILDCARD:=./$(TARGET_UNDERSCORE)/COMPONENT_$(CORE)/TOOLCHAIN_$(TOOLCHAIN)/ota/*_ota_ext.$(CY_TOOLCHAIN_LS_EXT)
else
    LINKER_SCRIPT_WILDCARD:=./$(TARGET_UNDERSCORE)/COMPONENT_$(CORE)/TOOLCHAIN_$(TOOLCHAIN)/ota/*_ota_int.$(CY_TOOLCHAIN_LS_EXT)
endif

```

##### 5.1.4.3 MCUBoot Support Code Location In Anycloud-OTA Library

Adding the SMIF support for CYW920829M2EVB-01, which is different from PSoC™ 6 targets, required separate directories in the library.

From:

```
MCUBOOT_DIR=./source/mcuboot
```

To:

```
ifeq ($(TARGET), CYW920829M2EVB-01)
    MCUBOOT_DIR=./source/COMPONENT_OTA_MCUBOOT_20829/mcuboot
else
    MCUBOOT_DIR=./source/COMPONENT_OTA_MCUBOOT_PSOC/mcuboot
endif
```

##### 5.1.4.4 Post Build changes

```
ifeq ($(TARGET), CYW920829M2EVB-01)
    MCUBOOT_SCRIPT_FILE_DIR=$(MCUBOOT_DIR)/scripts
    MCUBOOT_SCRIPT_FILE_PATH?=./scripts/mcuboot_script_20829.bash
    MCUBOOT_KEY_DIR=$(MCUBOOT_DIR)/keys
    MCUBOOT_KEY_FILE=cypress-test-ec-p256.pem
    IMGTOOL_SCRIPT_NAME=imgtool_v1.7.0/imgtool.py
    CY_ELF_TO_HEX=$(CY_HEX_TO_BIN)

    # Necessary for external flash access
    CY_TOC2_GENERATOR = ./libs/recipe-make-cat1b/make/scripts/20829/run_toc2_generator.sh

    ifeq ($(TOOLCHAIN),ARM)
        POSTBUILD=$(CY_CROSSPATH)/bin/fromelf "$(CY_CONFIG_DIR)/$(APPNAME).elf" --bin --output="$(CY_CONFIG_DIR)/$(APPNAME).bin";
    else
    ifeq ($(TOOLCHAIN),IAR)
        ifeq ($(APPTYPE),flash)
            OTA_POSTBUILD_PARAM=--bin-multi
        else
            OTA_POSTBUILD_PARAM=--bin
        endif
        POSTBUILD="$(CY_CROSSPATH)/bin/ielftool" "$(CY_CONFIG_DIR)/$(APPNAME).elf" $(CY_BSP_POSTBUILD_PARAM) "$(CY_CONFIG_DIR)/$(APPNAME).bin";
    else
    ifeq ($(TOOLCHAIN),GCC_ARM)
        POSTBUILD="$(CY_TOOLCHAIN_ELF2BIN)" "$(CY_CONFIG_DIR)/$(APPNAME).elf" -S -O binary "$(CY_CONFIG_DIR)/$(APPNAME).bin";
    endif # GCC_ARM
    endif # IAR
    endif # ARM

    POSTBUILD+=$(MCUBOOT_SCRIPT_FILE_PATH) $(CY_CROSSPATH) $(CY_PYTHON_PATH) $(OUTPUT_FILE_PATH) $(APPNAME) \
               $(CY_ELF_TO_HEX) $(CY_ELF_TO_HEX_OPTIONS) $(CY_ELF_TO_HEX_FILE_ORDER) $(CY_HEX_TO_BIN) \
               $(MCUBOOT_SCRIPT_FILE_DIR) $(IMGTOOL_SCRIPT_NAME) $(CY_FLASH_ERASE_VALUE) $(MCUBOOT_HEADER_SIZE) \
               $(CY_BUILD_VERSION) $(CY_BOOT_PRIMARY_1_START) $(CY_BOOT_PRIMARY_1_SIZE) $(CY_BOOT_SECONDARY_1_START) \
               $(CY_TOC2_GENERATOR) $(CY_LCS) $(APPTYPE) $(MCUBOOT_KEY_DIR) $(MCUBOOT_KEY_FILE) $(SMIF_CRYPTO_CONFIG);
    POSTBUILD+=rm -rf $(CY_CONFIG_DIR)/$(APPNAME).bin;

else
    # NON-CYW920829M2EVB-01
    # signing scripts and keys from MCUBoot
    # Defaults for 062 non-secure boards
    MCUBOOT_SCRIPT_FILE_DIR=$(MCUBOOT_DIR)/scripts
    SIGN_SCRIPT_FILE_PATH=./scripts/sign_script.bash
    MCUBOOT_KEY_DIR=$(MCUBOOT_DIR)/keys
    MCUBOOT_KEY_FILE=$(MCUBOOT_KEY_DIR)/cypress-test-ec-p256.pem
    IMGTOOL_SCRIPT_NAME=imgtool_v1.7.0/imgtool.py
    IMGTOOL_COMMAND_ARG=sign
    CY_SIGNING_KEY_ARG="-k $(MCUBOOT_KEY_FILE)"

    ifeq ($(TARGET),CY8CKIT-064B0S2-4343W)
        # values changed for 064B0S2 board
        IMGTOOL_COMMAND_ARG=do_not_sign
    endif
endif
```



### 5.2 Migrating from OTA 3.x to OTA 4.x

The OTA 4.x release has changes for supporting Bluetooth® transport.

Changes:

- Use of MCUBoot v1.7.2-cypress

- Support for ARM C compiler
  - All 062 based boards, WiFi and Bluetooth® support
  - CY8CKIT-064B0S2-4343W board, WiFi support

- Use of COMPONENT system for specifying transport support, allowing for smaller code and RAM footprint when supporting individual transports.
  - COMPONENTS+=OTA_HTTP
  - COMPONENTS+=OTA_MQTT
  - COMPONENTS+=OTA_BLUETOOTH
    - For secure Bluetooth®, add this define to the build.
      - DEFINES+=CY_OTA_BLE_SECURE_SUPPORT=1

- Exposed defines for customer over-rides

  - CY_OTA_HTTP_TIMEOUT_SEND
    - Time to wait for response after sending HTTP message before timing out.
  - CY_OTA_HTTP_TIMEOUT_RECEIVE
    - Time to wait for reply from HTTP request.

- New functions have been added to the API. Please refer to the API reference.

  - [OTA API reference guide](https://cypresssemiconductorco.github.io/anycloud-ota/api_reference_manual/html/index.html)



### 5.3 Migrating from OTA 2.x to OTA 3.x

The OTA 3.x release has new structure defines because of the changes in mqtt and http-client libraries.

Changes:

- `cy_ota_server_info_t`  - deprecated
  - New structure is `cy_awsport_server_info_t`, which has the same fields as the previous struct.
- `IotNetworkCredentials_t`  - deprecated
  - New structure is `cy_awsport_ssl_credentials_t`
- New fields in `cy_ota_cb_struct_t`
  - `cy_http_client_t` and `cy_mqtt_t`
- New field in cy_ota_agent_params_t
  - `do_not_send_result`

New field in `cy_ota_agent_params_t` :

- `do_not_send_result` - For Job Flow updates, the default is for a Result JSON is sent to the initial connection (Published to MQTT broker or "POST" to HTTP/HTTPS server). To disable this functionality, set this field to "1".

### 5.3 Migrating from OTA 1.X to OTA 2.X

You need to change the following when migrating from v1.x to v2.x:

- OTA 2.x no longer uses MCUboot as a library.
  - Verify that *mcuboot.lib* does not appear in the *deps* folder for your project. If present, delete the file.

  - Delete the *libs/mcuboot* folder.

- The structure used when calling `cy_ota_agent_start()` is the same, but some content has changed:

  - `cy_ota_network_params_t`

    Connection and credentials are accepted for both MQTT and HTTP connections so that if a Job document redirects from MQTT to HTTP, the OTA Agent will have the TLS credentials for both servers.

    - The following are deprecated:

      - `transport` - This directed the OTA Agent to use the  a transport type (MQTT or HTTP) for the entire OTA transfer.

      - `u` - This was a union of the MQTT and HTTP server and credential information. Because both are accepted now, the union was dissolved.

      - `server` - This has been moved to the individual MQTT and HTTP information structures.

      - `credentials` - This has been moved to the individual MQTT and HTTP information structures.

    - New:

      - `initial_connection` - This directs the OTA Agent to use a transport type as the first transport to get the Job document. The Job document may redirect to a different type of transport for downloading the data.

      - `mqtt` and `http` - These are now separated and provide information about the connections.

      - `use_get_job_flow` - This allows the application to determine whether a Direct download is required or the new Job Document Flow is preferred.

- The application callback has been expanded to provide more information to the application and allow for the application to modify some of the fields of the information.

  The new structure contains the similar "reason" for the callback, and more. See the documentation.

## 6. Minimum Requirements

- [ModusToolbox® software](https://www.cypress.com/products/modustoolbox-software-environment) v2.3.1 or higher

- [MCUboot](https://juullabs-oss.github.io/mcuboot/) v1.8.1

- Programming Language: C

- [Python 3.7 or higher](https://www.python.org/downloads//) (for signing and the Publisher script).

  Run *pip* to install the modules.

  ```
  pip install -r <repo root>/libs/anycloud-ota/source/mcuboot/scripts/requirements.txt
  ```



## 7. Note on Using Windows 10

When using ModusToolbox, you will need to install the pip requirements to Python in the ModusToolbox installation.

```
<ModusToolbox>/tools_2.3/python-3.7.155/python -m pip install -r <repo root>/libs/anycloud-ota/source/mcuboot/scripts/requirements.txt
```

## 8. Supported Toolchains

Please refer to RELEASE.md for latest information.

- GCC
- IAR
- ARM C

## 9. Supported OS

- FreeRTOS

## 10. Supported Kits

- [PSoC™ 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W)
- [PSoC™ 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)
- [PSoC™ 62S3 Wi-Fi BT Prototyping Kit ](https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062s3-4343w/)(CY8CPROTO-062S3-4343W)
- [PSoC™ 64 Secure Boot Wi-Fi BT Pioneer Kit](https://www.cypress.com/documentation/development-kitsboards/psoc-64-secure-boot-wi-fi-bt-pioneer-kit-cy8ckit-064b0s2-4343w) (CY8CKIT-064B0S2-4343W)
- [CY8CEVAL-062S2 Evaluation Kit][https://www.cypress.com/part/cy8ceval-062s2] (CY8CEVAL-062S2-LAI-4373M2 and CY8CEVAL-062S2-MUR-43439M2)
- CYW920829M2EVB-01

You will need to change the flash locations and sizes for the bootloader, primary slot (Slot 0) and secondary slot (Slot 1). See the Prepare MCUboot section below.

## 11. Hardware Setup

This example uses the board's default configuration. See the kit user guide to ensure the board is configured correctly.

**Note:** Before using this code example, make sure that the board is upgraded to KitProg3. The tool and instructions are available in the [Firmware Loader](https://github.com/cypresssemiconductorco/Firmware-loader) GitHub repository. If you do not upgrade, you will see an error like "unable to find CMSIS-DAP device" or "KitProg firmware is out of date".

## 12. Software Setup

1. Install a terminal emulator if you don't have one. Instructions in this document use [Tera Term](https://ttssh2.osdn.jp/index.html.en).

2. Python Interpreter. The supplied *publisher.py* script is tested with [Python 3.8.1](https://www.python.org/downloads/release/python-381/).

## 13. Enabling Debug Output

### 13.1 OTA-v2.2.0 and higher

Starting with OTA v2.2.0, the *cy_log* facility is used to enable more levels of logging. To use the logging features, you need to call the initialization function. After that, you can adjust the level of logging for various "facilities".

At the minimum, you can initialize the logging system by including the header file, and calling the initialization function with a logging level. The other arguments are for callbacks to allow the application to handle the actual output (this enables you to log to an alternate destination), and provide a time stamp. See the *cy_log.h* file for the log levels. For OTA logging, use the `CYLF_OTA` facility.

```
#include "cy_log.h"

   cy_log_init(CY_LOG_WARNING, NULL, NULL);
```

### 13.2 Pre OTA-v2.0.1

In *libs/anycloud-ota/include/cy_ota_api.h*, define `LIBRARY_LOG_LEVEL` to one of these defines, in the following order, from no debug output to maximum debug output.

  - `IOT_LOG_NONE`
  - `IOT_LOG_ERROR`
  - `IOT_LOG_WARN`
  - `IOT_LOG_INFO`
  - `IOT_LOG_DEBUG`

## 14 MCUBoot Instructions

### 14.1 Clone MCUboot

The CY8CKIT-064B0S2-4343W is a special kit that uses cysecureboot. See "cysecuretools for CY8CKIT-064B0S2-4343W" information below.

You need to first build and program the bootloader app *MCUBootApp* that is available in the MCUboot GitHub repo before programming this OTA app. The bootloader app is run by the CM0+ CPU while any OTA application is run by CM4. Clone the MCUboot repository onto your local machine, **outside of your application directory.**

1. Run the following command:

   ```
   git clone https://github.com/JuulLabs-OSS/mcuboot.git
   ```

2. Open a CLI terminal and navigate to the cloned *mcuboot* folder:
   ```
   cd mcuboot
   ```

3. Change the branch to get the Cypress version:

   ```
   git checkout v1.8.1-cypress
   ```

4. Pull in MCUboot sub-modules to build mcuboot:

   ```
   git submodule update --init --recursive
   ```

5. Install the required Python packages mentioned in *mcuboot/scripts/requirements.txt*:
   ```
   cd mcuboot/scripts

   pip install -r requirements.txt
   ```

### 14.2 Build MCUboot

MCUBootApp now supports using a JSON document to define the flash regions used for updating the application.

Read **"libs/anycloud-ota/configs/flashmap/MCUBoot_Build_Commands.md"** to build MCUBoot.

CyBootloader is used for CYW920829M2EVB-01 and PSoC™ 6 MCU targets, please also see section **15. cysecuretools for CY8CKIT-064B0S2-4343W**

NOTES:

- The addresses in the JSON file are ABSOLUTE addresses. In OTA v5.0.0 we use RELATIVE addresses.

- Take the ABSOLUTE addresses in the JSON file and set the upper 2 hex digits to 0x00 to get the RELATIVE address.

  ```
  ex: 0x18028000 becomes: 0x00028000
  ```



- Watch the MCUBootApp build output. If the python script flashmap.py detects errors in the flash layout, it will report them to stderr and stop the build.

- The naming conventions in the JSON files are different from the actual DEFINES used in the code, here’s a quick reference:



| JSON File                 | Description                          | Makefile DEFINE           | Notes                                  |
| ------------------------- | ------------------------------------ | ------------------------- | -------------------------------------- |
| "bootloader" "address"    | Bootloader start address             |                           |                                        |
| "size"                    | Bootloader size                      | MCUBOOT_BOOTLOADER_SIZE   |                                        |
| "scratch_address"         | Swap/Overwrite scratch start address | CY_BOOT_SCRATCH_START     | Used during upgrade process by MCUBoot |
| "scratch_size"            | Swap/Overwrite scratch size          | CY_BOOT_SCRATCH_SIZE      |                                        |
| "status_address"          | Swap/Overwrite status start address  | CY_BOOT_STATUS_START      | Used during upgrade process by MCUBoot |
| "status_size"             | Swap/Overwrite status size           | CY_BOOT_STATUS_SIZE       |                                        |
|                           |                                      |                           |                                        |
| “application_1” “address” | Main application start address       | CY_BOOT_PRIMARY_1_START   |                                        |
| "size"                    | Main application size                | CY_BOOT_PRIMARY_1_SIZE    |                                        |
| “upgrade_address”         | Main application upgrade address     | CY_BOOT_SECONDARY_1_START |                                        |
| “upgrade_size”            | Main application upgrade size        | CY_BOOT_SECONDARY_1_SIZE  | Must match CY_BOOT_PRIMARY_1_SIZE      |
|                           |                                      |                           |                                        |
| “application_2” “address” | Main application start address       | CY_BOOT_PRIMARY_2_START   |                                        |
| "size"                    | Main application size                | CY_BOOT_PRIMARY_2_SIZE    |                                        |
| “upgrade_address”         | Main application upgrade address     | CY_BOOT_SECONDARY_2_START |                                        |
| “upgrade_size”            | Main application upgrade size        | CY_BOOT_SECONDARY_2_SIZE  | Must match CY_BOOT_PRIMARY_2_SIZE      |



### 14.3 Program MCUBootApp

1. Connect the board to your PC using the provided USB cable through the USB connector.
2. Program the board using the instructions in your Customer Example Application notes.

## 15. cysecuretools for CY8CKIT-064B0S2-4343W

For the CY8CKIT-062B0S2-4343W, we use a procedure called "provisioning" to put the CyBootloader into the device. Please refer to the board instructions for this procedure.

[PSoC™ 64 Secure Boot Wi-Fi BT Pioneer Kit](https://www.cypress.com/documentation/development-kitsboards/psoc-64-secure-boot-wi-fi-bt-pioneer-kit-cy8ckit-064b0s2-4343w) (CY8CKIT-064B0S2-4343W)

## 16. Prepare for Building Your OTA Application

Copy *libs/anycloud-ota/configs/cy_ota_config.h* to your application's top-level directory, and adjust for your application needs.

   ```
   cp libs/anycloud-ota/configs/cy_ota_config.h <application dir>
   cp libs/anycloud-ota/configs/mbedtls_user_config.h <application dir>
   ```

### 16.1 Defines Used With The anycloud-ota Library

There are many defines for various functionality in the anycloud-ota library that are documented in the configuration file, libs/anycloud-ota/configs/cy_ota_config.h.

Typically, these are defines are included in the Makefile of the application as:

```
DEFINES+=<define_name>[=<define_value>]
```

You must also consult the README.md file for the example application.

#### 16.1.1 Platform Specific Defines

##### PSoC™ 6 MCU Target Internal Flash Sizes

| Target                     | Internal Flash Size | Define        | Notes                                                        |
| -------------------------- | ------------------- | ------------- | ------------------------------------------------------------ |
| CY8CPROTO-062-4343W        | 2M bytes            | PSOC_062_2M   | Used in anycloud-ota library file flash_qspi.c for pin definition |
| CY8CKIT-062S2-43012        | 2M bytes            | PSOC_062_2M   | Used in anycloud-ota library file flash_qspi.c for pin definition |
| CY8CKIT-064B0S2-4343W      | 2M bytes            | PSOC_062_2M   | Used in anycloud-ota library file flash_qspi.c for pin definition |
| CY8CEVAL-062S2-LAI-4373M2  | 2M bytes            | PSOC_062_2M   | Used in anycloud-ota library file flash_qspi.c for pin definition |
| CY8CEVAL-062S2-MUR-43439M2 | 2M bytes            | PSOC_062_2M   | Used in anycloud-ota library file flash_qspi.c for pin definition |
| CY8CPROTO-062S3-4343W      | 512K bytes          | PSOC_062_512K | Used in anycloud-ota library file flash_qspi.c for pin definition |

#### 16.1.2 Flash Related Code Defines

| Define                     | Value | Notes                                    |
| -------------------------- | ----- | ---------------------------------------- |
| CY_BOOT_USE_EXTERNAL_FLASH | 1     | Used in  aws-iot-device-sdk-port library |



#### 16.1.3 Flash Layout Defines

| Define                    | Description                                                  |
| ------------------------- | ------------------------------------------------------------ |
| CY_BOOT_PRIMARY_1_START   | Start of Application offset in internal flash.<br />(MCUBootApp uses 0x00000000 -0x00017FFF )<br />Referred to in MCUBoot JSON file as "application_1" "address". |
| CY_BOOT_PRIMARY_1_SIZE    | Size of Application + Radio Firmware + MCUBootApp header and trailer<br />Referred to in MCUBoot JSON file as "application_1" "size". |
| CY_BOOT_SECONDARY_1_START | Offset of Download area for Application in external flash<br />Referred to in MCUBoot JSON file as "application_1" "upgrade_address". |
| CY_BOOT_SECONDARY_1_SIZE  | Size of Application + Radio Firmware + MCUBootApp header and trailer<br />Referred to in MCUBoot JSON file as "application_1" "upgrade_size".<br />Must be the same as CY_BOOT_PRIMARY_1_SIZE |
| CY_BOOT_SCRATCH_START     | This is not used by anycloud-ota, must be set aside for MCUBootApp SWAP operation.<br />Not used for MCUBootApp OVERWRITE operation. |
| CY_BOOT_SCRATCH_SIZE      | This is not used by anycloud-ota, must be set aside for MCUBootApp SWAP operation.<br />Not used for MCUBootApp OVERWRITE operation. |
| CY_BOOT_STATUS_START      | This is not used by anycloud-ota, must be set aside for MCUBootApp SWAP operation.<br />Not used for MCUBootApp OVERWRITE operation. |
| CY_BOOT_STATUS_SIZE       | This is not used by anycloud-ota, must be set aside for MCUBootApp SWAP operation.<br />Not used for MCUBootApp OVERWRITE operation. |
|                           |                                                              |

##### 16.1.3.1 CY8CPROTO-062-4343W Using External Flash

| Define                    | Value      | Notes                                       |
| ------------------------- | ---------- | ------------------------------------------- |
| CY_BOOT_PRIMARY_1_START   | 0x00018000 | Offset from internal flash base             |
| CY_BOOT_PRIMARY_1_SIZE    | 0x001C0000 |                                             |
| CY_BOOT_SECONDARY_1_START | 0x00000200 | Offset from external flash base             |
| CY_BOOT_SECONDARY_1_SIZE  | 0x001C0000 | Must be same size as CY_BOOT_PRIMARY_1_SIZE |
| CY_BOOT_SCRATCH_START     | 0x00440000 | Offset from external flash base             |
| CY_BOOT_SCRATCH_SIZE      | 0x00080000 |                                             |
| CY_BOOT_STATUS_START      | 0x00070200 | Offset from internal flash base             |
| CY_BOOT_STATUS_SIZE       | 0x00006C00 |                                             |

##### 16.1.3.2 CY8CPROTO-062-4343W Using Internal Flash

| Define                    | Value      | Notes                                       |
| ------------------------- | ---------- | ------------------------------------------- |
| CY_BOOT_PRIMARY_1_START   | 0x00018000 | Offset from internal flash base             |
| CY_BOOT_PRIMARY_1_SIZE    | 0x000EE000 |                                             |
| CY_BOOT_SECONDARY_1_START | 0x00106000 | Offset from internal flash base             |
| CY_BOOT_SECONDARY_1_SIZE  | 0x000EE000 | Must be same size as CY_BOOT_PRIMARY_1_SIZE |
| CY_BOOT_SCRATCH_START     | 0x001f7c00 | Offset from external flash base             |
| CY_BOOT_SCRATCH_SIZE      | 0x00001000 |                                             |
| CY_BOOT_STATUS_START      | 0x001f4000 | Offset from internal flash base             |
| CY_BOOT_STATUS_SIZE       | 0x00003C00 |                                             |

##### 16.1.3.3 CY8CPROTO-062S3-4343W Using Internal Flash

This is a 512k part, uses external flash for upgrade and scratch area.

| Define                    | Value      | Notes                                       |
| ------------------------- | ---------- | ------------------------------------------- |
| CY_BOOT_PRIMARY_1_START   | 0x00018000 | Offset from internal flash base             |
| CY_BOOT_PRIMARY_1_SIZE    | 0x00058200 |                                             |
| CY_BOOT_SECONDARY_1_START | 0x00028000 | Offset from external flash base             |
| CY_BOOT_SECONDARY_1_SIZE  | 0x00058200 | Must be same size as CY_BOOT_PRIMARY_1_SIZE |
| CY_BOOT_SCRATCH_START     | 0x00440000 | Offset from external flash base             |
| CY_BOOT_SCRATCH_SIZE      | 0x00080000 |                                             |
| CY_BOOT_STATUS_START      | 0x00070200 | Offset from internal flash base             |
| CY_BOOT_STATUS_SIZE       | 0x00003C00 |                                             |

##### 16.1.3.4 CYW920829M2EVB-01 Using External Flash

This platform only has external flash.

| Define                    | Value      | Notes                                       |
| ------------------------- | ---------- | ------------------------------------------- |
| CY_BOOT_PRIMARY_1_START   | 0x00020000 | Offset from external flash base             |
| CY_BOOT_PRIMARY_1_SIZE    | 0x00060000 |                                             |
| CY_BOOT_SECONDARY_1_START | 0x00080000 | Offset from external flash base             |
| CY_BOOT_SECONDARY_1_SIZE  | 0x00060000 | Must be same size as CY_BOOT_PRIMARY_1_SIZE |
| CY_BOOT_SCRATCH_START     | 0x000EC000 | Offset from external flash base             |
| CY_BOOT_SCRATCH_SIZE      | 0x00002000 |                                             |
| CY_BOOT_STATUS_START      | 0x000E0000 | Offset from external flash base             |
| CY_BOOT_STATUS_SIZE       | 0x0000C000 |                                             |

#### 16.1.4 HTTP and MQTT Related Defines

| Define                                            | Notes                                                        |
| ------------------------------------------------- | ------------------------------------------------------------ |
| There are numerous HTTP and MQTT related defines. | Please use cy_ota_config.h in your project to adjust these defines. |



#### 16.1.5 BlueTooth® Related Defines

| Define                    | Notes                                                        |
| ------------------------- | ------------------------------------------------------------ |
| CY_OTA_BLE_SECURE_SUPPORT | Setting CY_OTA_BLE_SECURE_SUPPORT=1 enables a secure BlueTooth® connection. You must also adjust the BlueTooth® configuration for your application using bt-configurator.exe. |

#### 16.1.7 Other Defines

| Define                                | Platform              | Value | Notes                                                        |
| ------------------------------------- | --------------------- | ----- | ------------------------------------------------------------ |
| CYBSP_WIFI_CAPABLE                    | All PSoC™ 6 MCUs      | 1     | Used in TARGET_XXX libraries                                 |
| CY_SD_HOST_CLK_RAMP_UP_TIME_MS_WAKEUP | All PSoC™ 6 MCUs      | 0     | Used in mtb-hal-cat1 library                                 |
| CY_WIFI_HOST_WAKE_SW_FORCE            | CY8CPROTO-062-4343W   | 0     | Used inwhd-bsp-integration                                   |
| CYHAL_DISABLE_WEAK_FUNC_IMPL          | CY8CPROTO-062S3-4343W | 1     | Used in mtb-hal-cat1 library when executing code in place from external flash (XIP - eXecute In Place) |
| OTA_SUPPORT                           | All                   | 1     | Used in recipe-make-cat1a, needed for defining Python usage for POST build processing |




## 17. Limitations

1. If not using a Job document with MQTT, and the device is not subscribed to the topic on the MQTT Broker when the Publisher sends the update, it will miss the update.

  As the workaround, use a Job document, which has information about where the OTA update image is located.

2. Internal and external flash is supported. You can use the internal flash only in certain combinations with platforms that have 2M of internal flash. The size of the application is greatly limited if using internal flash only.

3. Ensure that you have a reliable network connection before starting an OTA update. If your network connection is poor, OTA update may fail due to lost packets or a lost connection.

4. Ensure that you have a fully charged device before starting an OTA update. If you device's battery is low, OTA may fail.

## 18. Additional Information

- [OTA RELEASE.md]()
- [OTA API reference guide](https://cypresssemiconductorco.github.io/anycloud-ota/api_reference_manual/html/index.html)
- Cypress ModusToolbox OTA Examples
  - [Cypress OTA MQTT Example](https://github.com/cypresssemiconductorco/mtb-example-anycloud-ota-mqtt )
  - [Cypress OTA HTTP Example](https://github.com/cypresssemiconductorco/mtb-example-anycloud-ota-http )
  - [Cypress OTA Bluetooth® Example ](https://github.com/cypresssemiconductorco/mtb-example-anycloud-ble-battery-server)
- [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)
-  [MCUboot](https://github.com/JuulLabs-OSS/mcuboot/blob/cypress/docs/design.md) documentation

Infineon also provides a wealth of data at www.infineon.com to help you select the right device, and quickly and effectively integrate it into your design.

For PSoC™ 6 MCU devices, see [How to Design with PSoC 6 MCU - KBA223067](https://community.cypress.com/docs/DOC-14644) in the Infineon community.

## 19. Document History

| Document Version | Description of Change                                      |
| ---------------- | ---------------------------------------------------------- |
| 1.4.5            | Update Migration guide for OTA v4.x to 5.x                 |
| 1.4.4            | Update for OTA v5.0.0 and CyBootloader v1.8.1              |
| 1.4.3            | Update publisher.py and subscriber.py documentation        |
| 1.4.2            | Update Copyright info                                      |
| 1.4.1            | Updated for OTA v4.0.0 BLE support and CyBootloader v1.7.2 |
| 1.4.0            | Updated for OTA v3.0.0                                     |
| 1.3.0            | Updated for new logging                                    |
| 1.2.0            | Updated for version 2.0.0 features                         |
| 1.1.0            | Updated for HTTP support                                   |
| 1.0.1            | Documentation updates                                      |
| 1.0.0            | New middleware library                                     |

------

© 2022, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
This software, associated documentation and materials ("Software") is owned by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and is protected by and subject to worldwide patent protection (United States and foreign), United States copyright laws and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software ("EULA"). If no EULA applies, then any reproduction, modification, translation, compilation, or representation of this Software is prohibited without the express written permission of Cypress.
Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the Software without notice. Cypress does not assume any liability arising out of the application or use of the Software or any product or circuit described in the Software. Cypress does not authorize its products for use in any products where a malfunction or failure of the Cypress product may reasonably be expected to result in significant property damage, injury or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the manufacturer of such system or application assumes all risk of such use and in doing so agrees to indemnify Cypress against all liability.


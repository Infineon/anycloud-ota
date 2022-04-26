### MCUBoot Build Commands

The JSON flash maps were verified to work with MCUBoot v1.8.1-cypress.

Choose the configuration to use and copy the flash map file to your mcuboot/boot/cypress directory.

#### PSoC 062 2M Internal Flash Platforms
- CY8CKIT-062S2-43012
- CY8CPROTO-062-4343W
- CY8CEVAL-062S2-MUR-43439M2
- CY8CEVAL-062S2-LAI-4373M2
- CY8CEVAL-062S2-CYW943439M2IPA1

There are flash maps for the upgrade slot in internal flash and in external flash. There are also versions for SWAP and OVERWRITE upgrade methods.

##### Building MCUBoot

To build MCUBoot for internal flash SWAP configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M FLASH_MAP=./psoc62_int_swap_single.json
```

To build MCUBoot for internal flash OVERWRITE configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M FLASH_MAP=./psoc62_int_overwrite_single.json
```

To build MCUBoot for external flash SWAP configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M FLASH_MAP=./psoc62_ext_swap_single.json
```

To build MCUBoot for external flash OVERWRITE configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M FLASH_MAP=./psoc62_ext_overwrite_single.json
```

#### CY8CPROTO-062S3-4343W Platform

The CY8CPROTO-062S3-4343W platform only supports an internal flash configuration at this time.

To build MCUBoot for internal flash SWAP configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_512K USE_CUSTOM_DEBUG_UART=1 FLASH_MAP=./psoc62s3_int_swap_single.json
```

To build MCUBoot for internal flash OVERWRITE configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_512K USE_CUSTOM_DEBUG_UART=1 FLASH_MAP=./psoc62s3_int_overwrite_single.json
```

#### CYW20829 Platform


The CYW920829M2EVB-01 platform only supports an external flash configuration.

To build MCUBoot for external flash SWAP configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=CYW20829 USE_CUSTOM_DEBUG_UART=1 USE_EXTERNAL_FLASH=1 USE_XIP=1 FLASH_MAP=./cyw20829_ext_swap_single.json
```

To build MCUBoot for external flash OVERWRITE configuration:

```
make clean app APP_NAME=MCUBootApp PLATFORM=CYW20829 USE_CUSTOM_DEBUG_UART=1 USE_EXTERNAL_FLASH=1 USE_XIP=1 FLASH_MAP=./cyw20829_ext_overwrite_single.json
```

#!/bin/bash
#
# This file is used by make to create the build commands to sign an OTA image
#
# Modify at your peril !
#
# break immediately on errors
set -e

#
# Arguments
# We have a lot
#
MY_TOOLCHAIN_PATH=$1
shift
CY_PYTHON_PATH=$1
shift
CY_OUTPUT_PATH=$1
shift
CY_OUTPUT_NAME=$1
shift
CY_ELF_TO_HEX=$1
shift
CY_ELF_TO_HEX_OPTIONS=$1
shift
CY_ELF_TO_HEX_FILE_ORDER=$1
shift
CY_HEX_TO_BIN=$1
shift
MCUBOOT_SCRIPT_FILE_DIR=$1
shift
IMGTOOL_SCRIPT_NAME=$1
shift
FLASH_ERASE_VALUE=$1
shift
MCUBOOT_HEADER_SIZE=$1
shift
CY_BUILD_VERSION=$1
shift
CY_BOOT_PRIMARY_1_START=$1
shift
CY_BOOT_PRIMARY_1_SIZE=$1
shift
CY_BOOT_SECONDARY_1_START=$1
shift
CY_TOC2_GENERATOR=$1
shift
CY_LCS=$1
shift
APPTYPE=$1
shift
MCUBOOT_KEY_DIR=$1
shift
MCUBOOT_KEY_FILE=$1
shift
SMIF_CRYPTO_CONFIG=$1

# Export these values for python3 click module
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

CY_OUTPUT_ELF=$CY_OUTPUT_PATH/$CY_OUTPUT_NAME.elf
CY_OUTPUT_BIN=$CY_OUTPUT_PATH/$CY_OUTPUT_NAME.bin
CY_OUTPUT_FINAL_BIN=$CY_OUTPUT_PATH/$CY_OUTPUT_NAME.final.bin
CY_OUTPUT_HEX=$CY_OUTPUT_PATH/$CY_OUTPUT_NAME.unsigned.hex
CY_OUTPUT_FINAL_HEX=$CY_OUTPUT_PATH/$CY_OUTPUT_NAME.final.hex
CY_OUTPUT_FILE_WILD=$CY_OUTPUT_PATH/$CY_OUTPUT_NAME.*
CY_OUTPUT_FILE_NAME_BIN=$CY_OUTPUT_NAME.bin
CY_OUTPUT_FILE_NAME_TAR=$CY_OUTPUT_NAME.tar
CY_COMPONENTS_JSON_NAME=components.json
#
# For elf -> hex conversion
#
if [ "$CY_ELF_TO_HEX_FILE_ORDER" == "elf_first" ]
then
    CY_ELF_TO_HEX_FILE_1=$CY_OUTPUT_ELF
    CY_ELF_TO_HEX_FILE_2=$CY_OUTPUT_HEX
else
    CY_ELF_TO_HEX_FILE_1=$CY_OUTPUT_HEX
    CY_ELF_TO_HEX_FILE_2=$CY_OUTPUT_ELF
fi

#
# Leave here for debugging
echo "--------------------- MCUBOOT SIGN Vars ----------------------------------------------------------------------"
echo "MY_TOOLCHAIN_PATH          =$MY_TOOLCHAIN_PATH"
echo "CY_PYTHON_PATH             =$CY_PYTHON_PATH"
echo "CY_OUTPUT_PATH             =$CY_OUTPUT_PATH"
echo "CY_OUTPUT_NAME             =$CY_OUTPUT_NAME"
echo "CY_ELF_TO_HEX              =$CY_ELF_TO_HEX"
echo "CY_ELF_TO_HEX_OPTIONS      =$CY_ELF_TO_HEX_OPTIONS"
echo "CY_ELF_TO_HEX_FILE_ORDER   =$CY_ELF_TO_HEX_FILE_ORDER"
echo "CY_HEX_TO_BIN              =$CY_HEX_TO_BIN"
echo "MCUBOOT_SCRIPT_FILE_DIR    =$MCUBOOT_SCRIPT_FILE_DIR"
echo "IMGTOOL_SCRIPT_NAME        =$IMGTOOL_SCRIPT_NAME"
echo "FLASH_ERASE_VALUE          =$FLASH_ERASE_VALUE"
echo "MCUBOOT_HEADER_SIZE        =$MCUBOOT_HEADER_SIZE"
echo "CY_BUILD_VERSION           =$CY_BUILD_VERSION"
echo "CY_BOOT_PRIMARY_1_START    =$CY_BOOT_PRIMARY_1_START"
echo "CY_BOOT_PRIMARY_1_SIZE     =$CY_BOOT_PRIMARY_1_SIZE"
echo "CY_BOOT_SECONDARY_1_START  =$CY_BOOT_SECONDARY_1_START"
echo "CY_TOC2_GENERATOR          =$CY_TOC2_GENERATOR"
echo "CY_LCS                     =$CY_LCS"
echo "APPTYPE                    =$APPTYPE"
echo "MCUBOOT_KEY_DIR            =$MCUBOOT_KEY_DIR"
echo "MCUBOOT_KEY_FILE           =$MCUBOOT_KEY_FILE"
echo "SMIF_CRYPTO_CONFIG         =$SMIF_CRYPTO_CONFIG"

# For FLASH_ERASE_VALUE
# If value is 0x00, we need to specify "-R 0"
# If value is 0xFF, we do not specify anything!
#
FLASH_ERASE_ARG=
if [ "$FLASH_ERASE_VALUE" == "0x00" ]
then
FLASH_ERASE_ARG="-R 0"
fi

PRJ_DIR=.

echo ""
echo "[Bin to Hex for debug (unsigned)]"
echo "$CY_HEX_TO_BIN -I binary -O ihex $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.bin $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.unsigned.hex"
$CY_HEX_TO_BIN -I binary -O ihex $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.bin $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.unsigned.hex

echo ""
echo "[TOC2_Generate] Execute toc2 generator script for $CY_OUTPUT_NAME"
echo "source $CY_TOC2_GENERATOR $CY_LCS $CY_OUTPUT_PATH $CY_OUTPUT_NAME $APPTYPE $PRJ_DIR $SMIF_CRYPTO_CONFIG $MY_TOOLCHAIN_PATH"
source $CY_TOC2_GENERATOR $CY_LCS $CY_OUTPUT_PATH $CY_OUTPUT_NAME $APPTYPE $PRJ_DIR $SMIF_CRYPTO_CONFIG $MY_TOOLCHAIN_PATH

echo ""
echo "Checking for TOC2 output file"
ls -l $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.final.bin

echo ""
echo "[SIGNING BOOT]"
#echo "$CY_PYTHON_PATH $MCUBOOT_SCRIPT_FILE_DIR/$IMGTOOL_SCRIPT_NAME sign -H 1024 --pad-header --align 8 -v $CY_BUILD_VERSION -L $CY_BOOT_PRIMARY_1_START -S $CY_BOOT_PRIMARY_1_SIZE -M 512 --overwrite-only -R 0xff -k keys/cypress-test-ec-p256.pem --pad $CY_OUTPUT_FINAL_BIN $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.signed.bin"
#$CY_PYTHON_PATH $MCUBOOT_SCRIPT_FILE_DIR/$IMGTOOL_SCRIPT_NAME sign -H 1024 --pad-header --align 8 -v $CY_BUILD_VERSION -L $CY_BOOT_PRIMARY_1_START -S $CY_BOOT_PRIMARY_1_SIZE -M 512 --overwrite-only -R 0xff -k keys/cypress-test-ec-p256.pem --pad $CY_OUTPUT_FINAL_BIN $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.signed.bin
echo "$CY_PYTHON_PATH $MCUBOOT_SCRIPT_FILE_DIR/$IMGTOOL_SCRIPT_NAME sign -H 1024 --pad-header --align 8 -v $CY_BUILD_VERSION -L $CY_BOOT_PRIMARY_1_START -S $CY_BOOT_PRIMARY_1_SIZE -M 512 --overwrite-only -R 0xff -k $MCUBOOT_KEY_DIR/$MCUBOOT_KEY_FILE --pad $CY_OUTPUT_FINAL_BIN $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.signed.bin"
$CY_PYTHON_PATH $MCUBOOT_SCRIPT_FILE_DIR/$IMGTOOL_SCRIPT_NAME sign -H 1024 --pad-header --align 8 -v $CY_BUILD_VERSION -L $CY_BOOT_PRIMARY_1_START -S $CY_BOOT_PRIMARY_1_SIZE -M 512 --overwrite-only -R 0xff -k $MCUBOOT_KEY_DIR/$MCUBOOT_KEY_FILE --pad $CY_OUTPUT_FINAL_BIN $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.signed.bin

echo ""
echo "[Bin to Hex for PRIMARY (BOOT) Slot]"
echo "$CY_HEX_TO_BIN --change-address=$CY_BOOT_PRIMARY_1_START -I binary -O ihex $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.signed.bin $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.final.hex"
$CY_HEX_TO_BIN --change-address=$CY_BOOT_PRIMARY_1_START -I binary -O ihex $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.signed.bin $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.final.hex


echo ""
echo "[SIGNING UPDATE ]"
echo "$CY_PYTHON_PATH $MCUBOOT_SCRIPT_FILE_DIR/$IMGTOOL_SCRIPT_NAME sign -H 1024 --pad-header --align 8 -v $CY_BUILD_VERSION -L $CY_BOOT_PRIMARY_1_START -S $CY_BOOT_PRIMARY_1_SIZE -M 512 --overwrite-only -R 0xff -k $MCUBOOT_KEY_DIR/$MCUBOOT_KEY_FILE --pad $CY_OUTPUT_FINAL_BIN $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.update.signed.hex"
$CY_PYTHON_PATH $MCUBOOT_SCRIPT_FILE_DIR/$IMGTOOL_SCRIPT_NAME sign -H 1024 --pad-header --align 8 -v $CY_BUILD_VERSION -L $CY_BOOT_PRIMARY_1_START -S $CY_BOOT_PRIMARY_1_SIZE -M 512 --overwrite-only -R 0xff -k $MCUBOOT_KEY_DIR/$MCUBOOT_KEY_FILE --pad $CY_OUTPUT_FINAL_BIN $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.update.signed.bin

echo ""
echo "[Bin to Hex for SECONDARY (UPDATE) Slot]"
echo "$CY_HEX_TO_BIN --change-address=$CY_BOOT_SECONDARY_1_START -I binary -O ihex $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.update.signed.bin $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.update.hex"
$CY_HEX_TO_BIN --change-address=$CY_BOOT_SECONDARY_1_START -I binary -O ihex $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.update.signed.bin $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.update.hex

# clean up temp files
rm $CY_OUTPUT_PATH/$CY_OUTPUT_NAME.final.bin

echo ""
ls -l $CY_OUTPUT_FILE_WILD
echo ""

import os
import random
import signal
import struct
import sys
import threading
import time
import traceback
import re
from dataclasses import dataclass

random.seed()

#
#   This is a utility to build a block of data that includes WiFi FW, CLM bloc and BT FW.
#
#   The goal is to create this header structure:
#
# typedef struct  cy_ota_fw_data_block_header
# {
#     uint8_t     magic[16];          /* Magic value                              */
#     uint32_t    crc;                /* CRC of FW Data Block                     */
#     uint32_t    FWDB_version;       /* FW Data Block version (this structure)   */
#
#     uint16_t    WiFi_FW_version[4]; /* WiFi FW version                          */
#     uint32_t    WiFi_FW_offset;     /* Offset to start of WiFi FW               */
#     uint32_t    WiFi_FW_size;       /* Size of WiFi FW                          */
#
#     uint32_t    CLM_blob_offset;    /* Offset to start of CLM Blob              */
#     uint32_t    CLM_blob_size;      /* Size of CLM Blob                         */
#
#     uint8_t     BT_FW_version[128]; /* BT FW version                            */
#     uint32_t    BT_FW_addr;         /* External Flash Addr of BT FW             */
#     uint32_t    BT_FW_size;         /* Size of BT FW                            */
# } cy_ota_fw_data_block_header_t;
#

FW_DATA_BLOCK_MAGIC_OFF  = 0
FW_DATA_BLOCK_MAGIC_SIZE = 16

CY_FW_BLOCK_DATA_START_OFFSET = 256         # 0x100 - offset to allow for header size

#
#==============================================================================
# Debugging help
#   To turn on logging, Set DEBUG_LOG to 1 (or use command line arg "-l")
#==============================================================================
DEBUG_LOG = 0

#==============================================================================
# Defines
#==============================================================================

# 16 bytes long
FW_DATA_block_header_info_MAGIC = "InfineonFWData  "   # "Magic" string to identify the header
FW_DATA_block_header_info_VERSION = 1

MSG_TYPE_ERROR = -1

#==============================================================================
# Define a class to encapsulate some variables
#==============================================================================

@dataclass
class FW_Data_block_header_info:
    internal_data_alignment: int = 4
    out_file: str = "fw_data_block.bin"
    out_file_size: int = 0
    crc: int = 0
    magic: str = FW_DATA_block_header_info_MAGIC
    version: int = FW_DATA_block_header_info_VERSION
    wifi_fw_ver_array_0: int = 0
    wifi_fw_ver_array_1: int = 0
    wifi_fw_ver_array_2: int = 0
    wifi_fw_ver_array_3: int = 0
    wifi_fw_src: str = ""
    wifi_fw_off: int = 0
    wifi_fw_size: int  = 0
    clm_blob_src: str = ""
    clm_blob_off: int  = 0
    clm_blob_size: int  = 0
    bt_fw_ver: str = ""
    bt_fw_c_src: str = ""
    bt_fw_src: str = ""
    bt_fw_off: int  = 0
    bt_fw_size: int  = 0

# Global block_header_info class
block_header_info = FW_Data_block_header_info()

#==============================================================================
# Handle ctrl-c to end program gracefully
#==============================================================================
terminate = False

# Signal handling function
def signal_handling(signum,frame):
   global terminate
   terminate = True

# take over the signals (SIGINT - MAC & Linux, SIGBREAK - Windows
signal.signal(signal.SIGINT,signal_handling)
if sys.platform.startswith('win'):
    signal.signal(signal.SIGBREAK,signal_handling)

#==============================================================================
#
#   fill_block_header_info()

block_header_for_output = bytearray(CY_FW_BLOCK_DATA_START_OFFSET)

#
# ---------------------------------------------------------
# right_quote = line.rindex('"')
# print("Found the version string at:", str(found) + " l: " + str(left_quote) + " r: " + str(right_quote) )
# block_header_info.bt_fw_ver = str(line[left_quote:right_quote])
# print("BT FW version: >" + block_header_info.bt_fw_ver + "<")
# Now parse next text section as 4 bytes
# block_header_info.wifi_fw_ver_array_0 = int()
# block_header_info.wifi_fw_ver_array_1 = int()
# block_header_info.wifi_fw_ver_array_2 = int()
# block_header_info.wifi_fw_ver_array_3 = int()

def parse_wifi_fw_version():
    global terminate
    if DEBUG_LOG == 1:
        print("Parse the .bin file in order to get the version name:" + block_header_info.wifi_fw_src)
    offset = 0
    chunk = bytearray(64)
    with open(block_header_info.wifi_fw_src, "rb") as in_file:
        if DEBUG_LOG == 1:
            print("Opened:  size: " + str(block_header_info.wifi_fw_size) + "   " + block_header_info.wifi_fw_src)
        for fileoff in range (0, block_header_info.wifi_fw_size):
            if terminate:
                exit(0)
            # find "V"
            in_file.seek(offset)
            chunk = in_file.read(1)
            chunk_str = str(chunk)
            # print("fileoff: " + str(fileoff) + " off: " + str(offset) + " chunk : >" + str(chunk) + "<" + "  >" + chunk_str + "<")
            offset += 1
            if chunk == b'V':
                # print("fileoff: " + str(fileoff) + " off: " + str(offset) + " chunk : >" + str(chunk) + "<" + "  >" + chunk_str + "<")
                chunk_str = str(in_file.read(64))
                found = chunk_str.find("ersion: ")
                if found > 0:
                    # print("WIFI FOUND: " + str(found) + " >" + chunk_str + "<")
                    word_list = chunk_str.split(" ")
                    if word_list:
                        number_str = word_list[1]
                        # Break into list of numbers
                        num_list = number_str.split(".")
                        block_header_info.wifi_fw_ver_array_0 = int(num_list[0])
                        block_header_info.wifi_fw_ver_array_1 = int(num_list[1])
                        block_header_info.wifi_fw_ver_array_2 = int(num_list[2])
                        block_header_info.wifi_fw_ver_array_3 = int(num_list[3])
                        # print(str.format("WiFi Ver: {:#04x}.{:#04x}.{:#04x}.{:#04x}", block_header_info.wifi_fw_ver_array_0, block_header_info.wifi_fw_ver_array_1, block_header_info.wifi_fw_ver_array_2, block_header_info.wifi_fw_ver_array_3) )
                        # print(str.format("WiFi Ver: {:d}.{:d}.{:d}.{:d}", block_header_info.wifi_fw_ver_array_0, block_header_info.wifi_fw_ver_array_1, block_header_info.wifi_fw_ver_array_2, block_header_info.wifi_fw_ver_array_3) )
    in_file.close()

def parse_bt_patch_data():
    # print("Parse the .c file in order to get the patch version name:" + block_header_info.bt_fw_c_src)
    # while open(block_header_info.bt_fw_c_src, 'r') as in_file:
    count = 0
    with open(block_header_info.bt_fw_c_src) as f:
        for line in f:
            # find substring in line that matches "brcm_patch_version[]"
            found = line.find("brcm_patch_version")
            if found > 0:
                left_quote = line.index('"') + 1
                right_quote = line.rindex('"')
                # print("Found the version string at:", str(found) + " l: " + str(left_quote) + " r: " + str(right_quote) )
                block_header_info.bt_fw_ver = str(line[left_quote:right_quote])
                print("BT FW version: >" + block_header_info.bt_fw_ver + "<")
                break

    f.close()

    # Now we can create the name of the BT FW Patch file (".hcd")
    # The file will be in the same directory
    # find last slash
    slash_index= block_header_info.bt_fw_c_src.rfind('/')
    if slash_index == 0:
        slash_index= block_header_info.bt_fw_c_src.rfind('\\')
    if slash_index == 0:
        print("Can't find slash in filename for BT FW!")
        exit(0)
    # Move past slash
    slash_index += 1
    block_header_info.bt_fw_src = block_header_info.bt_fw_c_src[0:slash_index] + block_header_info.bt_fw_ver + ".hcd"

    # get the size
    block_header_info.bt_fw_size = os.path.getsize(block_header_info.bt_fw_src)



def set_block_header_internal_offsets():
    global terminate
    global block_header_info
    current_offset = CY_FW_BLOCK_DATA_START_OFFSET
    quotient: int = 0
    remainder: int = 0

    if DEBUG_LOG == 1:
        print("current_offset: " + str(current_offset) )

    if block_header_info.wifi_fw_size != 0:
        if DEBUG_LOG == 1:
            print("current_offset WiFi: " + str(current_offset) )
        block_header_info.wifi_fw_off = current_offset
        current_offset += block_header_info.wifi_fw_size
        quotient, remainder = divmod(current_offset, block_header_info.internal_data_alignment)
        current_offset += block_header_info.internal_data_alignment - remainder

    if block_header_info.clm_blob_size != 0:
        if DEBUG_LOG == 1:
            print("current_offset CLM : " + str(current_offset) )
        block_header_info.clm_blob_off = current_offset
        current_offset += block_header_info.clm_blob_size
        quotient, remainder = divmod(current_offset, block_header_info.internal_data_alignment)
        current_offset += block_header_info.internal_data_alignment - remainder

    if block_header_info.bt_fw_size != 0:
        if DEBUG_LOG == 1:
            print("current_offset BT  : " + str(current_offset) )
        block_header_info.bt_fw_off = current_offset
        current_offset += block_header_info.bt_fw_size
        quotient, remainder = divmod(current_offset, block_header_info.internal_data_alignment)
        current_offset += block_header_info.internal_data_alignment - remainder

    if DEBUG_LOG == 1:
        print("FW DATA BLOCK Layout:")
        print("content    offset   size")
        print(str.format("Header :    {:#08x}", 0) + "  " + str.format("{:#08x}", CY_FW_BLOCK_DATA_START_OFFSET))
        print(str.format("WiFi FW:    {:#08x}", block_header_info.wifi_fw_off) + "  " + str.format("{:#08x}", block_header_info.wifi_fw_size))
        print(str.format("CLM Blob:   {:#08x}", block_header_info.clm_blob_off) + "  " + str.format("{:#08x}", block_header_info.clm_blob_size))
        print(str.format("BT FW:      {:#08x}", block_header_info.bt_fw_off) + "  " + str.format("{:#08x}", block_header_info.bt_fw_size))



def output_fw_data_block_file():
    global terminate
    global block_header_for_output

    # Create the header for output

#     uint8_t     magic[16];          /* Magic value                              */
#     uint32_t    crc;                /* CRC of FW Data Block                     */
#     uint32_t    FWDB_version;       /* FW Data Block version (this structure)   */
#
#     uint16_t    WiFi_FW_version[4]; /* WiFi FW version                          */
#     uint32_t    WiFi_FW_offset;     /* Offset to start of WiFi FW               */
#     uint32_t    WiFi_FW_size;       /* Size of WiFi FW                          */
#
#     uint32_t    CLM_blob_offset;    /* Offset to start of CLM Blob              */
#     uint32_t    CLM_blob_size;      /* Size of CLM Blob                         */
#
#     uint8_t     BT_FW_version[128]; /* BT FW version                            */
#     uint32_t    BT_FW_addr;         /* External Flash Addr of BT FW             */
#     uint32_t    BT_FW_size;         /* Size of BT FW                            */
#     uint8_t     pad[68];        /* Padding (room for future needs)              */

    # open the output file
    block_header_info.out_file_size = 0
    with open(block_header_info.out_file, 'wb') as out_file:
        pad_size = 0
        print("")
        print("Opened output file: " + block_header_info.out_file)

        # create the actual output header structure
        # < - little endian
        # s - string
        # B - unsigned char
        # H - 2 bytes integer
        # I - 4 bytes integer
        #
        struct.pack_into('<16s2I4H4I128s2I', block_header_for_output, 0, block_header_info.magic.encode('ascii'),
                            block_header_info.crc, block_header_info.version,
                            block_header_info.wifi_fw_ver_array_0, block_header_info.wifi_fw_ver_array_1,
                            block_header_info.wifi_fw_ver_array_2, block_header_info.wifi_fw_ver_array_3,
                            block_header_info.wifi_fw_off, block_header_info.wifi_fw_size,
                            block_header_info.clm_blob_off, block_header_info.clm_blob_size,
                            block_header_info.bt_fw_ver.encode('ascii'),
                            block_header_info.bt_fw_off, block_header_info.bt_fw_size )

        if DEBUG_LOG == 1:
            print(str.format("FW Data Block Header:") )
            print(str.format("Magic  :    >{:16s}<", block_header_info.magic) )
            print(str.format("            {:08x} ", block_header_info.crc, block_header_info.version) )
            print(str.format("            {:04x} {:04x} {:04x} {:04x} ", block_header_info.wifi_fw_ver_array_0, block_header_info.wifi_fw_ver_array_1, block_header_info.wifi_fw_ver_array_2, block_header_info.wifi_fw_ver_array_3) )
            print(str.format("            {:08x}", block_header_info.wifi_fw_off) )
            print(str.format("            {:08x} {:08x} {:08x} ", block_header_info.wifi_fw_size, block_header_info.clm_blob_off, block_header_info.clm_blob_size) )
            print(str.format("            >{:128s}<", block_header_info.bt_fw_ver) )
            print(str.format("            {:08x} {:08x}", block_header_info.bt_fw_off, block_header_info.bt_fw_size) )

        block_header_info.out_header_size = len(block_header_for_output);
        print("len(block_header_for_output): " + str(block_header_info.out_header_size) + "  " + hex(block_header_info.out_header_size) )

        out_file.write( block_header_for_output )
        print("out_file.write : block_header_for_output")
        block_header_info.out_file_size += len(block_header_for_output)
        file_index = 0

        # open the files (if they exist), read data, then write to output file

        read_back_offset = out_file.tell()
        print(" Before WiFi FW: read_back_offset " + str(read_back_offset) )
        if (block_header_info.out_file_size != read_back_offset):
            print("         Does not match block_header_info.out_file_size: " + str(block_header_info.out_file_size))

        # WiFi FW
        if (block_header_info.wifi_fw_src != "") and (block_header_info.wifi_fw_src != "NONE"):
            chunk_index= 0
            chunk_size_to_read = 4096
            with open(block_header_info.wifi_fw_src, 'rb') as in_file:
                if DEBUG_LOG == 1:
                    print("Opened WiFi FW file: " + block_header_info.wifi_fw_src)
                while True:
                    if terminate:
                        exit(0)

                    in_file.seek(file_index)
                    chunk = in_file.read(chunk_size_to_read)
                    chunk_size = len(chunk)
                    # if DEBUG_LOG == 1:
                    #     print("CHUNK "+ str(chunk_index) + " off: " + hex(file_index) + " size: " + hex(chunk_size))
                    out_file.write( chunk )
                    chunk_index += 1
                    file_index += chunk_size
                    block_header_info.out_file_size += chunk_size
                    if chunk_size == 0:
                        break

            in_file.close()
            if file_index != block_header_info.wifi_fw_size:
                if DEBUG_LOG == 1:
                    print(" Error reading WiFi FW src. size: " + hex(block_header_info.wifi_fw_size) + " read: " + hex(file_index) )
                exit(0)

        # Now we need to pad out to align value, which is the offset of the next file
        if (block_header_info.clm_blob_src != "") and (block_header_info.clm_blob_src != "NONE") and (block_header_info.clm_blob_off > 0):
            read_back_offset = out_file.tell()
            if (block_header_info.clm_blob_off != read_back_offset):
                if DEBUG_LOG == 1:
                    print("block_header_info.clm_blob_off: " + str(block_header_info.clm_blob_off) + " - file_index: " + str(file_index) + " - CY_FW_BLOCK_DATA_START_OFFSET: " + str(CY_FW_BLOCK_DATA_START_OFFSET) )
                pad_size = block_header_info.clm_blob_off - file_index - CY_FW_BLOCK_DATA_START_OFFSET
                if pad_size > 0:
                    if DEBUG_LOG == 1:
                        print("padding: " + str(pad_size) )
                    zero_padding = bytearray(pad_size)
                    out_file.write( zero_padding )
                    print("out_file.write : zero_padding after WiFi")
                    block_header_info.out_file_size += pad_size

        read_back_offset = out_file.tell()
        print("Before CLM Blob: read_back_offset " + str(read_back_offset))
        if (block_header_info.out_file_size != read_back_offset):
            print("     Does not match read_back_offset: " + str(block_header_info.out_file_size))

        # WiFi CLM Blob
        if (block_header_info.clm_blob_src != "") and (block_header_info.clm_blob_src != "NONE"):
            chunk_index= 0
            file_index = 0
            chunk_size_to_read = 4096
            with open(block_header_info.clm_blob_src, 'rb') as in_file:
                if DEBUG_LOG == 1:
                    print("Opened block_header_info.clm_blob_src: " + block_header_info.clm_blob_src)

                while True:
                    if terminate:
                        exit(0)

                    in_file.seek(file_index)
                    chunk = in_file.read(chunk_size_to_read)
                    chunk_size = len(chunk)
                    # if DEBUG_LOG == 1:
                    #     print("CHUNK "+ str(chunk_index) + " off: " + hex(file_index) + " size: " + hex(chunk_size))
                    out_file.write( chunk )
                    chunk_index += 1
                    file_index += chunk_size
                    block_header_info.out_file_size += chunk_size
                    if chunk_size == 0:
                        break

            in_file.close()
            if file_index != block_header_info.clm_blob_size:
                if DEBUG_LOG == 1:
                    print(" Error reading CLM BLob src. size: " + str(block_header_info.clm_blob_size) + " read: " + str(file_index) )
                exit(0)

        # Now we need to pad out to align value, which is the offset of the next file
        if (block_header_info.bt_fw_src != "") and (block_header_info.bt_fw_src != "NONE") and (block_header_info.bt_fw_off > 0):
            read_back_offset = out_file.tell()
            if (block_header_info.bt_fw_off != read_back_offset):
                if DEBUG_LOG == 1:
                    print("block_header_info.bt_fw_off: " + str(block_header_info.bt_fw_off) + " - block_header_info.clm_blob_off: " + str(block_header_info.clm_blob_off) + " - file_index: " + str(file_index) )
                pad_size = block_header_info.bt_fw_off - block_header_info.clm_blob_off - file_index
                if pad_size > 0 :
                    if DEBUG_LOG == 1:
                        print("padding: " + str(pad_size) )
                    zero_padding = bytearray(pad_size)
                    out_file.write( zero_padding )
                    print("out_file.write :  After CLM ")
                    block_header_info.out_file_size += pad_size

        read_back_offset = out_file.tell()
        print("Before BT FW: read_back_offset: " + str(read_back_offset))
        if (block_header_info.out_file_size != read_back_offset):
            print("         Does not match block_header_info.out_file_size: " + str(block_header_info.out_file_size))

        # BT FW
        if (block_header_info.bt_fw_src != "") and (block_header_info.bt_fw_src != "NONE"):
            chunk_index= 0
            file_index = 0
            chunk_size_to_read = 4096
            with open(block_header_info.bt_fw_src, 'rb') as in_file:
                if DEBUG_LOG == 1:
                    print("Opened block_header_info.bt_fw_src: " + block_header_info.bt_fw_src)

                while True:
                    if terminate:
                        exit(0)

                    in_file.seek(file_index)
                    chunk = in_file.read(chunk_size_to_read)
                    chunk_size = len(chunk)
                    # if DEBUG_LOG == 1:
                    #    print("CHUNK "+ str(chunk_index) + " off: " + hex(file_index) + " size: " + hex(chunk_size))
                    out_file.write( chunk )
                    chunk_index += 1
                    file_index += chunk_size
                    block_header_info.out_file_size += chunk_size
                    if chunk_size == 0:
                        break

            in_file.close()
            if file_index != block_header_info.bt_fw_size:
                if DEBUG_LOG == 1:
                    print(" Error reading BT FW src. size: " + hex(block_header_info.bt_fw_size) + " read: " + hex(file_index) )
                exit(0)

        # We need to pad out to align value ?
        total_size = block_header_info.bt_fw_off + block_header_info.bt_fw_size
        quotient, remainder = divmod(total_size, block_header_info.internal_data_alignment)
        pad_size = block_header_info.internal_data_alignment - remainder
        if pad_size > 0 :
            if DEBUG_LOG == 1:
                print("padding: " + str(pad_size) )
            zero_padding = bytearray(pad_size)
            out_file.write( zero_padding )
            print("out_file.write : After BT" )
            block_header_info.out_file_size += pad_size

        read_back_offset = out_file.tell()
        print("All done: read_back_offset: " + str(read_back_offset))
        if (block_header_info.out_file_size != read_back_offset):
            print("         Does not match block_header_info.out_file_size: " + str(block_header_info.out_file_size))

        if DEBUG_LOG == 1:
            print(" DONE WRITING ALL FILES!" )
        out_file.close()


        # check output
    with open(block_header_info.out_file, 'rb') as out_file:
        # WiFi
        if (block_header_info.wifi_fw_src != "") and (block_header_info.wifi_fw_src != "NONE"):
            with open(block_header_info.wifi_fw_src, 'rb') as in_file:
                if DEBUG_LOG == 1:
                    print("Opened WiFi FW file: " + block_header_info.wifi_fw_src)
                out_file.seek(block_header_info.wifi_fw_off)
                chunk = out_file.read(chunk_size_to_read)
                test_chunk = in_file.read(chunk_size_to_read)
                if(chunk != test_chunk):
                    print("WiFi readback fail: ")
            in_file.close()

        # CLM Blob
        if (block_header_info.clm_blob_src != "") and (block_header_info.clm_blob_src != "NONE"):
            chunk_index= 0
            file_index = 0
            chunk_size_to_read = 4096
            with open(block_header_info.clm_blob_src, 'rb') as in_file:
                if DEBUG_LOG == 1:
                    print("Opened block_header_info.clm_blob_src: " + block_header_info.clm_blob_src)
                out_file.seek(block_header_info.clm_blob_off)
                chunk = out_file.read(chunk_size_to_read)
                test_chunk = in_file.read(chunk_size_to_read)
                if(chunk != test_chunk):
                    print("CLM Blub readback fail: ")
            in_file.close()


        # BT FW
        if (block_header_info.bt_fw_src != "") and (block_header_info.bt_fw_src != "NONE"):
            chunk_index= 0
            file_index = 0
            chunk_size_to_read = 4096
            with open(block_header_info.bt_fw_src, 'rb') as in_file:
                if DEBUG_LOG == 1:
                    print("Opened block_header_info.bt_fw_src: " + block_header_info.bt_fw_src)
                out_file.seek(block_header_info.bt_fw_off)
                chunk = out_file.read(chunk_size_to_read)
                test_chunk = in_file.read(chunk_size_to_read)
                if(chunk != test_chunk):
                    print("BT FW readback fail: ")
            in_file.close()


# =====================================================================
#
# Start of "main"
#
# Look at arguments and find what mode we are in.
#
# Possible arguments TLS setting for MQTT connection (default is non-TLS)
#
# Usage:
#   python publisher.py [TLS]
#
# =====================================================================

if __name__ == "__main__":
    print("Infineon FW Data Block Builder.")
    print("   Usage: 'python build_fw_data_block.py [-l] [-a <align>] [-wifi_src <filename>] [-clm_src <filename>] [-bt_src <filename>] [-out_file <filename>'")
    print("-l                   - Turn on extra logging")
    print("-a                   - alignment for start of data within the data block (default=4)")
    print("-wifi_src <filename> - filename of WiFi FW")
    print("-clm_src <filename>  - filename of CLM blob")
    print("-bt_src <filename>   - filename of BT FW")
    print("-out_file <filename> - output filename")

    last_arg = ""
    for i, arg in enumerate(sys.argv):
        # print(f"Argument {i:>4}: {arg}")
        if arg == "-l":
            DEBUG_LOG = 1
        if last_arg == "-a":
            block_header_info.internal_data_alignment = int(arg)
        if last_arg == "-wifi_src":
            block_header_info.wifi_fw_src = arg
        if last_arg == "-clm_src":
            block_header_info.clm_blob_src = arg
        if last_arg == "-bt_src":
            block_header_info.bt_fw_c_src = arg
        if last_arg == "-out_file":
            block_header_info.out_file = arg
        last_arg = arg

print("\n")
DEBUG_LOG = 1

print("Values for this run:\n")
print(" Internal data alignment: " + str(block_header_info.internal_data_alignment) )
if (block_header_info.wifi_fw_src != "") and (block_header_info.wifi_fw_src != "NONE"):
    print("WiFi     file: " + block_header_info.wifi_fw_src)
    block_header_info.wifi_fw_size = os.path.getsize(block_header_info.wifi_fw_src)
    # Get WiFi version info
    parse_wifi_fw_version()
    print(str.format("          ver: {:d}.{:d}.{:d}.{:d}", block_header_info.wifi_fw_ver_array_0, block_header_info.wifi_fw_ver_array_1, block_header_info.wifi_fw_ver_array_2, block_header_info.wifi_fw_ver_array_3) )
    print("         size: " + str(block_header_info.wifi_fw_size) )
if (block_header_info.clm_blob_src != "") and (block_header_info.clm_blob_src != "NONE"):
    print("CLM Blob file: " + block_header_info.clm_blob_src)
    block_header_info.clm_blob_size = os.path.getsize(block_header_info.clm_blob_src)
    print("         size: " + str(block_header_info.clm_blob_size) )
if (block_header_info.bt_fw_c_src != "") and (block_header_info.bt_fw_c_src != "NONE"):
    # Get BT version info and bin filename
    parse_bt_patch_data()
    print("BT FW    file: " + block_header_info.bt_fw_src)
    print("          ver: " + str(block_header_info.bt_fw_ver) )
    print("         size: " + str(block_header_info.bt_fw_size) )
print("  Output file: " + block_header_info.out_file)

# Determine the offsets
set_block_header_internal_offsets()

# Output the FW Data Block file
output_fw_data_block_file()

print("Done creating file: " +  str(block_header_info.out_file) )
print("              size: " +  str(block_header_info.out_file_size) )

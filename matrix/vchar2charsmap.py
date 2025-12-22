#!/usr/bin/env python3
"""
VChar64 to C Header Converter

Converts VChar64 project files (.vchar64proj) or assembler exports (.s)
to C header files with unsigned char arrays for cc65.

Usage:
    python vchar2charsmap.py input.vchar64proj output.h
    python vchar2charsmap.py input.s output.h
"""
import sys
import os

def convert_vchar64proj(input_file, output_file):
    """Convert binary .vchar64proj file to C header."""
    with open(input_file, "rb") as f:
        data = f.read()
    
    # VChar64 format: header starts with "VChar", charset data at offset 0x20
    if not data.startswith(b'VChar'):
        print(f"Warning: {input_file} doesn't have VChar header, treating as raw charset")
        charset_data = data
    else:
        # Skip header (32 bytes) to get to charset data
        charset_data = data[0x20:]
    
    # Charset is 256 characters * 8 bytes = 2048 bytes
    num_chars = min(len(charset_data) // 8, 256)
    charset_bytes = charset_data[:num_chars * 8]
    
    with open(output_file, "w") as f:
        f.write("/* Generated from {} */\n".format(os.path.basename(input_file)))
        f.write("/* {} characters, {} bytes */\n\n".format(num_chars, len(charset_bytes)))
        f.write("const unsigned char charmap[] = {\n")
        
        for char_idx in range(num_chars):
            offset = char_idx * 8
            char_bytes = charset_bytes[offset:offset+8]
            byte_strs = [str(b) for b in char_bytes]
            
            # Add comment every 8 characters for readability
            if char_idx % 8 == 0:
                f.write("    /* Chars {}-{} */\n".format(char_idx, min(char_idx+7, num_chars-1)))
            
            line = "    " + ", ".join(byte_strs)
            if char_idx < num_chars - 1:
                line += ","
            line += "  /* char {} */\n".format(char_idx)
            f.write(line)
        
        f.write("};\n")
    
    print(f"Converted {num_chars} characters ({len(charset_bytes)} bytes) to {output_file}")

def convert_asm(input_file, output_file):
    """Convert assembler .s file to C header (legacy mode)."""
    buffer = ""
    
    with open(input_file, "r") as vchar_input:
        for line in vchar_input:
            if ".byte" in line:
                line = line.replace(".byte ", "")
                line = line.replace("$", "0x")
                
                if ";" in line:
                    characters, index = line.split(";")
                else:
                    characters = line.strip()
                    index = ""
                
                charbytes = ','.join([str(int(k, 16)) for k in characters.split(",") if k.strip()])
                if charbytes:
                    buffer += "{}, // {}\n    ".format(charbytes, index.strip())
    
    with open(output_file, "w") as charset_output:
        charset_output.write("/* Generated from {} */\n\n".format(os.path.basename(input_file)))
        charset_output.write("const unsigned char charmap[] = {\n    ")
        charset_output.write(buffer.rstrip(", \n"))
        charset_output.write("\n};\n")
    
    print(f"Converted {input_file} to {output_file}")

def main():
    if len(sys.argv) < 3:
        print("Usage: {} <input.vchar64proj|input.s> <output.h>".format(sys.argv[0]))
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    if input_file.endswith('.vchar64proj') or not input_file.endswith('.s'):
        # Try binary format first
        try:
            convert_vchar64proj(input_file, output_file)
        except Exception as e:
            print(f"Binary conversion failed: {e}, trying asm format...")
            convert_asm(input_file, output_file)
    else:
        convert_asm(input_file, output_file)

if __name__ == "__main__":
    main()

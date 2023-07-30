#!/usr/bin/env python3
# Firefly SAO ROM generator

rom = [
    0x4c, 0x49, 0x46, 0x45, 0x0e, 0x07, 0x06, 0x02,
    0x46, 0x69, 0x72, 0x65, 0x66, 0x6c, 0x79, 0x20,
    0x66, 0x72, 0x69, 0x65, 0x6e, 0x64, 0x73, 0x74,
    0x6f, 0x72, 0x61, 0x67, 0x65, 0x00, 0x50, 0x0e,
    0x06, 0x01, 0x00, 0x03, 0x08, 0x61, 0x70, 0x70,
    0x66, 0x69, 0x72, 0x65, 0x66, 0x6c, 0x79, 0x00,
    0x07, 0x04, 0x66, 0x69, 0x72, 0x65, 0x66, 0x6c,
    0x79, 0x01, 0x01
]

def num_to_arr(num):
    return [num & 255, (num >> 8) & 255]

def format_rom(serial):
    global rom
    serial = int(serial)
    return rom + num_to_arr(serial)

def print_rom_writer(custom_rom):
    print("import machine")
    for i in range(0, len(custom_rom), 64):
        snippet = num_to_arr(i) + custom_rom[i:i+64]
        print("machine.I2C(0).writeto(0x50, {})".format(repr(bytes(snippet))))

def _print_help():
    print("Usage: sao_rom.py <Serial No.>")
    print("Outputs an MCH2022 python SAO formatting command")

if __name__ == "__main__":
    from sys import argv
    if len(argv) != 2:
        _print_help()
    else:
        try:
            custom = format_rom(int(argv[1]))
            print_rom_writer(custom)
        except:
            _print_help()

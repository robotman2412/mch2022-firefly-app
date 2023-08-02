#!/usr/bin/env python3
# Firefly SAO ROM generator

import machine, time

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

def write_rom(custom_rom):
    # Writing
    for i in range(0, len(custom_rom), 64):
        snippet = num_to_arr(i) + custom_rom[i:i+64]
        machine.I2C(0).writeto(0x50, bytes(snippet))
        time.sleep(0.1)
    # Verification
    machine.I2C(0).writeto(0x50, b'\x00\x00')
    if machine.I2C(0).readfrom(0x50, len(custom_rom)) == bytes(custom_rom):
        print("\n\nROM validated!")
        return True
    else:
        print("\n\nROM mismatch!")
        return False

def format_and_write(serial, tries=3):
    custom_rom = format_rom(serial)
    for i in range(tries):
        if write_rom(custom_rom):
            return
        else:
            print("Attempt {}/{} failed".format(i+1, tries))
            time.sleep(0.5)

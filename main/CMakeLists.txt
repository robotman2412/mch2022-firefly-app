# This example code is in the Public Domain (or CC0 licensed, at your option.)
# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.

idf_component_register(
    SRCS
        "main.c"
        "sao_eeprom.c"
    INCLUDE_DIRS
        "." "include"
    EMBED_FILES
        "firefly_qr.png"
    REQUIRES
        appfs
        bus-i2c
        eeprom
        mch2022-bsp
        mch2022-rp2040
        pax-codecs
        pax-graphics
        pax-keyboard
        sdcard
        spi-ice40
        spi-ili9341
        ws2812
)

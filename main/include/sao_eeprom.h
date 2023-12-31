#pragma once

#include <esp_system.h>
#include <stdint.h>

typedef enum { SAO_NONE, SAO_UNFORMATTED, SAO_BINARY, SAO_JSON } sao_type_t;

#define SAO_MAX_FIELD_LENGTH 255
#define SAO_MAX_NUM_DRIVERS  8

typedef struct __attribute__((__packed__)) {
    uint8_t magic[4];
    uint8_t name_length;
    uint8_t driver_name_length;
    uint8_t driver_data_length;
    uint8_t number_of_extra_drivers;
} sao_binary_header_t;

typedef struct __attribute__((__packed__)) {
    uint8_t driver_name_length;
    uint8_t driver_data_length;
} sao_binary_extra_driver_t;

/* ==== Storage driver ==== */

#define SAO_DRIVER_STORAGE_NAME "storage"

typedef struct __attribute__((__packed__)) {
    uint8_t flags;          // Reserved, set to 0
    uint8_t address;        // I2C address of the data EEPROM (0x50 when using main EEPROM, usually 0x51 when using a separate data EEPROM)
    uint8_t size_exp;       // For example 15 for 32 kbit
    uint8_t page_size_exp;  // For example 6 for 64 bytes
    uint8_t data_offset;    // In pages, needed to skip header
    uint8_t reserved;       // Reserved, set to 0
} sao_driver_storage_data_t;



/* ==== Basic IO driver ==== */

#define SAO_DRIVER_BASIC_IO_NAME "basic_io"

typedef struct __attribute__((__packed__)) {
    uint8_t io1_function;
    uint8_t io2_function;
    uint8_t reserved;  // Reserved, set to 0
} sao_driver_basic_io_data_t;

enum SAO_DRIVER_BASIC_IO_FUNC {
    SAO_DRIVER_BASIC_IO_FUNC_NONE       = 0,
    SAO_DRIVER_BASIC_IO_FUNC_LED        = 1,
    SAO_DRIVER_BASIC_IO_FUNC_BUTTON     = 2,
    SAO_DRIVER_BASIC_IO_FUNC_LED_RED    = 3,
    SAO_DRIVER_BASIC_IO_FUNC_LED_GREEN  = 4,
    SAO_DRIVER_BASIC_IO_FUNC_LED_BLUE   = 5,
    SAO_DRIVER_BASIC_IO_FUNC_LED_YELLOW = 6,
    SAO_DRIVER_BASIC_IO_FUNC_LED_AMBER  = 7,
    SAO_DRIVER_BASIC_IO_FUNC_LED_WHITE  = 8,
};



/* ==== Neopixel driver ==== */

#define SAO_DRIVER_NEOPIXEL_NAME "neopixel"

enum SAO_DRIVER_NEOPIXEL_COLOR_ORDER {
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RGB  = 0,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RBG  = 1,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GRB  = 2,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GBR  = 3,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BRG  = 4,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BGR  = 5,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_WRGB = 6,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_WRBG = 7,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_WGRB = 8,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_WGBR = 9,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_WBRG = 10,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_WBGR = 11,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RWGB = 12,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RWBG = 13,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RGWB = 14,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RGBW = 15,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RBWG = 16,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_RBGW = 17,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GWRB = 18,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GWBR = 19,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GRWB = 20,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GRBW = 21,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GBWR = 22,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_GBRW = 23,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BWRG = 24,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BWGR = 25,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BRWG = 26,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BRGW = 27,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BGWR = 28,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_BGRW = 29,
    SAO_DRIVER_NEOPIXEL_COLOR_ORDER_MAX
};

typedef struct __attribute__((__packed__)) {
    uint16_t length;       // Length in LEDs
    uint8_t  color_order;  // One of the values defined in the color order enum
    uint8_t  reserved;     // Reserved, set to 0
} sao_driver_neopixel_data_t;



/* ==== SSD1306 driver ==== */

#define SAO_DRIVER_SSD1306_NAME "ssd1306"

typedef struct __attribute__((__packed__)) {
    uint8_t address;   // I2C address of the SSD1306 OLED (usually 0x3C)
    uint8_t height;    // 32 or 64, in pixels
    uint8_t reserved;  // Reserved, set to 0
} sao_driver_ssd1306_data_t;



/* ==== NTAG NFC driver ==== */

#define SAO_DRIVER_NTAG_NAME "ntag"

typedef struct __attribute__((__packed__)) {
    uint8_t address;        // I2C address of the NTAG IC (usually 0x55)
    uint8_t size_exp;       // 10 (1k) for NT3H2111 or 11 (2k) for NT3H2211
    uint8_t interrupt_pin;  // 0 for not connected, 1 for IO1 and 2 for IO2
    uint8_t reserved;       // Reserved, set to 0
} sao_driver_ntag_data_t;



/* ==== App link driver ==== */

#define SAO_DRIVER_APP_NAME "app"
// data is a string containing the slug name of the app, null terminated



/* ==== Firefly driver ==== */

#define SAO_DRIVER_FIREFLY_NAME "firefly"

typedef struct __attribute__((__packed__)) {
    uint8_t batch_no;     // Production batch in total
    uint8_t hardware_ver; // Hardware revision
    uint8_t serial_no_lo; // Serial No. shown on package
    uint8_t serial_no_hi; // Serial No. shown on package
} sao_driver_firefly_data_t;

typedef struct {
    char    name[SAO_MAX_FIELD_LENGTH + 1];
    union {
        uint8_t data[SAO_MAX_FIELD_LENGTH];
        sao_driver_storage_data_t  storage;
        sao_driver_basic_io_data_t basic_io;
        sao_driver_neopixel_data_t neopixel;
        sao_driver_neopixel_data_t ssd1306;
        sao_driver_ntag_data_t     ntag;
        sao_driver_firefly_data_t  firefly;
    };
    uint8_t data_length;
} sao_driver_t;

typedef struct {
    uint8_t      type;  // sao_type_t;
    char         name[SAO_MAX_FIELD_LENGTH + 1];
    uint8_t      amount_of_drivers;
    sao_driver_t drivers[SAO_MAX_NUM_DRIVERS];
} SAO;



void dump_eeprom_contents();

esp_err_t sao_identify(SAO* sao);
esp_err_t sao_write_raw(size_t offset, uint8_t* buffer, size_t buffer_length);
esp_err_t sao_format_old(const char* name, const char* driver, const uint8_t* driver_data, uint8_t driver_data_length, const char* driver2,
                     const uint8_t* driver2_data, uint8_t driver2_data_length, const char* driver3, const uint8_t* driver3_data, uint8_t driver3_data_length,
                     bool small);
// Format an SAO.
// If `add_storage_driver` is 1, a storage driver representing the free space in the SAO EEPROM is added as the first driver.
// If `add_storage_driver` is 1, `drivers` is optional.
// If `eeprom_page_size` is 0, a guess is made based on `eeprom_size`.
esp_err_t sao_format(char const *name, size_t eeprom_size, size_t eeprom_page_size, sao_driver_t const *drivers, size_t drivers_len, bool add_storage_driver);
// Format an SAO, but do not write it.
// If `add_storage_driver` is 1, a storage driver representing the free space in the SAO EEPROM is added as the first driver.
// If `eeprom_page_size` is 0, a guess is made based on `eeprom_size`.
// If `add_storage_driver` is 1, `drivers` is optional.
esp_err_t sao_format_data(char const *name, size_t eeprom_size, size_t eeprom_page_size, sao_driver_t const *drivers, size_t drivers_len, bool add_storage_driver, uint8_t **out_buf_ptr, size_t *out_size);

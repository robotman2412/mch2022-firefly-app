/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

// This file contains a simple Hello World app which you can base you own
// native Badge apps on.

#include "main.h"
#include "esp_now.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <driver/i2c.h>

#define FIREFLY_BTN_PIN 0
#define FIREFLY_LED_PIN 1

static pax_buf_t buf;
xQueueHandle buttonQueue;

#include <esp_log.h>
static const char *TAG = "mch2022-demo-app";

// Updates the screen with the latest buffer.
void disp_flush() {
    ili9341_write(get_ili9341(), buf.buf);
}

// Exits the app, returning to the launcher.
void exit_to_launcher() {
    REG_WRITE(RTC_CNTL_STORE0_REG, 0);
    esp_restart();
}

// Minimum LED on time.
#define LED_ON_DURATION_MIN 1000
// Maximum LED on time.
#define LED_ON_DURATION_MAX 1250
// Maximum LED on time drift.
#define LED_ON_DURATION_DRIFT 50

// Minimum LED off time.
#define LED_OFF_DURATION_MIN 1500
// Maximum LED off time.
#define LED_OFF_DURATION_MAX 5000
// Maximum LED off time drift.
#define LED_OFF_DURATION_DRIFT 100

// Synchronisation error time.
#define LED_SYNC_ERROR_MIN 100
// Synchronisation error time.
#define LED_SYNC_ERROR_MAX 250

// Probability of hearing packet in perect.
#define PACKET_HEARD_PERCENT 70

// Current LED state.
volatile bool led_state = false;
// Current LED on time setting.
volatile int64_t led_on_duration;
// Current LED off time setting.
volatile int64_t led_off_duration;
// Start of the last blink time.
volatile int64_t last_blink_time;

// The LED TIME MUTEX.
SemaphoreHandle_t mtx;

// LEDs turning ON flag.
#define PACKET_FLAG_LED_ON  0x00000001
// LEDs turning OFF flag.
#define PACKET_FLAG_LED_OFF 0x00000002

#define I2C_WRITE_ADDR(x) ((x) << 1)
#define I2C_READ_ADDR(x) (((x) << 1) | 1)

typedef enum {
    SCREEN_SELECT,
    SCREEN_PROGRAM,
    SCREEN_VIRTUAL,
    SCREEN_REAL,
} screen_t;

static uint8_t const broadcast_mac[] = {0xff,0xff,0xff,0xff,0xff,0xff};
static uint8_t const packet_magic[] = "SAO.Firefly";
typedef struct {
    uint8_t magic[sizeof(packet_magic)];
    uint32_t flags;
    uint32_t total_duration;
} packet_t;

bool firefly_detect() {
    static uint8_t rxbuf[4096];
    
    // Dump the EEPROM.
    uint8_t txbuf[2] = {0, 0};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_WRITE_ADDR(0x50), false);
    i2c_master_write_byte(cmd, 0, false);
    i2c_master_write_byte(cmd, 0, false);
    i2c_master_start(cmd);
    i2c_master_read(cmd, rxbuf, sizeof(rxbuf), I2C_MASTER_ACK);
    i2c_master_stop(cmd);
    
    
}

void espnow_recv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    int64_t now = esp_timer_get_time() / 1000;
    
    if (data_len < sizeof(packet_t)) {
        // Too short; ignore this packet.
        return;
    }
    packet_t packet = *(packet_t const *) data;
    if (memcmp(packet.magic, packet_magic, sizeof(packet_magic))) {
        // Invalid magic; ignore this packet.
        return;
    }
    
    if (esp_random() % 100 >= PACKET_HEARD_PERCENT) {
        // Randomly throw out packet.
        return;
    }
    
    xSemaphoreTake(mtx, portMAX_DELAY);
    uint32_t total_duration = led_on_duration + led_off_duration;
    if (total_duration < packet.total_duration) {
        // We're too fase; increase cycle time.
        led_off_duration += (int) (esp_random() % LED_ON_DURATION_DRIFT) - LED_ON_DURATION_DRIFT / 4;
        if (led_off_duration > LED_OFF_DURATION_MAX) led_off_duration = LED_OFF_DURATION_MAX;
    } else if (total_duration > packet.total_duration) {
        // We're too slow; decrease cycle time.
        led_off_duration -= (int) (esp_random() % LED_ON_DURATION_DRIFT) - LED_ON_DURATION_DRIFT / 4;
        if (led_off_duration < LED_OFF_DURATION_MIN) led_off_duration = LED_OFF_DURATION_MIN;
    }
    
    if (packet.flags & PACKET_FLAG_LED_ON) {
        // LED turned on.
        ESP_LOGI("espnow", "Recv ON  packet");
        if (now - last_blink_time < led_on_duration + LED_OFF_DURATION_MIN) {
            // Cannot blink right now.
        } else if (!led_state && now > last_blink_time + led_on_duration + LED_OFF_DURATION_MIN) {
            // Acceptable timing; turns ON.
            last_blink_time = now + (int) (esp_random() % (LED_SYNC_ERROR_MAX - LED_SYNC_ERROR_MIN)) + LED_SYNC_ERROR_MIN;
        }
    }
    xSemaphoreGive(mtx);
}

void espnow_send_off() {
    packet_t packet;
    memcpy(packet.magic, packet_magic, sizeof(packet_magic));
    packet.flags = PACKET_FLAG_LED_OFF;
    packet.total_duration = led_on_duration + led_off_duration;
    esp_now_send(broadcast_mac, (void const *) &packet, sizeof(packet));
    ESP_LOGI("espnow", "Send OFF packet");
}

void espnow_send_on() {
    packet_t packet;
    memcpy(packet.magic, packet_magic, sizeof(packet_magic));
    packet.flags = PACKET_FLAG_LED_ON;
    packet.total_duration = led_on_duration + led_off_duration;
    esp_now_send(broadcast_mac, (void const *) &packet, sizeof(packet));
    ESP_LOGI("espnow", "Send ON  packet");
}

void espnow_init() {
    // Initialise WiFi AP.
    wifi_config_t wifi_config = {0};
    wifi_config.ap.authmode       = WIFI_AUTH_OPEN;
    wifi_config.ap.channel        = 1;
    strcpy((char *) wifi_config.ap.ssid, "Firefly");
    wifi_config.ap.ssid_len       = 0;
    wifi_config.ap.ssid_hidden    = 1;
    wifi_config.ap.max_connection = 1;
    
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    // Initialise the API.
    esp_now_init();
    // Register callback for incoming data.
    esp_now_register_recv_cb(espnow_recv);
    
    // Add the broadcast peer.
    esp_now_peer_info_t peer = {
        .channel = 0,
        .encrypt = false,
        .ifidx   = WIFI_IF_AP,
    };
    memcpy(peer.peer_addr, broadcast_mac, sizeof(broadcast_mac));
    esp_now_add_peer(&peer);
}

void randomise_times() {
    led_on_duration += (int) (esp_random() % LED_ON_DURATION_DRIFT) - LED_ON_DURATION_DRIFT / 2;
    if (led_on_duration < LED_ON_DURATION_MIN) led_on_duration = LED_ON_DURATION_MIN;
    if (led_on_duration > LED_ON_DURATION_MAX) led_on_duration = LED_ON_DURATION_MAX;
    
    led_off_duration += (int) (esp_random() % LED_OFF_DURATION_DRIFT) - LED_OFF_DURATION_DRIFT / 2;
    if (led_off_duration < LED_OFF_DURATION_MIN) led_off_duration = LED_OFF_DURATION_MIN;
    if (led_off_duration > LED_OFF_DURATION_MAX) led_off_duration = LED_OFF_DURATION_MAX;
}

void draw_debug() {
    // Debug information.
    pax_col_t col = led_state ? 0xffff0000 : 0xff3f0000;
    pax_draw_rect(&buf, col, 5, 5, 20, 20);
    char txtbuf[256];
    snprintf(txtbuf, sizeof(txtbuf)-1, "On:  %4llu\nOff: %4llu\nTot: %4llu", led_on_duration, led_off_duration, led_on_duration + led_off_duration);
    pax_draw_text(&buf, 0xffffffff, pax_font_sky_mono, 9, 30, 5, txtbuf);
}

void app_main() {
    ESP_LOGI(TAG, "Welcome to the template app!");

    // Initialize the screen, the I2C and the SPI busses.
    bsp_init();

    // Initialize the RP2040 (responsible for buttons, etc).
    bsp_rp2040_init();
    
    // This queue is used to receive button presses.
    buttonQueue = get_rp2040()->queue;
    
    // Initialize graphics for the screen.
    pax_buf_init(&buf, NULL, 320, 240, PAX_BUF_16_565RGB);
    
    // Init butterfly pins.
    rp2040_set_gpio_dir(get_rp2040(), FIREFLY_LED_PIN, true);
    
    // Initial randomisation.
    led_on_duration  = esp_random() % (LED_ON_DURATION_MAX  - LED_ON_DURATION_MIN)  + LED_ON_DURATION_MIN;
    led_off_duration = esp_random() % (LED_OFF_DURATION_MAX - LED_OFF_DURATION_MIN) + LED_OFF_DURATION_MIN;
    
    // Init mutex.
    mtx = xSemaphoreCreateMutex();
    
    // Init networking.
    nvs_flash_init();
    wifi_init();
    espnow_init();
    
    while (1) {
        int64_t now = esp_timer_get_time() / 1000;
        
        xSemaphoreTake(mtx, portMAX_DELAY);
        if (now > last_blink_time + led_on_duration && led_state) {
            // Turn OFF LED.
            led_state = false;
            rp2040_set_gpio_value(get_rp2040(), 1, true);
            espnow_send_off();
        } else if (now >= last_blink_time && (now < last_blink_time + led_on_duration || now > last_blink_time + led_on_duration + led_off_duration) && !led_state) {
            // Turn ON LED.
            led_state = true;
            last_blink_time = now;
            rp2040_set_gpio_value(get_rp2040(), 1, false);
            espnow_send_on();
            randomise_times();
        }
        xSemaphoreGive(mtx);
        
        // Check for button press.
        rp2040_input_message_t message;
        if (xQueueReceive(buttonQueue, &message, 1) && message.state) {
            if (message.input == RP2040_INPUT_BUTTON_HOME) {
                // If home is pressed, exit to launcher.
                exit_to_launcher();
            }
        }
    }
}

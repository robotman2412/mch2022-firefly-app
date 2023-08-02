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
#include "sao_eeprom.h"
#include "pax_codecs.h"

extern uint8_t firefly_qr_start[] asm("_binary_firefly_qr_png_start");
extern uint8_t firefly_qr_end[]   asm("_binary_firefly_qr_png_end");

int menu_pos = 1;
#define MENU_NUM 3

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

// Time between SAO detecting moments.
#define SAO_DETECT_INTERVAL 1000

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

// Last SAO detection time.
int64_t sao_detect_time = 0;
// Is there a firefly SAO?
bool sao_detected = false;
// Is the blinking enabled?
bool blink_enable = false;
// Current LED state.
volatile bool led_state = false;
// Current LED on time setting.
volatile int64_t led_on_duration;
// Current LED off time setting.
volatile int64_t led_off_duration;
// Start of the last blink time.
volatile int64_t last_blink_time;
// Random ID decided at startup.
uint32_t randid;
// Number of detected fireflies.
size_t firefly_count;

// The LED TIME MUTEX.
SemaphoreHandle_t mtx;

// Amount of IDs to keep track of at most.
#define ID_TABLE_LEN        1337
// Maximum age of IDs in milliseconds.
#define ID_TIMEOUT          6000
// LEDs turning ON flag.
#define PACKET_FLAG_LED_ON  0x00000001
// LEDs turning OFF flag.
#define PACKET_FLAG_LED_OFF 0x00000002
// Firefly detected flag.
#define PACKET_FLAG_SAO     0x00000004

// Randid buffer.
uint32_t id_table[ID_TABLE_LEN];
// Randid recv timestamp.
int64_t  id_time_table[ID_TABLE_LEN];

static uint8_t const broadcast_mac[] = {0xff,0xff,0xff,0xff,0xff,0xff};
static uint8_t const packet_magic[] = "SAO.Firefly";
typedef struct {
    uint8_t magic[sizeof(packet_magic)];
    uint32_t flags;
    uint32_t total_duration;
    uint32_t randid;
} packet_t;

SAO sao;
sao_driver_firefly_data_t firefly_data;

bool firefly_detect() {
    if (!sao_identify(&sao)) {
        for (size_t i = 0; i < sao.amount_of_drivers; i++) {
            if (!strcmp(sao.drivers[i].name, SAO_DRIVER_FIREFLY_NAME)) {
                firefly_data = sao.drivers[i].firefly;
                return true;
            }
        }
    }
    return false;
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
    
    bool has_update = false;
    for (size_t i = 0; i < ID_TABLE_LEN; i++) {
        if (id_table[i] == packet.randid) {
            id_time_table[i] = now;
            has_update = true;
        }
    }
    if (!has_update) {
        for (size_t i = 0; i < ID_TABLE_LEN; i++) {
            if (now > id_time_table[i] + ID_TIMEOUT) {
                id_table[i] = packet.randid;
                id_time_table[i] = now;
                break;
            }
        }
    }
    
    if (packet.flags & PACKET_FLAG_LED_ON) {
        // LED turned on.
        ESP_LOGD("espnow", "Recv ON  packet");
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
    packet.flags = PACKET_FLAG_LED_OFF | PACKET_FLAG_SAO * sao_detected;
    packet.total_duration = led_on_duration + led_off_duration;
    packet.randid = randid;
    esp_now_send(broadcast_mac, (void const *) &packet, sizeof(packet));
    ESP_LOGD("espnow", "Send OFF packet");
}

void espnow_send_on() {
    packet_t packet;
    memcpy(packet.magic, packet_magic, sizeof(packet_magic));
    packet.flags = PACKET_FLAG_LED_ON | PACKET_FLAG_SAO * sao_detected;
    packet.total_duration = led_on_duration + led_off_duration;
    packet.randid = randid;
    esp_now_send(broadcast_mac, (void const *) &packet, sizeof(packet));
    ESP_LOGD("espnow", "Send ON  packet");
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

void draw_ui() {
    pax_background(&buf, 0);
    
    if (!blink_enable && !sao_detected) {
        // Show an INFO.
        pax_insert_png_buf(&buf, firefly_qr_start, firefly_qr_end-firefly_qr_start, 104, 64, 0);
        pax_center_text(&buf, 0xffffffff, pax_font_saira_regular, 18, 160, 10, "Firefly not detected!");
        pax_center_text(&buf, 0xffffffff, pax_font_saira_regular, 18, 160, 28, "Scan the QR for more info:");
        pax_center_text(&buf, 0xffffffff, pax_font_saira_regular, 18, 160, 194, "If you want to proceed anyway,");
        pax_center_text(&buf, 0xffffffff, pax_font_saira_regular, 18, 160, 212, "Press the 🅰 button.");
        
    } else {
        if (!sao_detected) {
            pax_center_text(&buf, 0xffffffff, pax_font_saira_regular, 18, 160, 10, "Firefly not detected!");
        }
        char tmp[32];
        snprintf(tmp, sizeof(tmp)-1, "%d %s nearby.", firefly_count, firefly_count == 1 ? "firefly" : "fireflies");
        pax_center_text(&buf, 0xffffffff, pax_font_saira_regular, 18, 160, 212, tmp);
    }
    
    disp_flush();
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
    for (size_t i = 0; i < ID_TABLE_LEN; i++) {
        id_time_table[i] = 0;
    }
    
    // Init networking.
    randid = esp_random();
    nvs_flash_init();
    wifi_init();
    espnow_init();
    
    while (1) {
        int64_t now = esp_timer_get_time() / 1000;
        
        if (now > sao_detect_time + SAO_DETECT_INTERVAL) {
            bool pdet = sao_detected;
            sao_detected = firefly_detect();
            if (pdet && !sao_detected) {
                ESP_LOGI("firefly", "SAO firefly disconnected");
                blink_enable = false;
                draw_ui();
            } else if (pdet && sao_detected) {
                ESP_LOGI("firefly", "SAO firefly detected:");
                ESP_LOGI("firefly", "    Batch:  %d", firefly_data.batch_no);
                ESP_LOGI("firefly", "    Rev.:   %d", firefly_data.hardware_ver);
                ESP_LOGI("firefly", "    Serial: %d", firefly_data.serial_no_lo + firefly_data.serial_no_hi * 256);
                blink_enable = true;
                draw_ui();
            } else if (sao_detect_time == 0) {
                draw_ui();
            }
            sao_detect_time = now;
        }
        
        if (blink_enable) {
            xSemaphoreTake(mtx, portMAX_DELAY);
            size_t on = 0;
            for (size_t i = 0; i < ID_TABLE_LEN; i++) {
                if (id_time_table[i] + ID_TIMEOUT > now) {
                    on ++;
                }
            }
            if (firefly_count != on) {
                draw_ui();
            }
            firefly_count = on;
            
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
        }
        
        // Check for button press.
        rp2040_input_message_t message;
        if (xQueueReceive(buttonQueue, &message, 1) && message.state) {
            if (message.input == RP2040_INPUT_BUTTON_HOME) {
                // If home is pressed, exit to launcher.
                exit_to_launcher();
            } else if (message.input == RP2040_INPUT_BUTTON_ACCEPT) {
                // Enable the blinking.
                blink_enable = true;
            } else if (message.input == RP2040_INPUT_BUTTON_BACK) {
                // Disable the blinking if there is no SAO detected.
                blink_enable = sao_detected;
            }
            draw_ui();
        }
    }
}

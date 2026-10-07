#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_rom_sys.h"

// --- Pin Definitions ---
#define DIR_PIN_X    GPIO_NUM_5
#define STEP_PIN_X   GPIO_NUM_4
#define EN_PIN_X     GPIO_NUM_8   // DRV8825 ENABLE - active LOW

#define DIR_PIN_Y    GPIO_NUM_7
#define STEP_PIN_Y   GPIO_NUM_6
#define EN_PIN_Y     GPIO_NUM_9   // DRV8825 ENABLE - active LOW

#define SOLENOID_PIN GPIO_NUM_15

#define MODE0_PIN GPIO_NUM_14
#define MODE1_PIN GPIO_NUM_13
#define MODE2_PIN GPIO_NUM_12

// --- Machine Properties ---
// Adjust to your lead screw: steps_per_rev / mm_per_rev
// e.g. NEMA17 (200 steps) + 8mm pitch = 25 steps/mm
#define STEPS_PER_MM_X 800.0f
#define STEPS_PER_MM_Y 800.0f
#define STEP_DELAY_US  100

// --- State ---
float current_x = 0.0f;
float current_y = 0.0f;
float current_z = 0.0f;
static int incremental_mode = 0;  // 0 = G90 absolute, 1 = G91 incremental

#define BUF_SIZE 256

// --------------------------------------------------------------------------
// USB Serial JTAG helper
// --------------------------------------------------------------------------
static void usb_send(const char *str) {
    usb_serial_jtag_write_bytes((const uint8_t *)str, strlen(str), 20 / portTICK_PERIOD_MS);
}

// --------------------------------------------------------------------------
// Hardware init
// --------------------------------------------------------------------------
void init_hardware(void) {
    gpio_set_direction(DIR_PIN_X,    GPIO_MODE_OUTPUT);
    gpio_set_direction(STEP_PIN_X,   GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_PIN_X,     GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_PIN_Y,    GPIO_MODE_OUTPUT);
    gpio_set_direction(STEP_PIN_Y,   GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_PIN_Y,     GPIO_MODE_OUTPUT);
    gpio_set_direction(SOLENOID_PIN, GPIO_MODE_OUTPUT);

    gpio_set_level(EN_PIN_X, 1);     // Disabled at boot
    gpio_set_level(EN_PIN_Y, 1);
    gpio_set_level(SOLENOID_PIN, 0); // Off at boot

    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
    };
    usb_serial_jtag_driver_install(&cfg);

    gpio_set_direction(MODE0_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MODE1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MODE2_PIN, GPIO_MODE_OUTPUT);

    // FORZAR PASO COMPLETO (0, 0, 0) para ambos drivers
    gpio_set_level(MODE0_PIN, 1);
    gpio_set_level(MODE1_PIN, 1);
    gpio_set_level(MODE2_PIN, 1);
}

// --------------------------------------------------------------------------
// Move motors to absolute XY position using Bresenham stepping
// --------------------------------------------------------------------------
void move_to(float tx, float ty) {
    long sx = (long)round((tx - current_x) * STEPS_PER_MM_X);
    long sy = (long)round((ty - current_y) * STEPS_PER_MM_Y);

    if (sx == 0 && sy == 0) return;

    // Enable drivers (active LOW)
    if (sx != 0) gpio_set_level(EN_PIN_X, 0);
    if (sy != 0) gpio_set_level(EN_PIN_Y, 0);
    vTaskDelay(pdMS_TO_TICKS(5)); // DRV8825 settle time

    gpio_set_level(DIR_PIN_X, (sx >= 0) ? 1 : 0);
    gpio_set_level(DIR_PIN_Y, (sy >= 0) ? 1 : 0);

    sx = abs(sx);
    sy = abs(sy);

    long over = 0;
    if (sx >= sy) {
        for (long i = 0; i < sx; i++) {
            gpio_set_level(STEP_PIN_X, 1);
            over += sy;
            if (over >= sx) { over -= sx; gpio_set_level(STEP_PIN_Y, 1); }
            esp_rom_delay_us(STEP_DELAY_US);
            gpio_set_level(STEP_PIN_X, 0);
            gpio_set_level(STEP_PIN_Y, 0);
            esp_rom_delay_us(STEP_DELAY_US);
        }
    } else {
        for (long i = 0; i < sy; i++) {
            gpio_set_level(STEP_PIN_Y, 1);
            over += sx;
            if (over >= sy) { over -= sy; gpio_set_level(STEP_PIN_X, 1); }
            esp_rom_delay_us(STEP_DELAY_US);
            gpio_set_level(STEP_PIN_Y, 0);
            gpio_set_level(STEP_PIN_X, 0);
            esp_rom_delay_us(STEP_DELAY_US);
        }
    }

    current_x = tx;
    current_y = ty;

    // Disable to prevent overheating
    gpio_set_level(EN_PIN_X, 1);
    gpio_set_level(EN_PIN_Y, 1);
}

// --------------------------------------------------------------------------
// Fire solenoid punch
// --------------------------------------------------------------------------
void fire_solenoid(void) {
    gpio_set_level(SOLENOID_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(SOLENOID_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
}

// --------------------------------------------------------------------------
// G-Code parser — supports multi-token lines e.g. "G91 G0 X1.5"
//                 and G90/G91 absolute/incremental modes
// --------------------------------------------------------------------------
void execute_gcode(char *raw) {
    // Strip comments
    char *c = strchr(raw, ';');
    if (c) *c = '\0';

    // Local uppercase copy
    char line[BUF_SIZE];
    strncpy(line, raw, BUF_SIZE - 1);
    line[BUF_SIZE - 1] = '\0';
    for (int i = 0; line[i]; i++) {
        if (line[i] >= 'a' && line[i] <= 'z') line[i] -= 32;
    }

    char *p = line;
    while (*p == ' ') p++;
    if (*p == '\0') { usb_send("ok\n"); return; }

    // --- Scan coordinate parameters from the entire line ---
    float px = 0, py = 0, pz = 0;
    int hx = 0, hy = 0, hz = 0;
    char *q = p;
    while (*q) {
        if      (*q == 'X') { px = atof(q + 1); hx = 1; }
        else if (*q == 'Y') { py = atof(q + 1); hy = 1; }
        else if (*q == 'Z') { pz = atof(q + 1); hz = 1; }
        q++;
    }

    // --- Scan for G/M command words ---
    int has_motion = 0;
    q = p;
    while (*q) {
        if (*q == 'G') {
            int n = atoi(q + 1);
            if      (n == 90) incremental_mode = 0;
            else if (n == 91) incremental_mode = 1;
            else if (n == 0 || n == 1) has_motion = 1;
        } else if (*q == 'M') {
            int n = atoi(q + 1);
            if (n == 30) move_to(0.0f, 0.0f);
        }
        q++;
    }

    // --- Execute motion ---
    if (has_motion) {
        float tx = current_x, ty = current_y, tz = current_z;

        if (hx) tx = incremental_mode ? current_x + px : px;
        if (hy) ty = incremental_mode ? current_y + py : py;
        if (hz) tz = incremental_mode ? current_z + pz : pz;

        // Fire solenoid any time Z is commanded negative (remove state check to fix re-fire bug)
        if (hz && tz < 0.0f) fire_solenoid();
        current_z = 0.0f; // Always reset Z to safe state after solenoid logic

        if (tx != current_x || ty != current_y) move_to(tx, ty);
    }

    usb_send("ok\n");
}

// --------------------------------------------------------------------------
// Main task: read USB Serial JTAG, parse G-code line by line
// --------------------------------------------------------------------------
void gcode_task(void *pvParameters) {
    uint8_t buf[BUF_SIZE];
    char    line[BUF_SIZE];
    int     idx = 0;

    vTaskDelay(pdMS_TO_TICKS(500));
    usb_send("Grbl 1.1h ['$' for help]\n");
    usb_send("ok\n");

    while (1) {
        int len = usb_serial_jtag_read_bytes(buf, sizeof(buf) - 1, 20 / portTICK_PERIOD_MS);

        for (int i = 0; i < len; i++) {
            char ch = (char)buf[i];
            if (ch == '\n' || ch == '\r') {
                if (idx > 0) {
                    line[idx] = '\0';
                    execute_gcode(line);
                    idx = 0;
                }
            } else {
                if (idx < BUF_SIZE - 1) line[idx++] = ch;
            }
        }

        if (len == 0) vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// --------------------------------------------------------------------------
// Entry point
// --------------------------------------------------------------------------
void app_main(void) {
    init_hardware();
    xTaskCreate(gcode_task, "gcode_parser", 4096, NULL, 10, NULL);
}
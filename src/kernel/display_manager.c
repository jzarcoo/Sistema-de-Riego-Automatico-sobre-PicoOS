/**
 * @file display_manager.c
 * @brief Driver del LCD 2004A (20x4) via I2C (PCF8574 + HD44780).
 *
 * Layout fijo — cada fila se actualiza independientemente.
 * El diff buffer solo reescribe filas que cambiaron (1 transaccion
 * I2C batch por fila modificada).
 */

#include "display_manager.h"
#include "kernel_hw_config.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define LCD_I2C_ADDR    0x27
#define LCD_COLS        20
#define LCD_ROWS        4

#define LCD_BACKLIGHT   0x08
#define LCD_EN          0x04
#define LCD_RS          0x01

#define LCD_CMD_CLEAR        0x01
#define LCD_CMD_ENTRY_MODE   0x06
#define LCD_CMD_DISPLAY_ON   0x0C
#define LCD_CMD_FUNCTION_SET 0x28
#define LCD_CMD_SET_DDRAM    0x80

#define I2C_PORT i2c0

static const uint8_t row_offsets[4] = {0x00, 0x40, 0x14, 0x54};

static bool lcd_connected = false;
static int lcd_fail_count = 0;
static int lcd_refresh_count = 0;

#define LCD_MAX_FAILURES    3
#define LCD_RETRY_INTERVAL  50

static char front_buf[LCD_ROWS][LCD_COLS];
static char back_buf[LCD_ROWS][LCD_COLS];

#define TX_BUF_SIZE 88
static uint8_t tx_buf[TX_BUF_SIZE];
static int tx_pos;

static void lcd_i2c_write_buf(uint8_t *buf, int len) {
    int ret = i2c_write_timeout_us(I2C_PORT, LCD_I2C_ADDR, buf, len, false, 10000);
    if (ret < 0) {
        lcd_fail_count++;
        if (lcd_fail_count >= LCD_MAX_FAILURES) {
            lcd_connected = false;
        }
    } else {
        lcd_fail_count = 0;
    }
}

static void tx_append_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    tx_buf[tx_pos++] = data | LCD_EN;
    tx_buf[tx_pos++] = data;
}

static void tx_append_byte(uint8_t val, uint8_t mode) {
    tx_append_nibble(val & 0xF0, mode);
    tx_append_nibble((val << 4) & 0xF0, mode);
}

static void lcd_cmd_single(uint8_t cmd) {
    uint8_t buf[4];
    uint8_t hi = (cmd & 0xF0) | LCD_BACKLIGHT;
    uint8_t lo = ((cmd << 4) & 0xF0) | LCD_BACKLIGHT;
    buf[0] = hi | LCD_EN;
    buf[1] = hi;
    buf[2] = lo | LCD_EN;
    buf[3] = lo;
    lcd_i2c_write_buf(buf, 4);
}

static void lcd_nibble_single(uint8_t nibble) {
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT;
    uint8_t buf[2] = { data | LCD_EN, data };
    lcd_i2c_write_buf(buf, 2);
}

void display_manager_init(void) {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(DISPLAY_SDA_PIN);
    gpio_pull_up(DISPLAY_SCL_PIN);

    uint8_t rxdata;
    int ret = i2c_read_timeout_us(I2C_PORT, LCD_I2C_ADDR, &rxdata, 1, false, 5000);
    if (ret < 0) {
        printf("[LCD] No responde en 0x%02X.\n", LCD_I2C_ADDR);
        return;
    }
    lcd_connected = true;
    printf("[LCD] Conectado (0x%02X).\n", LCD_I2C_ADDR);

    sleep_ms(50);
    lcd_nibble_single(0x30); sleep_ms(5);
    lcd_nibble_single(0x30); sleep_ms(1);
    lcd_nibble_single(0x30);
    lcd_nibble_single(0x20);

    lcd_cmd_single(LCD_CMD_FUNCTION_SET);
    lcd_cmd_single(LCD_CMD_DISPLAY_ON);
    lcd_cmd_single(LCD_CMD_CLEAR);
    sleep_ms(2);
    lcd_cmd_single(LCD_CMD_ENTRY_MODE);

    memset(front_buf, ' ', sizeof(front_buf));
    memset(back_buf, ' ', sizeof(back_buf));

    printf("[LCD] Inicializado (20x4)\n");
}

void display_manager_set_row(int row, const char *text) {
    if (row < 0 || row >= LCD_ROWS) return;

    memset(back_buf[row], ' ', LCD_COLS);
    for (int i = 0; i < LCD_COLS && text[i] != '\0'; i++) {
        back_buf[row][i] = text[i];
    }
}

void display_manager_refresh(void) {
    if (!lcd_connected) {
        lcd_refresh_count++;
        if (lcd_refresh_count >= LCD_RETRY_INTERVAL) {
            lcd_refresh_count = 0;
            uint8_t rxdata;
            int ret = i2c_read_timeout_us(I2C_PORT, LCD_I2C_ADDR, &rxdata, 1, false, 5000);
            if (ret >= 0) {
                lcd_connected = true;
                lcd_fail_count = 0;
                printf("[LCD] Reconectado.\n");
            }
        }
        return;
    }

    for (int r = 0; r < LCD_ROWS; r++) {
        if (memcmp(front_buf[r], back_buf[r], LCD_COLS) != 0) {
            tx_pos = 0;
            uint8_t cmd = LCD_CMD_SET_DDRAM | row_offsets[r];
            tx_append_byte(cmd, 0);
            for (int c = 0; c < LCD_COLS; c++) {
                tx_append_byte(back_buf[r][c], LCD_RS);
            }
            lcd_i2c_write_buf(tx_buf, tx_pos);
            if (!lcd_connected) return;
            memcpy(front_buf[r], back_buf[r], LCD_COLS);
        }
    }
}

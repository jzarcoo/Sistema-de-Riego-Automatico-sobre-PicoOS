/**
 * @file display_manager.c
 * @brief Driver del LCD 2004A (20x4) via I2C (PCF8574 + HD44780).
 *
 * Optimizacion: en lugar de una transaccion I2C por cada nibble,
 * se arma un buffer con todos los bytes de una fila completa y
 * se envia en una sola transaccion I2C. Esto reduce el overhead
 * de start/stop/ACK de ~320 transacciones a ~4 por update.
 */

#include "display_manager.h"
#include "kernel_hw_config.h"

#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define LCD_I2C_ADDR         0x27

#define LCD_COLS             20
#define LCD_ROWS             4

#define LCD_CMD_CLEAR       0x01
#define LCD_CMD_ENTRY_MODE  0x06
#define LCD_CMD_DISPLAY_ON  0x0C
#define LCD_CMD_FUNCTION_SET 0x28
#define LCD_CMD_SET_DDRAM    0x80

#define I2C_PORT             i2c0

static const uint8_t row_offsets[4] = {0x00, 0x40, 0x14, 0x54};

static bool lcd_connected = false;

static char front_buf[LCD_ROWS][LCD_COLS + 1];
static char back_buf[LCD_ROWS][LCD_COLS + 1];

/* Buffer para batchar una fila entera en una transaccion I2C.
 * Por caracter: 4 bytes (high nibble EN=1, EN=0, low nibble EN=1, EN=0)
 * Set cursor: 4 bytes (cmd nibble pair)
 * Max: 4 + (20 * 4) = 84 bytes por fila */
#define TX_BUF_SIZE 88
static uint8_t tx_buf[TX_BUF_SIZE];

static void lcd_i2c_write_buf(uint8_t *buf, int len) {
    i2c_write_timeout_us(I2C_PORT, LCD_I2C_ADDR, buf, len, false, 50000);
}

static int tx_pos;

static void tx_append_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | lcd_backlight;
    tx_buf[tx_pos++] = data | LCD_EN;
    tx_buf[tx_pos++] = data;
}

static void tx_append_byte(uint8_t val, uint8_t mode) {
    tx_append_nibble(val & 0xF0, mode);
    tx_append_nibble((val << 4) & 0xF0, mode);
}

static void lcd_cmd_slow(uint8_t cmd) {
    uint8_t buf[4];
    uint8_t hi = (cmd & 0xF0) | lcd_backlight;
    uint8_t lo = ((cmd << 4) & 0xF0) | lcd_backlight;
    buf[0] = hi | LCD_EN;
    buf[1] = hi;
    buf[2] = lo | LCD_EN;
    buf[3] = lo;
    lcd_i2c_write_buf(buf, 4);
}

static void lcd_send_nibble_slow(uint8_t nibble) {
    uint8_t data = (nibble & 0xF0) | lcd_backlight;
    uint8_t buf[2] = { data | LCD_EN, data };
    lcd_i2c_write_buf(buf, 2);
}

/**
 * @brief Inicializa LCD.
 */
void display_manager_init(void) {

    i2c_init(I2C_PORT, 400000);

    gpio_set_function(
        DISPLAY_SDA_PIN,
        GPIO_FUNC_I2C
    );

    gpio_set_function(
        DISPLAY_SCL_PIN,
        GPIO_FUNC_I2C
    );

    gpio_pull_up(DISPLAY_SDA_PIN);
    gpio_pull_up(DISPLAY_SCL_PIN);

    printf("[LCD] Escaneando I2C...\n");
    uint8_t rxdata;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        int ret = i2c_read_timeout_us(I2C_PORT, addr, &rxdata, 1, false, 2000);
        if (ret >= 0) {
            printf("[LCD] Encontrado en 0x%02X\n", addr);
            lcd_connected = true;
        }
    }
    if (!lcd_connected) {
        printf("[LCD] No encontrado.\n");
        return;
    }

    /* Init HD44780 modo 4-bit (secuencia por datasheet) */
    sleep_ms(50);
    lcd_send_nibble_slow(0x30); sleep_ms(5);
    lcd_send_nibble_slow(0x30); sleep_ms(1);
    lcd_send_nibble_slow(0x30);
    lcd_send_nibble_slow(0x20);

    lcd_cmd_slow(LCD_CMD_FUNCTION_SET);
    lcd_cmd_slow(LCD_CMD_DISPLAY_ON);
    lcd_cmd_slow(LCD_CMD_CLEAR);
    sleep_ms(2);
    lcd_cmd_slow(LCD_CMD_ENTRY_MODE);

    lcd_cmd(LCD_CMD_ENTRY_MODE);

    memset(
        front_buf,
        ' ',
        sizeof(front_buf)
    );

    memset(
        back_buf,
        ' ',
        sizeof(back_buf)
    );
}

/**
 * @brief Actualiza contenido del LCD.
 */
void display_manager_update(
    const char *text
) {

    if (!lcd_connected || text == NULL) {
        return;
    }

    memset(
        back_buf,
        ' ',
        sizeof(back_buf)
    );

    int row = 0;
    int col = 0;

    for (int i = 0;
         text[i] != '\0'
         && row < LCD_ROWS;
         i++) {

    memset(back_buf, ' ', sizeof(back_buf));
    int row = 0, col = 0;
    for (int i = 0; text[i] != '\0' && row < LCD_ROWS; i++) {
        if (text[i] == '\n') {

            row++;
            col = 0;

        } else {

            if (col < LCD_COLS) {

                back_buf[row][col] =
                    text[i];

                col++;
            }
        }
    }

    for (int r = 0; r < LCD_ROWS; r++) {
        if (memcmp(front_buf[r], back_buf[r], LCD_COLS) != 0) {
            tx_pos = 0;

            /* Set cursor command (DDRAM address) */
            uint8_t cmd = LCD_CMD_SET_DDRAM | row_offsets[r];
            tx_append_byte(cmd, 0);

            /* Todos los caracteres de la fila */
            for (int c = 0; c < LCD_COLS; c++) {
                tx_append_byte(back_buf[r][c], LCD_RS);
            }

            /* Enviar todo en UNA transaccion I2C */
            lcd_i2c_write_buf(tx_buf, tx_pos);

            memcpy(front_buf[r], back_buf[r], LCD_COLS);
        }
    }
}
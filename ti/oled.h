#ifndef OLED_H
#define OLED_H

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "drv_err.h"

/* SSD1306 硬件参数 */
typedef enum {
    OLED_I2C_ADDR = 0x78, /* SSD1306 I2C 地址 */
    OLED_WIDTH = 128,     /* 显示宽度（像素） */
    OLED_HEIGHT = 64,     /* 显示高度（像素） */
    OLED_PAGES = 8,       /* 页数（高度/8） */
} oled_hw_param_t;

/* 字体尺寸 */
typedef enum {
    OLED_FONT_6X8 = 6,
    OLED_FONT_8X16 = 8,
} oled_font_size_t;

/* OLED 定时刷新标志位（ISR 置1，task 清0） */
extern volatile uint8_t oled_tick_flag;

drv_err_t oled_init(I2C_Regs *hi2c);
void oled_task(void);
void oled_clear(void);
void oled_update(void);
void oled_show_char(int16_t x, int16_t y, char ch, uint8_t font_size);
void oled_show_string(int16_t x, int16_t y, const char *str, uint8_t font_size);
void oled_show_num(int16_t x, int16_t y, uint32_t num, uint8_t len, uint8_t font_size);
void oled_show_signed_num(
    int16_t x, int16_t y, int32_t num, uint8_t len, uint8_t font_size);
void oled_draw_point(int16_t x, int16_t y);
void oled_error_callback(I2C_Regs *hi2c);

#endif

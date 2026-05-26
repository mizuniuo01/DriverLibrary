#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stm32f4xx_hal.h>

#define OLED_I2C_ADDR 0x78 /* SSD1306 I2C 地址 */
#define OLED_WIDTH 128     /* 显示宽度（像素） */
#define OLED_HEIGHT 64     /* 显示高度（像素） */
#define OLED_PAGES 8       /* 页数（高度/8） */

#define OLED_FONT_6X8 6  /* 6×8 字体 */
#define OLED_FONT_8X16 8 /* 8×16 字体 */

void oled_init(I2C_HandleTypeDef *hi2c);
void oled_clear(void);
void oled_update(void);
void oled_show_char(int16_t x, int16_t y, char ch, uint8_t font_size);
void oled_show_string(int16_t x, int16_t y, const char *str, uint8_t font_size);
void oled_show_num(int16_t x, int16_t y, uint32_t num, uint8_t len, uint8_t font_size);
void oled_show_signed_num(
    int16_t x, int16_t y, int32_t num, uint8_t len, uint8_t font_size);
void oled_draw_point(int16_t x, int16_t y);

#endif

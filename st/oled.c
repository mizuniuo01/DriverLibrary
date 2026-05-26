/**
 * @file    oled.c
 * @brief   SSD1306 OLED 驱动模块（0.96 寸，I2C，128×64）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    仅适配 0.96 寸 SSD1306 I2C 接口 128×64 OLED，不支持 SPI
 * @note    依赖：I2C 外设已在 CubeMX 中配置
 * @note    使用 HAL_I2C_Mem_Write 发送命令和数据
 *
 * @usage
 * oled_init(&hi2c1);
 * oled_clear();
 * oled_show_string(0, 0, "Hello", OLED_FONT_8X16);
 * oled_update();
 */

#include "oled.h"
#include "oled_data.h"
#include <string.h>

/* 显存 */
static uint8_t oled_buffer[OLED_PAGES][OLED_WIDTH];
static I2C_HandleTypeDef *oled_i2c;

/*
 * I2C 超时（ms）。SSD1306 单页刷新 128 字节约需 4ms，
 * 全屏刷新约 32ms，100ms 留有充足余量应对时钟拉伸和总线竞争
 */
#define OLED_I2C_TIMEOUT_MS 100

/* SSD1306 初始化命令序列 */
static const uint8_t oled_init_cmds[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
                                         0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
                                         0xDB, 0x30, 0xA4, 0xA6, 0x8D, 0x14, 0xAF};

/**
 * @brief  发送命令字节
 * @param  cmd  命令字节
 * @retval 无
 */
static void oled_write_cmd(uint8_t cmd)
{
    HAL_I2C_Mem_Write(
        oled_i2c, OLED_I2C_ADDR << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, OLED_I2C_TIMEOUT_MS);
}

/**
 * @brief  OLED 初始化
 * @param  hi2c  I2C 外设句柄
 * @retval 无
 */
void oled_init(I2C_HandleTypeDef *hi2c)
{
    uint8_t i;

    if (!hi2c) {
        return;
    }

    oled_i2c = hi2c;

    for (i = 0; i < sizeof(oled_init_cmds); i++) {
        oled_write_cmd(oled_init_cmds[i]);
    }

    memset(oled_buffer, 0, sizeof(oled_buffer));
}

/**
 * @brief  清除显存
 * @param  无
 * @retval 无
 */
void oled_clear(void)
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

/**
 * @brief  将显存刷新到 OLED
 * @note   逐页发送显存数据到 SSD1306
 * @param  无
 * @retval 无
 */
void oled_update(void)
{
    uint8_t page;

    for (page = 0; page < OLED_PAGES; page++) {
        uint8_t cmds[3];

        cmds[0] = 0xB0 | page;
        cmds[1] = 0x00;
        cmds[2] = 0x10;

        HAL_I2C_Mem_Write(
            oled_i2c, OLED_I2C_ADDR << 1, 0x00, I2C_MEMADD_SIZE_8BIT, cmds, 3, OLED_I2C_TIMEOUT_MS);
        HAL_I2C_Mem_Write(oled_i2c,
                          OLED_I2C_ADDR << 1,
                          0x40,
                          I2C_MEMADD_SIZE_8BIT,
                          oled_buffer[page],
                          OLED_WIDTH,
                          OLED_I2C_TIMEOUT_MS);
    }
}

/**
 * @brief  绘制一个像素点
 * @param  x  横坐标
 * @param  y  纵坐标
 * @retval 无
 */
void oled_draw_point(int16_t x, int16_t y)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    oled_buffer[y / 8][x] |= (1 << (y % 8));
}

/**
 * @brief  显示一个字符
 * @param  x         横坐标
 * @param  y         纵坐标
 * @param  ch        字符
 * @param  font_size 字体大小
 * @retval 无
 */
void oled_show_char(int16_t x, int16_t y, char ch, uint8_t font_size)
{
    uint8_t i;
    uint8_t j;
    uint8_t temp;

    if (x < 0 || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    ch = ch - ' ';

    if (font_size == OLED_FONT_8X16) {
        for (i = 0; i < 16; i++) {
            temp = oled_font_8x16[(uint8_t)ch][i];
            for (j = 0; j < 8; j++) {
                if (temp & (0x80 >> j)) {
                    oled_draw_point(x + j, y + i);
                } else {
                    if (x + j < OLED_WIDTH && y + i < OLED_HEIGHT) {
                        oled_buffer[(y + i) / 8][x + j] &= ~(1 << ((y + i) % 8));
                    }
                }
            }
        }
    } else {
        for (i = 0; i < 6; i++) {
            temp = oled_font_6x8[(uint8_t)ch][i];
            for (j = 0; j < 8; j++) {
                if (temp & (0x80 >> j)) {
                    oled_draw_point(x + j, y + i);
                } else {
                    if (x + j < OLED_WIDTH && y + i < OLED_HEIGHT) {
                        oled_buffer[(y + i) / 8][x + j] &= ~(1 << ((y + i) % 8));
                    }
                }
            }
        }
    }
}

/**
 * @brief  显示字符串
 * @param  x         横坐标
 * @param  y         纵坐标
 * @param  str       字符串
 * @param  font_size 字体大小
 * @retval 无
 */
void oled_show_string(int16_t x, int16_t y, const char *str, uint8_t font_size)
{
    if (!str) {
        return;
    }

    while (*str) {
        oled_show_char(x, y, *str, font_size);
        x += font_size;
        if (x >= OLED_WIDTH) {
            x = 0;
            y += (font_size == OLED_FONT_8X16) ? 16 : 8;
        }
        str++;
    }
}

/**
 * @brief  显示无符号数字
 * @param  x         横坐标
 * @param  y         纵坐标
 * @param  num       数字
 * @param  len       显示位数
 * @param  font_size 字体大小
 * @retval 无
 */
void oled_show_num(int16_t x, int16_t y, uint32_t num, uint8_t len, uint8_t font_size)
{
    uint8_t i;
    uint8_t digit;

    for (i = 0; i < len; i++) {
        digit = num % 10;
        oled_show_char(x + (len - 1 - i) * font_size, y, '0' + digit, font_size);
        num /= 10;
    }
}

/**
 * @brief  显示有符号数字
 * @param  x         横坐标
 * @param  y         纵坐标
 * @param  num       数字
 * @param  len       显示位数（不含符号）
 * @param  font_size 字体大小
 * @retval 无
 */
void oled_show_signed_num(
    int16_t x, int16_t y, int32_t num, uint8_t len, uint8_t font_size)
{
    if (num < 0) {
        oled_show_char(x, y, '-', font_size);
        num = -num;
        x += font_size;
    }
    oled_show_num(x, y, (uint32_t)num, len, font_size);
}

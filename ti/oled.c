/**
 * @file    oled.c
 * @brief   SSD1306 OLED 驱动模块（0.96 寸，I2C，128×64）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    仅适配 0.96 寸 SSD1306 I2C 接口 128×64 OLED，不支持 SPI
 * @note    依赖：I2C 外设已在 SysConfig 中配置
 * @note    非阻塞 I2C 状态机，oled_task 由 oled_tick_flag 驱动
 *
 * @usage
 * oled_init(I2C_OLED_INST);
 * oled_clear();
 * oled_show_string(0, 0, "Hello", OLED_FONT_8X16);
 * oled_update();
 * // ISR 中周期性置 oled_tick_flag = 1
 * // 主循环中调用 oled_task();
 */

#include "oled.h"
#include "oled_data.h"
#include <string.h>

/* 显存 */
static uint8_t oled_buffer[OLED_PAGES][OLED_WIDTH];

/* I2C 发送状态机 */
typedef enum {
    OLED_TX_IDLE = 0,
    OLED_TX_BUSY,
} oled_tx_state_t;

typedef enum {
    OLED_RUN_IDLE = 0,
    OLED_RUN_INIT,
    OLED_RUN_UPDATE,
} oled_run_state_t;

static I2C_Regs *oled_i2c;
static oled_tx_state_t oled_tx_state = OLED_TX_IDLE;
static oled_run_state_t oled_run_state = OLED_RUN_IDLE;

volatile uint8_t oled_tick_flag;

/* SSD1306 初始化命令序列 */
static const uint8_t oled_init_cmds[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
    0xDB, 0x30, 0xA4, 0xA6, 0x8D, 0x14, 0xAF
};

/* 当前 I2C 发送的单字节缓冲 */
static uint8_t oled_tx_byte;

/**
 * @brief  启动 I2C 发送（控制字节 + 数据）
 * @param  ctrl  控制字节（0x00=命令, 0x40=数据）
 * @param  data  数据指针
 * @param  count 数据长度
 * @retval 无
 */
static void oled_tx_start(uint8_t ctrl, const uint8_t *data,
                          uint16_t count)
{
    DL_I2C_fillControllerTXFIFO(oled_i2c, &ctrl, 1);
    if (count > 0) {
        DL_I2C_fillControllerTXFIFO(oled_i2c, data, count);
    }
    DL_I2C_startControllerTransfer(oled_i2c, OLED_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1 + count);
    oled_tx_state = OLED_TX_BUSY;
}

/**
 * @brief  发送单字节
 * @param  ctrl  控制字节
 * @param  byte  数据字节
 * @retval 无
 */
static void oled_tx_byte_start(uint8_t ctrl, uint8_t byte)
{
    oled_tx_byte = byte;
    oled_tx_start(ctrl, &oled_tx_byte, 1);
}

/**
 * @brief  OLED 初始化
 * @param  hi2c  I2C 外设句柄
 * @retval 无
 */
void oled_init(I2C_Regs *hi2c)
{
    if (!hi2c) {
        return;
    }

    NVIC_EnableIRQ(I2C_OLED_INST_INT_IRQN);
    DL_I2C_setTimeoutACount(I2C_OLED_INST, 0xFFFF);
    DL_I2C_enableTimeoutA(I2C_OLED_INST);

    oled_i2c = hi2c;
    oled_run_state = OLED_RUN_IDLE;
    oled_tx_state = OLED_TX_IDLE;

    memset(oled_buffer, 0, sizeof(oled_buffer));
}

/**
 * @brief  I2C 错误回调（ISR 中调用）
 * @param  hi2c  I2C 句柄
 * @retval 无
 */
void oled_error_callback(I2C_Regs *hi2c)
{
    if (!hi2c || hi2c != oled_i2c) {
        return;
    }

    DL_I2C_resetControllerTransfer(hi2c);
    DL_I2C_clearInterruptStatus(hi2c, 0xFFFFFFFF);
    oled_tx_state = OLED_TX_IDLE;
}

/**
 * @brief  等待 I2C 发送完成（轮询控制器状态）
 * @param  无
 * @retval 无
 */
static void oled_wait_tx_done(void)
{
    while (!(DL_I2C_getControllerStatus(oled_i2c)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        /* 等待 I2C 控制器空闲 */
    }
}

/**
 * @brief  刷新一页到 OLED
 * @param  page  页号（0~7）
 * @retval 无
 */
static void oled_update_page(uint8_t page)
{
    uint8_t cmds[3];

    cmds[0] = 0xB0 | page;
    cmds[1] = 0x00;
    cmds[2] = 0x10;

    oled_tx_start(0x00, cmds, 3);
    oled_wait_tx_done();

    oled_tx_start(0x40, oled_buffer[page], OLED_WIDTH);
    oled_wait_tx_done();
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
 * @param  无
 * @retval 无
 */
void oled_update(void)
{
    uint8_t page;

    for (page = 0; page < OLED_PAGES; page++) {
        oled_update_page(page);
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
 * @param  font_size 字体大小（OLED_FONT_6X8 或 OLED_FONT_8X16）
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
                        oled_buffer[(y + i) / 8][x + j] &=
                            ~(1 << ((y + i) % 8));
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
                        oled_buffer[(y + i) / 8][x + j] &=
                            ~(1 << ((y + i) % 8));
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
void oled_show_string(int16_t x, int16_t y, const char *str,
                      uint8_t font_size)
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
void oled_show_num(int16_t x, int16_t y, uint32_t num, uint8_t len,
                   uint8_t font_size)
{
    uint8_t i;
    uint8_t digit;

    for (i = 0; i < len; i++) {
        digit = num % 10;
        oled_show_char(x + (len - 1 - i) * font_size, y,
                       '0' + digit, font_size);
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
void oled_show_signed_num(int16_t x, int16_t y, int32_t num,
                          uint8_t len, uint8_t font_size)
{
    if (num < 0) {
        oled_show_char(x, y, '-', font_size);
        num = -num;
        x += font_size;
    }
    oled_show_num(x, y, (uint32_t)num, len, font_size);
}

/**
 * @brief  发送初始化命令
 * @param  无
 * @retval 无
 */
static void oled_run_init(void)
{
    uint8_t i;

    /* 逐字节发送初始化命令 */
    for (i = 0; i < sizeof(oled_init_cmds); i++) {
        oled_tx_byte_start(0x00, oled_init_cmds[i]);
        oled_wait_tx_done();
    }

    oled_run_state = OLED_RUN_UPDATE;
}

/**
 * @brief  OLED 周期性任务（主循环中调用）
 * @note   由 oled_tick_flag 驱动，非阻塞
 * @param  无
 * @retval 无
 */
void oled_task(void)
{
    if (!oled_tick_flag) {
        return;
    }
    oled_tick_flag = 0;

    switch (oled_run_state) {
        case OLED_RUN_IDLE:
            oled_run_state = OLED_RUN_INIT;
            break;

        case OLED_RUN_INIT:
            oled_run_init();
            break;

        case OLED_RUN_UPDATE:
            oled_update();
            break;

        default:
            oled_run_state = OLED_RUN_IDLE;
            break;
    }
}

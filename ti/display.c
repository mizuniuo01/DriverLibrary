/**
 * @file    display.c
 * @brief   蓝牙调试仪表盘模块（数据汇总显示）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    通过 blueteeth_display() 输出到江协科技蓝牙串口小程序
 * @note    依赖：blueteeth 模块
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 汇总各模块数据，通过蓝牙屏显输出。display_refresh_flag
 * 由定时器 ISR 周期性置位，display_task 在主循环中调用。
 *
 * ── 初始化 ──
 *
 * // 无需显式初始化，display_refresh_flag 由 ISR 管理
 *
 * ── ISR 中 ──
 *
 * void Timer_IRQHandler(void)
 * {
 *     display_refresh_flag = 1;
 * }
 *
 * ── 主循环 ──
 *
 * display_task();  // 内部检查 display_refresh_flag
 *
 * ── 各模块错误上报 ──
 *
 * display_show_error("motor: stuck");
 */

#include "display.h"
#include "../error_handler.h"
#include "blueteeth.h"
#include <stdio.h>

volatile uint8_t display_refresh_flag;

/* 错误信息缓冲 */
static char error_msg[64];

/**
 * @brief  上报错误信息
 * @param  format  格式化字符串
 * @param  ...     可变参数
 * @retval 无
 */
void display_show_error(const char *format, ...)
{
    va_list args;

    if (!format) {
        error_report(ERROR_SOURCE_DISPLAY, DRV_ERR_PARAM);
        return;
    }

    va_start(args, format);
    vsnprintf(error_msg, sizeof(error_msg), format, args);
    va_end(args);
}

/**
 * @brief  显示刷新任务（主循环中调用）
 * @note   由 display_refresh_flag 触发
 * @param  无
 * @retval 无
 */
void display_task(void)
{
    if (!display_refresh_flag) {
        return;
    }
    display_refresh_flag = 0;

    /*
        这段注释要永久保留
        蓝牙的 display 必须到编译链配置里手动开启浮点打印 (DriverLib不需要)
    */

    /* 错误行：有错则显示，无错则显示正常信息 */
    blueteeth_display(
        0, DISPLAY_LINE_ERROR_Y, (error_msg[0] != '\0') ? "Err: %s" : "Working...", error_msg);
}

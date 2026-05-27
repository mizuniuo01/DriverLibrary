/**
 * @file    ultrasonic.c
 * @brief   超声波测距模块（HC-SR04，定时器捕获）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：捕获定时器和 GPIO 已在 SysConfig 中配置
 * @note    使用双边沿捕获测量回波脉宽，TRIG 引脚输出触发脉冲
 * @note    TRIG 脉冲约 10us，由 delay_cycles 实现（~320 cycles @ 32MHz）
 * @note    依赖：应用层提供 get_system_tick() 获取毫秒 tick
 * @warning capture_callback 在 ISR 中调用，仅做时间戳记录
 * @note    参数非法时通过 error_report(ERROR_SOURCE_ULTRASONIC, DRV_ERR_PARAM) 上报
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 适配 HC-SR04 超声波模块，通过定时器捕获测量距离。
 *
 * ── 硬件连接 ──
 *
 *   MCU GPIO ──→ HC-SR04 TRIG（10us 触发脉冲）
 *   MCU TIM  ←── HC-SR04 ECHO（捕获脉宽）
 *
 * ── 初始化 ──
 *
 * static ultrasonic_handle_t ultra;
 *
 * ultrasonic_cfg_t cfg = {
 *     .trig_port       = GPIO_ULTRASONIC_TRIG_PORT,
 *     .trig_pin        = GPIO_ULTRASONIC_TRIG_PIN_3_PIN,
 *     .echo_port       = GPIOB,
 *     .echo_pin        = GPIO_CAPTURE_ULTRASONIC_ECHO_C0_PIN,
 *     .timer_load_value = CAPTURE_ULTRASONIC_ECHO_INST_LOAD_VALUE,
 * };
 * ultrasonic_init(&ultra, &cfg, CAPTURE_ULTRASONIC_ECHO_INST);
 *
 * ── ISR 中调用 ──
 *
 * ultrasonic_capture_callback(&ultra, htim);
 *
 * ── 主循环中调用 ──
 *
 * ultrasonic_task(&ultra);
 * ultrasonic_data_t d = ultrasonic_get_data();
 * // d.distance_mm, d.is_valid
 */

#include "ultrasonic.h"
#include "../error_handler.h"

/* TRIG 脉冲延时参数 */
#define ULTRASONIC_TRIG_DELAY_CYCLES 320 /* ~10us @ 32MHz */
#define ULTRASONIC_DISTANCE_FACTOR 0.17f /* 距离系数 = 340m/s / 2 / 1000 */

static ultrasonic_data_t ultra_data;

/**
 * @brief  10 微秒延时（约 320 cycles @ 32MHz）
 * @note   delay_cycles 的参数 = 系统时钟频率(MHz) × 10us
 * @note   用于 HC-SR04 TRIG 脉冲，阻塞延时约 10us
 * @param  无
 * @retval 无
 */
static void delay_10us(void)
{
    delay_cycles(ULTRASONIC_TRIG_DELAY_CYCLES);
}

/**
 * @brief  超声波模块初始化
 * @param  handle  超声波句柄指针
 * @param  cfg     超声波配置指针
 * @param  htim    捕获定时器句柄
 * @retval 无
 */
void ultrasonic_init(ultrasonic_handle_t *handle, const ultrasonic_cfg_t *cfg,
    GPTIMER_Regs *htim)
{
    if (!handle || !cfg || !htim) {
        error_report(ERROR_SOURCE_ULTRASONIC, DRV_ERR_PARAM);
        return;
    }

    NVIC_EnableIRQ(CAPTURE_ULTRASONIC_ECHO_INST_INT_IRQN);

    handle->htim = htim;
    handle->trig_port = cfg->trig_port;
    handle->trig_pin = cfg->trig_pin;
    handle->state = ULTRASONIC_STATE_IDLE;
    handle->capture_flag = 0;
    handle->last_trigger_tick = get_system_tick();

    DL_Timer_startCounter(handle->htim);
}

/**
 * @brief  超声波捕获中断回调（ISR 中调用）
 * @note   双边沿捕获：上升沿记录 start_time，下降沿记录 end_time
 * @param  handle  超声波句柄指针
 * @param  htim    触发中断的定时器句柄
 * @retval 无
 */
void ultrasonic_capture_callback(ultrasonic_handle_t *handle, GPTIMER_Regs *htim)
{
    uint32_t val;

    if (!handle || !htim) {
        error_report(ERROR_SOURCE_ULTRASONIC, DRV_ERR_PARAM);
        return;
    }

    if (htim != handle->htim) {
        error_report(ERROR_SOURCE_ULTRASONIC, DRV_ERR_PARAM);
        return;
    }

    /* 仅在等待回波状态下处理捕获 */
    if (handle->state != ULTRASONIC_STATE_WAIT_ECHO) {
        error_report(ERROR_SOURCE_ULTRASONIC, DRV_ERR_PARAM);
        return;
    }

    val = DL_Timer_getCaptureCompareValue(htim, DL_TIMER_CC_0_INDEX);

    if (handle->capture_flag == 0) {
        handle->start_time = val;
        handle->capture_flag = 1;
    } else if (handle->capture_flag == 1) {
        handle->end_time = val;
        handle->capture_flag = 2;
    }
}

/**
 * @brief  超声波周期性任务（主循环中调用）
 * @note   状态机：IDLE → 发 TRIG 脉冲 → 等待捕获完成 → 计算距离
 * @param  handle  超声波句柄指针
 * @retval 无
 */
void ultrasonic_task(ultrasonic_handle_t *handle)
{
    uint32_t current_tick;
    uint32_t duration;

    if (!handle) {
        error_report(ERROR_SOURCE_ULTRASONIC, DRV_ERR_PARAM);
        return;
    }

    current_tick = get_system_tick();

    switch (handle->state) {
        case ULTRASONIC_STATE_IDLE:
            if (current_tick - handle->last_trigger_tick >= ULTRASONIC_PERIOD_MS) {
                handle->last_trigger_tick = current_tick;
                handle->capture_flag = 0;

                /* 发送 10us 高电平 TRIG 脉冲 */
                DL_GPIO_setPins(handle->trig_port, handle->trig_pin);
                delay_10us();
                DL_GPIO_clearPins(handle->trig_port, handle->trig_pin);

                handle->state = ULTRASONIC_STATE_WAIT_ECHO;
            }
            break;

        case ULTRASONIC_STATE_WAIT_ECHO:
            if (handle->capture_flag == 2) {
                /* 处理定时器向上计数溢出的差值计算 */
                if (handle->end_time >= handle->start_time) {
                    duration = handle->end_time - handle->start_time;
                } else {
                    /* 定时器溢出回绕 */
                    duration =
                        (CAPTURE_ULTRASONIC_ECHO_INST_LOAD_VALUE - handle->start_time) +
                        handle->end_time + 1;
                }

                /* 有效范围判定 */
                if (duration > 0 && duration < ULTRASONIC_MAX_TIME_US) {
                    /* 距离(mm) = 持续时间(us) × 340m/s / 2 */
                    /* 1us × 0.34mm/us / 2 = 0.17mm/us */
                    ultra_data.distance_mm = (float)duration * ULTRASONIC_DISTANCE_FACTOR;
                    ultra_data.is_valid = 1;
                } else {
                    ultra_data.is_valid = 0;
                }

                handle->state = ULTRASONIC_STATE_IDLE;
            } else if (current_tick - handle->last_trigger_tick >= ULTRASONIC_PERIOD_MS) {
                /* 超时无回波，复位 */
                ultra_data.is_valid = 0;
                handle->state = ULTRASONIC_STATE_IDLE;
                handle->capture_flag = 0;
            }
            break;

        default:
            handle->state = ULTRASONIC_STATE_IDLE;
            break;
    }
}

/**
 * @brief  获取超声波测距结果
 * @param  无
 * @retval 测距数据结构体（distance_mm + is_valid）
 */
ultrasonic_data_t ultrasonic_get_data(void)
{
    return ultra_data;
}

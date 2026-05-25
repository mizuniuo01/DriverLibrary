/**
 * @file    encoder.c
 * @brief   编码器驱动模块（左轮 QEI + 右轮捕获）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    左轮使用定时器 QEI 模式，硬件 4x 计数，软件 /2 对齐精度
 * @note    右轮使用定时器 A 相双边沿捕获，方向由 motor 模块软件标志位提供
 * @note    encoder_scan_left / encoder_scan_right 为 B 类操作，ISR 中直接调用
 * @note    依赖：motor 模块（右轮方向标志位）
 * @warning 右轮捕获 ISR 中仅更新计数，增量计算由 encoder_scan_right 在 ISR 对应时间槽完成
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 适配两路编码电机方案，与 pwm、motor 模块配合使用。
 *
 * 硬件拓扑：
 *   左电机 ── DRV8874 ── 左编码器（QEI 模式，TIMG 硬件 4x 计数）
 *   右电机 ── DRV8874 ── 右编码器（捕获模式，TIMG 双边沿捕获）
 *   右编码器无方向信息，方向由 motor 模块的软件标志位补偿。
 *
 * ── 初始化 ──
 *
 * void system_init(void)
 * {
 *     encoder_init(ENCODER_LEFT_QEI_INST,
 *                  ENCODER_RIGHT_CAPTURE_INST);
 * }
 *
 * ── ISR 配置 ──
 *
 * // 左编码器：周期性读取 QEI 计数器（B 类，如 5ms）
 * void Timer_IRQHandler(void)
 * {
 *     encoder_scan_left(ENCODER_LEFT_QEI_INST);
 * }
 *
 * // 右编码器：周期性计算捕获增量（B 类）
 *     encoder_scan_right();
 *
 * // 注意：右轮捕获 ISR CAPTURE_ENCODER_RIGHT_INST_IRQHandler
 * // 在本文件中已实现，仅累加 timer_capture_right_count。
 *
 * ── 数据读取 ──
 *
 * int16_t left  = encoder_get_left();
 * int16_t right = encoder_get_right();
 *
 * ── 跨模块依赖 ──
 *
 * encoder → motor：通过 motor_get_right_direction_sign() 获取右轮方向。
 * 因此 motor_init() 必须在 encoder_scan_right() 之前完成。
 */

#include "encoder.h"

#include "motor.h"

/* 模块私有数据 */
static int16_t encoder_left_val;
static int16_t encoder_right_val;

volatile uint16_t timer_capture_right_count;

/**
 * @brief  编码器初始化
 * @param  htim_left_qei       左轮 QEI 定时器句柄
 * @param  htim_right_capture  右轮捕获定时器句柄
 * @retval 无
 */
void encoder_init(GPTIMER_Regs *htim_left_qei,
                  GPTIMER_Regs *htim_right_capture)
{
    if (htim_left_qei) {
        DL_Timer_setTimerCount(htim_left_qei, 0);
        DL_Timer_startCounter(htim_left_qei);
    }

    if (htim_right_capture) {
        DL_TimerA_stopCounter(htim_right_capture);
        DL_TimerA_disableClock(htim_right_capture);
        DL_TimerA_setCounterMode(htim_right_capture,
                                 DL_TIMER_COUNT_MODE_UP);
        DL_Timer_setTimerCount(htim_right_capture, 0);

        /* A 相双边沿捕获 */
        DL_TimerA_setCaptureCompareCtl(htim_right_capture,
            DL_TIMER_CC_MODE_CAPTURE,
            (DL_TIMER_CC_ZCOND_NONE
             | DL_TIMER_CC_ACOND_TIMCLK
             | DL_TIMER_CC_LCOND_NONE
             | DL_TIMER_CC_CCOND_TRIG_EDGE),
            DL_TIMER_CC_0_INDEX);

        /* CC1 禁用捕获，方向由 motor 模块软件标志位提供 */
        DL_TimerA_setCaptureCompareCtl(htim_right_capture,
            DL_TIMER_CC_MODE_CAPTURE,
            (DL_TIMER_CC_ZCOND_NONE
             | DL_TIMER_CC_ACOND_TIMCLK
             | DL_TIMER_CC_LCOND_NONE
             | DL_TIMER_CC_CCOND_NOCAPTURE),
            DL_TIMER_CC_1_INDEX);

        DL_GPIO_initDigitalInputFeatures(
            ENCODER_RIGHT_DIR_PIN_1_IOMUX,
            DL_GPIO_INVERSION_DISABLE,
            DL_GPIO_RESISTOR_NONE,
            DL_GPIO_HYSTERESIS_DISABLE,
            DL_GPIO_WAKEUP_DISABLE);

        DL_TimerA_enableClock(htim_right_capture);
        DL_TimerA_startCounter(htim_right_capture);

        DL_TimerA_disableInterrupt(htim_right_capture,
            DL_TIMERA_INTERRUPT_CC0_DN_EVENT
            | DL_TIMERA_INTERRUPT_CC0_UP_EVENT
            | DL_TIMERA_INTERRUPT_CC1_DN_EVENT
            | DL_TIMERA_INTERRUPT_CC1_UP_EVENT);

        DL_TimerA_enableInterrupt(htim_right_capture,
            DL_TIMERA_INTERRUPT_CC0_DN_EVENT
            | DL_TIMERA_INTERRUPT_CC0_UP_EVENT);

        NVIC_EnableIRQ(CAPTURE_ENCODER_RIGHT_INST_INT_IRQN);
    }
}

/**
 * @brief  左轮编码器扫描（B 类，ISR 中直接调用）
 * @note   QEI 4x 计数 → /2 与右轮对齐 → 取反修正安装方向
 * @param  htim  左轮 QEI 定时器句柄
 * @retval 无
 */
void encoder_scan_left(GPTIMER_Regs *htim)
{
    static uint16_t last_count_left = 0;
    uint16_t current_count;
    int16_t diff_value;

    if (!htim) {
        return;
    }

    current_count = (uint16_t)DL_Timer_getTimerCount(htim);
    diff_value = (int16_t)(current_count - last_count_left);

    /* QEI 为 4x 计数，/2 与右轮口径对齐；取反修正安装方向 */
    encoder_left_val = (int16_t)(-(diff_value / 2));
    last_count_left = current_count;
}

/**
 * @brief  右轮编码器扫描（B 类，ISR 中直接调用）
 * @note   从捕获中断累计值计算增量，方向由 motor 模块提供
 * @param  无
 * @retval 无
 */
void encoder_scan_right(void)
{
    static uint16_t last_count_right = 0;
    uint32_t primask;
    uint16_t current_count;
    int16_t diff_value;

    primask = __get_PRIMASK();
    __disable_irq();
    current_count = timer_capture_right_count;
    __set_PRIMASK(primask);

    diff_value = (int16_t)(current_count - last_count_right);

    if (motor_get_right_direction_sign() < 0) {
        diff_value = -diff_value;
    }

    encoder_right_val = diff_value;
    last_count_right = current_count;
}

/**
 * @brief  获取左轮编码器值
 * @param  无
 * @retval 左轮当前增量（编码器计数值）
 */
int16_t encoder_get_left(void)
{
    return encoder_left_val;
}

/**
 * @brief  获取右轮编码器值
 * @param  无
 * @retval 右轮当前增量（编码器计数值）
 */
int16_t encoder_get_right(void)
{
    return encoder_right_val;
}

/**
 * @brief  右轮捕获中断服务函数
 * @note   用户项目中需将此函数名配置为对应中断向量的 ISR
 * @param  无
 * @retval 无
 */
void CAPTURE_ENCODER_RIGHT_INST_IRQHandler(void)
{
    DL_TIMER_IIDX pending;

    pending = DL_Timer_getPendingInterrupt(CAPTURE_ENCODER_RIGHT_INST);

    while (pending == DL_TIMER_IIDX_CC0_UP
           || pending == DL_TIMER_IIDX_CC0_DN) {
        (void)DL_TimerA_getCaptureCompareValue(
            CAPTURE_ENCODER_RIGHT_INST, DL_TIMER_CC_0_INDEX);

        timer_capture_right_count++;

        pending = DL_Timer_getPendingInterrupt(
            CAPTURE_ENCODER_RIGHT_INST);
    }
}

/**
 * @file    ultrasonic.c
 * @brief   超声波测距模块（HC-SR04，定时器输入捕获）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：捕获定时器和 GPIO 已在 CubeMX 中配置
 * @note    使用双边沿输入捕获测量回波脉宽
 * @note    三态非阻塞触发：IDLE → WAIT_TRIGGER_END → WAIT_ECHO
 * @warning capture_callback 在 ISR 中调用，仅做时间戳记录和极性切换
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 适配 HC-SR04 超声波模块，非阻塞触发 + 定时器输入捕获。
 *
 * ── 硬件连接 ──
 *
 *   MCU GPIO ──→ HC-SR04 TRIG（>10us 高电平脉冲）
 *   MCU TIM CH1 ←── HC-SR04 ECHO（双边沿输入捕获）
 *
 * ── CubeMX 配置 ──
 *
 * - 定时器 CH1 设为 Input Capture（双边沿）
 * - GPIO 设为 Output Push-Pull
 *
 * ── 初始化 ──
 *
 * static ultrasonic_handle_t ultra;
 *
 * ultrasonic_cfg_t cfg = {
 *     .trig_port = ultrasonic_Trig_GPIO_Port,
 *     .trig_pin  = ultrasonic_Trig_Pin,
 * };
 * ultrasonic_init(&ultra, &cfg, &htim2);
 *
 * ── ISR 中调用 ──
 *
 * void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
 * {
 *     ultrasonic_capture_callback(&ultra, htim);
 * }
 *
 * ── 主循环中调用 ──
 *
 * ultrasonic_task(&ultra);
 * ultrasonic_data_t d = ultrasonic_get_data();
 * // d.distance_mm, d.is_valid
 */

#include "ultrasonic.h"

static ultrasonic_data_t ultra_data;

/**
 * @brief  超声波模块初始化
 * @param  handle  超声波句柄指针
 * @param  cfg     超声波配置指针
 * @param  htim    输入捕获定时器句柄
 * @retval 无
 */
void ultrasonic_init(ultrasonic_handle_t *handle,
                     const ultrasonic_cfg_t *cfg,
                     TIM_HandleTypeDef *htim)
{
    if (!handle || !cfg || !htim) {
        return;
    }

    handle->htim = htim;
    handle->trig_port = cfg->trig_port;
    handle->trig_pin = cfg->trig_pin;
    handle->state = ULTRASONIC_STATE_IDLE;
    handle->capture_flag = 0;
    handle->last_trigger_tick = HAL_GetTick();

    /* 启动上升沿输入捕获中断 */
    HAL_TIM_IC_Start_IT(handle->htim, TIM_CHANNEL_1);
}

/**
 * @brief  超声波输入捕获中断回调（ISR 中调用）
 * @note   上升沿记录 start_time → 切换下降沿 → 下降沿记录 end_time
 * @param  handle  超声波句柄指针
 * @param  htim    触发中断的定时器句柄
 * @retval 无
 */
void ultrasonic_capture_callback(ultrasonic_handle_t *handle,
                                 TIM_HandleTypeDef *htim)
{
    if (!handle || !htim) {
        return;
    }

    if (htim->Instance != handle->htim->Instance) {
        return;
    }

    if (handle->capture_flag == 0) {
        handle->start_time =
            HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        handle->capture_flag = 1;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                      TIM_CHANNEL_1,
                                      TIM_INPUTCHANNELPOLARITY_FALLING);
    } else if (handle->capture_flag == 1) {
        handle->end_time =
            HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        handle->capture_flag = 2;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                      TIM_CHANNEL_1,
                                      TIM_INPUTCHANNELPOLARITY_RISING);
    }
}

/**
 * @brief  超声波周期性任务（主循环中调用，非阻塞）
 * @note   三态状态机：IDLE → WAIT_TRIGGER_END → WAIT_ECHO
 * @param  handle  超声波句柄指针
 * @retval 无
 */
void ultrasonic_task(ultrasonic_handle_t *handle)
{
    uint32_t current_tick;

    if (!handle) {
        return;
    }

    current_tick = HAL_GetTick();

    switch (handle->state) {
        case ULTRASONIC_STATE_IDLE:
            if (current_tick - handle->last_trigger_tick
                >= ULTRASONIC_PERIOD_MS) {
                HAL_GPIO_WritePin(handle->trig_port,
                                  handle->trig_pin,
                                  GPIO_PIN_SET);
                handle->last_trigger_tick = current_tick;

                /* 强制重置捕获极性为上升沿 */
                __HAL_TIM_SET_CAPTUREPOLARITY(
                    handle->htim,
                    TIM_CHANNEL_1,
                    TIM_INPUTCHANNELPOLARITY_RISING);
                handle->capture_flag = 0;

                handle->state = ULTRASONIC_STATE_WAIT_TRIGGER_END;
            }
            break;

        case ULTRASONIC_STATE_WAIT_TRIGGER_END:
            /* TRIG 脉冲维持 >10us 后拉低 */
            if (current_tick - handle->last_trigger_tick >= 1) {
                HAL_GPIO_WritePin(handle->trig_port,
                                  handle->trig_pin,
                                  GPIO_PIN_RESET);
                handle->state = ULTRASONIC_STATE_WAIT_ECHO;
            }
            break;

        case ULTRASONIC_STATE_WAIT_ECHO:
            if (handle->capture_flag == 2) {
                uint32_t duration;

                /* 处理定时器向上计数溢出的差值计算 */
                if (handle->end_time >= handle->start_time) {
                    duration = handle->end_time
                               - handle->start_time;
                } else {
                    /* 32 位计数器溢出回绕 */
                    duration = (0xFFFFFFFF - handle->start_time)
                               + handle->end_time + 1;
                }

                if (duration > 0
                    && duration < ULTRASONIC_MAX_TIME_US) {
                    ultra_data.distance_mm =
                        (float)duration
                        * (ULTRASONIC_SOUND_SPEED / 1000.0f)
                        / 2.0f;
                    ultra_data.is_valid = 1;
                } else {
                    ultra_data.is_valid = 0;
                }

                handle->state = ULTRASONIC_STATE_IDLE;
            } else if (current_tick - handle->last_trigger_tick
                       >= ULTRASONIC_PERIOD_MS) {
                /* 超时无回波，复位 */
                ultra_data.is_valid = 0;
                handle->state = ULTRASONIC_STATE_IDLE;
                handle->capture_flag = 0;
                __HAL_TIM_SET_CAPTUREPOLARITY(
                    handle->htim,
                    TIM_CHANNEL_1,
                    TIM_INPUTCHANNELPOLARITY_RISING);
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

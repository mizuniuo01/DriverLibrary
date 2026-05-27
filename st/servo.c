/**
 * @file    servo.c
 * @brief   FashionStar RA8-U25-M 串口舵机驱动（UART DMA + 环形缓冲区）
 * @author  mizuniuo01
 * @date    2026-05-26
 * @version 1.0.0
 * @note    依赖 FashionStar 官方 SDK（fashion_star_uart_servo.h、ring_buffer.h）
 * @note    通信层：DMA + IDLE 中断 + 环形缓冲区（§12.3）
 * @note    控制层：封装官方 FSUS_* API，增加角度限幅
 * @note    错误通过 error_report(ERROR_SOURCE_SERVO, code) 上报
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * FashionStar RA8-U25-M 总线伺服舵机，UART 指令模式。
 *
 * ── 配置 ──
 *
 * servo_cfg_t cfg = {
 *     .huart = &huart3,
 *     .servo_id_x = 9,
 *     .servo_id_y = 6,
 *     .x_angle_min = -95.0f,  .x_angle_max = 95.0f,
 *     .y_angle_min = -45.0f,  .y_angle_max = 55.0f,
 *     .center_angle_x = 10.0f,
 *     .center_angle_y = 8.0f,
 *     .default_interval_ms = 100,
 * };
 *
 * ── 初始化 ──
 *
 * servo_handle_t servo;
 * servo_init(&servo, &cfg);
 *
 * ── HAL 回调（ISR 中）──
 *
 * void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
 * {
 *     servo_rx_callback(&servo, huart, size);
 * }
 *
 * ── 主循环 ──
 *
 * servo_tx_task(&servo);  // 发送调度
 *
 * ── 控制 ──
 *
 * servo_set_angle(&servo, 9, 45.0f, 500);
 * servo_set_sync_angle(&servo, 30.0f, -15.0f, 800);
 * servo_reset_center(&servo);
 *
 * ── 查询 ──
 *
 * float angle;
 * servo_get_angle(&servo, 9, &angle);
 */

#include "servo.h"
#include "../error_handler.h"

/*
 * X 轴：向左为负(-)，向右为正(+)。
 * Y 轴：向下为负(-)，向上为正(+)。
 */

/**
 * @brief  角度限幅
 * @param  cfg       舵机配置（含各轴限位值）
 * @param  servo_id  舵机 ID
 * @param  angle     目标角度
 * @retval 限幅后的角度
 */
static float constrain_angle(const servo_cfg_t *cfg, uint8_t servo_id, float angle)
{
    if (servo_id == cfg->servo_id_x) {
        if (angle < cfg->x_angle_min) {
            error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
            return cfg->x_angle_min;
        }
        if (angle > cfg->x_angle_max) {
            error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
            return cfg->x_angle_max;
        }
    } else if (servo_id == cfg->servo_id_y) {
        if (angle < cfg->y_angle_min) {
            error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
            return cfg->y_angle_min;
        }
        if (angle > cfg->y_angle_max) {
            error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
            return cfg->y_angle_max;
        }
    }
    return angle;
}

/**
 * @brief  上报 FSUS 通信状态
 * @param  status  FSUS 状态码
 * @retval 无
 */
static void report_fsus_status(FSUS_STATUS status)
{
    if (status != FSUS_STATUS_SUCCESS) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_IO);
    }
}

/**
 * @brief  舵机初始化
 * @param  handle  舵机句柄
 * @param  cfg     舵机配置
 * @retval 无
 */
void servo_init(servo_handle_t *handle, const servo_cfg_t *cfg)
{
    if (!handle || !cfg || !cfg->huart) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    handle->cfg = *cfg;

    RingBuffer_Init(&handle->rx_ring, SERVO_RX_FIFO_SIZE, handle->rx_fifo_buf);
    RingBuffer_Init(&handle->tx_ring, SERVO_TX_FIFO_SIZE, handle->tx_fifo_buf);

    handle->usart.huart = cfg->huart;
    handle->usart.recvBuf = &handle->rx_ring;
    handle->usart.sendBuf = &handle->tx_ring;

    HAL_StatusTypeDef status;

    status = HAL_UARTEx_ReceiveToIdle_DMA(cfg->huart, handle->dma_rx_buf,
        SERVO_DMA_RX_BUF_SIZE);
    if (status == HAL_BUSY) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_BUSY);
    } else if (status != HAL_OK) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_IO);
    }
}

/**
 * @brief  DMA 接收完成回调（ISR 中调用）
 * @param  handle  舵机句柄
 * @param  huart   触发中断的串口句柄
 * @param  size    接收数据长度
 * @retval 无
 */
void servo_rx_callback(servo_handle_t *handle, UART_HandleTypeDef *huart, uint16_t size)
{
    HAL_StatusTypeDef status;
    uint16_t i;

    if (!handle || !huart) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    if (huart->Instance != handle->cfg.huart->Instance) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    if (size > 0) {
        for (i = 0; i < size; i++) {
            RingBuffer_Push(&handle->rx_ring, handle->dma_rx_buf[i]);
        }
    }

    memset(handle->dma_rx_buf, 0, SERVO_DMA_RX_BUF_SIZE);
    status = HAL_UARTEx_ReceiveToIdle_DMA(handle->cfg.huart, handle->dma_rx_buf,
        SERVO_DMA_RX_BUF_SIZE);
    if (status == HAL_BUSY) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_BUSY);
    } else if (status != HAL_OK) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_IO);
    }
}

/**
 * @brief  发送调度任务（主循环中调用）
 * @note   从 TX FIFO 取出数据并通过 UART 发送
 * @param  handle  舵机句柄
 * @retval 无
 */
void servo_tx_task(servo_handle_t *handle)
{
    HAL_StatusTypeDef status;
    uint16_t num_to_send;
    uint8_t temp_buf[SERVO_TX_FIFO_SIZE];

    if (!handle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    num_to_send = RingBuffer_GetByteUsed(&handle->tx_ring);
    if (num_to_send == 0) {
        return;
    }

    RingBuffer_ReadByteArray(&handle->tx_ring, temp_buf, num_to_send);
    status = HAL_UART_Transmit(handle->cfg.huart, temp_buf, num_to_send,
        SERVO_DEFAULT_INTERVAL_MS);
    if (status == HAL_BUSY) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_BUSY);
    } else if (status == HAL_TIMEOUT) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_TIMEOUT);
    } else if (status != HAL_OK) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_IO);
    }
}

/**
 * @brief  设置单轴角度
 * @param  handle       舵机句柄
 * @param  servo_id     舵机 ID
 * @param  angle        目标角度（度）
 * @param  interval_ms  到达时间（ms）
 * @retval 无
 */
void servo_set_angle(servo_handle_t *handle, uint8_t servo_id, float angle,
    uint16_t interval_ms)
{
    FSUS_STATUS status;
    float safe_angle;

    if (!handle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    safe_angle = constrain_angle(&handle->cfg, servo_id, angle);
    status = FSUS_SetServoAngle(&handle->usart, servo_id, safe_angle, interval_ms, 0);
    report_fsus_status(status);
}

/**
 * @brief  设置双轴同步角度
 * @param  handle       舵机句柄
 * @param  angle_x      X 轴目标角度（度）
 * @param  angle_y      Y 轴目标角度（度）
 * @param  interval_ms  到达时间（ms）
 * @retval 无
 */
void servo_set_sync_angle(servo_handle_t *handle, float angle_x, float angle_y,
    uint16_t interval_ms)
{
    FSUS_STATUS status_x;
    FSUS_STATUS status_y;
    float safe_x;
    float safe_y;

    if (!handle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    safe_x = constrain_angle(&handle->cfg, handle->cfg.servo_id_x, angle_x);
    safe_y = constrain_angle(&handle->cfg, handle->cfg.servo_id_y, angle_y);

    status_x = FSUS_SetServoAngle(&handle->usart, handle->cfg.servo_id_x, safe_x,
        interval_ms, 0);
    status_y = FSUS_SetServoAngle(&handle->usart, handle->cfg.servo_id_y, safe_y,
        interval_ms, 0);

    if (status_x != FSUS_STATUS_SUCCESS) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_IO);
        return;
    }
    if (status_y != FSUS_STATUS_SUCCESS) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_IO);
        return;
    }
}

/**
 * @brief  查询舵机当前角度
 * @param  handle    舵机句柄
 * @param  servo_id  舵机 ID
 * @param  angle     输出参数，当前角度（度）
 * @retval 无
 */
void servo_get_angle(servo_handle_t *handle, uint8_t servo_id, float *angle)
{
    FSUS_STATUS status;

    if (!handle || !angle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    status = FSUS_QueryServoAngle(&handle->usart, servo_id, angle);
    report_fsus_status(status);
}

/**
 * @brief  云台复位到中心位置
 * @param  handle  舵机句柄
 * @retval 无
 */
void servo_reset_center(servo_handle_t *handle)
{
    if (!handle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    servo_set_sync_angle(handle, handle->cfg.center_angle_x, handle->cfg.center_angle_y,
        handle->cfg.default_interval_ms);
}

/**
 * @brief  舵机掉电释放
 * @param  handle    舵机句柄
 * @param  servo_id  舵机 ID
 * @retval 无
 */
void servo_release(servo_handle_t *handle, uint8_t servo_id)
{
    if (!handle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    FSUS_StopOnControlMode(&handle->usart, servo_id, 0, 0);
}

/**
 * @brief  舵机通讯检测（Ping）
 * @param  handle    舵机句柄
 * @param  servo_id  舵机 ID
 * @retval 无
 */
void servo_ping(servo_handle_t *handle, uint8_t servo_id)
{
    FSUS_STATUS status;

    if (!handle) {
        error_report(ERROR_SOURCE_SERVO, DRV_ERR_PARAM);
        return;
    }

    status = FSUS_Ping(&handle->usart, servo_id);
    report_fsus_status(status);
}

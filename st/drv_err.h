#ifndef DRV_ERR_H
#define DRV_ERR_H

/*
 * 驱动库统一错误码（无 .c 文件，定义即文档）
 *
 * 返回值约定：0 表示成功，负值表示错误。调用者可用 if (ret != DRV_OK)
 * 或 if (ret < 0) 判断成功/失败，也可精确匹配具体错误码。
 *
 * DRV_ERR_PARAM (-1)  参数非法：空指针、越界、无效配置值。
 *                     用于所有 xxx_init() 和 set 类函数的入口校验。
 *
 * DRV_ERR_BUSY (-2)   设备忙：DMA/I2C/UART 正在传输中，当前不能接受新操作。
 *                     用于 sensor_request_dma、blueteeth_printf 等需要排队的函数。
 *
 * DRV_ERR_TIMEOUT (-3) 操作超时：等待硬件响应超时、capture 超时、心跳超时。
 *                      用于 ultrasonic、step_motor 等需要等待外部事件的模块。
 *
 * DRV_ERR_IO (-4)     硬件通信错误：HAL/TI DriverLib 函数返回非 OK、
 *                      I2C NACK、UART Frame Error、DMA 传输错误。
 *                     用于 FSUS 状态码转换（servo）、HAL_I2C_Mem_Write 返回值检查（oled）等。
 *
 * DRV_ERR_STATE (-5)  状态机非法状态：switch 进入 default 分支。
 *                      用于状态机模块的防御性检测，通常内部复位到 IDLE。
 *
 * 当前使用情况：
 *   DRV_ERR_PARAM  — 所有模块 init()
 *   DRV_ERR_IO     — servo（FSUS 状态码转换、UART 发送）
 *   其余错误码保留供项目实际需要时启用，无需删除。
 */
typedef enum {
    DRV_OK = 0,
    DRV_ERR_PARAM = -1,
    DRV_ERR_BUSY = -2,
    DRV_ERR_TIMEOUT = -3,
    DRV_ERR_IO = -4,
    DRV_ERR_STATE = -5,
} drv_err_t;

#endif

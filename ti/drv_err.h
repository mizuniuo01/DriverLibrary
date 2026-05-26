#ifndef DRV_ERR_H
#define DRV_ERR_H

/* 驱动库统一错误码 */
typedef enum {
    DRV_OK = 0,           /* 成功 */
    DRV_ERR_PARAM = -1,   /* 参数非法（含空指针、越界） */
    DRV_ERR_BUSY = -2,    /* 设备忙 */
    DRV_ERR_TIMEOUT = -3, /* 操作超时 */
    DRV_ERR_IO = -4,      /* 硬件通信错误 */
    DRV_ERR_STATE = -5,   /* 状态机状态非法 */
} drv_err_t;

#endif

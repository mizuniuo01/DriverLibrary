#ifndef BLUETEETH_H
#define BLUETEETH_H

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#define BLUETEETH_DMA_RX_BUF_SIZE 128 /* DMA 单次接收最大缓存量 */
#define BLUETEETH_DMA_TX_BUF_SIZE 128 /* DMA 单次发送最大缓存量 */
#define BLUETEETH_RX_FIFO_SIZE 1024 /* 接收环形队列容量 */
#define BLUETEETH_TX_FIFO_SIZE 1024 /* 发送环形队列容量 */
#define BLUETEETH_MAX_FRAME_LEN 128 /* 单帧协议最大长度 */

#define CMD_TABLE_SIZE (sizeof(cmd_table) / sizeof(cmd_table[0]))

#define FRAME_HEADER '@'
#define FRAME_TAIL '#'

typedef enum {
    STATE_WAIT_HEADER = 0,
    STATE_RECEIVING_DATA
} BlueteethFrameState_t;

typedef struct {
    const char *cmd_string;
    void (*cmd_handler)(void);
} BlueteethCommandMap_t;

typedef struct {
    UART_Regs *huart;

    uint8_t dma_rx_buffer[BLUETEETH_DMA_RX_BUF_SIZE];
    uint8_t rx_fifo[BLUETEETH_RX_FIFO_SIZE];
    volatile uint16_t rx_write_pos;
    volatile uint16_t rx_read_pos;

    uint8_t dma_tx_buffer[BLUETEETH_DMA_TX_BUF_SIZE];
    uint8_t tx_fifo[BLUETEETH_TX_FIFO_SIZE];
    volatile uint16_t tx_write_pos;
    volatile uint16_t tx_read_pos;
    volatile uint8_t is_tx_busy;

    BlueteethFrameState_t rx_state;
    uint8_t frame_buffer[BLUETEETH_MAX_FRAME_LEN];
    uint16_t frame_index;
} BlueteethHandle_t;

extern volatile uint8_t blueteeth_check_idle_flag;

void blueteeth_init(UART_Regs *huart);
void blueteeth_printf(const char *format, ...);
void blueteeth_display(int x, int y, const char *format, ...);
void blueteeth_clear(void);
void blueteeth_plot(const char *format, ...);
void blueteeth_tx_callback(UART_Regs *huart);
void blueteeth_rx_callback(UART_Regs *huart, uint16_t size);
void blueteeth_error_callback(UART_Regs *huart);
void blueteeth_task(void);

#endif

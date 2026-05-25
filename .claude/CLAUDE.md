# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

个人的嵌入式外设驱动库。**旧代码已放入，待逐个按规范重写。**

- `st/` — STM32 平台驱动（依赖 STM32 HAL）
- `ti/` — TI MSP 平台驱动（依赖 TI DriverLib，目前仅适配 MSPM0G3507）
- `CODING_STANDARD.md` — 唯一的权威规范文档（v3.0），包含代码风格和软件设计模式

## 无构建系统

本仓库是代码参考集合，不包含 Makefile、CMakeLists 或编译器配置。驱动文件在各自目标项目中编译，构建命令由具体项目决定。

## 核心规范（CODING_STANDARD.md）

所有代码必须遵循 `CODING_STANDARD.md`。关键约定：

- **命名**：函数 `snake_case`，类型 `PascalCase`+`_t`，宏 `UPPER_SNAKE_CASE`，文件名 `snake_case`
- **格式**：Linux 风格花括号（控制流 K&R，函数 Allman），4 空格缩进（禁止 Tab），80 列宽，指针 `*` 靠右
- **头文件保护**：`MODULE_H`（禁止双下划线 `__MODULE_H`）
- **注释**：中文，Doxygen 格式，`.c` 写详细文件头（`@file/@brief/@author/@date/@version/@note/@warning`）+ 函数注释（`@brief/@param/@retval`），`.h` 不写文件头
- **数据类型**：`<stdint.h>` 固定宽度类型（`uint8_t`/`int16_t` 等），禁止 `int`/`long`
- **禁止**：动态内存、递归、VLA、阻塞延时、条件中赋值

## 软件设计模式（CODING_STANDARD.md §12）

旧代码重写时必须遵循的架构模式：

- **12.2 统一调度**：定时器 ISR 统一管理所有模块的执行周期，分频计数器 + `volatile` 标志位。A 类（标志位触发，复杂状态机在 task 中处理），B 类（ISR 直接执行，仅限极简读取操作）。调度周期由被控对象物理特性决定，不拍脑袋定值
- **12.3 串口**：DMA + IDLE 中断 + 环形缓冲区（保留一个空位判别空/满）。TI 平台用 DMA 余量不变判定法替代硬件 IDLE。发送侧对称使用 TX FIFO + DMA
- **12.4 ISR**：只做读寄存器→置标志位→搬数据→清中断。中断优先级：控制类（高）> 通信类（中）> 系统 tick（低）
- **12.5 状态机**：每个有状态模块一个主状态枚举 + `xxx_task()` 中 switch 推进，每个等待状态必须有超时兜底
- **12.6 模块接口**：`init()` → `task()` / 回调 的标准函数模式，返回值 0 成功 / 负值错误码（`DRV_ERR_PARAM`/`DRV_ERR_BUSY`/`DRV_ERR_TIMEOUT`/`DRV_ERR_IO`/`DRV_ERR_STATE`）
- **12.7 数据共享**：`static` 私有变量 + getter 函数暴露。模块间通信三种方式按场景选用：直接函数调用（一对一）、getter 函数（一写多读）、回调函数（异步事件）
- **12.8 单/多实例**：默认多实例（句柄注入），客观上唯一的硬件允许单实例
- **12.9 错误处理**：公开接口完整校验，内部函数轻量校验，ISR 最简校验。错误恢复：错误上报→复位外设→自动重试（限次数）→仍失败则上报故障
- **12.11 错误上报**：`Display` 模块内部维护 `g_error_msg[64]` 字符串，对外提供 `display_show_error(format, ...)` 接口。`display_task()` 刷新时通过 `blueteeth_display()` 打印到 `DISPLAY_LINE_ERROR_Y` 行，无错时输出空字符串覆盖。蓝牙串口软件使用江协科技蓝牙串口小程序

## 旧代码 → 新代码迁移要点

当前旧文件存在的共性问题（重写时需修正）：

1. 函数命名 `PascalCase`（`Motor_Init`）→ `snake_case`（`motor_init`）
2. 头文件保护 `__MOTOR_H` → `MOTOR_H`
3. `#include "main.h"` → 去掉，改为 `<stdint.h>` + 句柄注入
4. 引脚 `#define` 写死在驱动 `.h` → 改为 `xxx_init()` 结构体参数注入
5. 全局变量 `extern xxxData` → `static` + getter 函数
6. `HAL_Delay()` 阻塞 → 状态机 + tick 比较
7. 注释掉的 dead code → 删除
8. 缺少文件头注释 → 按 Doxygen 模板补全

## 已重写模块

- `ti/blueteeth`、`st/blueteeth` — 蓝牙串口透传

## 平台差异备忘（通信类模块重写时参考）

| | TI MSPM0 | STM32 |
|---|---|---|
| 平台头文件 | `"ti_msp_dl_config.h"` | `<stm32f4xx_hal.h>` |
| DMARX启动 | 手动 `DL_DMA_setSrcAddr`/`setDestAddr`/... | `HAL_UARTEx_ReceiveToIdle_DMA` |
| 帧结束检测 | DMA 余量不变判定法（task 中轮询） | 硬件 IDLE 中断 |
| ISR 回调判空 | 无（用户 ISR 已确认实例） | 有（HAL 全局回调需防御） |
| 实例匹配 | `huart == inst.huart`（指针比较） | `huart->Instance == inst.huart->Instance` |
| 中断配置 | `DL_UART_Main_setRXInterruptTimeout` + `NVIC_EnableIRQ` | HAL 内部处理 |

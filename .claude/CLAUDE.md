# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

个人的嵌入式外设驱动库。**全部模块已完成重写，待实际测试验证。**

- `st/` — STM32 平台驱动（依赖 STM32 HAL）
- `ti/` — TI MSP 平台驱动（依赖 TI DriverLib，目前仅适配 MSPM0G3507）
- `CODING_STANDARD.md` — 唯一的权威规范文档（v1.3 Release），包含代码风格和软件设计模式
- `error_handler.h` / `error_handler.c` — 统一错误处理模块，包含 `drv_err_t`、`error_source_t`、错误队列、上报和 tick 驱动处理框架

## 无构建系统

本仓库是代码参考集合，不包含 Makefile、CMakeLists 或编译器配置。驱动文件在各自目标项目中编译，构建命令由具体项目决定。

## 核心规范（CODING_STANDARD.md）

所有代码遵循 `CODING_STANDARD.md`（参照 BARR-C:2018 + Linux 内核编码规范）。关键约定：

- **命名**：函数 `snake_case`，类型 `snake_case_t`（BARR-C 规则 5.1.a），宏 `UPPER_SNAKE_CASE`，文件名 `snake_case`。模块对外标识符以模块名为前缀（如 `motor_init`）
- **格式**：Linux 风格花括号（控制流 K&R，函数 Allman），4 空格缩进（禁止 Tab），90 列宽，指针 `*` 靠右。连续变量声明/赋值进行对齐。变量在代码块开头声明
- **头文件保护**：`MODULE_H`（禁止双下划线 `__MODULE_H`）
- **注释**：中文，Doxygen 格式，`.c` 写详细文件头（`@file/@brief/@author/@date/@version/@note/@warning`）+ 函数注释（`@brief/@param/@retval`），`.h` 不写文件头。宏定义优先上行注释；行尾注释仅限短宏，超 100 列时注释放上一行。不得用注释屏蔽代码（用 `#if 0`）
- **数据类型**：`<stdint.h>` 固定宽度类型（`uint8_t`/`int16_t` 等），禁止 `int`/`long`。`char` 仅用于 ASCII/字符串。位操作必须用无符号类型。无 FPU 时避免浮点数，优先定点数
- **结构体/联合体**：联合体必须有判别字段；映射硬件或协议的结构体用 `__attribute__((packed))`
- **枚举 vs 宏**：逻辑相关的整数常量用 `typedef enum`（命令码、状态码、设备 ID、帧标记、速度限值、缓冲区/帧大小、硬件参数等），孤立的配置常量（单个 timeout、校验值）和浮点常量用 `#define`。运算宏（如 `SENSOR_I2C_ADDR (ADDR_7BIT << 1)`）因预处理器无法识别 enum 成员，也必须保留 `#define`。BARR-C:2018 规则 1.8：相关常量放入枚举，与数量无关——2 个也该用 enum
- **函数**：无参写 `(void)`，显式声明返回类型（禁止隐式 `int`）。避免不必要类型转换
- **初始化**：不要为消除编译器警告做无意义初始化（掩盖真正的未初始化 bug）
- **循环**：禁止空循环体、禁止在循环体内修改循环变量
- **禁止**：动态内存、递归、VLA、阻塞延时、条件中赋值、`auto`/`register`/`continue`（`goto` 仅限统一错误出口）

## 软件设计模式（CODING_STANDARD.md §12）

所有模块遵循的架构模式：

- **12.2 统一调度**：定时器 ISR 统一管理所有模块的执行周期，分频计数器 + `volatile` 标志位。A 类（标志位触发，复杂状态机在 task 中处理），B 类（ISR 直接执行，仅限极简读取操作）。调度周期由被控对象物理特性决定，不拍脑袋定值
- **12.3 串口**：DMA + IDLE 中断 + 环形缓冲区（保留一个空位判别空/满）。TI 平台用 DMA 余量不变判定法替代硬件 IDLE。发送侧对称使用 TX FIFO + DMA
- **12.4 ISR**：只做读寄存器→置标志位→搬数据→清中断。中断优先级：控制类（高）> 通信类（中）> 系统 tick（低）
- **12.5 状态机**：每个有状态模块一个主状态枚举 + `xxx_task()` 中 switch 推进，每个等待状态必须有超时兜底
- **12.6 模块接口**：`init()` → `task()` / 回调 的标准函数模式。驱动函数统一返回 `void` 或具体数据类型，错误通过 `error_report(source, code)` 报告。`drv_err_t` 定义在 `error_handler.h`，由 error_handler 模块统一管理
- **12.7 数据共享**：`static` 私有变量 + getter 函数暴露。模块间通信三种方式按场景选用：直接函数调用（一对一）、getter 函数（一写多读）、回调函数（异步事件）
- **12.8 单/多实例**：默认多实例（句柄注入），客观上唯一的硬件允许单实例
- **12.9 错误处理**：三层分离，全部逻辑集中在 `error_handler` 模块。错误来源用项目级枚举 `error_source_t` 标识——传输（`error_report(source, code)` 记录到队列）、上报（`error_report_display` 只报第一个错误）、处理（`error_process` 每次 tick 从队列弹出一个，`switch(source)` 分派处理）。驱动层不调 display，应用层不介入错误决策
- **12.11 看门狗**：硬件 IWDG，喂狗在 main() 循环统一位置。软件任务心跳监控按需启用

## 旧代码 → 新代码迁移要点（历史参考）

重写过程中修正的共性问题：

1. 函数命名 `PascalCase`（`Motor_Init`）→ `snake_case`（`motor_init`）
2. 头文件保护 `__MOTOR_H` → `MOTOR_H`
3. `#include "main.h"` → 去掉，改为 `<stdint.h>` + 句柄注入
4. 引脚 `#define` 写死在驱动 `.h` → 改为 `xxx_init()` 结构体参数注入
5. 全局变量 `extern xxxData` → `static` + getter 函数
6. `HAL_Delay()` 阻塞 → 状态机 + tick 比较
7. 注释掉的 dead code → 删除
8. 缺少文件头注释 → 按 Doxygen 模板补全
9. 相关常量 `#define` 散落 → 按逻辑组归入 `typedef enum`（`motor_cmd_t`、`motor_param_t`、`motor_status_t`、`motor_id_t`、`motor_speed_limit_t`、`display_line_y_t`、`cam_frame_byte_t`、`blueteeth_frame_byte_t`、`gyro_frame_byte_t`、`oled_font_size_t`、`stepmotor_sync_flag_t`、`blueteeth_buf_size_t`、`cam_buf_size_t`、`gyro_buf_size_t`、`stepmotor_buf_size_t`、`ultrasonic_cfg_param_t`、`key_cfg_param_t`、`oled_hw_param_t`、`pwm_cfg_t`）
   - 例外：浮点常量（`100.0f`）、运算宏（`(X << 1)`）、孤立单值保留 `#define`

## 已重写模块

- `ti/blueteeth`、`st/blueteeth` — 蓝牙串口透传
- `ti/buzzer`、`st/buzzer` — 蜂鸣器 GPIO 控制（多实例句柄注入）
- `ti/led`、`st/led` — LED GPIO 控制（多实例句柄注入，支持高低电平）
- `ti/key`、`st/key` — 按键扫描（双 task 架构：B 类消抖 + A 类事件分类，短按/长按/连发）
- `ti/encoder`、`st/encoder` — 编码器（TI: 左QEI+右捕获，STM32: 双QEI；static+getter）
- `ti/pwm`、`st/pwm` — PWM 输出（TI: 20kHz/CH0+CH1，STM32: ARR=8400/CH3+CH4）
- `ti/motor`、`st/motor` — 直流有刷电机，适配 DRV8874（IN/IN 模式，直接 PWM 比较值，左右方向引脚反相；引脚 cfg 注入）
- `ti/cam`、`st/cam` — 摄像头串口通信（UART DMA + FIFO + 帧解析）
  - st/cam：`cam_frame_ready`（`extern volatile uint8_t`），收到完整帧时置 1，外部轮询并手动清零
- `ti/gyroscope`、`st/gyroscope` — 姿态传感器（UART DMA + FIFO + 三态帧解析，STM32 额外含 yaw 增量追踪）
- `ti/ultrasonic`、`st/ultrasonic` — 超声波测距（HC-SR04，定时器捕获 + GPIO 触发 + 状态机）
- `ti/laser`、`st/laser` — 激光 GPIO 控制（多实例句柄注入）
- `ti/display`、`st/display` — 蓝牙调试仪表盘（数据汇总屏显，错误上报）
- `ti/pattern`、`st/pattern` — 循迹图案识别（查表法，纯算法）
- `ti/pid`、`st/pid` — PID 控制器（TI: 微分-on-实际值，STM32: 微分-on-误差）
- `ti/sensor`、`st/sensor` — 感为科技灰度传感器
  - mcu/：I2C 通信版（八路，TI: 轮询状态机，STM32: DMA）
  - non-mcu/：GPIO 直读版（通用，通道数和有效电平可配置，默认最大 8 路）
- `ti/oled`、`st/oled` + `oled_data` — SSD1306 OLED（0.96 寸 I2C 128×64，ASCII 字模）
  - TI：非阻塞 I2C 状态机（`oled_task` 每 tick 推进一个 I2C 操作，`oled_update` 仅置 pending 标志位）
  - STM32：HAL_I2C_Mem_Write 同步发送，超时 `OLED_I2C_TIMEOUT_MS=100ms`（128 字节/页约 4ms，100ms 留有充足余量应对时钟拉伸）
- `st/step_motor`、`ti/step_motor` — 张大头 ZDT X42S 步进电机（UART DMA+FIFO+二进制协议，TI 用软件 IDLE）
- `st/servo` — FashionStar RA8-U25-M 串口舵机（UART DMA+FIFO+官方 SDK 封装，句柄注入+角度限幅）
- `error_handler` — 错误处理模块（三层框架：传输/上报/处理，项目级 `error_source_t` 枚举 + 队列缓冲 + `switch(source)` 分派，含 `drv_err_t` 定义，tick 驱动 task）

## 电机闭环链路（encoder + pwm + motor）

三个模块配合 DRV8874 驱动两路编码电机。TI 和 STM32 版本的速度量纲不同：

| | TI | STM32 |
|---|---|---|
| 速度量纲 | PWM 比较值（0~2000） | PWM 比较值（0~8400） |
| PWM 映射 | 1:1（speed 直接写入 CC） | 1:1（speed 直接写入 CC） |
| PWM 通道 | CH0 / CH1 | CH3 / CH4 |
| PWM 载波 | 20kHz（CC=时钟/20000） | CubeMX ARR 决定（当前 8400） |
| 编码器 | 左 QEI + 右捕获 | 双 QEI（硬件方向判定） |
| 方向信息 | motor 软件标志位 → encoder | 编码器硬件自主判定 |
| 电机驱动芯片 | DRV8874 | DRV8874 |

闭环链路：

```
encoder_get_left/right()  →  PID  →  motor_set_speed_left/right()
       ↑                                │
encoder_scan_left/right()               ↓
       ↑                          pwm_set_compare_ch0/ch1()
[编码器硬件]                              ↓
                                  [DRV8874 → 电机]
```

PWM 参数以 20kHz 载波为基准：`PWM_MAX_COMPARE = 定时器时钟 / 20000`。SysConfig 中修改定时器时钟后必须同步更新 `pwm.h` 的 `PWM_MAX_COMPARE` 和 `motor.h` 的 `MOTOR_MAX_SPEED`。右编码器方向由 `motor_get_right_direction_sign()` 提供（encoder 依赖 motor）。

## 平台差异备忘

| | TI MSPM0 | STM32 |
|---|---|---|
| 平台头文件 | `"ti_msp_dl_config.h"` | `<stm32f4xx_hal.h>` |
| GPIO 端口类型 | `GPIO_Regs *` | `GPIO_TypeDef *` |
| GPIO 引脚类型 | `uint32_t` | `uint16_t` |
| GPIO 读取 | `DL_GPIO_readPins(port, mask)` 批量 | `HAL_GPIO_ReadPin(port, pin)` 单引脚 |
| DMARX启动 | 手动 `DL_DMA_setSrcAddr`/`setDestAddr`/... | `HAL_UARTEx_ReceiveToIdle_DMA` |
| 帧结束检测 | DMA 余量不变判定法（task 中轮询） | 硬件 IDLE 中断 |
| ISR 回调判空 | 无（用户 ISR 已确认实例） | 有（HAL 全局回调需防御） |
| 实例匹配 | `huart == inst.huart`（指针比较） | `huart->Instance == inst.huart->Instance` |
| 中断配置 | `DL_UART_Main_setRXInterruptTimeout` + `NVIC_EnableIRQ` | HAL 内部处理 |

## key 双 task 架构（重写其他输入模块时参考）

key 模块将按键处理拆分为两层 task，这是输入类模块的推荐架构：

- **key_scan_task**（B 类，ISR 1ms 直接调用）：积分式消抖 + 边沿检测，只做 GPIO 读取和计数器增减
- **key_task**（A 类，主循环 10ms flag 触发）：长按计时、连发计时、事件分类（SHORT/LONG/REPEAT/RELEASE）

两层之间通过 `volatile` 标志位（`just_pressed[]`/`just_released[]`）通信，scan 只写、task 只读并清零。

事件读取提供两种方式：`key_get_event()` 轮询和 `key_set_callback()` 回调。

## 待修改清单（v1.3 规范对照，等待逐一修改）

以下条目仍需对照 CODING_STANDARD.md v1.3 逐步确认。

### 一、错误处理框架（待目标工程补齐）

- [ ] **`error_process()` 具体恢复动作**：当前已为每个 `error_source_t` 条目补齐 `case`，但具体重新初始化、复位外设、降级运行等动作依赖目标工程实例和初始化上下文，需在实际项目中补充。

### 二、代码风格与硬件约束（待确认）

- [ ] **浮点数使用评估**：pid、gyroscope、servo、ultrasonic、step_motor 模块使用了 `float`。若目标 MCU 无 FPU，需改为定点数。

### 三、待验证项（需人工复查确认）

- [ ] **变量声明位置**：v1.3 要求变量在代码块开头声明，需逐文件确认是否有声明在代码块中间的遗漏
- [ ] **连续变量声明对齐**：v1.3 要求连续声明/赋值进行对齐，需逐文件确认
- [ ] **协议/硬件映射结构体**：检查是否有需加 `__attribute__((packed))` 的结构体遗漏

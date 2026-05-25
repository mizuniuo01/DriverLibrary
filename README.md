# DriverLibrary

个人嵌入式外设驱动库，适用于 STM32（HAL）和 TI MSPM0（DriverLib）系列 MCU。

所有代码遵循 `CODING_STANDARD.md`（v4.0），C 语言部分参照 BARR-C:2018 + Linux 内核编码规范，Python 部分参照 PEP 8。

> **注意**：全部模块已完成代码重写，但**均未实际测试**，使用前请验证。

## 目录结构

```
DriverLibrary/
├── st/                    # STM32 平台驱动（依赖 STM32 HAL）
├── ti/                    # TI MSP 平台驱动（依赖 TI DriverLib）
├── .claude/               # Claude Code 配置与 AI 协作指引
├── CODING_STANDARD.md     # 软件设计规范（代码风格 + 设计模式）
└── README.md
```

## 设计原则

- **非阻塞**：所有驱动基于状态机，禁止阻塞延时和死循环等待（初始化阶段除外）
- **模块独立**：每个驱动可单独复制到其他项目使用，不依赖项目级头文件
- **句柄注入**：硬件引脚和外设实例通过 `xxx_init()` 结构体参数传入
- **统一调度**：定时器 ISR 管理各模块执行周期，`volatile` 标志位驱动 task
- **数据封装**：`static` 私有变量 + getter 函数，禁止 `extern` 全局变量

详见 `CODING_STANDARD.md` §12。

## 模块清单

### 通信层

| 模块 | STM32 | TI | 说明 |
|------|:--:|:--:|------|
| blueteeth | ✓ | ✓ | 蓝牙串口透传（DMA+FIFO+帧解析），江协科技小程序 |
| cam | ✓ | ✓ | 摄像头串口通信（DMA+FIFO+帧解析） |
| gyroscope | ✓ | ✓ | 姿态传感器（DMA+FIFO+三态帧解析+校验，TI 用软件 IDLE） |
| display | ✓ | ✓ | 蓝牙调试仪表盘（数据汇总屏显，错误上报） |
| step_motor | ✓ | ✓ | 张大头 ZDT X42S 步进电机（UART 二进制协议，X 固件，TI 用软件 IDLE） |

### 驱动层

| 模块 | STM32 | TI | 说明 |
|------|:--:|:--:|------|
| motor | ✓ | ✓ | 直流有刷电机，DRV8874 IN/IN 模式，双路 PWM+方向 |
| encoder | ✓ | ✓ | 编码器（TI: 左QEI+右捕获，STM32: 双QEI） |
| pwm | ✓ | ✓ | PWM 输出（TI: CH0+CH1/20kHz，STM32: CH3+CH4/ARR=8400） |
| ultrasonic | ✓ | ✓ | 超声波测距 HC-SR04（定时器捕获，STM32: 三态非阻塞触发） |
| sensor | ✓ | ✓ | 感为科技八路灰度传感器 I2C（TI: 轮询状态机，STM32: DMA） |
| oled | ✓ | ✓ | OLED 0.96 寸 SSD1306 I2C 128×64（ASCII 字模 6×8 + 8×16） |

### GPIO 控制

| 模块 | STM32 | TI | 说明 |
|------|:--:|:--:|------|
| buzzer | ✓ | ✓ | 蜂鸣器 |
| led | ✓ | ✓ | LED（高低电平适配） |
| laser | ✓ | ✓ | 激光 |
| key | ✓ | ✓ | 按键（双 task：B 类消抖 + A 类事件分类，短按/长按/连发） |

### 算法层

| 模块 | STM32 | TI | 说明 |
|------|:--:|:--:|------|
| pid | ✓ | ✓ | PID 控制器（TI: 微分-on-实际值，STM32: 微分-on-误差） |
| pattern | ✓ | ✓ | 循迹图案识别（查表法，8 位灰度传感器） |

## 使用方式

驱动文件按需复制到目标项目，修改 `xxx_init()` 的参数（引脚、外设句柄、配置值）即可。每个 `.c` 文件的文件头中包含完整的 `@usage` 使用示例。

## 测试情况

| 模块 | STM32 | TI | 备注 |
|------|:--:|:--:|------|
| blueteeth | 未测 | 未测 | |
| cam | 未测 | 未测 | 数据解析逻辑待项目补充 |
| gyroscope | 未测 | 未测 | |
| display | 未测 | 未测 | |
| step_motor | 未测 | 未测 | 需配合 ZDT X42S 硬件实测 |
| motor | 未测 | 未测 | |
| encoder | 未测 | 未测 | |
| pwm | 未测 | 未测 | |
| ultrasonic | 未测 | 未测 | |
| sensor | 未测 | 未测 | 需配合感为科技八路灰度硬件 |
| oled | 未测 | 未测 | TI 版 I2C 忙等需验证 |
| buzzer | 未测 | 未测 | |
| led | 未测 | 未测 | |
| laser | 未测 | 未测 | |
| key | 未测 | 未测 | |
| pid | 未测 | 未测 | |
| pattern | 未测 | 未测 | 查表逻辑需配合实际传感器验证 |

## 规范

所有代码遵循 `CODING_STANDARD.md`（v4.0），C 语言部分参照 BARR-C:2018 + Linux 内核编码规范，Python 部分参照 PEP 8。

所有 `.c` 文件的文件头中包含完整的 `@usage` 使用示例。各模块详细说明见 `CLAUDE.md`。

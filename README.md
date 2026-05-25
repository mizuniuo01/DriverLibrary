# DriverLibrary

个人的嵌入式外设驱动库，适用于 STM32 和 TI MSP 系列 MCU。

> **注意**：TI 平台驱动目前仅适配 MSPM0G3507，其他 MSP 型号需自行调整。

## 目录结构

```
DriverLibrary/
├── st/                    # STM32 平台驱动（依赖 STM32 HAL）
├── ti/                    # TI MSP 平台驱动（依赖 TI DriverLib）
├── .claude/               # Claude Code 配置、技能与使用指引
├── CODING_STANDARD.md     # 软件设计规范（代码风格 + 设计模式）
└── README.md
```

## 设计原则

- **非阻塞**：所有驱动基于状态机，禁止 `HAL_Delay()` 和死循环等待
- **模块独立**：每个驱动可单独复制到其他项目使用，不依赖项目级头文件
- **句柄注入**：硬件引脚和外设实例通过结构体指针传入，不硬编码
- **统一调度**：定时器 ISR 管理所有模块的执行周期，`volatile` 标志位驱动 task

详见 `CODING_STANDARD.md` §12。

## 状态

旧代码已放入 `st/` 和 `ti/` 目录，待逐个按 `CODING_STANDARD.md` 重写。重写完成前，旧代码仅供参考。

已重写模块：`ti/blueteeth`。

## 模块清单

| 模块 | STM32 | TI | 说明 |
|------|-------|-----|------|
| blueteeth | ✓ | ✓ | 蓝牙串口通信（DMA+FIFO+协议解析），TI 平台已重写 |
| buzzer | ✓ | ✓ | 蜂鸣器 |
| cam | ✓ | ✓ | 摄像头串口通信 |
| display | — | ✓ | 蓝牙调试仪表盘 |
| encoder | ✓ | ✓ | 正交编码器 |
| gyroscope | ✓ | ✓ | 陀螺仪 |
| key | ✓ | ✓ | 按键扫描 |
| laser | ✓ | — | 激光测距 |
| led | ✓ | ✓ | LED 控制 |
| motor | ✓ | ✓ | 直流有刷电机（IN/IN 模式） |
| oled | ✓ | ✓ | OLED 显示屏 |
| pattern | ✓ | ✓ | 图案/模式检测 |
| pid | ✓ | ✓ | PID 控制器 |
| pwm | ✓ | ✓ | PWM 输出 |
| sensor | ✓ | ✓ | I2C 传感器 |
| step_motor | ✓ | — | 步进电机 |
| ultrasonic | ✓ | ✓ | 超声波测距 |

## 使用方式

驱动文件按需复制到目标项目，修改 `xxx_init()` 的参数（引脚、外设句柄、配置值）即可。

## 规范

所有代码遵循 `CODING_STANDARD.md`（v3.0），C 语言部分参照 BARR-C:2018，Python 部分参照 PEP 8。

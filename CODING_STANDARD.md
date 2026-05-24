# 嵌入式 C / Python 软件设计规范 (v3.0)

本规范 C 语言部分参照 **BARR-C:2018**（Embedded C Coding Standard），Python 部分参照 **PEP 8**。

第 1~11 章为代码风格规范（命名、格式、注释、类型），第 12 章为嵌入式软件设计模式规范（调度、状态机、模块接口、通信架构、错误处理等）。

适用范围：个人所有嵌入式项目（STM32 / TI MSP 系列）。

---

## 1. 总则

- **可读性第一**：代码是写给人读的，编译器不是你的读者。
- **一致性**：整个项目必须风格统一，一个单词在一个项目中只能有一种写法。
- **简洁性**：能用一行表达清楚的逻辑不要写三行，但不要为了省行数牺牲可读性。
- **语言**：注释使用**中文**，简短到位，不啰嗦。

---

## 2. 命名规范

### 2.1 宏 / 常量

`UPPER_SNAKE_CASE`，全大写，单词间下划线分隔。

```c
#define MAX_PWM_VALUE       2000
#define UART_TIMEOUT_MS     100
#define BUFFER_SIZE         256
```

### 2.2 变量

`snake_case`，全小写，单词间下划线分隔。

```c
uint16_t tx_buffer[BUFFER_SIZE];
uint32_t motor_position;
uint8_t  sensor_value;
```

**规则**：
- 禁止过度缩写，除非是极通用的（`i`, `j` 用作循环索引、`tmp` 用作临时值）。
- 允许的特例缩写：`rx`/`tx`（收发）、`pos`（position）、`cfg`（config）、`buf`（buffer）、`err`（error）、`cnt`（count）。
- 同一个单词在整个项目中只能有一种写法（严禁同时出现 `txBuf` 和 `tx_buffer`）。

### 2.3 函数

`snake_case`，全小写，下划线分隔，**动词开头**。

```c
void motor_move_to(uint16_t position);
void sensor_read(SensorData *data);
int  pid_compute(PidController *pid, float setpoint, float actual);
```

### 2.4 类型名

`PascalCase`（大驼峰），每个单词首字母大写。

```c
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} GpioMapping;

typedef enum {
    MOTOR_STATE_IDLE,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_ERROR,
} MotorState;

typedef struct MotorController MotorController;
```

**规则**：
- 类型名（`struct`、`enum`、`typedef`、`union`）必须一眼能看出是类型，不是变量实例。
- 枚举值使用 `UPPER_SNAKE_CASE` 或与类型名同前缀，如 `MOTOR_STATE_RUNNING`。

### 2.5 文件名

`snake_case.c` / `snake_case.h`，全小写，下划线分隔。

```
motor_controller.c
sensor_adc.c
pid_algorithm.c
```

**为什么不是 PascalCase**：`PascalCase` 在不同文件系统（尤其 Windows/macOS 不区分大小写）和 Git 中容易出问题，且 Linux 内核、Git、SQLite 等所有主流 C 项目都用 `snake_case`。

**规则**：
- 一对 `.c`/`.h` 的文件名必须完全相同。
- 不用大写字母，不用连接符（`-`），不用空格。

---

## 3. 代码格式

### 3.1 花括号（K&R 风格）

左括号不换行，右括号独占一行。

```c
if (condition) {
    // ...
}

while (condition) {
    // ...
}

for (int i = 0; i < n; i++) {
    // ...
}

void function(void) {
    // ...
}
```

**规则**：
- 控制流必须始终使用花括号，即使只有一条语句（BARR-C 规则 8.8）。

反例：
```c
/* 禁止：左括号换行（Allman 风格） */
if (condition)
{
    // ...
}

/* 禁止：省略花括号 */
if (condition) do_something();
```

### 3.2 缩进

- **4 空格**，禁止使用 Tab 字符。
- 预处理指令 `#` 顶格，内部内容缩进。

```c
#if CONFIG_USE_DMA
    #define DMA_BUFFER_SIZE  1024
#endif
```

### 3.3 列宽

**80 字符**。超长的函数声明、运算符表达式在合理位置换行。

### 3.4 换行

#### 原则

- 超过列宽（100）的行必须换行。
- 换行后的续行缩进一层（4 空格），与当前上下文对齐，不是随意缩进。
- 在**运算符之后**换行，不在运算符之前。

#### 函数声明与调用

参数少时放一行；参数多、放一行超过列宽时，**每个参数独占一行**：

```c
/* 一行能放下 */
void uart_init(UART_HandleTypeDef *huart, uint32_t baudrate);

/* 参数多，每个参数独占一行 */
void motor_controller_init(
    MotorController *motor,
    GPIO_TypeDef *step_port,
    uint16_t step_pin,
    GPIO_TypeDef *dir_port,
    uint16_t dir_pin,
    uint32_t max_speed
);
```

#### 长表达式

在低优先级运算符之后换行：

```c
/* 在比较运算符之后的逻辑运算符处换行 */
if ((sensor_read(&adc1) > THRESHOLD_HIGH)
    && (system_state == STATE_ACTIVE)
    && (safety_check_passed())) {
    // ...
}

/* 长算术表达式：在运算符之后换行 */
uint32_t total = base_value 
                 + offset_calculate(&config) 
                 + compensation_factor * temperature_read();
```

#### 三元运算符

在 `?` 和 `:` 之后换行：

```c
uint16_t result = (mode == MODE_FAST)
    ? quick_calculate(input)
    : precise_calculate(input);
```

#### 字符串

长字符串用相邻字符串自动拼接，不写超长一行：

```c
HAL_UART_Transmit(&huart1,
    "{\"cmd\":\"move\",\"pos\":1000,\"speed\":500}\r\n"
    "{\"cmd\":\"wait\",\"ms\":200}\r\n",
    len, TIMEOUT_MS);
```

#### 禁止多句同行

每条语句独占一行。禁止：

```c
/* 禁止 */
a = 1; b = 2; c = a + b;
```

### 3.5 指针 `*`

`*` 紧靠变量名（靠右）。

```c
uint8_t *rx_buffer;
void motor_init(UART_HandleTypeDef *huart);
int *a, *b;  /* 每个变量单独声明，不要这样批量写在一行 */
```

### 3.6 变量声明

- **一行只声明一个变量**。
- 变量声明放在所在代码块的开头（C99 及以后不强制，但保持一致性）。

```c
void function(void) {
    uint32_t count = 0;
    uint8_t  flag  = 0;
    int      i;

    for (i = 0; i < 10; i++) {
        // ...
    }
}
```

### 3.7 空格

| 位置 | 规则 | 示例 |
|------|------|------|
| 赋值运算符前后 | 加空格 | `x = y + z;` |
| 二元运算符前后 | 加空格 | `a + b`, `x == y` |
| 逗号后 | 加空格 | `func(a, b, c);` |
| 分号后（for 循环） | 加空格 | `for (i = 0; i < n; i++)` |
| 控制语句关键字后 | 加空格 | `if (`, `while (`, `for (` |
| 函数名与括号 | **不加**空格 | `func(x)`, **不是** `func (x)` |
| 一元运算符 | **不加**空格 | `!flag`, `*ptr`, `++i` |
| 数组下标 | **不加**空格 | `arr[i]` |
| `->` / `.` | **不加**空格 | `ptr->field` |

### 3.8 空行

- 函数与函数之间：一个空行。
- 逻辑关联度低的不同代码块之间：一个空行。
- 函数体内的逻辑段落之间：一个空行。
- 文件末尾：**必须且仅有一个**空行。
- 禁止连续多个空行。

### 3.9 对齐

**不进行人为对齐**。对齐会制造无意义的 diff，增加维护负担。

反例：
```c
/* 禁止：手动对齐产生无意义空白 diff */
uint32_t count       = 0;
uint8_t  small_flag  = 0;
int      i           = 0;
```

正例：
```c
uint32_t count = 0;
uint8_t small_flag = 0;
int i = 0;
```

### 3.10 禁止内容

- 禁止短 if / for / while 体与条件写在同一行。
- 禁止省略任何控制流的花括号。
- 禁止用 Tab 字符。
- 禁止行尾有空白字符。

### 3.11 switch / case

`case` 缩进一层，`case` 体再缩进一层。`default` 放在最后。

```c
switch (motor->state) {
    case MOTOR_STATE_IDLE:
        /* 空闲处理 */
        motor_stop(motor);
        break;
    case MOTOR_STATE_RUNNING:
        /* 运行处理 */
        motor_update_speed(motor);
        break;
    case MOTOR_STATE_ERROR:
        /* 错误处理 */
        motor_emergency_stop(motor);
        break;
    default:
        /* 意外状态，记录并复位 */
        motor->state = MOTOR_STATE_IDLE;
        break;
}
```

**规则**：
- 每个 `case` 必须以 `break` 结束。
- 故意的 fall-through 必须加注释说明： `/* fall through */`。
- `default` 必须存在，处理意外值。
- 不要在 `case` 中声明变量（除非用 `{}` 包裹整个 `case` 块的作用域）。

---

## 4. 注释规范

### 4.1 语言

注释统一使用**中文**。简洁到位，不啰嗦。不要写长篇解释——代码命名已经说明了"做什么"，注释只需要说明"为什么"。

### 4.2 函数注释（仅 `.c` 文件）

每个函数定义前必须使用以下格式：

```c
/**
 * @brief  根据当前位置计算电机的目标速度
 * @param  motor    电机控制器指针
 * @param  target   目标位置（编码器计数值）
 * @retval 无
 */
void motor_move_to(MotorController *motor, int32_t target) {
    // ...
}
```

**规则**：
- 此注释**只在 `.c` 文件中写**，`.h` 文件中禁止。
- 如果确实不需要参数和返回值，可以简化为单行 `/** @brief ... */`。
- 必填字段：`@brief`、`@param`（每个参数一个）、`@retval`。

### 4.3 宏注释

每个宏定义必须添加行注释。

```c
#define MAX_PWM_VALUE       2000    /* PWM 最大占空比值（对应 20ms 周期） */
#define PID_KP_DEFAULT      1.5f    /* PID 比例系数默认值 */
```

### 4.4 文件头注释

每个 `.c` 和 `.h` 文件开头写简短说明：

```c
/**
 * @file    motor_controller.c
 * @brief   步进电机运动控制模块
 */
```

### 4.5 注释位置

**注释独占一行，写在被注释代码的上方。** 不放在代码同一行的右侧。

```c
/* 正例：注释在上一行 */
/* 根据编码器方向校正位置符号 */
if (motor->direction == DIR_REVERSE) {
    position = -position;
}
```

```c
/* 反例：注释堆在代码行尾 */
if (motor->direction == DIR_REVERSE) {
    position = -position;  /* 根据编码器方向校正位置符号 */
}
```

**例外**：
- **宏定义**允许行尾注释（这是 C 工业界的惯例，见 4.3 示例）。
- **极短的变量用途说明**允许行尾注释： `volatile uint8_t dma_done; /* DMA 完成标志，ISR 中置位 */`

### 4.6 注释内容

- 说明"为什么这么做"，不说明"做了什么"——代码命名已经说明了"做什么"。
- 禁止在注释中向 AI 或其他人隔空喊话、解释历史原因、贴临时的设计讨论。

---

## 5. 文件组织

### 5.1 .c / .h 分离

- 接口（函数声明、类型定义、宏）放在 `.h` 文件。
- 实现（函数体、模块私有变量）放在 `.c` 文件。
- 不可暴露的实现细节用 `static` 限制作用域。

### 5.2 头文件保护

统一使用 `#ifndef` / `#define` / `#endif` 格式：

```c
#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

/* ... 内容 ... */

#endif /* MOTOR_CONTROLLER_H */
```

禁止使用 `#pragma once`（非标准，可移植性存疑）。

### 5.3 #include 顺序

1. 项目自身的头文件（如 `"motor_controller.h"`）
2. 项目内其他模块头文件
3. 第三方库头文件
4. 标准库头文件
5. HAL 等平台头文件

每组之间空一行。

```c
#include "motor_controller.h"

#include "sensor_adc.h"
#include "pid_algorithm.h"

#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"
```

### 5.4 头文件内容

头文件内：
- 宏定义和常量
- 类型定义（`typedef struct/enum/union`）
- 全局变量声明（`extern`）
- 函数声明
- 禁止写函数实现（`static inline` 小函数除外）。
- 禁止写函数文档注释。

---

## 6. 数据类型

### 6.1 固定宽度类型

**必须使用** `<stdint.h>` 的固定宽度类型，禁止使用 `int`、`long`、`short` 等平台相关类型（除非是 HAL 库返回值）。

```c
uint8_t   status;       /* 8 位无符号 */
int32_t   position;     /* 32 位有符号 */
uint16_t  pwm_duty;     /* 16 位无符号 */
bool      is_ready;     /* 使用 <stdbool.h> */
```

### 6.2 类型修饰符

- 不变变量加 `const`。
- 中断/并发访问的变量加 `volatile`。
- 模块私有函数/变量加 `static`。
- 使用这些修饰符时，在注释中简短说明原因。

```c
static MotorController motor;          /* 模块私有，外部通过函数接口访问 */
volatile uint8_t       g_dma_done;     /* DMA 中断中置位，主循环中读取 */
const uint16_t         pwm_max = 2000; /* 硬件决定的固定上限 */
```

### 6.3 枚举

优先使用 `enum` 代替一堆 `#define` 做状态标识。

```c
typedef enum {
    UART_STATE_IDLE,
    UART_STATE_RECEIVING,
    UART_STATE_DONE,
} UartRxState;
```

---

## 7. 预处理

### 7.1 宏函数

- 多行宏用 `do { ... } while (0)` 包裹。
- 参数和整体表达式用括号保护。

```c
#define CLAMP(val, min, max)  do { \
    if ((val) < (min)) (val) = (min); \
    if ((val) > (max)) (val) = (max); \
} while (0)
```

### 7.2 条件编译

- `#if` 和 `#else` 后加简短注释说明条件。
- 嵌套不超过 2 层。

```c
#if CONFIG_USE_DMA
    /* DMA 模式初始化 */
    HAL_UART_Receive_DMA(&huart1, rx_buffer, BUFFER_SIZE);
#else
    /* 中断模式初始化 */
    HAL_UART_Receive_IT(&huart1, rx_buffer, BUFFER_SIZE);
#endif
```

---

## 8. 函数设计

### 8.1 大小

一个函数只做一件事。行数没有硬性上限，但如果一个函数做的事开始分裂为多个独立逻辑，就该拆分。

### 8.2 参数

- 参数过多时考虑用结构体封装。
- 输出参数放最后。

### 8.3 防错处理

- 所有通过指针传入的参数，函数开头做空指针检查。

```c
void motor_set_speed(MotorController *motor, uint16_t speed) {
    if (motor == NULL) return;
    if (speed > motor->max_speed) return;

    motor->target_speed = speed;
}
```

### 8.4 返回类型

- 可能失败的函数返回错误码（`int`，0 表示成功，负值表示错误）。
- 简单的 setter/getter 用 `void`。

### 8.5 递归

**嵌入式系统中严格禁止使用递归**（BARR-C 规则 6.4）。递归导致栈占用不可预测，在栈空间有限的 MCU 上极易引发栈溢出。

### 8.6 动态内存

**嵌入式系统中严禁使用 `malloc`、`free`、`calloc`、`realloc`**（BARR-C 规则 1.4）。动态分配导致内存碎片、分配时间不可预测、分配失败难以恢复。所有内存需求必须在编译期确定。

### 8.7 变长数组

**禁止使用变长数组（VLA）**。`int arr[n]` 中 `n` 为变量时，同样在栈上动态分配，风险与递归相同。C11 已将 VLA 改为可选特性。

---

## 9. 变量与存储

### 9.1 初始化

所有局部变量必须在声明时或使用前初始化。

### 9.2 全局变量

- 全局变量尽可能少用。
- 必须使用时，通过 `extern` 在头文件中声明，在 `.c` 中定义。
- 不额外使用 `g_` 前缀，靠 `static` 和作用域管理可见性。

### 9.3 作用域最小化

能用局部变量就不用文件级变量，能用文件级变量就不用全局变量。能用 `static` 就加 `static`。

### 9.4 goto

`goto` 仅允许用于函数末尾的统一错误清理（common exit path）。禁止用 `goto` 向前跳转或构成循环。

```c
int spi_transfer(SPI_Handle *spi, uint8_t *tx, uint8_t *rx, uint16_t len) {
    int ret = 0;

    if (spi == NULL) { ret = -1; goto exit; }
    if (spi_init(spi) != 0) { ret = -2; goto exit; }

    /* ... 数据传输 ... */

exit:
    spi_deinit(spi);
    return ret;
}
```

### 9.5 无限循环

使用 `for (;;)`，不使用 `while (1)`。两者等价，`for (;;)` 是嵌入式惯例，且某些编译器对 `while (1)` 会产生条件判断警告。

---

## 10. 表达式与运算

### 10.1 括号

复杂表达式必须用括号明确优先级。不要依赖运算符优先级记忆。

```c
/* 正例：括号明确意图 */
if ((a > b) && ((c < d) || (e == f))) {
    // ...
}

/* 反例：依赖优先级，难以阅读 */
if (a > b && c < d || e == f) {
    // ...
}
```

### 10.2 布尔比较

- 禁止与 `true`/`false` 比较：用 `if (flag)` 而非 `if (flag == true)`。
- 禁止与 `NULL` 比较时写成 `== NULL` 或 `!= NULL`：用 `if (!ptr)` 或 `if (ptr)`。
- 指针直接判断：`if (ptr)` 等价于 `if (ptr != NULL)`。

```c
/* 正例 */
if (data_ready) { /* ... */ }
if (!ptr) return;

/* 反例 */
if (data_ready == true) { /* ... */ }
if (ptr == NULL) return;
```

**例外**：与 0 比较的数值型判断必须显式写出比较运算符：

```c
if (count == 0) { /* ... */ }
if (ret != 0) { /* ... */ }
```

### 10.3 赋值与条件分离

**禁止在条件语句中进行赋值**（BARR-C 规则 9.1）。

```c
/* 反例 */
if ((ret = read_data()) != 0) { /* ... */ }

/* 正例 */
ret = read_data();
if (ret != 0) { /* ... */ }
```

### 10.4 sizeof

`sizeof` 用于变量名而非类型名，确保类型变更时无需同步修改：

```c
/* 正例 */
memset(&cfg, 0, sizeof(cfg));

/* 反例 */
memset(&cfg, 0, sizeof(ConfigStruct));
```

### 10.5 有符号/无符号混用

避免在同一个表达式中混用 `int32_t` 和 `uint32_t`。如需混用，显式做类型转换。

```c
int32_t  delta;
uint32_t position;

/* 正例：显式转换 */
delta = (int32_t)(target - position);
```

---

## 11. 标准库使用

### 11.1 限制库函数

嵌入式系统中，以下标准库函数被限制或禁止：

| 函数 | 状态 | 替代方案 |
|------|------|----------|
| `malloc`/`free`/`calloc`/`realloc` | 禁止 | 静态分配或编译期确定大小 |
| `printf`/`sprintf` | 限制 | 仅用于 debug 输出，禁止在 ISR 中调用 |
| `strcpy`/`strcat`/`gets` | 禁止 | 使用 `strncpy`/`strncat`，始终指定最大长度 |
| `atoi`/`atol`/`atof` | 禁止 | 使用 `strtol`/`strtoul`，可检测错误 |
| `memcpy`/`memset` | 允许 | 注意 length 参数不要溢出 |
| `rand`/`srand` | 避免 | 嵌入式一般不需要随机数，需要时用硬件 TRNG |
| `assert` | 允许 | 仅用于开发阶段发现 bug，发布版本应禁用 |

### 11.2 字符串格式化

- 使用 `snprintf` 而非 `sprintf`，始终指定缓冲区大小。
- 禁止使用 `%s` 格式说明符而不限制宽度。

```c
/* 正例 */
snprintf(buf, sizeof(buf), "Sensor: %lu", value);

/* 反例 */
sprintf(buf, "Sensor: %lu", value);      /* 无边界保护 */
snprintf(buf, 256, "Sensor: %lu", value); /* 硬编码大小 */
```

---

## 12. 嵌入式专项规范

### 12.1 非阻塞（核心原则）

**严格禁止**在主循环或任务中使用 `HAL_Delay()` 或死循环等待标志位。必须使用状态机、定时器计数或标志位轮询。

例外：上电初始化阶段的少量延时（如等待外设稳定上电）可以使用阻塞等待。

### 12.2 统一调度机制

所有模块的执行周期由一个硬件定时器中断统一管理。ISR 内维护分频计数器，到周期后置 `volatile` 标志位；主循环无脑调用各模块的 `xxx_task()`；各 task 检测自己的标志位，若置位则执行一次并清零。

**标志位规范**：

- 类型：`volatile uint8_t`（bool 型标志），需要累积计数的场合用 `volatile uint32_t`
- 命名：`<module>_tick_flag`（如 `motor_tick_flag`、`sensor_tick_flag`）
- ISR 只写（置 1），task 读并清零
- 声明：模块 `.c` 定义，`.h` 中以 `extern` 暴露（供 ISR 所在文件引用）

**调度周期确定原则**：

周期由被控对象的物理特性决定，不可拍脑袋定值。确定方法：

- **控制类**（电机电流环、循迹传感器）：由系统带宽决定。带宽越高、响应越快的外设，周期越短。原则是先确定系统所需的控制频率 $f$，再取周期 $T = 1/f$，并预留至少 2 倍余量。
- **通信类**（UART 协议检测、I2C 传感器轮询）：由数据到达速率决定。周期应短于数据帧间隔，确保不会漏帧。
- **人机交互类**（OLED 刷新、LED 闪烁）：由人眼感知阈值决定。OLED 刷新 30~50Hz 即可，LED 闪烁 1~2Hz 即可。
- **低频监控类**（任务心跳检测、电池电压）：秒级即可，无实时性要求。

具体数值在各自项目的 ISR 中根据实际情况设定，规范不做硬性规定。

**ISR 模板**：

```c
void SysTick_Handler(void)
{
    static uint8_t motor_tick_cnt = 0;
    static uint8_t sensor_tick_cnt = 0;

    tick_ms++;

    /* 1ms：极简操作，直接在 ISR 内完成，不置标志位 */
    key_tick_process();

    /* 5ms：置标志位，推迟到 task 中处理 */
    motor_tick_cnt++;
    if (motor_tick_cnt >= 5) {
        motor_tick_cnt = 0;
        motor_tick_flag = 1;
    }

    /* 20ms */
    sensor_tick_cnt++;
    if (sensor_tick_cnt >= 20) {
        sensor_tick_cnt = 0;
        sensor_tick_flag = 1;
    }
}
```

**主循环模板**：

```c
int main(void)
{
    system_init();
    while (1) {
        wdt_feed();
        motor_task();
        sensor_task();
        blueteeth_task();
        oled_task();
    }
}
```

**Task 函数模板**：

```c
void motor_task(void)
{
    if (!motor_tick_flag) return;
    motor_tick_flag = 0;

    /* 实际工作逻辑 */
}
```

少数周期不固定或极低频的模块，允许在 task 内部用 `static last_tick` + `tick_ms` 做自计时，不作为主要方式。

**Task 操作分类**：

| 类型 | 适用场景 | 执行位置 | 约束 |
|------|----------|----------|------|
| A 类（标志位触发） | 复杂状态机、协议解析、控制算法 | 主循环 task 中 | ISR 只置 flag |
| B 类（ISR 直接执行） | 极简读取：读寄存器、算差值、存缓存 | ISR 对应时间槽中 | 禁止循环等待、禁止 DMA 操作、禁止浮点运算 |

B 类示例：`Encoder_Get_Left` 在 ISR 的对应时间槽中直接调用（读 CNT 寄存器 → 算差值 → 存缓存）。A 类示例：`Blueteeth_Task` 在标志位触发后执行帧解析状态机。

### 12.3 串口通信架构

统一采用 **DMA + IDLE 中断 + 环形缓冲区（FIFO）** 架构：

```
[UART 硬件] ←→ [DMA 缓冲区] ←→ [Ring FIFO] ←→ [协议解析器]
```

- DMA 负责数据搬运，不占 CPU。
- IDLE 中断（或 RX Timeout）检测帧结束。
- 环形缓冲区解耦"接收"和"处理"，两者异步运行。

**环形缓冲区实现标准**：

- 数据结构：`uint8_t buffer[SIZE]` + `volatile uint16_t read_pos` + `volatile uint16_t write_pos`
- 空/满判断：**保留一个空位**。`write_pos == read_pos` 判空，`(write_pos + 1) % SIZE == read_pos` 判满
- 写操作（ISR 中调用）：将 DMA 缓冲数据逐个推入 FIFO，满则丢弃
- 读操作（task 中调用）：逐字节取出喂给协议解析器
- 提供标准操作：`push` / `pop` / `available` / `flush`

**UART 接收完整流程**：

1. `xxx_init()`：启动 DMA 接收（IDLE 模式），清空 FIFO，保存句柄
2. ISR 检测到 IDLE 或 DMA Done → 调 `xxx_rx_callback(huart, rx_len)`
3. `xxx_rx_callback()`：将 DMA 缓冲数据推入 RX FIFO，重启 DMA 接收
4. `xxx_task()`：从 RX FIFO 逐字节取出，喂给帧解析状态机

**UART 发送架构：TX FIFO + DMA**：

与接收对称。应用层写 TX FIFO → 若发送空闲则启动 DMA → DMA Done ISR 中从 TX FIFO 取下一批继续发送，直到 FIFO 空。

**TI 平台补充**：TI DriverLib 的 UART DMA 模式无硬件 IDLE 中断，使用 **DMA 余量不变判定法**——在 task 中每 2ms 检查 DMA 剩余传输量，若两次连续检查余量不变，则判定为帧结束。此方法为 TI 平台特化，STM32 使用硬件 IDLE 中断即可。

**环形缓冲区写入示例（ISR 中调用）**：

```c
void ringbuf_push_isr(ringbuf_t *rb, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        uint16_t next = (rb->write_pos + 1) % FIFO_SIZE;
        if (next != rb->read_pos) {
            rb->buffer[rb->write_pos] = data[i];
            rb->write_pos = next;
        }
    }
}
```

### 12.4 中断服务函数（ISR）

中断服务函数必须**极简**：

**允许的操作**：

- 读外设状态寄存器、清中断标志
- 读写 `volatile` 标志位
- 从 DMA 缓冲区搬数据到 FIFO
- 调用极简回调（仅做数据搬运和置标志位）

**禁止的操作**：

- 浮点运算（除非确认 FPU 上下文已保存）
- `printf` / `snprintf` 等重量级函数
- `HAL_Delay` / 循环等待标志位
- 复杂状态机推进
- `malloc` / `free`

**中断优先级原则**：

| 优先级 | 外设类型 | 说明 |
|--------|----------|------|
| 高 | PWM、编码器定时器、紧急停机 | 控制类外设，抖动直接影响系统性能 |
| 中 | UART、I2C、SPI、DMA | 通信外设，数据完整性要求 |
| 低 | SysTick、系统定时器 | 仅做计数和置标志位，可被其他 ISR 抢占 |

```c
void UART_xxx_IRQHandler(void)
{
    /* 1. 读状态寄存器，判断中断源 */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
        /* 2. 清中断标志 */
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        /* 3. 调回调，回调内只做数据搬运和置标志位 */
        xxx_rx_callback(&huart1, rx_len);
    }
    /* 4. 错误检测 */
    if (error_condition) {
        xxx_error_callback(&huart1);
    }
}
```

### 12.5 状态机设计

每个有状态的外设驱动必须使用状态机模式，杜绝阻塞等待。状态机在 `xxx_task()` 中推进，每次调用只走一步。

**状态定义**：

```c
typedef enum {
    XXX_STATE_IDLE = 0,       /* 必须存在，作为初始/恢复状态 */
    XXX_STATE_BUSY,
    XXX_STATE_WAIT_DMA,
    XXX_STATE_ERROR,
} xxx_state_t;
```

- 枚举值 `UPPER_SNAKE_CASE`，前缀为模块名大写
- 必须包含 `IDLE` 状态
- 每个有等待逻辑的状态必须设**超时上限**，防止硬件异常导致状态机卡死

**状态机模板**：

```c
void xxx_task(void)
{
    if (!xxx_tick_flag) return;
    xxx_tick_flag = 0;

    switch (xxx.state) {
        case XXX_STATE_IDLE:
            /* 启动操作，进入下一状态 */
            xxx.state = XXX_STATE_BUSY;
            break;

        case XXX_STATE_BUSY:
            if (操作完成条件) {
                xxx.state = XXX_STATE_IDLE;
            } else if (超时条件) {
                xxx.state = XXX_STATE_IDLE;
            }
            break;

        case XXX_STATE_ERROR:
            /* 错误恢复：复位外设、清空缓冲、重置状态 */
            xxx_reset();
            xxx.state = XXX_STATE_IDLE;
            break;
    }
}
```

### 12.6 模块接口规范

每个驱动模块对外暴露一组固定模式的函数：

```c
/* === 生命周期 === */
void xxx_init(XxxHandle *h, XxxCfg *cfg);   /* 初始化：绑定硬件、设置默认参数 */
void xxx_deinit(XxxHandle *h);               /* 反初始化：安全停机（按需） */

/* === 周期性任务 === */
void xxx_task(void);                         /* 状态机推进，主循环调用 */

/* === 控制与查询接口 === */
int  xxx_set_xxx(XxxHandle *h, ...);         /* 设置参数，返回 0 成功 / 负值错误 */
int  xxx_get_xxx(XxxHandle *h, ...);         /* 获取数据 */

/* === 中断回调（ISR 调用，不对外暴露） === */
void xxx_rx_callback(XxxHandle *h, ...);     /* 接收完成回调 */
void xxx_tx_callback(XxxHandle *h);          /* 发送完成回调 */
void xxx_error_callback(XxxHandle *h);       /* 硬件错误回调 */
```

不需要每个模块都实现全部函数——按实际需要取舍。

**返回值约定**：

```c
#define DRV_OK          0
#define DRV_ERR_PARAM  (-1)   /* 参数非法（含空指针、越界） */
#define DRV_ERR_BUSY   (-2)   /* 设备忙 */
#define DRV_ERR_TIMEOUT(-3)   /* 操作超时 */
#define DRV_ERR_IO     (-4)   /* 硬件通信错误 */
#define DRV_ERR_STATE  (-5)   /* 状态机状态非法 */
```

- 简单 setter/getter：返回 `void`
- 可能失败的操作：返回 `int`，0 成功，负值对应错误码
- 数据获取需区分有效/无效时，在数据结构中设 `is_valid` 字段

### 12.7 模块间数据共享

模块内部数据用 `static` 限定作用域，外部通过**函数接口**访问，禁止暴露全局变量。

```c
/* === 正例：函数接口封装 === */

/* encoder.h */
int16_t encoder_get_left(void);
int16_t encoder_get_right(void);
void    encoder_get_both(int16_t *left, int16_t *right);

/* encoder.c */
static int16_t encoder_left_val;   /* 模块私有 */
static int16_t encoder_right_val;

int16_t encoder_get_left(void) {
    return encoder_left_val;
}
```

```c
/* === 反例：暴露全局变量 === */

/* encoder.h */
extern Encoder_Data_t encoderData;  /* 外部可随意读写 */
```

**模块间通信方式**：

| 方式 | 适用场景 | 示例 |
|------|----------|------|
| 直接函数调用 | 一对一数据流 | `pid_calc()` 返回值传给 `motor_set_speed()` |
| Getter 函数 | 一个生产者、多个消费者 | `encoder_get_left()` 被 PID、Trace、Navi 等模块调用 |
| 回调函数 | 异步事件通知（DMA 完成、按键触发、硬件错误） | `xxx_rx_callback()` 在 ISR 中被调用 |

**规则**：

- 生产者模块提供 getter，消费者通过 getter 拉取数据
- 禁止消费者直接写生产者的数据
- 回调函数内只做数据搬运和置标志位，不做复杂逻辑

### 12.8 单实例与多实例

默认采用**多实例设计**，句柄由调用者分配。模块内部无全局状态，所有状态在句柄中。

```c
/* 多实例：句柄在栈上或静态分配 */
motor_t motor_left;
motor_t motor_right;

motor_init(&motor_left, &motor_left_cfg);
motor_init(&motor_right, &motor_right_cfg);
motor_set_speed(&motor_left, 500);
```

以下情况允许**单实例**（模块内部 `static` 变量，无需传句柄）：

- 硬件资源全局唯一（系统时钟、看门狗、电源管理）
- 板级设计客观上只有一个（唯一的蜂鸣器、唯一的板载 LED）
- 纯算法模块无内部状态（函数式，输入→计算→输出）

### 12.9 错误处理策略

**错误分类**：

| 类别 | 检测方式 | 处理策略 |
|------|----------|----------|
| 参数错误（空指针、越界） | 函数入口处同步检查 | 立即返回错误码 |
| 硬件通信错误（I2C NACK、UART Frame Error） | ISR 中检测，调 error_callback | 回调置错误标志位 + 复位外设 |
| 超时错误（DMA 等待超时、传感器无应答） | Task 中 tick 比较 | 状态机超时恢复 + 上报 |
| 逻辑错误（状态机非法状态） | `switch` 的 `default` 分支 | 强制复位到 IDLE + 错误上报 |

**错误恢复流程**：

```
检测错误 → 置错误标志位 → 错误上报（蓝牙屏显） → 复位外设 → 自动重试（限次数）→ 仍失败则上报故障
```

**参数校验深度**：

| 接口层级 | 校验深度 | 方式 |
|----------|----------|------|
| 公开接口（`.h` 声明的函数） | 完整校验 | 判空指针、判参数范围，返回错误码 |
| 模块内部函数（`static`） | 轻量校验 | 仅在关键路径判空，其余用前置条件注释说明 |
| ISR 回调 | 最小校验 | 仅判句柄匹配（Instance 比较） |

### 12.10 初始化依赖链

`system_init()` 中的初始化顺序必须满足：**被依赖的模块先初始化**。

典型依赖链：

```
SYSCFG / 时钟
  → GPIO（LED、按键、方向引脚）
    → 通信外设（UART、I2C、SPI）
      → PWM / QEI / Capture 定时器
        → 驱动模块（Motor、Encoder、Sensor）
          → 算法模块（PID、Trace、Navi）
```

每个模块的 `xxx_init()` 函数注释中必须标注它依赖哪些外设已初始化：

```c
/**
 * @brief  电机初始化
 * @note   依赖：PWM 定时器已初始化，GPIO 方向引脚已配置
 */
void motor_init(motor_t *motor, const motor_cfg_t *cfg);
```

### 12.11 看门狗与错误上报

**硬件看门狗**：

- 使用独立看门狗（IWDG）
- 喂狗在 `main()` 循环的**一个统一位置**（循环顶部或底部），禁止在 task 内部分散喂狗
- 看门狗复位后，在初始化阶段检测复位原因并做提示（如 LED 快闪）

**软件看门狗（任务心跳监控，按需启用）**：

为关键 task 维护心跳计数器，在低频监控 task 中检测。若某 task 的心跳计数在两个检测周期之间未变化，则判定该 task 已卡死，触发错误上报。

**错误上报**：

采用蓝牙屏显输出，通过 `Display` 模块统一管理。`Display` 模块内部维护一个错误字符串，对外提供修改接口，在 `Display_Task()` 刷新周期中通过 `Blueteeth_Display()` 打印到指定的错误行。

```c
/* === display.h === */
#define DISPLAY_LINE_ERROR_Y  260   /* 错误信息专用行 */

void display_show_error(const char *format, ...);

/* === display.c === */
static char g_error_msg[64];

void display_show_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_error_msg, sizeof(g_error_msg), format, args);
    va_end(args);
}

void Display_Task(void)
{
    if (!displayRefreshFlag) return;
    displayRefreshFlag = 0;

    /* ... 其他数据行 ... */

    /* 错误行：有错则显示，无错则清空 */
    Blueteeth_Display(0, DISPLAY_LINE_ERROR_Y,
                      (g_error_msg[0] != '\0') ? "Err: %s" : "",
                      g_error_msg);
}

/* === 各模块中触发错误上报 === */
void motor_task(void)
{
    if (motor.error_flag) {
        motor.error_flag = 0;
        motor.retry_count++;

        if (motor.retry_count > MAX_RETRY) {
            display_show_error("motor: stuck");
            motor.fault = 1;
        }
    }
}
```

原则：

- 蓝牙串口软件：江协科技蓝牙串口小程序
- 只上报错误，不记录一般信息和调试信息
- 一个时刻通常只有一个错误源，一条屏显行足够
- 错误格式：模块名 + 错误简述，如 `"motor: init fail"`、`"sensor: i2c nack"`
- `Display_Task` 中无错误时该行输出空字符串，覆盖旧的错误信息

### 12.12 句柄传递

编写外设函数时，**传递 HAL 句柄指针**，而非在函数内部写死 `&huart1`：

```c
/* 正例：通用、可测试 */
void uart_send(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len);

/* 反例：写死句柄 */
void uart_send(uint8_t *data, uint16_t len) {
    HAL_UART_Transmit_DMA(&huart1, data, len);
}
```

### 12.13 原子操作与临界区

在 main loop 和中断**同时访问**同一个变量时，必须使用临界区保护：

```c
uint32_t primask = __get_PRIMASK();
__disable_irq();
g_data_ready = 0;   /* 关中断保护 */
if (!primask) __enable_irq();
```

**规则**：

- 只有 ISR 写 `write_pos`，只有 task 写 `read_pos`（单写者原则，可消除大部分竞态）
- task 中写 `read_pos` 和 ISR 中读 `read_pos` 仍有竞态：Cortex-M 上对 16 位变量的单指令对齐访问是原子的，对 32 位变量或 MSP430 等 16 位架构则需关中断

### 12.14 安全保护

- 所有控制输出（PWM、速度、角度）在驱动层或逻辑层做范围限幅。
- PID 输出做积分饱和限制。
- 使用看门狗（IWDG），喂狗放在主循环的一个统一位置。

```c
pwm_output = CLAMP(pwm_output, PWM_MIN, PWM_MAX);
motor_set_pwm(pwm_output);
```

### 12.15 魔法数字

严禁在代码中出现无名称的常量。必须在头文件中用 `#define` 或 `const` 语句定义，并按类型分组排列。

配置常量（如 `MOTOR_MAX_SPEED`）直接定义在驱动 `.h` 中。不同项目使用时自行修改驱动文件中的值，无需额外的配置覆盖机制。

```c
/* motor.h */

/* PWM 参数 */
#define PWM_PERIOD          2000
#define PWM_MAX             2000
#define PWM_MIN             0

/* 运动参数 */
#define MOTOR_MAX_SPEED     5000
#define MOTOR_ACCELERATION  100
```

### 12.16 模块独立性检查清单

每个驱动模块必须满足以下全部条件：

- [ ] `.c` 文件第一个 `#include` 是本模块的 `.h`
- [ ] 不 `#include` 任何项目级头文件（`main.h`、`header.h`）
- [ ] 不硬编码引脚、外设实例、外设基地址
- [ ] 硬件依赖通过 `xxx_init()` 的结构体参数注入
- [ ] 模块内部状态全部 `static`
- [ ] 外部只通过函数接口访问模块数据
- [ ] 默认多实例设计（除非符合单实例条件）
- [ ] 可以脱离当前项目单独复制到其他项目使用

---

## 13. Python 规范（PEP 8）

### 13.1 命名

| 元素 | 规则 | 示例 |
|------|------|------|
| 函数 | `snake_case` | `def save_frame():` |
| 变量 | `snake_case` | `frame_count = 0` |
| 类 | `PascalCase` | `class FrameProcessor:` |
| 常量 | `UPPER_SNAKE_CASE` | `MAX_FPS = 30` |
| 私有成员 | 单下划线前缀 | `_internal_buffer` |

### 13.2 格式

- 4 空格缩进，禁止 Tab。
- 列宽 79（PEP 8 规定）。
- 运算符前后各一个空格。
- 逗号后一个空格。
- 函数定义间两个空行，类方法间一个空行。
- 文件末尾**必须且仅有一个**空行。

### 13.3 import 顺序

1. 标准库
2. 第三方库
3. 本地模块

每组之间一个空行。

```python
import os
import json

import cv2
import numpy as np

from sensor import CameraReader
from processor import FramePipeline
```

### 13.4 注释

- 注释使用**中文**（与 C 统一）。
- 函数用 docstring（`"""..."""`），描述参数和返回值。

```python
def detect_circle(image, min_radius=10, max_radius=100):
    """对输入图像做霍夫圆检测，返回圆心和半径列表。"""
    pass
```

### 13.5 其他

- 使用 `if __name__ == "__main__"` 保护入口。
- 使用 f-string 做字符串格式化，不用 `%`。
- 类型提示推荐但不强制。
- 异常捕获指定具体类型，不使用裸 `except:`。

---

## 14. 项目组织

### 14.1 目录结构

不强制规定目录布局。使用代码生成工具（STM32CubeMX、TI SysConfig 等）时，**以工具生成的结构为准**，不要为了统一而去改动工具的输出目录。

手动创建的项目按实际需要组织即可，不需要遵循固定模板。

### 14.2 编译

- 编译警告必须全部消除，禁止提交有警告的代码。
- 建议启用 `-Wall -Wextra -Werror`（将所有警告视为错误）。

### 14.3 Git

- `.gitignore` 至少忽略：`build/`、`*.o`、`*.elf`、`*.bin`、`*.hex`、`*.swp`、`*.orig`，以及 IDE 自动生成的配置目录（如 `.vscode/` 中的非配置文件）。
- Commit message 使用英文，动词开头，简洁说明变更原因。
- 不使用 `git commit --amend` 修改已推送的提交。

### 14.4 README

每个项目必须有 `README.md`，至少包含：
- 项目名称与用途说明
- 所用硬件平台和引脚分配
- 编译方法（工具链、依赖）
- 烧录与调试方法

---

## 附录 A：.clang-format 关键对齐项

以下设置严格对齐本文档第 3 章（代码格式），在 `.clang-format` 中对应：

| 规则 | clang-format 设置 |
|------|-------------------|
| K&R 花括号 | `BreakBeforeBraces: Attach` |
| 4 空格缩进 | `IndentWidth: 4` / `UseTab: Never` |
| 列宽 80 | `ColumnLimit: 80` |
| 指针靠右 | `PointerAlignment: Right` |
| 控制语句后空格 | `SpaceBeforeParens: ControlStatements` |
| 函数名后不加空格 | 此为默认，不设置 |
| 禁止短 if/for 同⾏ | `AllowShortIfStatementsOnASingleLine: false` / `AllowShortLoopsOnASingleLine: false` |
| 不人为对齐 | `AlignConsecutiveAssignments: false` / `AlignConsecutiveDeclarations: false` / `AlignTrailingComments: false` |
| 缩进 case 标签 | `IndentCaseLabels: true` |
| include 排序 | `SortIncludes: true` |

---

## 附录 B：各规范简要对照

| 元素 | 本规范（BARR-C 对齐） | Linux 内核 | Google C++ | STM32 HAL |
|------|----------------------|------------|------------|-----------|
| 变量命名 | `snake_case` | `snake_case` | `snake_case` | `snake_case` |
| 函数命名 | `snake_case` | `snake_case` | `PascalCase` | `PascalCase` |
| 类型命名 | `PascalCase` | `snake_case` | `PascalCase` | `PascalCase` |
| 宏命名 | `UPPER_SNAKE_CASE` | `UPPER_SNAKE_CASE` | `UPPER_SNAKE_CASE` | `UPPER_SNAKE_CASE` |
| 花括号 | K&R | K&R | K&R | K&R |
| 缩进 | 4 空格 | 8 空格 | 2 空格 | 2/4 空格 |
| 指针 `*` | 靠右 | 靠右 | 靠左 | 靠右 |
| 列宽 | 80 | 80 | 80 | 无明确 |

# 远程数据采集装置 — Remote DAQ

> **智能仪器仪表课程设计 · 课题 #33**  
> 基于 STM32F407ZGT6 的 8 路远程数据采集系统  
> GitHub: [wensedong/Smart_Device_Course_Design](https://github.com/wensedong/Smart_Device_Course_Design)

---

## 1. 功能概述

| 特性 | 说明 |
|------|------|
| **8 路 IO 输入** | 独立按钮（触发翻转对应位状态） |
| **8 路 LED 指示** | PB0~PB7，实时跟随 IO 状态 |
| **0.96" OLED 显示** | SSD1306 I2C，周期刷新 IO 状态 + 采样计数 |
| **串口通信** | USART1 115200bps，数据帧收发 |
| **双模式发送** | 手动发送（按键触发）/ 自动发送（1s 周期） |
| **PC 远程查询** | 上位机发送 0x01 指令 → 立即返回数据帧 |

---

## 2. 硬件平台

### 2.1 核心板

- **MCU**: STM32F407ZGT6（Cortex-M4，168MHz）
- **晶振**: HSE 8MHz → PLL → SYSCLK 168MHz
- **最小系统板**（用户自有）

### 2.2 外设

| 外设 | 型号/参数 | 接口 |
|------|-----------|------|
| OLED 显示屏 | 0.96" SSD1306 128×64 | I2C2（PB10=SCL, PB11=SDA, 100kHz） |
| USB-UART | CH340 / CP2102 | USART1（PA9=TX, PA10=RX, 115200 8N1） |
| 8 路 IO 按钮 | 自复位按键（配下拉电阻） | PD1~PD9（EXTI，上升沿触发） |
| 8 路 LED | 5mm 红色（配限流电阻 220Ω） | PB0~PB7（推挽输出） |
| 模式按键 | 开发板板载 KEY_UP + 预留 PE4 | PA0 + PE4（EXTI，上升沿触发） |

### 2.3 引脚分配

```
                         ┌──────────────────────┐
                         │   STM32F407ZGT6      │
                         │                      │
    IO bit0 (IN2)   PD1─┤                      ├─PB0  LED0
    IO bit1 (IN3)   PD2─┤                      ├─PB1  LED1
    IO bit2 (IN4)   PD3─┤                      ├─PB2  LED2
    IO bit3 (IN6)   PD5─┤                      ├─PB3  LED3
    IO bit4 (IN7)   PD6─┤                      ├─PB4  LED4
    IO bit5 (IN8)   PD7─┤                      ├─PB5  LED5
    IO bit6 (IN1)   PD8─┤                      ├─PB6  LED6
    IO bit7 (IN5)   PD9─┤                      ├─PB7  LED7
                         │                      │
    手动发送 KEY_AUTO PA0─┤                      ├─PA9  USART1_TX
    自动模式 KEY_MAUUL PE4─┤                      ├─PA10 USART1_RX
                         │                      │
    OLED SCL        PB10─┤                      │
    OLED SDA        PB11─┤                      │
                         └──────────────────────┘
```

> **按钮硬件**: 按键一端接 GND，另一端接 MCU 引脚。CubeMX 配置为 Pull-down + Rising edge 触发（开发板 KEY_UP 本身接 3.3V，故用 Rising edge）。

---

## 3. 软件架构

### 3.1 文件结构

```
Smart_Device_Course_Design/
├── Mycode/                        ← 自定义应用代码
│   ├── codetab.h                  # 16×16 中文字库（历史项目迁移）
│   ├── oled.h / oled.c            # SSD1306 OLED 驱动（硬件 I2C2）
│   ├── daq.h / daq.c              # 数据采集、EXTI、串口通信核心
│   └── display.h / display.c      # LED + OLED 显示层
├── Core/                          ← CubeMX 生成（HAL 库）
│   ├── Inc/ (main.h, gpio.h, i2c.h, tim.h, usart.h, stm32f4xx_it.h)
│   └── Src/ (main.c, gpio.c, i2c.c, tim.c, usart.c, stm32f4xx_it.c)
├── Drivers/                       ← CMSIS + HAL 驱动
├── MDK-ARM/                       ← Keil 工程文件
└── Smart_Device_Course_Design.ioc ← CubeMX 项目
```

### 3.2 模块架构

```
┌────────────────────────────────────────────────┐
│                    主循环 (main.c)               │
│  send_flag? → SendDataFrame() → sample_count++ │
│  UpdateLEDs(io_state)          // 实时刷新      │
│  oled_flag? → UpdateOLEDDisplay(io_state)      │
└──────────────────┬─────────────────────────────┘
                   ↑ 读取/写入 volatile flags
    ┌──────────────┼──────────────┐
    │              │              │
    ▼              ▼              ▼
┌──────────┐ ┌──────────┐ ┌──────────┐
│ daq.c    │ │display.c │ │ oled.c   │
│ 数据采集 │ │ LED+OLED │ │ SSD1306  │
│ EXTI回调 │ │ 应用层   │ │ 底层驱动 │
│ TIM2定时 │ │          │ │          │
│ USART通信│ │          │ │          │
└──────────┘ └──────────┘ └──────────┘
```

### 3.3 中断系统

| 中断源 | 优先级 | 周期/触发 | 功能 |
|--------|--------|-----------|------|
| **TIM2** | 1（最高） | 100ms 定时 | 自动模式 1s 计数 + OLED 500ms 定时 |
| **USART1** | 2 | 收到字节 | 接收 PC 指令（0x01=查询） |
| **EXTI0** | 3 | PA0 上升沿 | KEY_AUTO：切手动 → 立即发送一次 |
| **EXTI1** | 3 | PD1 上升沿 | IN2：翻转 io_state bit1 |
| **EXTI2** | 3 | PD2 上升沿 | IN3：翻转 io_state bit2 |
| **EXTI3** | 3 | PD3 上升沿 | IN4：翻转 io_state bit3 |
| **EXTI4** | 3 | PE4 上升沿 | KEY_MAUUL：切自动模式 |
| **EXTI9_5** | 3 | 上升沿 | IN6/IN7/IN8/IN1/IN5（PD5/6/7/8/9） |
| **SysTick** | 0 | 1ms | HAL 时基 |

---

## 4. 通信协议

### 4.1 数据帧格式（MCU → PC）

```
┌──────┬──────┬───────┬──────┐
│ 0xA5 │ data │ ~data │ 0x5A │
│ 帧头 │ 数据 │ 取反  │ 帧尾 │
└──────┴──────┴───────┴──────┘
  4 字节，data 的 bit0~bit7 对应 IN1~IN8 状态
```

### 4.2 PC 指令（PC → MCU）

| 指令码 | 含义 | 响应 |
|--------|------|------|
| `0x01` | 查询当前 IO 状态 | MCU 立即返回数据帧 |

### 4.3 发送模式

| 模式 | 触发方式 | 行为 |
|------|----------|------|
| **手动**（MODE_MANUAL） | 按 KEY_AUTO（PA0） | 立即发送一次数据帧，停止自动发送 |
| **自动**（MODE_AUTO） | 按 KEY_MAUUL（PE4） | 每 1s 自动发送一次数据帧 |

---

## 5. 已知问题与调试记录

| # | 问题 | 原因 | 解决方案 |
|---|------|------|----------|
| 1 | 串口乱码（????噜?） | HSE 晶振：CubeMX 默认 25MHz，实际板载 8MHz | CubeMX 改为 HSE=8MHz, PLLM=8 |
| 2 | OLED 无显示 | 屏幕与 MCU **未共地** | 连接 OLED GND ↔ MCU GND |
| 3 | OLED WriteDat 卡死 | I2C 上拉不足 | 使能 PB10/PB11 内部上拉（或外接 4.7kΩ） |
| 4 | I2C Mem_Write 不可靠 | HAL 函数选择问题 | 改用 `HAL_I2C_Master_Transmit` |
| 5 | OLED_ShowCN 字体错乱 | `adder = 32 * N` 逻辑错误 | 改为 `adder = 0`（每个汉字仅 32 字节） |
| 6 | EXTI 引脚冲突 | PD0↔PA0（EXTI0）、PD4↔PE4（EXTI4） | 释放 PD0/PD4，IO 改用 PD1~PD9，保留 PA0/PE4 给模式键 |

---

## 6. 时钟配置

```
HSE (8MHz) → /PLLM(8) → ×PLLN(336) → /PLLP(2) → SYSCLK = 168MHz
                                          /PLLQ(4) → 42MHz

AHB  = 168MHz  (HCLK)
APB1 = 42MHz   (PCLK1, ÷4) → TIM2 Timer Clock = 84MHz (×2)
APB2 = 84MHz   (PCLK2, ÷2)

TIM2: PSC=839, ARR=9999 → 84MHz/840/10000 = 10Hz (100ms)
I2C2: 100kHz Standard Mode
USART1: 115200bps, 8N1
```

---

## 7. 开发环境

| 工具 | 版本/说明 |
|------|-----------|
| CubeMX | STM32CubeMX（HAL 代码生成） |
| IDE | Keil MDK-ARM V5.32 |
| 编译器 | ARMCC V5.06 |
| 串口助手 | 任意（波特率 115200, 8N1, HEX 显示） |
| 下载器 | ST-Link V2（SWD） |

---

## 8. 后续计划

- [ ] PCB 扩展板设计与打样
- [ ] 信号调理电路（光耦隔离 / 电平转换）
- [ ] 上位机 GUI（接收数据帧并可视化显示 8 路状态）
- [ ] 扩展 PC 指令集（修改采样周期、远程启停等）

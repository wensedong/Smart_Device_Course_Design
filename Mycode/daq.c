/**
 ******************************************************************************
 * @file    daq.c
 * @brief   数据采集与通信模块实现
 *          ┌──────────────────────────────────────────────┐
 *          │  架构: EXTI 改状态, TIM2 管发送, 主循环刷屏    │
 *          ├──────────────────────────────────────────────┤
 *          │  8x IO按钮 (PD1~PD9) → EXTI → 翻转 bit       │
 *          │  KEY_AUTO  (PA0)     → EXTI0 → 手动发送       │
 *          │  KEY_MAUUL (PE4)     → EXTI4 → AUTO/Pause切换 │
 *          │  TIM2 100ms          → 自动1s发 + OLED定时     │
 *          │  USART1              → PC指令 0x01查询        │
 *          │                                               │
 *          │  计数逻辑:                                     │
 *          │  - total_count: 永不重置, 任何发送都+1          │
 *          │  - mode_count:  切 MANUAL↔AUTO 清零, 同模式累加│
 *          └──────────────────────────────────────────────┘
 ******************************************************************************
 */

#include "daq.h"

/* ======================== 全局变量定义 ======================== */

volatile uint8_t  io_state     = 0;
volatile uint8_t  mode         = MODE_MANUAL;   /* 默认手动 */
volatile uint8_t  send_flag    = 0;
volatile uint8_t  oled_flag    = 0;
volatile uint32_t total_count  = 0;             /* 总计数 (永不重置) */
volatile uint32_t mode_count   = 0;             /* 当前模式内计数 */

static uint8_t  rx_byte;                        /* UART接收缓冲 */
static uint32_t last_tick[10];                  /* 消抖: 记录每个bit的上次触发时刻 */
static uint8_t  auto_tick;                      /* 自动模式: 100ms计数×10=1s */

/* ======================== Pin→Bit 映射 ======================== */

/**
 * @brief  INx 引脚 → io_state 位号
 * @retval 0~7, 0xFF=无效
 */
static uint8_t pin_to_bit(uint16_t pin)
{
    switch (pin) {
        case IN1_Pin: return 0;   /* PD8 → bit0 */
        case IN2_Pin: return 1;   /* PD1 → bit1 */
        case IN3_Pin: return 2;   /* PD2 → bit2 */
        case IN4_Pin: return 3;   /* PD3 → bit3 */
        case IN5_Pin: return 4;   /* PD9 → bit4 */
        case IN6_Pin: return 5;   /* PD5 → bit5 */
        case IN7_Pin: return 6;   /* PD6 → bit6 */
        case IN8_Pin: return 7;   /* PD7 → bit7 */
        default:      return 0xFF;
    }
}

/* ======================== 初始化 ======================== */

void DAQ_Init(void)
{
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    auto_tick = 0;
}

/* ======================== 数据帧发送 ======================== */

/**
 * @brief  发送8位二进制字符串到上位机 (ASCII文本格式)
 *         输出格式: "d7d6d5d4d3d2d1d0\r\n" (共10字节)
 *         例如:  "00110101\r\n" 表示 bit7=0,bit6=0,bit5=1,...
 */
void SendDataFrame(uint8_t data)
{
    char buf[12];
    uint8_t i;

    /* 逐位转换为 '0' / '1' 字符 (高位在前) */
    for (i = 0; i < 8; i++) {
        buf[i] = (data >> (7 - i)) & 1 ? '1' : '0';
    }
    buf[8]  = '\r';
    buf[9]  = '\n';
    buf[10] = '\0';

    HAL_UART_Transmit(&huart1, (uint8_t *)buf, 10, 100);
}

/* ======================== 指令解析 ======================== */

void ProcessCommand(uint8_t cmd)
{
    if (cmd == CMD_QUERY) {
        SendDataFrame(io_state);
    }
}

/* ======================== GPIO EXTI 回调 ======================== */

/**
 * @brief  所有GPIO EXTI的统一回调
 *         - IO按钮 (IN1~IN8): 消抖 → 翻转 io_state 对应位
 *         - KEY_AUTO  (PA0): 消抖 → 切手动 → 清零mode_count → 立即发送
 *         - KEY_MAUUL (PE4): 消抖 → 三态循环 MANUAL→AUTO→AUTO_PAUSE→AUTO→...
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t now = HAL_GetTick();
    uint8_t  bit;

    /* ---- 8路IO按钮: 翻转状态 ---- */
    bit = pin_to_bit(GPIO_Pin);
    if (bit < 8) {
        if (now - last_tick[bit] < DEBOUNCE_MS) return;
        last_tick[bit] = now;
        io_state ^= (1 << bit);         /* 翻转对应位 */
        return;
    }

    /* ---- KEY_AUTO (PA0): 手动发送 ---- */
    if (GPIO_Pin == KEY_AUTO_Pin) {
        if (now - last_tick[8] < DEBOUNCE_MS) return;
        last_tick[8] = now;

        /* 防抖附加: 确认引脚确实为高电平 (滤除释放回弹的毛刺) */
        if (HAL_GPIO_ReadPin(KEY_AUTO_GPIO_Port, KEY_AUTO_Pin) == GPIO_PIN_RESET)
            return;

        /* 仅在从其他模式切入时清零 mode_count, 已在MANUAL则保持累加 */
        if (mode != MODE_MANUAL) {
            mode       = MODE_MANUAL;
            mode_count = 0;
        }
        auto_tick  = 0;
        send_flag  = 1;                 /* 主循环: total_count++, mode_count++ */
        return;
    }

    /* ---- KEY_MAUUL (PE4): 三态切换 ---- */
    if (GPIO_Pin == KEY_MAUUL_Pin) {
        if (now - last_tick[9] < DEBOUNCE_MS) return;
        last_tick[9] = now;

        /* 防抖附加: 确认引脚确实为高电平 */
        if (HAL_GPIO_ReadPin(KEY_MAUUL_GPIO_Port, KEY_MAUUL_Pin) == GPIO_PIN_RESET)
            return;

        if (mode == MODE_MANUAL) {
            mode       = MODE_AUTO;     /* 手动 → 自动(运行) */
            mode_count = 0;             /* 切模式 → 清零 */
        } else if (mode == MODE_AUTO) {
            mode = MODE_AUTO_PAUSE;     /* 自动(运行) → 自动(暂停) */
            /* mode_count 保留, 暂停时不增长 */
        } else { /* MODE_AUTO_PAUSE */
            mode = MODE_AUTO;           /* 自动(暂停) → 自动(运行) */
        }
        auto_tick = 0;                  /* 重新对齐 1s 周期 */
        return;
    }
}

/* ======================== HAL中断回调 ======================== */

/**
 * @brief  TIM2定时中断回调 (每100ms触发)
 *         - MODE_AUTO:       每10次(1s)置 send_flag
 *         - MODE_AUTO_PAUSE: 不发送 (暂停)
 *         - MODE_MANUAL:     不发送 (由 PA0 触发)
 *         - 每5次(500ms)置 oled_flag
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim != &htim2) return;

    static uint8_t tick_oled = 0;

    /* 自动模式 (运行中): 1s发一次 */
    if (mode == MODE_AUTO) {
        if (++auto_tick >= 10) {
            auto_tick = 0;
            send_flag = 1;
        }
    }
    /* MODE_AUTO_PAUSE / MODE_MANUAL: 不自动发送 */

    /* OLED: 500ms刷新 */
    if (++tick_oled >= 5) {
        tick_oled = 0;
        oled_flag = 1;
    }
}

/**
 * @brief  USART1接收中断回调
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        ProcessCommand(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

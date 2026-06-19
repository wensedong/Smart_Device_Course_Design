/**
 ******************************************************************************
 * @file    daq.c
 * @brief   数据采集与通信模块实现
 *          ┌─────────────────────────────────────────┐
 *          │  架构: EXTI 改状态, TIM2 管发送, 主循环刷屏 │
 *          ├─────────────────────────────────────────┤
 *          │  8x IO按钮 (PD1~PD9) → EXTI → 翻转 bit   │
 *          │  KEY_AUTO  (PA0)     → EXTI0 → 手动发送   │
 *          │  KEY_MAUUL (PE4)     → EXTI4 → 切换自动   │
 *          │  TIM2 100ms          → 自动1s发 + OLED定时 │
 *          │  USART1              → PC指令 0x01查询     │
 *          └─────────────────────────────────────────┘
 ******************************************************************************
 */

#include "daq.h"

/* ======================== 全局变量定义 ======================== */

volatile uint8_t  io_state     = 0;
volatile uint8_t  mode         = MODE_MANUAL;   /* 默认手动 */
volatile uint8_t  send_flag    = 0;
volatile uint8_t  oled_flag    = 0;
volatile uint32_t sample_count = 0;

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
 * @brief  发送4字节数据帧到上位机
 *         帧格式: {0xA5, data, ~data, 0x5A}
 */
void SendDataFrame(uint8_t data)
{
    uint8_t frame[4] = {
        FRAME_HEADER,
        data,
        ~data,
        FRAME_FOOTER
    };
    HAL_UART_Transmit(&huart1, frame, 4, 100);
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
 *         - KEY_AUTO  (PA0): 消抖 → 切手动 → 立即发送一次
 *         - KEY_MAUUL (PE4): 消抖 → 切自动
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

    /* ---- KEY_AUTO (PA0): 手动发送一次 ---- */
    if (GPIO_Pin == KEY_AUTO_Pin) {
        if (now - last_tick[8] < DEBOUNCE_MS) return;
        last_tick[8] = now;
        mode      = MODE_MANUAL;
        auto_tick = 0;
        send_flag = 1;                  /* 通知主循环发送 */
        return;
    }

    /* ---- KEY_MAUUL (PE4): 切换自动模式 ---- */
    if (GPIO_Pin == KEY_MAUUL_Pin) {
        if (now - last_tick[9] < DEBOUNCE_MS) return;
        last_tick[9] = now;
        mode      = MODE_AUTO;
        auto_tick = 0;                  /* 清计数, 立即对齐 */
        return;
    }
}

/* ======================== HAL中断回调 ======================== */

/**
 * @brief  TIM2定时中断回调 (每100ms触发)
 *         - 自动模式: 每10次(1s)置 send_flag
 *         - 每5次(500ms)置 oled_flag
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim != &htim2) return;

    static uint8_t tick_oled = 0;

    /* 自动模式: 1s发一次 */
    if (mode == MODE_AUTO) {
        if (++auto_tick >= 10) {
            auto_tick = 0;
            send_flag = 1;
        }
    }

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

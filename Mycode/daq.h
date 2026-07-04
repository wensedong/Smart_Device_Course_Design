/**
 ******************************************************************************
 * @file    daq.h
 * @brief   数据采集与通信模块
 *          - 8路IO按钮 (PD1~PD9 EXTI) → 翻转 io_state 对应位
 *          - KEY_AUTO  (PA0 EXTI0) → 手动模式: 立即发送一次
 *          - KEY_MAUUL (PE4 EXTI4) → 自动模式: 1s周期发送
 *          - TIM2 100ms → 自动模式计数 + OLED刷新定时
 *          - USART1 → PC指令接收
 ******************************************************************************
 */

#ifndef __DAQ_H__
#define __DAQ_H__

#include "main.h"
#include "tim.h"
#include "usart.h"

/* ======================== 数据帧协议 ======================== */

#define FRAME_HEADER     0xA5    /* 帧头 (保留,未使用) */
#define FRAME_FOOTER     0x5A    /* 帧尾 (保留,未使用) */
#define CMD_QUERY        0x01    /* PC指令: 查询当前IO状态 */

/* 当前输出格式: ASCII二进制字符串 (串口助手文本模式直读)
 * 每帧10字节: "d7d6d5d4d3d2d1d0\r\n"
 * 例: io_state = 0x35 → 发送 "00110101\r\n"
 */

/* ======================== 工作模式 ======================== */

#define MODE_MANUAL      0
#define MODE_AUTO        1

/* ======================== 消抖 ======================== */

#define DEBOUNCE_MS      50      /* EXTI消抖时间 */

/* ======================== 全局状态变量 ======================== */

extern volatile uint8_t  io_state;       /* 8路IO状态 (bit0~bit7) */
extern volatile uint8_t  mode;           /* 当前工作模式 */
extern volatile uint8_t  send_flag;      /* 主循环发送标志 (1=需要发送) */
extern volatile uint8_t  oled_flag;      /* 主循环OLED刷新标志 */
extern volatile uint32_t sample_count;   /* 累计发送次数 */

/* ======================== 函数接口 ======================== */

void DAQ_Init(void);                          /* 启动定时器和串口接收 */
void SendDataFrame(uint8_t data);             /* 发送4字节数据帧到PC */
void ProcessCommand(uint8_t cmd);             /* 解析PC下发指令 */

/* HAL中断回调 (覆盖__weak默认实现) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#endif /* __DAQ_H__ */

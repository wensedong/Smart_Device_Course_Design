/**
 ******************************************************************************
 * @file    daq.h
 * @brief   数据采集与通信模块
 *          - 定时读取8路IO状态
 *          - 串口数据帧发送
 *          - PC指令解析
 ******************************************************************************
 */

#ifndef __DAQ_H__
#define __DAQ_H__

#include "main.h"
#include "tim.h"
#include "usart.h"

/* ======================== 数据帧协议 ======================== */

#define FRAME_HEADER     0xA5    /* 帧头 */
#define FRAME_FOOTER     0x5A    /* 帧尾 */
#define CMD_QUERY        0x01    /* PC指令: 查询当前IO状态 */

/* ======================== 全局状态变量 ======================== */

extern volatile uint8_t  io_status;      /* 8路IO状态 (bit0=IN0 ... bit7=IN7) */
extern volatile uint8_t  data_ready;     /* 新数据就绪标志 */
extern volatile uint32_t sample_count;   /* 累计采样次数 */

/* ======================== 函数接口 ======================== */

void DAQ_Init(void);                          /* 启动定时器和串口接收 */
void SendDataFrame(uint8_t data);             /* 发送4字节数据帧到PC */
void ProcessCommand(uint8_t cmd);             /* 解析PC下发指令 */

/* HAL中断回调 (覆盖__weak默认实现) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif /* __DAQ_H__ */

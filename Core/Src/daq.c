/**
 ******************************************************************************
 * @file    daq.c
 * @brief   数据采集与通信模块实现
 *          - TIM2 每100ms中断 → 读取PD0~PD7 → 置 data_ready
 *          - main() 检测 data_ready → 调用 SendDataFrame()
 *          - USART1 中断 → 接收PC指令 → ProcessCommand()
 ******************************************************************************
 */

#include "daq.h"

/* ======================== 全局变量定义 ======================== */

volatile uint8_t  io_status    = 0;
volatile uint8_t  data_ready   = 0;
volatile uint32_t sample_count = 0;

static uint8_t rx_byte;  /* UART接收缓冲 (模块内部使用) */

/* ======================== 初始化 ======================== */

/**
 * @brief  启动数据采集
 *         - 启动TIM2定时中断 (100ms周期)
 *         - 启动USART1接收中断
 * @note   在 main() 中 MX_xxx_Init() 之后调用
 */
void DAQ_Init(void)
{
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

/* ======================== 数据帧发送 ======================== */

/**
 * @brief  发送4字节数据帧到上位机
 *         帧格式: {0xA5, data, ~data, 0x5A}
 * @param  data : 8路IO状态 (bit0=IN0 ... bit7=IN7)
 */
void SendDataFrame(uint8_t data)
{
    uint8_t frame[4] = {
        FRAME_HEADER,
        data,
        ~data,          /* 校验字节: 数据取反 */
        FRAME_FOOTER
    };
    HAL_UART_Transmit(&huart1, frame, 4, 100);
}

/* ======================== 指令解析 ======================== */

/**
 * @brief  解析PC下发指令 (单字节命令)
 * @param  cmd : 指令码
 *          0x01 = 立即返回当前IO状态帧
 *          (可扩展: 0x02=设置采样周期, 0x03=启停采集...)
 */
void ProcessCommand(uint8_t cmd)
{
    switch (cmd) {
        case CMD_QUERY:
            SendDataFrame(io_status);
            break;
        default:
            break;
    }
}

/* ======================== HAL中断回调 ======================== */

/**
 * @brief  TIM2定时中断回调 (每100ms触发)
 * @note   优先级: 1, 高于USART1
 *         读取GPIOD->IDR低8位 → 存入 io_status → 置 data_ready
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim2) {
        io_status = (uint8_t)(GPIOD->IDR & 0x00FF);  /* PD0~PD7 */
        data_ready = 1;
        sample_count++;
    }
}

/**
 * @brief  USART1接收中断回调
 * @note   优先级: 2, 低于TIM2
 *         接收1字节 → ProcessCommand() → 重新使能接收
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        ProcessCommand(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  /* 继续接收 */
    }
}

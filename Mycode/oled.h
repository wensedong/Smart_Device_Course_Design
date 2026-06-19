/**
 ******************************************************************************
 * @file    oled.h
 * @brief   SSD1306 0.96寸 OLED 驱动 (基于旧驱动迁移, 改为硬件I2C2)
 * @note    迁移自: 董文泽微机课设/BSP/OLED_I2C.h
 *          改动: 软件I2C -> HAL硬件I2C2 (PB10=SCL, PB11=SDA)
 ******************************************************************************
 */

#ifndef __OLED_H__
#define __OLED_H__

#include "main.h"
#include "i2c.h"

/* ======================== OLED I2C 地址 ======================== */
#define OLED_ADDRESS    0x78    /* SSD1306 8-bit I2C地址 */

/* ======================== 基础函数 ======================== */
void OLED_Init(void);
void OLED_SetPos(unsigned char x, unsigned char y);
void OLED_Fill(unsigned char fill_Data);
void OLED_CLS(void);
void OLED_ON(void);
void OLED_OFF(void);

/* ======================== 字符串显示 ======================== */
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize);
void OLED_ShowStr_S(unsigned char x, unsigned char y, unsigned char ch[], char POS, unsigned char len, unsigned char TextSize);

/* ======================== 中文显示 (codetab.h) ======================== */
void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char N);
void OLED_ShowCC(unsigned char x, unsigned char y, unsigned char *s);
void OLED_ShowCC_S(unsigned char x, unsigned char y, unsigned char *s, char POS, unsigned char len);

/* ======================== 位图显示 ======================== */
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char BMP[]);

/* ======================== 帧缓冲模式 (局部刷新/滚动) ======================== */
extern uint8_t OLED_GRAM[128][8];
void OLED_Refresh_Gram(void);
void OLED_Clear_Gram(void);
void OLED_Set_Pos(uint8_t x, uint8_t y);
void OLED_DrawPoint_Gram(int16_t x, int16_t y, uint8_t t);
void OLED_ShowCC_Scroll(int16_t x, int16_t y, unsigned char *s);

#endif /* __OLED_H__ */

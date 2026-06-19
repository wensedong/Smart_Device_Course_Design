/**
 ******************************************************************************
 * @file    oled.c
 * @brief   SSD1306 OLED 驱动 — 硬件I2C2版本
 * @note    迁移自: 董文泽微机课设/BSP/OLED_I2C.c (原为软件I2C bit-banging)
 *          改动: 底层I2C改为HAL硬件I2C2, 上层显示函数保持不变
 *          I2C2: PB10=SCL, PB11=SDA, 地址0x78
 ******************************************************************************
 */

#include "oled.h"
#include "codetab.h"

/* ======================== 全局帧缓冲 ======================== */
uint8_t OLED_GRAM[128][8];  /* 128列×8页 (列优先) */

/* ======================== 延时 ======================== */
static void DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ======================== 底层 HAL I2C 通信 ======================== */

/**
 * @brief  向SSD1306发送命令字节
 * @note   使用 HAL_I2C_Master_Transmit (比 Mem_Write 更可靠)
 *         发送序列: {0x00, cmd} → 控制字节0x00表示命令
 */
static void WriteCmd(unsigned char cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c2, OLED_ADDRESS, buf, 2, 100);
}

/**
 * @brief  向SSD1306发送数据字节
 * @note   发送序列: {0x40, data} → 控制字节0x40表示数据
 */
static void WriteDat(unsigned char data)
{
    uint8_t buf[2] = {0x40, data};
    HAL_I2C_Master_Transmit(&hi2c2, OLED_ADDRESS, buf, 2, 100);
}

/* ======================== 初始化 ======================== */

/**
 * @brief  SSD1306 初始化序列
 * @note   原调用 IIC_GPIO_Config() 初始化软件I2C引脚,
 *         现由 CubeMX MX_I2C2_Init() 完成,故删除
 */
void OLED_Init(void)
{
    DelayMs(100);           /* 等待OLED上电稳定 */

    WriteCmd(0xAE);         /* 关闭显示 */

    WriteCmd(0x20);         /* 设置内存寻址模式 */
    WriteCmd(0x10);         /* 页寻址模式 (Page Addressing Mode) */

    WriteCmd(0xB0);         /* 页起始地址 = 0 */

    WriteCmd(0xC8);         /* COM扫描方向: 从下往上 */

    WriteCmd(0x00);         /* 列低4位 = 0 */
    WriteCmd(0x10);         /* 列高4位 = 0 */

    WriteCmd(0x40);         /* 显示起始行 = 0 */

    WriteCmd(0x81);         /* 对比度控制 */
    WriteCmd(0xFF);         /* 对比度 = 最大 */

    WriteCmd(0xA1);         /* 列重映射 (左右镜像) */

    WriteCmd(0xA6);         /* 正常显示 (非反白) */

    WriteCmd(0xA8);         /* 多路复用比 */
    WriteCmd(0x3F);         /* 1/64 duty */

    WriteCmd(0xA4);         /* 输出跟随RAM内容 */

    WriteCmd(0xD3);         /* 显示偏移 */
    WriteCmd(0x00);         /* 无偏移 */

    WriteCmd(0xD5);         /* 时钟分频/振荡频率 */
    WriteCmd(0xF0);

    WriteCmd(0xD9);         /* 预充电周期 */
    WriteCmd(0x22);

    WriteCmd(0xDA);         /* COM引脚配置 */
    WriteCmd(0x12);

    WriteCmd(0xDB);         /* VCOMH电压 */
    WriteCmd(0x20);         /* 0.77×VCC */

    WriteCmd(0x8D);         /* 启用电荷泵 */
    WriteCmd(0x14);

    WriteCmd(0xAF);         /* 打开显示 */
}

/* ======================== 光标定位 ======================== */

/**
 * @brief  设置OLED显示位置 (页寻址模式)
 * @param  x : 列地址 (0~127)
 * @param  y : 页地址 (0~7)
 */
void OLED_SetPos(unsigned char x, unsigned char y)
{
    WriteCmd(0xB0 + y);
    WriteCmd(((x & 0xF0) >> 4) | 0x10);
    WriteCmd((x & 0x0F) | 0x01);
}

/* ======================== 全屏操作 ======================== */

/**
 * @brief  全屏填充 (直接写入, 不使用帧缓冲)
 */
void OLED_Fill(unsigned char fill_Data)
{
    unsigned char m, n;
    for (m = 0; m < 8; m++)
    {
        WriteCmd(0xB0 + m);
        WriteCmd(0x00);
        WriteCmd(0x10);
        for (n = 0; n < 128; n++)
        {
            WriteDat(fill_Data);
        }
    }
}

/**
 * @brief  清屏 (全黑)
 */
void OLED_CLS(void)
{
    OLED_Fill(0x00);
}

/* ======================== 电源控制 ======================== */

void OLED_ON(void)
{
    WriteCmd(0x8D);     /* 电荷泵 */
    WriteCmd(0x14);     /* 开启 */
    WriteCmd(0xAF);     /* 显示开 */
}

void OLED_OFF(void)
{
    WriteCmd(0x8D);     /* 电荷泵 */
    WriteCmd(0x10);     /* 关闭 */
    WriteCmd(0xAE);     /* 显示关 */
}

/* ======================== 字符串显示 (6x8 / 8x16) ======================== */

/**
 * @brief  在指定位置显示字符串
 * @param  x : 起始列 (0~127)
 * @param  y : 起始页 (0~7)
 * @param  ch[] : 字符串
 * @param  TextSize : 1=6x8字体, 2=8x16字体
 */
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize)
{
    unsigned char c = 0, i = 0, j = 0;

    switch (TextSize)
    {
        case 1:     /* 6×8 字体 */
        {
            while (ch[j] != '\0')
            {
                c = ch[j] - 32;
                if (x > 126)
                {
                    x = 0;
                    y++;
                }
                OLED_SetPos(x, y);
                for (i = 0; i < 6; i++)
                    WriteDat(F6x8[c][i]);
                x += 6;
                j++;
            }
        } break;

        case 2:     /* 8×16 字体 */
        {
            while (ch[j] != '\0')
            {
                c = ch[j] - 32;
                if (x > 120)
                {
                    x = 0;
                    y++;
                }
                OLED_SetPos(x, y);
                for (i = 0; i < 8; i++)
                    WriteDat(F8X16[c * 16 + i]);
                OLED_SetPos(x, y + 1);
                for (i = 0; i < 8; i++)
                    WriteDat(F8X16[c * 16 + i + 8]);
                x += 8;
                j++;
            }
        } break;
    }
}

/* ======================== 字符串显示(带反白选中) ======================== */

/**
 * @brief  显示字符串并反白指定区域 (用于菜单选中效果)
 * @param  POS : 反白起始字符位置 (1-based)
 * @param  len : 反白长度
 */
void OLED_ShowStr_S(unsigned char x, unsigned char y, unsigned char ch[],
                    char POS, unsigned char len, unsigned char TextSize)
{
    unsigned char c = 0, i = 0, j = 0;

    POS = POS - 1;
    switch (TextSize)
    {
        case 1:
        {
            while (ch[j] != '\0')
            {
                c = ch[j] - 32;
                if (x > 126) { x = 0; y++; }
                OLED_SetPos(x, y);
                for (i = 0; i < 6; i++)
                {
                    if ((j >= POS) & (j < (POS + len)))
                        WriteDat(~F6x8[c][i]);     /* 反白 */
                    else
                        WriteDat(F6x8[c][i]);
                }
                x += 6;
                j++;
            }
        } break;

        case 2:
        {
            while (ch[j] != '\0')
            {
                c = ch[j] - 32;
                if (x > 120) { x = 0; y++; }
                OLED_SetPos(x, y);
                for (i = 0; i < 8; i++)
                {
                    if ((j > POS - 1) & (j < (POS + len)))
                        WriteDat(~F8X16[c * 16 + i]);
                    else
                        WriteDat(F8X16[c * 16 + i]);
                }
                OLED_SetPos(x, y + 1);
                for (i = 0; i < 8; i++)
                {
                    if ((j >= POS) & (j < (POS + len)))
                        WriteDat(~F8X16[c * 16 + i + 8]);
                    else
                        WriteDat(F8X16[c * 16 + i + 8]);
                }
                x += 8;
                j++;
            }
        } break;
    }
}

/* ======================== 中文显示 ======================== */

/**
 * @brief  显示单个16×16中文汉字 (按索引)
 * @param  N : tfont16[]数组中的索引
 */
void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char N)
{
    unsigned char wm = 0;
    unsigned int  adder = 0;          /* 从0开始, Msk[]共32字节 */

    OLED_SetPos(x, y);
    for (wm = 0; wm < 16; wm++)
    {
        WriteDat(tfont16[N].Msk[adder]);
        adder += 1;
    }
    OLED_SetPos(x, y + 1);
    for (wm = 0; wm < 16; wm++)
    {
        WriteDat(tfont16[N].Msk[adder]);
        adder += 1;
    }
}

/**
 * @brief  显示混合字符串(ASCII+中文), 自动识别
 * @note   ASCII字符用8×16字体显示, 中文用16×16
 *         传入字符串中 >0x80 的字节当作中文首字节处理
 */
void OLED_ShowCC(unsigned char x, unsigned char y, unsigned char *s)
{
    uint16_t HZnum;
    uint16_t k, i, c;
    uint8_t wm = 0;
    uint8_t adder = 0;

    while (*s != 0)
    {
        if (*s <= 0x80)     /* ASCII */
        {
            c = *s - 32;
            OLED_SetPos(x, y);
            for (i = 0; i < 8; i++)
                WriteDat(F8X16[c * 16 + i]);
            OLED_SetPos(x, y + 1);
            for (i = 0; i < 8; i++)
                WriteDat(F8X16[c * 16 + i + 8]);
            x += 8;
            s += 1;
        }
        else                /* 中文 */
        {
            HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
            for (k = 0; k < HZnum; k++)
            {
                adder = 0;
                if ((tfont16[k].Index[0] == *(s)) &&
                    (tfont16[k].Index[1] == *(s + 1)))
                {
                    OLED_SetPos(x, y);
                    for (wm = 0; wm < 16; wm++)
                    {
                        WriteDat(tfont16[k].Msk[adder]);
                        adder += 1;
                    }
                    OLED_SetPos(x, y + 1);
                    for (wm = 0; wm < 16; wm++)
                    {
                        WriteDat(tfont16[k].Msk[adder]);
                        adder += 1;
                    }
                    break;
                }
            }
            s += 2;
            x += 16;
        }
    }
}

/* ======================== 混合字符串(带反白选中) ======================== */

void OLED_ShowCC_S(unsigned char x, unsigned char y, unsigned char *s,
                   char POS, unsigned char len)
{
    uint16_t HZnum;
    uint16_t k, i, c;
    uint8_t wm = 0, j = 0;
    uint8_t adder = 0;

    POS = POS - 1;
    if ((int)POS < 0) POS = 0;

    while (*s != 0)
    {
        if (*s <= 0x80)     /* ASCII */
        {
            c = *s - 32;
            if (x > 120) { x = 0; y++; }
            OLED_SetPos(x, y);
            for (i = 0; i < 8; i++)
            {
                if ((j > POS - 1) & (j < (POS + len)))
                    WriteDat(~F8X16[c * 16 + i]);
                else
                    WriteDat(F8X16[c * 16 + i]);
            }
            OLED_SetPos(x, y + 1);
            for (i = 0; i < 8; i++)
            {
                if ((j >= POS) & (j < (POS + len)))
                    WriteDat(~F8X16[c * 16 + i + 8]);
                else
                    WriteDat(F8X16[c * 16 + i + 8]);
            }
            x += 8;
            s += 1;
            j += 1;
        }
        else                /* 中文 */
        {
            HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
            for (k = 0; k < HZnum; k++)
            {
                adder = 0;
                if ((tfont16[k].Index[0] == *(s)) &&
                    (tfont16[k].Index[1] == *(s + 1)))
                {
                    OLED_SetPos(x, y);
                    for (wm = 0; wm < 16; wm++)
                    {
                        if ((j > POS - 1) & (j < (POS + len)))
                            WriteDat(~tfont16[k].Msk[adder]);
                        else
                            WriteDat(tfont16[k].Msk[adder]);
                        adder += 1;
                    }
                    OLED_SetPos(x, y + 1);
                    for (wm = 0; wm < 16; wm++)
                    {
                        if ((j > POS - 1) & (j < (POS + len)))
                            WriteDat(~tfont16[k].Msk[adder]);
                        else
                            WriteDat(tfont16[k].Msk[adder]);
                        adder += 1;
                    }
                    break;
                }
            }
            s += 2;
            j += 2;
            x += 16;
        }
    }
}

/* ======================== 位图显示 ======================== */

void OLED_DrawBMP(unsigned char x0, unsigned char y0,
                  unsigned char x1, unsigned char y1,
                  unsigned char BMP[])
{
    unsigned int j = 0;
    unsigned char x, y;

    if (y1 % 8 == 0)
        y = y1 / 8;
    else
        y = y1 / 8 + 1;

    for (y = y0; y < y1; y++)
    {
        OLED_SetPos(x0, y);
        for (x = x0; x < x1; x++)
        {
            WriteDat(BMP[j++]);
        }
    }
}

/* ======================== 帧缓冲操作 ======================== */

/**
 * @brief  备用定位函数 (带列偏移+2)
 */
void OLED_Set_Pos(uint8_t x, uint8_t y)
{
    WriteCmd(0xB0 + y);
    WriteCmd((((x + 2) & 0xF0) >> 4) | 0x10);
    WriteCmd(((x + 2) & 0x0F));
}

/**
 * @brief  清空帧缓冲
 */
void OLED_Clear_Gram(void)
{
    uint8_t i, n;
    for (i = 0; i < 8; i++)
    {
        for (n = 0; n < 128; n++)
        {
            OLED_GRAM[n][i] = 0;
        }
    }
}

/**
 * @brief  将帧缓冲刷新到OLED屏幕
 * @note   优化: 使用临时页缓冲 + HAL_I2C_Mem_Write 批量发送,
 *          替代原逐字节 Write_IIC_Byte, 大幅减少I2C事务开销
 */
void OLED_Refresh_Gram(void)
{
    uint8_t i, n;
    uint8_t page_buf[129];  /* 控制字节(1) + 页数据(128) = 129字节 */

    page_buf[0] = 0x40;     /* 控制字节: 数据模式 */

    for (i = 0; i < 8; i++)
    {
        WriteCmd(0xB0 + i);     /* 设置页地址 */
        WriteCmd(0x00);         /* 列低4位 = 0 */
        WriteCmd(0x10);         /* 列高4位 = 0 */

        /* 从列优先帧缓冲中提取第i页的128字节 */
        for (n = 0; n < 128; n++)
        {
            page_buf[n + 1] = OLED_GRAM[n][i];
        }

        /* 一次I2C事务发送: 控制字节 + 整页128字节 = 129字节 */
        HAL_I2C_Master_Transmit(&hi2c2, OLED_ADDRESS, page_buf, 129, 500);
    }
}

/**
 * @brief  在帧缓冲中画点
 * @param  t : 1=点亮, 0=熄灭
 */
void OLED_DrawPoint_Gram(int16_t x, int16_t y, uint8_t t)
{
    if (x > 127 || x < 0 || y > 63 || y < 0)
        return;  /* 越界, 不处理 */

    if (t)
        OLED_GRAM[x][y / 8] |= (1 << (y % 8));
    else
        OLED_GRAM[x][y / 8] &= ~(1 << (y % 8));
}

/**
 * @brief  使用帧缓冲方式显示中文混合字符串 (支持滚动)
 * @param  x, y : 可以为负数, 实现屏幕外偏移/滚动效果
 */
void OLED_ShowCC_Scroll(int16_t x, int16_t y, unsigned char *s)
{
    uint16_t HZnum;
    uint16_t k, wm, adder;

    while (*s != 0)
    {
        if (*s > 0x80)      /* 中文 */
        {
            HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
            for (k = 0; k < HZnum; k++)
            {
                if ((tfont16[k].Index[0] == *(s)) &&
                    (tfont16[k].Index[1] == *(s + 1)))
                {
                    /* 上半页 (Page y) */
                    for (wm = 0; wm < 16; wm++)
                    {
                        adder = wm;
                        if (x + wm >= 0 && x + wm <= 127)
                        {
                            OLED_GRAM[x + wm][y] = tfont16[k].Msk[adder];
                        }
                    }
                    /* 下半页 (Page y+1) */
                    for (wm = 0; wm < 16; wm++)
                    {
                        adder = wm + 16;
                        if (x + wm >= 0 && x + wm <= 127)
                        {
                            OLED_GRAM[x + wm][y + 1] = tfont16[k].Msk[adder];
                        }
                    }
                    break;
                }
            }
            s += 2;
            x += 16;
        }
        else                /* ASCII (简化处理) */
        {
            s++;
            x += 8;
        }
    }
}

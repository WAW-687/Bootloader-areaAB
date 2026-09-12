#include "stm32f10x.h"
#include "Delay.h"

/**
  * @brief  按键初始化
  *         PB13 = 确认/切换
  *         PB0  = 减少
  *         PA6  = 增加
  * @note   全部上拉输入，按下为低电平
  */
void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* PB0, PB13 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_13;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PA6 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  获取按键值
  * @retval 1 = PB13 (确认/切换)
  *         2 = PB0  (减少)
  *         3 = PA6  (增加)
  *         0 = 无按键按下
  */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;

    /* PB13 — 确认/切换 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0);
        Delay_ms(20);
        KeyNum = 1;
    }

    /* PB0 — 减少 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0);
        Delay_ms(20);
        KeyNum = 2;
    }

    /* PA6 — 增加 */
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == 0);
        Delay_ms(20);
        KeyNum = 3;
    }

    return KeyNum;
}

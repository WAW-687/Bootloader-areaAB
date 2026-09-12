#include "stm32f10x.h"
#include "LightSensor.h"
#include "AD.h"

/**
  * @brief  光敏传感器初始化（使用 ADC1 通道 0，PA0）
  * @note   光敏电阻 + 分压电路接入 PA0，通过 ADC 采集模拟电压
  */
void LightSensor_Init(void)
{
    AD_Init();  // 复用 AD.c 的 ADC1 初始化
}

/**
  * @brief  获取 ADC 原始值
  * @retval 0~4095（对应 0~3.3V）
  */
uint16_t LightSensor_Get(void)
{
    return AD_GetValue();
}

/**
  * @brief  获取光照强度百分比
  * @retval 0~100（0 = 最暗，100 = 最亮）
  * @note   暗处 ADC 值高（光敏电阻暗阻大，分压高）
  *         亮处 ADC 值低（光敏电阻亮阻小，分压低）
  *         公式：光照度(%) = (4095 - raw) * 100 / 4095
  */
uint8_t LightSensor_GetPercent(void)
{
    uint16_t raw = AD_GetValue();
    uint32_t percent = (4095UL - raw) * 100UL / 4095UL;
    return (uint8_t)percent;
}

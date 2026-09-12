#ifndef __LIGHT_SENSOR_H
#define __LIGHT_SENSOR_H

#include "stm32f10x.h"

void LightSensor_Init(void);
uint16_t LightSensor_Get(void);         // 返回 ADC 原始值 0~4095
uint8_t  LightSensor_GetPercent(void);  // 返回光照强度百分比 0~100

#endif

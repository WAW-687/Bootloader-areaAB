#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

/* DHT11 数据引脚定义 (PB11) */
#define DHT11_GPIO_PORT		GPIOB
#define DHT11_GPIO_PIN		GPIO_Pin_11
#define DHT11_GPIO_CLK		RCC_APB2Periph_GPIOB

/* 引脚操作宏 */
#define DHT11_OUT_H()		GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN)
#define DHT11_OUT_L()		GPIO_ResetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN)
#define DHT11_IN_READ()		GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN)

/* DHT11 返回状态 */
#define DHT11_OK			0
#define DHT11_TIMEOUT		1
#define DHT11_CHECKSUM_ERR	2

/* 函数声明 */
void DHT11_Init(void);
uint8_t DHT11_ReadData(uint8_t *humidity, uint8_t *temperature);

#endif

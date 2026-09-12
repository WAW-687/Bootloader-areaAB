#include "DHT11.h"
#include "Delay.h"

/**
  * @brief  将 DHT11 数据引脚设为推挽输出模式
  */
static void DHT11_SetOutput(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

/**
  * @brief  将 DHT11 数据引脚设为上拉输入模式
  */
static void DHT11_SetInput(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

/**
  * @brief  DHT11 初始化
  * @note   初始化后引脚处于高阻上拉输入状态
  */
void DHT11_Init(void)
{
	RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);
	DHT11_SetInput();
	DHT11_OUT_H();		// 拉高，确保总线空闲
}

/**
  * @brief  等待引脚电平变化，超时返回
  * @param  expected 期望的电平（0=低，1=高）
  * @param  timeout_us 超时时间（微秒）
  * @retval 0=成功等待到期望电平, 1=超时
  */
static uint8_t DHT11_WaitLevel(uint8_t expected, uint32_t timeout_us)
{
	while (DHT11_IN_READ() != expected)
	{
		if (timeout_us == 0)
			return DHT11_TIMEOUT;
		Delay_us(1);
		timeout_us--;
	}
	return DHT11_OK;
}

/**
  * @brief  读取一个数据位
  * @retval 0 或 1
  */
static uint8_t DHT11_ReadBit(void)
{
	/* 等待 DHT11 拉低（每 bit 起始标志，约 50μs） */
	while (DHT11_IN_READ() == 1);
	/* 等待 DHT11 拉高 */
	while (DHT11_IN_READ() == 0);
	/* 延时 40μs 后采样：
	 * 若仍为高 → bit 1（高电平宽度约 70μs）
	 * 若变为低 → bit 0（高电平宽度约 26~28μs）
	 */
	Delay_us(40);
	if (DHT11_IN_READ() == 1)
		return 1;
	else
		return 0;
}

/**
  * @brief  读取一个字节（8 位，MSB 在先）
  */
static uint8_t DHT11_ReadByte(void)
{
	uint8_t data = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		data <<= 1;
		data |= DHT11_ReadBit();
	}
	return data;
}

/**
  * @brief  读取 DHT11 温湿度数据
  * @param  humidity    湿度值指针（单位：%RH，整数部分）
  * @param  temperature 温度值指针（单位：℃）
  * @retval DHT11_OK            成功
  * @retval DHT11_TIMEOUT       通信超时（传感器无响应）
  * @retval DHT11_CHECKSUM_ERR  校验和错误
  */
uint8_t DHT11_ReadData(uint8_t *humidity, uint8_t *temperature)
{
	uint8_t buf[5];
	uint8_t ret;

	/* 1. MCU 发送起始信号 */
	DHT11_SetOutput();
	DHT11_OUT_L();					// 拉低总线
	Delay_ms(20);					// 保持低电平 ≥18ms
	DHT11_OUT_H();					// 拉高总线
	Delay_us(30);					// 保持高电平 20~40μs

	/* 2. 切换为输入，等待 DHT11 响应 */
	DHT11_SetInput();
	Delay_us(10);					// 稍作延时让 DHT11 准备

	/* 等待 DHT11 拉低总线（响应起始标志） */
	ret = DHT11_WaitLevel(0, 100);
	if (ret != DHT11_OK) return ret;

	/* 等待 DHT11 拉高总线 */
	ret = DHT11_WaitLevel(1, 100);
	if (ret != DHT11_OK) return ret;

	/* 等待 DHT11 再次拉低（准备发送数据） */
	ret = DHT11_WaitLevel(0, 100);
	if (ret != DHT11_OK) return ret;

	/* 3. 读取 40 位数据（5 字节） */
	for (uint8_t i = 0; i < 5; i++)
	{
		buf[i] = DHT11_ReadByte();
	}

	/* 4. 等待 DHT11 释放总线 */
	DHT11_WaitLevel(1, 100);

	/* 5. 校验和验证 */
	if ((buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
	{
		return DHT11_CHECKSUM_ERR;
	}

	/* 6. 输出结果 */
	*humidity    = buf[0];		// 湿度整数部分
	*temperature = buf[2];		// 温度整数部分

	return DHT11_OK;
}

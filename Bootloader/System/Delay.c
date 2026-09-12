#include "stm32f10x.h"

/**
  * 微秒级延时（江协结构 + GD32位操作 结合版）
  * 说明：
  *   1. 自包含，无需先调用任何 Init 函数
  *   2. 用 |= / &=~ 只操作 ENABLE 位，不破坏 CTRL 其他位（如 TICKINT）
  *   3. |= CLKSOURCE 保证每次调用时钟源都是 HCLK（72MHz）
  *   4. 系数 72 = 系统主频(MHz)，若主频改变需同步修改
  * @param  us 延时时长，范围：0~232700（24位计数器上限 16777215/72）
  */
void Delay_us(uint32_t us)
{
	SysTick->LOAD = 72 * us;								//设置定时器重装值
	SysTick->VAL = 0x00;									//清空当前计数值
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;	//时钟源=HCLK + 启动
	while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));	//等待计数到0（COUNTFLAG=bit16）
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;				//关闭定时器（只清ENABLE位）
}

/**
  * @brief  毫秒级延时（内部循环调用微秒级，规避24位计数器上限）
  * @param  ms 延时时长，范围：0~4294967295
  */
void Delay_ms(uint32_t ms)
{
	while(ms--)
	{
		Delay_us(1000);
	}
}

/**
  * @brief  秒级延时
  * @param  s 延时时长，范围：0~4294967295
  */
void Delay_s(uint32_t s)
{
	while(s--)
	{
		Delay_ms(1000);
	}
}

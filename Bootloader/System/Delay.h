#ifndef __DELAY_H
#define __DELAY_H

/* ===================== SysTick 阻塞延时（自包含，无需初始化） ===================== */
void Delay_us(uint32_t us);     /* 微秒延时：0~232700（SysTick 24位计数器上限决定） */
void Delay_ms(uint32_t ms);     /* 毫秒延时：内部循环调用 Delay_us(1000) */
void Delay_s(uint32_t s);       /* 秒级延时：内部循环调用 Delay_ms(1000) */

#endif

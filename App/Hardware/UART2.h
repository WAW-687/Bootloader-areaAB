#ifndef __UART2_H
#define __UART2_H

#include "stm32f10x.h"
#include <string.h>

/* ESP-01S 接收缓冲区大小 */
#define ESP_RX_BUF_SIZE     256

void     ESP_Init(void);                    // 初始化 USART2 (115200, 中断接收) 与 ESP-01S 通信
void     ESP_SendByte(uint8_t Byte);
void     ESP_SendString(char *String);
uint8_t  ESP_SendCmd(char *Cmd, uint16_t TimeoutMs);
void     ESP_ClearRxBuf(void);
uint8_t  ESP_GetRxBuf(char *Buf, uint16_t MaxLen);
uint8_t  ESP_ReadByte(uint8_t *data);               // 取一个原始字节(非阻塞)，OTA 下载用
uint8_t  ESP_WaitResp(char *Expect, uint16_t TimeoutMs);

#endif

#include "stm32f10x.h"
#include "UART2.h"
#include "Delay.h"

/* ESP-01S 环形接收缓冲区（USART2） */
static volatile uint8_t  ESP_RxBuf[ESP_RX_BUF_SIZE];
static volatile uint16_t ESP_RxHead = 0;
static volatile uint16_t ESP_RxTail = 0;
static volatile uint8_t  ESP_RxNewData = 0;

/**
  * @brief  USART2 初始化 (PA2=TX, PA3=RX, 115200bps, 中断接收)
  * @note   用于与 ESP-01S 通信
  */
void ESP_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART2, &USART_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);

    ESP_RxHead = 0;
    ESP_RxTail = 0;
    ESP_RxNewData = 0;
}

void ESP_SendByte(uint8_t Byte)
{
    USART_SendData(USART2, Byte);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

void ESP_SendString(char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        ESP_SendByte(String[i]);
    }
}

void ESP_ClearRxBuf(void)
{
    ESP_RxHead = 0;
    ESP_RxTail = 0;
    ESP_RxNewData = 0;
}

uint8_t ESP_GetRxBuf(char *Buf, uint16_t MaxLen)
{
    uint16_t i = 0;
    while (ESP_RxTail != ESP_RxHead && i < MaxLen - 1)
    {
        Buf[i] = (char)ESP_RxBuf[ESP_RxTail];
        ESP_RxTail = (ESP_RxTail + 1) % ESP_RX_BUF_SIZE;
        i++;
    }
    Buf[i] = '\0';
    return i;
}

/**
  * @brief  从环形缓冲取一个原始字节（非阻塞）
  * @param  out 取出的字节
  * @retval 1=取到  0=缓冲区空
  * @note   OTA 下载固件专用：115200 波特率下每毫秒约来 11 个字节，
  *         必须逐字节紧密消费，用字符串级的 GetRxBuf/等待应答都来不及
  */
uint8_t ESP_ReadByte(uint8_t *data)
{
    if (ESP_RxTail == ESP_RxHead)
        return 0;

    *data = ESP_RxBuf[ESP_RxTail];
    ESP_RxTail = (ESP_RxTail + 1) % ESP_RX_BUF_SIZE;
    return 1;
}

uint8_t ESP_WaitResp(char *Expect, uint16_t TimeoutMs)
{
    static char TempBuf[ESP_RX_BUF_SIZE];
    uint16_t pos = 0;
    uint32_t t;

    ESP_ClearRxBuf();

    t = TimeoutMs;
    while (t)
    {
        if (ESP_RxNewData)
        {
            uint8_t data = ESP_RxBuf[ESP_RxTail];
            ESP_RxTail = (ESP_RxTail + 1) % ESP_RX_BUF_SIZE;
            if (ESP_RxTail == ESP_RxHead) ESP_RxNewData = 0;

            if (pos < ESP_RX_BUF_SIZE - 1)
            {
                TempBuf[pos++] = (char)data;
                TempBuf[pos] = '\0';
            }

            if (strstr(TempBuf, Expect) != NULL)
                return 0;

            if (strstr(TempBuf, "ERROR") != NULL || strstr(TempBuf, "FAIL") != NULL)
                return 2;
        }
        Delay_ms(1);
        t--;
    }
    return 1;
}

uint8_t ESP_SendCmd(char *Cmd, uint16_t TimeoutMs)
{
    static char TempBuf[ESP_RX_BUF_SIZE];
    uint16_t pos = 0;
    uint32_t t;

    ESP_RxHead = 0;
    ESP_RxTail = 0;
    ESP_RxNewData = 0;
    pos = 0;

    ESP_SendString(Cmd);

    t = TimeoutMs;
    while (t)
    {
        if (ESP_RxNewData)
        {
            while (ESP_RxTail != ESP_RxHead)
            {
                uint8_t data = ESP_RxBuf[ESP_RxTail];
                ESP_RxTail = (ESP_RxTail + 1) % ESP_RX_BUF_SIZE;

                if (pos < ESP_RX_BUF_SIZE - 1)
                {
                    TempBuf[pos++] = (char)data;
                    TempBuf[pos] = '\0';
                }
            }
            ESP_RxNewData = 0;

            if (strstr(TempBuf, "OK") != NULL)
            {
                Delay_ms(50);
                return 0;
            }
            if (strstr(TempBuf, "ERROR") != NULL || strstr(TempBuf, "FAIL") != NULL)
                return 2;
        }
        Delay_ms(1);
        t--;
    }
    return 1;
}

/**
  * @brief  USART2 中断服务函数（接收 ESP-01S 返回数据）
  */
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        uint8_t data = USART_ReceiveData(USART2);

        uint16_t nextHead = (ESP_RxHead + 1) % ESP_RX_BUF_SIZE;
        if (nextHead != ESP_RxTail)
        {
            ESP_RxBuf[ESP_RxHead] = data;
            ESP_RxHead = nextHead;
            ESP_RxNewData = 1;
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

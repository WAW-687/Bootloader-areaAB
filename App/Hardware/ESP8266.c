#include "stm32f10x.h"
#include "ESP8266.h"
#include "UART2.h"
#include "Delay.h"
#include "OLED.h"
#include "Serial.h"
#include <stdio.h>

uint8_t ESP8266_ConnectWiFi(void)
{
    uint8_t retry;

    OLED_ShowString(2, 1, "AT Test...      ");
    Serial_Printf("ESP: AT Test...\r\n");
    for (retry = 0; retry < 3; retry++)
    {
        if (ESP_SendCmd("AT\r\n", 2000) == 0)
            break;
        Serial_Printf("ESP: AT retry %d\r\n", retry + 1);
    }
    if (retry >= 3)
    {
        OLED_ShowString(2, 1, "WiFi FAIL!      ");
        Serial_Printf("ESP: AT FAIL!\r\n");
        return 1;
    }

    Serial_Printf("ESP: AT OK\r\n");
    ESP_SendCmd("AT+CWMODE=1\r\n", 1000);

    OLED_ShowString(2, 1, "Connecting...   ");
    Serial_Printf("ESP: Connecting WiFi...\r\n");
    for (retry = 0; retry < WIFI_MAX_RETRY; retry++)
    {
        char cmd[100];
        sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);

        uint8_t res = ESP_SendCmd(cmd, 12000);
        if (res == 0)
        {
            Serial_Printf("ESP: WiFi Connected\r\n");
            break;
        }

        Serial_Printf("ESP: WiFi retry %d\r\n", retry + 1);
        if (retry < WIFI_MAX_RETRY - 1)
        {
            OLED_ShowString(2, 1, "Retry...        ");
            Delay_ms(1000);
        }
    }

    if (retry >= WIFI_MAX_RETRY)
    {
        OLED_ShowString(2, 1, "WiFi FAIL!      ");
        Serial_Printf("ESP: WiFi FAIL!\r\n");
        return 1;
    }

    /* 留给 main.c 显示 "WiFi OK"，此处只清除进度文字 */
    OLED_ShowString(2, 1, "                ");
    Delay_ms(300);
    return 0;
}

uint8_t ESP8266_ConnectTCP(void)
{
    uint8_t retry;
    char cmd[80];

    ESP_SendCmd("AT+CIPCLOSE\r\n", 1000);

    for (retry = 0; retry < 3; retry++)
    {
        OLED_ShowString(3, 1, "TCP Conn...     ");
        Serial_Printf("ESP: TCP try %d...\r\n", retry + 1);

        sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", TCP_SERVER_IP, TCP_SERVER_PORT);
        uint8_t res = ESP_SendCmd(cmd, 8000);

        if (res == 0)
        {
            Serial_Printf("ESP: TCP Connected\r\n");
            return 0;
        }
        else if (res == 2)
            Serial_Printf("ESP: TCP Error, retry...\r\n");
        else
            Serial_Printf("ESP: TCP Timeout, retry...\r\n");

        if (retry < 2)
            Delay_ms(1000);
    }

    Serial_Printf("ESP: TCP FAIL!\r\n");
    return 1;
}

uint8_t ESP8266_SendData(uint8_t *data, uint16_t len)
{
    char cmd[20];

    sprintf(cmd, "AT+CIPSEND=%d\r\n", len);
    if (ESP_SendCmd(cmd, 3000) != 0)
    {
        Serial_Printf("ESP: CIPSEND timeout!\r\n");
        return 1;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        ESP_SendByte(data[i]);
    }
    Serial_Printf("ESP: Sent %d bytes\r\n", len);
    return 0;
}

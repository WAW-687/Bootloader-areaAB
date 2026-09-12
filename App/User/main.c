#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "LED.h"
#include "LightSensor.h"
#include "DHT11.h"
#include "Serial.h"
#include "UART2.h"
#include "ESP8266.h"
#include "Key.h"
#include "ota.h"
#include "w25q64.h"

int main(void)
{
    uint8_t humi, temp;
    uint8_t lightPercent;
    uint16_t adcRaw;
    uint8_t keyNum;

    uint8_t  lightThreshold = 30;
    uint8_t  editMode = 0;
    uint8_t  blinkShow = 1;
    uint16_t blinkCnt = 0;
    uint16_t sensorCnt = 0;
    uint16_t otaCnt = 0;
    uint8_t  wifiOk = 0;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    OLED_Init();
    LED_Init();
    LightSensor_Init();
    DHT11_Init();
    Serial_Init();
    Key_Init();
    W25Q64_Init();      /* SPI1 + W25Q64 初始化（OTA 元数据/固件存储必须先初始化） */

    OLED_ShowString(1, 1, "T:--C H:--%");
    OLED_ShowString(2, 1, "WiFi:Conn...");
    OLED_ShowString(3, 1, "TCP:Wait...");
    OLED_ShowString(4, 1, "Light: 0%--30% ");

    Serial_Printf("System Start v%d.%d.%d\r\n",
                  FW_VER_MAJOR, FW_VER_MINOR, FW_VER_PATCH);

    /* ===== ESP-01S 联网 ===== */
    ESP_Init();

    if (ESP8266_ConnectWiFi() == 0)
    {
        wifiOk = 1;
        OLED_ShowString(2, 1, "WiFi OK         ");

        /* ---- 开机首查版本（OTA 触发入口，不等 60 秒轮询） ---- */
        {
            uint8_t otaRes;
            OLED_ShowString(3, 1, "OTA Check...    ");
            otaRes = OTA_CheckUpdate();

            if (otaRes == OTA_HAS_UPDATE)
            {
                OLED_ShowString(3, 1, "OTA Updating... ");
                if (OTA_Download() != OTA_OK)   /* 成功不会返回（内部已复位） */
                {
                    OLED_ShowString(3, 1, "OTA FAIL!       ");
                    Serial_Printf("OTA: Download FAIL!\r\n");
                }
                OLED_ShowString(3, 1, "                ");
            }
            else if (otaRes == OTA_ERR_NET)
            {
                Serial_Printf("OTA: Check FAIL!\r\n");
            }
            else if (otaRes == OTA_NO_UPDATE)
            {
                Serial_Printf("OTA: No update\r\n");
            }
        }

        if (ESP8266_ConnectTCP() == 0)
            OLED_ShowString(3, 1, "TCP OK          ");
        else
            OLED_ShowString(3, 1, "TCP FAIL!       ");
    }
    else
    {
        OLED_ShowString(2, 1, "WiFi FAIL!      ");
        OLED_ShowString(3, 1, "TCP FAIL!       ");
    }

    Serial_Printf("Enter Main Loop\r\n");

    while (1)
    {
        /* ---- 按键 ---- */
        keyNum = Key_GetNum();
        if (keyNum == 1)
        {
            editMode = !editMode;
            blinkCnt = 0;
            blinkShow = 1;
            Serial_Printf("Edit:%s Th:%d%%\r\n", editMode ? "ON" : "OFF", lightThreshold);
        }
        else if (editMode)
        {
            if (keyNum == 2 && lightThreshold >= 5) lightThreshold -= 5;
            else if (keyNum == 2) lightThreshold = 0;
            if (keyNum == 3 && lightThreshold <= 95) lightThreshold += 5;
            else if (keyNum == 3) lightThreshold = 100;
            if (keyNum == 2 || keyNum == 3)
            {
                blinkCnt = 0;
                blinkShow = 1;
                Serial_Printf("Threshold:%d%%\r\n", lightThreshold);
            }
        }

        /* ---- 光敏 ---- */
        adcRaw = LightSensor_Get();
        lightPercent = LightSensor_GetPercent();
        if (lightPercent < lightThreshold)
            LED1_ON();
        else
            LED1_OFF();

        /* ---- OLED 第 4 行 ---- */
        {
            char line4[17];
            if (editMode)
            {
                blinkCnt++;
                if (blinkCnt >= 5)
                {
                    blinkCnt = 0;
                    blinkShow = !blinkShow;
                }
                if (blinkShow)
                    sprintf(line4, "Light:%3d%%--%2d%%", lightPercent, lightThreshold);
                else
                    sprintf(line4, "Light:%3d%%     ", lightPercent);
            }
            else
            {
                sprintf(line4, "Light:%3d%%--%2d%%", lightPercent, lightThreshold);
            }
            OLED_ShowString(4, 1, line4);
        }

        /* ---- DHT11 + TCP 发送（每秒） ---- */
        sensorCnt++;
        if (sensorCnt >= 10)
        {
            sensorCnt = 0;

            if (DHT11_ReadData(&humi, &temp) == DHT11_OK)
            {
                OLED_ShowChar(1, 3, temp / 10 + '0');
                OLED_ShowChar(1, 4, temp % 10 + '0');
                OLED_ShowChar(1, 9, humi / 10 + '0');
                OLED_ShowChar(1, 10, humi % 10 + '0');

                char sendBuf[32];
                uint16_t len = sprintf(sendBuf, "T:%d H:%d L:%d%%\r\n", temp, humi, lightPercent);
                ESP8266_SendData((uint8_t *)sendBuf, len);

                Serial_Printf("TCP> %s", sendBuf);
            }
            else
            {
                OLED_ShowString(1, 1, "T:--C H:--%");
                Serial_Printf("DHT11 Fail!\r\n");
            }
        }

        /* ---- OTA 周期查版本（每 60 秒，WiFi 可用时才查） ---- */
        otaCnt++;
        if (otaCnt >= 600 && wifiOk)
        {
            otaCnt = 0;
            if (OTA_CheckUpdate() == OTA_HAS_UPDATE)
            {
                OLED_ShowString(3, 1, "OTA Updating... ");
                if (OTA_Download() != OTA_OK)   /* 成功不会返回（内部已复位） */
                {
                    OLED_ShowString(3, 1, "OTA FAIL!       ");
                    Serial_Printf("OTA: Download FAIL!\r\n");
                }
            }
        }

        Delay_ms(100);
    }
}

#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

/* ========= WiFi 配置（按需修改） ========= */
#define WIFI_SSID       "vivo S19 Pro"      // 请修改为你的 WiFi 名称
#define WIFI_PASSWORD   "15935700"      // 请修改为你的 WiFi 密码
#define WIFI_MAX_RETRY  3                   // 最大重连次数

/* ========= TCP 目标（手机热点/服务器，传感器数据上报用） ========= */
#define TCP_SERVER_IP   "10.218.5.102"      // 请修改为你的服务器 IP
#define TCP_SERVER_PORT 8080                // TCP 端口

/* ========= OTA 固件服务器（花生壳穿透地址，OTA 模块专用） ========= */
#define OTA_SERVER_IP   "1298ew61nf633.vicp.fun"   // 花生壳赠送的外网域名（必须用域名，网关按域名路由隧道）
//#define OTA_SERVER_IP "115.236.153.174"        // 网关IP：连上只是到贝锐官网服务器，不进隧道，禁止使用
#define OTA_SERVER_PORT 20184                      // 花生壳映射分配的外网端口

uint8_t ESP8266_ConnectWiFi(void);          // 连接 WiFi，0=成功
uint8_t ESP8266_ConnectTCP(void);           // 建立 TCP，0=成功
uint8_t ESP8266_SendData(uint8_t *data, uint16_t len);  // 发送数据，0=成功

#endif

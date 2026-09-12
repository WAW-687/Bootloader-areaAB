#ifndef __OTA_H
#define __OTA_H

#include "stm32f10x.h"

/**
  ******************************************************************************
  * @file    ota.h
  * @brief   A 区（App）OTA 客户端：连固件服务器查版本 / 下载固件到 W25Q64 / 置更新标志
  * @note    配套关系：
  *          服务器 = D:\Project\STM32\OTA服务器\ota_server.py（花生壳穿透可达）
  *          协议   = GETVER -> "VER x.y.z" / UPDATE -> "LEN n CRC xxxx" + bin 流
  *          复位后 Bootloader 检测 OTA_flag == OTA_SET_FLAG 自动搬运新固件
  ******************************************************************************
  */

/* ===================== 本固件版本号（发布新版本时改这里重新编译） ===================== */
#define FW_VER_MAJOR   2
#define FW_VER_MINOR   0
#define FW_VER_PATCH   0
/* 版本数字化：2.0.0 -> 20000，便于和服务器版本比大小 */
#define FW_VER_NUM     ((uint32_t)FW_VER_MAJOR * 10000 + (uint32_t)FW_VER_MINOR * 100 + FW_VER_PATCH)

/* ===================== W25Q64 元数据布局（必须与 Bootloader 工程的 main.h 严格一致） ===== */
#define W25Q64_TOTAL_SIZE   0x800000                                  /* 8MB */
#define W25Q64_SECTOR_SIZE  4096                                      /* 4KB 扇区 */
#define W25Q64_PAGE_SIZE    256                                       /* 256B 编程页 */
#define W25Q64_META_SADDR   (W25Q64_TOTAL_SIZE - W25Q64_SECTOR_SIZE)  /* 元数据扇区 0x7FF000 */
#define W25Q64_META_PAGE    (W25Q64_META_SADDR / W25Q64_PAGE_SIZE)    /* 元数据页号 32760 */
#define W25Q64_FW_BLOCK     0                                         /* 固件存放：0 号 64KB 块 */

#define OTA_SET_FLAG        0xAABB1122                                /* OTA 更新标志魔数（同 Bootloader） */

/* ===================== 元数据结构（字段顺序不得改动，与 Bootloader 共享同一块 Flash） ===== */
typedef struct {
    uint32_t OTA_flag;         /* 0 号成员固定在首位（魔数判断依赖） */
    uint32_t Firelen[11];      /* [0] = 固件长度，其余预留 */
    uint8_t  OTA_ver[32];      /* 版本字符串 */
} OTA_InfoCB;

/* ===================== 返回码 ===================== */
#define OTA_OK           0    /* 成功 */
#define OTA_NO_UPDATE    0    /* 查版本：无更新 */
#define OTA_HAS_UPDATE   1    /* 查版本：服务器版本更新 */
#define OTA_ERR_NET      2    /* 网络/连接/超时失败 */
#define OTA_ERR_PROTO    3    /* 服务器应答格式错误 */
#define OTA_ERR_FLASH    4    /* W25Q64 读写失败 */
#define OTA_ERR_CRC      5    /* 固件 CRC 校验不通过 */

uint8_t OTA_CheckUpdate(void);        /* 查版本：0=无更新 1=有更新 2=网络失败 */
uint8_t OTA_Download(void);           /* 下载固件，成功则写标志并复位（不返回） */

#endif

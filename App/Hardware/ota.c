/**
  ******************************************************************************
  * @file    ota.c
  * @brief   A 区（App）OTA 客户端实现
 * @note    流程：连花生壳域名 -> GETVER 比版本 -> 有更新则 UPDATE 拿固件头，
 *          再按 1KB 分块 DATA <偏移> <长度> 拉取写入 W25Q64 0 号块（块残缺重收该块），
 *          回读 CRC 校验 -> 通过才置 OTA_flag 并复位（失败不置标志，老固件照常跑）
  *          服务器一次连接只处理一条命令，所以查版本/下载各自独立建连、用完即断
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "UART2.h"
#include "ESP8266.h"
#include "Delay.h"
#include "w25q64.h"
#include "Serial.h"
#include "ota.h"
#include <stdio.h>
#include <string.h>

/* ===================== +IPD 载荷解析器 ===================== */
/* AT 指令模式下，服务器的数据以 "+IPD,<长度>:<数据>" 形式夹杂在 AT 应答文本里到达。
   状态机逐字节过滤：0=正在找 "+IPD,"  1=在读十进制长度  2=正在输出载荷            */
typedef struct {
    uint8_t  state;
    uint8_t  match;      /* 已匹配到 "+IPD," 的第几个字符 */
    uint32_t ipdLen;     /* 本段载荷总长 */
    uint32_t cnt;        /* 本段已输出字节数 */
} IPDParser;

static const char IPD_HEAD[5] = {'+', 'I', 'P', 'D', ','};

static void IPD_Init(IPDParser *p)
{
    p->state = 0;
    p->match = 0;
    p->ipdLen = 0;
    p->cnt = 0;
}

/**
  * @brief  喂一个接收字节给解析器
  * @param  in  从 ESP 环形缓冲取出的原始字节
  * @param  out 载荷输出（仅当返回 1 时有效）
  * @retval 1=out 是服务器发来的有效载荷字节  0=该字节是 AT 文本被跳过
  */
static uint8_t IPD_Feed(IPDParser *p, uint8_t in, uint8_t *out)
{
    switch (p->state)
    {
    case 0:                                  /* 找 "+IPD," 字面量 */
        if (in == IPD_HEAD[p->match])
        {
            p->match++;
            if (p->match >= 5) { p->state = 1; p->ipdLen = 0; }
        }
        else
        {
            p->match = (in == '+') ? 1 : 0;  /* 失配回退：'+' 可能是新串开头 */
        }
        return 0;

    case 1:                                  /* 读长度数字，直到冒号 */
        if (in >= '0' && in <= '9')
            p->ipdLen = p->ipdLen * 10 + (in - '0');
        else if (in == ':')
        {
            p->state = 2;
            p->cnt = 0;
        }
        else
            p->state = 0;                    /* 异常帧，重新找头 */
        return 0;

    default:                                 /* 输出载荷 */
        *out = in;
        p->cnt++;
        if (p->cnt >= p->ipdLen)
            p->state = 0;                    /* 本段收完，继续找下一段头 */
        return 1;
    }
}

/* ===================== 内部工具 ===================== */

/**
  * @brief  从 ESP 环形缓冲取一个字节，缓冲空则延时 1ms 重试
  * @retval 1=取到字节  0=timeoutMs 内一直无数据
  */
static uint8_t OTA_RecvByte(uint8_t *out, uint16_t timeoutMs)
{
    while (timeoutMs--)
    {
        if (ESP_ReadByte(out))
            return 1;
        Delay_ms(1);
    }
    return 0;
}

/**
  * @brief  XMODEM CRC16（多项式 0x1021，初值传入支持分段链式计算）
  */
static uint16_t OTA_CRC16_Update(uint16_t crc, uint8_t *data, uint16_t len)
{
    uint8_t i;

    while (len--)
    {
        crc ^= (uint16_t)(*data) << 8;
        for (i = 0; i < 8; i++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
        data++;
    }
    return crc;
}

/**
  * @brief  连接到 OTA 固件服务器（花生壳域名）
  * @note   先发 CIPCLOSE 防呆（清除可能残留的旧连接），再连 OTA_SERVER_IP
  */
static uint8_t OTA_ConnectServer(void)
{
    char cmd[80];
    uint8_t retry;

    ESP_SendCmd("AT+CIPCLOSE\r\n", 1000);

    for (retry = 0; retry < 3; retry++)
    {
        sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", OTA_SERVER_IP, OTA_SERVER_PORT);
        if (ESP_SendCmd(cmd, 8000) == 0)
            return OTA_OK;
        Delay_ms(500);
    }
    Serial_Printf("OTA: Connect Server FAIL!\r\n");
    return OTA_ERR_NET;
}

/**
  * @brief  收一条以 '\n' 结尾的服务器文本应答（自动剥离 AT 文本，只留 +IPD 载荷）
  */
static uint8_t OTA_RecvLine(char *line, uint16_t maxSize, IPDParser *parser)
{
    uint8_t b, pay;
    uint16_t li = 0;

    while (1)
    {
        if (!OTA_RecvByte(&b, 5000))
            return OTA_ERR_NET;

        if (IPD_Feed(parser, b, &pay))
        {
            if (li < maxSize - 1)
                line[li++] = (char)pay;

            if (pay == '\n' || li >= maxSize - 1)
                break;
        }
    }
    line[li] = '\0';
    return OTA_OK;
}

/**
  * @brief  把元数据写入 W25Q64 最后一个扇区（与 Bootloader 的 WriteOTAInfo 同一套逻辑）
  */
static void OTA_WriteMeta(OTA_InfoCB *meta)
{
    uint8_t pagebuf[256];

    if (W25Q64_Erase4K(W25Q64_META_SADDR) != W25Q64_OK)
    {
        Serial_Printf("OTA: Meta Erase FAIL!\r\n");
        return;
    }

    memset(pagebuf, 0xFF, sizeof(pagebuf));
    memcpy(pagebuf, meta, sizeof(OTA_InfoCB));
    W25Q64_PageWrite(pagebuf, W25Q64_META_PAGE);
}

/* ===================== 对外接口 ===================== */

/**
  * @brief  查询服务器固件版本，与本地 FW_VER_NUM 比较
  * @retval OTA_NO_UPDATE=无更新  OTA_HAS_UPDATE=有更新  OTA_ERR_NET=通信失败
  */
uint8_t OTA_CheckUpdate(void)
{
    IPDParser parser;
    OTA_InfoCB meta;
    char line[24];
    int rMaj = 0, rMin = 0, rPat = 0;
    uint8_t res = OTA_NO_UPDATE;

    W25Q64_Read((uint8_t *)&meta, W25Q64_META_SADDR, sizeof(OTA_InfoCB));
    Serial_Printf("OTA: Local Ver %d.%d.%d\r\n",
                  FW_VER_MAJOR, FW_VER_MINOR, FW_VER_PATCH);

    if (OTA_ConnectServer() != OTA_OK)
        return OTA_ERR_NET;

    if (ESP8266_SendData((uint8_t *)"GETVER\n", 7) != OTA_OK)
    {
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_NET;
    }

    IPD_Init(&parser);
    if (OTA_RecvLine(line, sizeof(line), &parser) != OTA_OK)
    {
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_NET;
    }
    Serial_Printf("OTA: Server Reply %s", line);

    if (sscanf(line, "VER %d.%d.%d", &rMaj, &rMin, &rPat) == 3)
    {
        uint32_t remote = (uint32_t)rMaj * 10000 + (uint32_t)rMin * 100 + (uint32_t)rPat;

        if (remote > FW_VER_NUM)
            res = OTA_HAS_UPDATE;
    }

    ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
    return res;
}

/**
  * @brief  下载固件到 W25Q64 0 号块，CRC 校验通过后置标志并复位
  * @retval 失败时返回错误码（老固件继续跑）；成功时不会返回（直接系统复位）
  */
uint8_t OTA_Download(void)
{
    IPDParser parser;
    OTA_InfoCB meta;
    static uint8_t chunk[1024];              /* 整块收齐才写Flash；static 防默认1KB栈溢出 */
    uint8_t rdbuf[256];
    uint8_t b, pay;
    char line[40];
    uint32_t fwLen = 0, crcTmp = 0;
    uint32_t offset = 0, got = 0, want = 0;
    uint32_t i, j;
    uint16_t crcWant, crcGot = 0, crcRx = 0;
    uint16_t pageNB = 0;
    uint16_t rd, retry, idle;

    /* ---- 建连 + 先擦块（此时尚未发下载请求，服务器没有数据发来，
            150ms 擦除期间不会丢字节；擦完再请求，收头后可无缝进入接收循环） ---- */
    if (OTA_ConnectServer() != OTA_OK)
        return OTA_ERR_NET;

    Serial_Printf("OTA: Erase Block0...\r\n");
    if (W25Q64_Erase64K(W25Q64_FW_BLOCK) != W25Q64_OK)
    {
        Serial_Printf("OTA: Block Erase FAIL!\r\n");
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_FLASH;
    }

    if (ESP8266_SendData((uint8_t *)"UPDATE\n", 7) != OTA_OK)
    {
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_NET;
    }

    /* ---- 阶段1：收头行 "LEN 长度 CRC 校验值" ---- */
    IPD_Init(&parser);
    if (OTA_RecvLine(line, sizeof(line), &parser) != OTA_OK)
    {
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_NET;
    }
    Serial_Printf("OTA: FW Head %s", line);

    if (sscanf(line, "LEN %lu CRC %lx", &fwLen, &crcTmp) != 2)
    {
        Serial_Printf("OTA: Bad Head Format!\r\n");
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_PROTO;
    }
    crcWant = (uint16_t)crcTmp;

    /* 防护：空长度或超出 64KB 块容量都拒绝 */
    if (fwLen == 0 || fwLen > 65536)
    {
        Serial_Printf("OTA: Bad FW Len %lu!\r\n", fwLen);
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_PROTO;
    }

    /* ---- 阶段2：分块拉取固件（每块1KB，按偏移请求，收齐一块写一块） ----
       流式收整包对"连接中途截断"零抵抗（尾块残缺只能整包重来）；
       分块后哪块丢了就重发哪块的请求，天然断点续传。
       页写在两块之间进行，此刻线上无数据，环形缓冲不存在溢出风险。 ---- */
    for (offset = 0; offset < fwLen; offset += got)
    {
        want = (fwLen - offset >= 1024) ? 1024 : (fwLen - offset);

        for (retry = 0; retry < 3; retry++)
        {
            sprintf(line, "DATA %lu %lu\n", offset, want);
            if (ESP8266_SendData((uint8_t *)line, strlen(line)) != OTA_OK)
                continue;                        /* CIPSEND失败，重试 */

            IPD_Init(&parser);
            got = 0;
            idle = 0;
            while (got < want)
            {
                if (ESP_ReadByte(&b))
                {
                    idle = 0;
                    if (IPD_Feed(&parser, b, &pay))
                        chunk[got++] = pay;
                }
                else
                {
                    Delay_ms(1);
                    if (++idle >= 5000)          /* 5秒无数据 = 本块残缺 */
                        break;
                }
            }
            if (got == want)
                break;

            Serial_Printf("OTA: Chunk@%lu short %lu/%lu, retry\r\n", offset, got, want);

            /* 丢弃旧块可能仍在途的尾巴（以1秒静默为准），避免混入重发块 */
            idle = 0;
            while (idle < 1000)
            {
                if (ESP_ReadByte(&b))
                    idle = 0;
                else
                {
                    Delay_ms(1);
                    idle++;
                }
            }
        }
        if (got != want)
        {
            Serial_Printf("OTA: Chunk FAIL @%lu!\r\n", offset);
            ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
            return OTA_ERR_NET;
        }

        /* 诊断1：对"刚收到的数据"单独算CRC，与flash回读CRC分离定位问题侧 */
        crcRx = OTA_CRC16_Update(crcRx, chunk, (uint16_t)want);

        /* 整块收齐才碰Flash：按256B逐页写入，页号连续递增。
           写完立刻回读校验，不符就重写该页（防编程被电源尖峰静默吞掉），最多3次。
           重写的是同一份数据：NOR 编程只能把1写成0，同数据重写不会越写越错 */
        Delay_ms(2);                         /* 让ESP发完AT尾包，电源稳一下再编程 */
        for (j = 0; j < want; j += 256)
        {
            rd = (want - j >= 256) ? 256 : (uint16_t)(want - j);

            for (retry = 0; retry < 3; retry++)
            {
                if (W25Q64_PageWrite(&chunk[j], pageNB + j / 256) != W25Q64_OK)
                    continue;

                if (W25Q64_Read(rdbuf, offset + j, rd) != W25Q64_OK)
                    continue;

                for (i = 0; i < rd; i++)
                    if (rdbuf[i] != chunk[j + i])
                        break;

                if (i >= rd)
                    break;                   /* 本页写入并回读一致 */
                Serial_Printf("OTA: Page@%lu mismatch, rewrite\r\n", offset + j);
            }
            if (retry >= 3)
            {
                Serial_Printf("OTA: Page@%lu Write FAIL!\r\n", offset + j);
                ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
                return OTA_ERR_FLASH;
            }
        }
        pageNB += (want + 255) / 256;

        Serial_Printf("OTA: Recv %lu/%lu\r\n", offset + want, fwLen);
    }

    /* ---- 阶段3：从 W25Q64 回读算 CRC（同时验证传输与写入两条链路） ---- */
    for (i = 0; i < fwLen; i += 256)
    {
        rd = (fwLen - i >= 256) ? 256 : (uint16_t)(fwLen - i);
        if (W25Q64_Read(rdbuf, i, rd) != W25Q64_OK)
        {
            Serial_Printf("OTA: ReadBack FAIL!\r\n");
            ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
            return OTA_ERR_FLASH;
        }
        crcGot = OTA_CRC16_Update(crcGot, rdbuf, rd);
    }

    if (crcGot != crcWant)
    {
        /* rx=CRC(收到流) flash=CRC(回读) want=服务器宣称；三者对比定位问题侧 */
        Serial_Printf("OTA: CRC FAIL! want=%04X rx=%04X flash=%04X\r\n",
                      crcWant, crcRx, crcGot);
        ESP_SendCmd("AT+CIPCLOSE\r\n", 500);
        return OTA_ERR_CRC;                  /* 不置标志：老固件继续跑，永不变砖 */
    }

    /* ---- 通知服务器校验通过，断开 ---- */
    ESP8266_SendData((uint8_t *)"OK\n", 3);
    ESP_SendCmd("AT+CIPCLOSE\r\n", 500);

    /* ---- 写元数据：置 OTA 标志 + 固件长度 + 新版本号 ---- */
    memset(&meta, 0, sizeof(meta));
    meta.OTA_flag  = OTA_SET_FLAG;
    meta.Firelen[0] = fwLen;
    sprintf((char *)meta.OTA_ver, "VER=%d.%d.%d-OTA",
            FW_VER_MAJOR, FW_VER_MINOR, FW_VER_PATCH);
    OTA_WriteMeta(&meta);

    Serial_Printf("OTA: OK! %lu bytes, Reboot...\r\n", fwLen);
    Delay_ms(100);
    NVIC_SystemReset();                      /* 复位后 Bootloader 接手搬运 */

    return OTA_OK;                           /* 永远到不了这里 */
}

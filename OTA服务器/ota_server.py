# OTA 正式服务器：版本查询 + 固件分块下发
# 协议（命令以 \n 结尾）：
#   客户端发 "GETVER\n"            → 服务器回 "VER 2.1.0\n"
#   客户端发 "UPDATE\n"            → 服务器回 "LEN 20840 CRC 3F2A\n"（只发头，不发固件）
#   客户端发 "DATA <偏移> <长度>\n" → 服务器回该段固件原始字节（如 DATA 0 1024）
#                                    块收丢了设备会重发同一偏移的请求，天然断点续传
#   客户端校验通过后回 "OK\n"（服务器仅记录日志）
#   其他命令                       → 回 "ERR CMD\n"
# 日常发版操作：改 version.txt 内容 + 替换 App.bin 文件，无需重启本脚本

import socket
import os
import datetime

PORT = 8080
BASE = os.path.dirname(os.path.abspath(__file__))
VER_FILE = os.path.join(BASE, "version.txt")
BIN_FILE = os.path.join(BASE, "App.bin")


def log(msg):
    print("[%s] %s" % (datetime.datetime.now().strftime("%H:%M:%S"), msg), flush=True)


def crc16_xmodem(data):
    """Xmodem CRC16，多项式0x1021，初值0——与单片机端 Xmodem_CRC16 同一套"""
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def read_version():
    try:
        with open(VER_FILE, "r") as f:
            return f.read().strip()
    except OSError:
        return None


def handle(conn, addr):
    log("设备连入 %s:%d" % (addr[0], addr[1]))
    conn.settimeout(30)
    fw = None            # 本连接会话内缓存的固件（UPDATE 时加载，DATA 时按偏移切片）
    try:
        while True:
            # 读一行命令（等到\n）
            buf = b""
            while b"\n" not in buf:
                d = conn.recv(256)
                if not d:
                    log("对方未发命令就断开")
                    return
                buf += d
            cmd = buf.split(b"\n")[0].decode("ascii", "ignore").strip()
            log("收到命令: %r" % cmd)

            if cmd == "GETVER":
                ver = read_version()
                if ver:
                    conn.sendall(("VER %s\n" % ver).encode())
                    log("已回版本: %s" % ver)
                else:
                    conn.sendall(b"ERR NOVER\n")
                    log("version.txt 读取失败，已回 ERR NOVER")

            elif cmd == "UPDATE":
                ver = read_version()
                if not ver or not os.path.exists(BIN_FILE):
                    conn.sendall(b"ERR NOFILE\n")
                    log("固件文件缺失，已回 ERR NOFILE")
                    continue
                with open(BIN_FILE, "rb") as f:
                    fw = f.read()
                crc = crc16_xmodem(fw)
                conn.sendall(("LEN %d CRC %04X\n" % (len(fw), crc)).encode())
                log("固件就绪: %d 字节, CRC=0x%04X, 版本=%s（等待分块请求）" % (len(fw), crc, ver))

            elif cmd.startswith("DATA"):
                if fw is None:
                    conn.sendall(b"ERR ORDER\n")
                    log("未收到 UPDATE 就请求 DATA，已回 ERR ORDER")
                    continue
                try:
                    _, off_s, len_s = cmd.split()
                    off, ln = int(off_s), int(len_s)
                except ValueError:
                    conn.sendall(b"ERR ARG\n")
                    continue
                if ln <= 0 or ln > 1024 or off < 0 or off + ln > len(fw):
                    conn.sendall(b"ERR RANGE\n")
                    log("非法范围 DATA %d %d，已回 ERR RANGE" % (off, ln))
                    continue
                conn.sendall(fw[off:off + ln])
                log("下发分块: offset=%d len=%d (%d/%d)" % (off, ln, off + ln, len(fw)))

            elif cmd == "OK":
                log("设备校验通过，OTA 下载完成!")
                break

            else:
                conn.sendall(b"ERR CMD\n")
                log("未知命令，已回 ERR CMD")

    except socket.timeout:
        log("连接超时")
    except ConnectionResetError:
        log("连接被对方重置")
    except Exception as e:
        log("连接异常: %s" % e)
    finally:
        conn.close()
        log("连接断开")


def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", PORT))
    srv.listen(2)
    log("OTA服务器启动，监听 %d 端口" % PORT)
    log("当前版本: %s | App.bin: %s" % (
        read_version(), "存在" if os.path.exists(BIN_FILE) else "未放置"))
    while True:
        conn, addr = srv.accept()
        handle(conn, addr)      # 顺序处理，单设备场景足够


if __name__ == "__main__":
    main()

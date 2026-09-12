# -*- coding: utf-8 -*-
"""
花生壳隧道探针：检测 OTA 服务器链路当前是否真正可达。
用法：
    python probe_tunnel.py
判定：
    回复 VER x.y.z   -> 隧道+服务器全通
    回复 HTTP/1.1... -> 连到了贝锐网关的网页 = 隧道未生效（花生壳客户端离线/映射暂停/流量耗尽）
    连接被拒/超时     -> 网关侧端口未开放，或客户端离线
"""
import socket

DOMAIN = "1298ew61nf633.vicp.fun"
PORT = 20184

def probe(name, host):
    print("==== 通过%s连接 %s:%d ..." % (name, host, PORT))
    try:
        s = socket.create_connection((host, PORT), timeout=8)
        s.settimeout(8)
        s.sendall(b"GETVER\n")
        data = s.recv(300)
        text = data[:200]
        print("回复:", text)
        if text.startswith(b"VER "):
            print(">>> 链路全通：隧道生效，OTA服务器已应答")
        elif text.startswith(b"HTTP/"):
            print(">>> 隧道未生效！连到的是贝锐网关网页。")
            print("    检查：1.花生壳客户端是否已登录在线  2.映射是否在线/被暂停  3.免费版月流量是否耗尽")
            print("    另外：本机 ota_server.py 必须在运行（监听8080）")
        else:
            print(">>> 未知回复，把上面内容发我分析")
        s.close()
    except ConnectionRefusedError:
        print(">>> 连接被拒绝：花生壳客户端大概率离线（网关无人应答此端口）")
    except socket.timeout:
        print(">>> 连接/应答超时：网关无响应，检查花生壳客户端在线状态")
    except Exception as e:
        print(">>> 失败: %s %s" % (type(e).__name__, e))
    print()

if __name__ == "__main__":
    probe("域名", DOMAIN)

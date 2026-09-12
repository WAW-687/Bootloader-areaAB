# 临时测试服务器：验证花生壳穿透链路用
# 功能：监听 8080 端口，把收到的任何数据原样发回去（回显）
# 正式服务器(ota_server.py)写好后，本文件即可删除

import socket

def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", 8080))
    srv.listen(1)
    print("测试服务器已启动，监听 8080 端口，等待连接...")
    while True:
        conn, addr = srv.accept()
        print("有设备连入:", addr)
        try:
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                print("收到:", data)
                conn.sendall(data)          # 原样回显
        except Exception as e:
            print("连接异常:", e)
        finally:
            conn.close()
            print("连接断开，继续等待...")

if __name__ == "__main__":
    main()

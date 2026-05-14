#! /usr/bin/env python3
import socket
import time

# UECS NODESCANの設定
UDP_PORT = 16529
MESSAGE = b'<?xml version="1.0"?><UECS ver="1.00-E10"><NODESCAN/></UECS>'
# ブロードキャストアドレス（255.255.255.255 でローカルネットワーク全体に叫ぶ）
BROADCAST_IP = "255.255.255.255"

print(f"=== UECS NODESCAN TEST ===")
print(f"Broadcasting {MESSAGE.decode()} to Port {UDP_PORT}...")

# UDPソケットの作成
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# ブロードキャスト送信を許可する設定
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
# 応答待ちのタイムアウトを3秒に設定
sock.settimeout(3.0)

try:
    # ネットワーク全体にNODESCANパケットを送信
    sock.sendto(MESSAGE, (BROADCAST_IP, UDP_PORT))
    
    print("Waiting for replies...\n")
    
    # 応答を待機して表示
    while True:
        data, addr = sock.recvfrom(1024) # 最大1024バイト受信
        print(f"--- Reply from Node: {addr[0]} ---")
        
        # 受信したXMLデータを文字列としてデコードして表示
        try:
            xml_text = data.decode('utf-8')
            print(xml_text)
        except UnicodeDecodeError:
            print(f"(Raw Binary) {data}")
            
except socket.timeout:
    print("\n--- Scan Finished (3 seconds timeout) ---")
except Exception as e:
    print(f"Error: {e}")
finally:
    sock.close()

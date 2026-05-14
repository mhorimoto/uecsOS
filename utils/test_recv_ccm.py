#! /usr/bin/env python3
import socket
import time

# UECS CCM通信用のポート
UDP_PORT = 16520
BROADCAST_IP = "255.255.255.255"

# ダミーの気温(25.4度)をブロードキャストするXML
# type="InAirTemp", room="1", region="1"
MESSAGE = b"""<?xml version="1.0"?>
<UECS ver="1.00-E10">
<DATA type="InAirTemp" room="1" region="1" order="1" priority="29">25.4</DATA>
<IP>192.168.1.100</IP>
</UECS>"""

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

print(f"Broadcasting InAirTemp (25.4) to Port {UDP_PORT}...")
sock.sendto(MESSAGE, (BROADCAST_IP, UDP_PORT))
sock.close()
print("Done.")

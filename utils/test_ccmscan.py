#! /usr/bin/env python3

import socket

UDP_PORT = 16529
# UECS規約に準拠したCCMSCAN要求電文
MESSAGE = b'<?xml version="1.0"?><UECS ver="1.00-E10"><CCMSCAN/></UECS>'
BROADCAST_IP = "255.255.255.255"

print("=== UECS CCMSCAN TEST ===")
print(f"Broadcasting CCMSCAN to Port {UDP_PORT}...")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(3.0)

try:
    sock.sendto(MESSAGE, (BROADCAST_IP, UDP_PORT))
    print("Waiting for replies...\n")
    
    while True:
        data, addr = sock.recvfrom(2048)
        print(f"--- Reply from Node: {addr[0]} ---")
        try:
            print(data.decode('utf-8'))
        except UnicodeDecodeError:
            print(f"(Raw Binary) {data}")
            
except socket.timeout:
    print("\n--- Scan Finished (3 seconds timeout) ---")
finally:
    sock.close()


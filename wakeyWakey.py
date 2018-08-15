#!/usr/bin/python3.6

import socket
import time

print("Wakey wakey rice it's time to start the day!")

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.26', 21))
print(sock.recv(100).decode())
sock.send("UP 5\0".encode())
sock.close()

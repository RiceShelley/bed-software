#!/usr/bin/python3.6

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.26', 21))
sock.send("ADOWN\0".encode())
sock.close()

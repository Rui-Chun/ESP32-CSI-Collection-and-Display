import socket

UDP_IP = "192.168.4.2"
UDP_PORT = 8848

sock = socket.socket(socket.AF_INET, # Internet
                    socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

while True:
    data, addr = sock.recvfrom(2048) # buffer size is 1024 bytes
    print("received message: ", str(data, encoding="ascii"))

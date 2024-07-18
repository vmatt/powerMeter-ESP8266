import socket
import struct

def listen_for_udp_broadcast(port=6969):
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Allow reuse of the address (in case the script is restarted quickly)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # Bind to all interfaces on the specified port
    sock.bind(('', port))
    
    print(f"Listening for UDP broadcasts on port {port}...")
    
    while True:
        data, addr = sock.recvfrom(1024)  # Buffer size is 1024 bytes
        print(f"Received message from {addr}: {data.decode('utf-8')}")

if __name__ == "__main__":
    listen_for_udp_broadcast()

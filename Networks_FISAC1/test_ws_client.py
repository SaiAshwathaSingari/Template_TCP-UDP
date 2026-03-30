import socket
import hashlib
import base64
import os
import struct
import json

def websocket_test():
    host = '127.0.0.1'
    port = 9090

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    print(f"Connected to {host}:{port}")

    # Generate WebSocket key
    key = base64.b64encode(os.urandom(16)).decode()
    print(f"Key: {key}")

    # Send upgrade request
    request = (
        f"GET / HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    )
    s.sendall(request.encode())
    print("Sent upgrade request")

    # Read response
    response = b""
    while b"\r\n\r\n" not in response:
        chunk = s.recv(1024)
        if not chunk:
            print("Connection closed before handshake complete!")
            return
        response += chunk
    print(f"Response:\n{response.decode()}")

    # Verify accept key
    magic = "258EAFA5-E914-47DA-95CA-5AB4AA29BE5E"
    expected = base64.b64encode(hashlib.sha1((key + magic).encode()).digest()).decode()
    if expected in response.decode():
        print("Accept key VALID!")
    else:
        print(f"Accept key MISMATCH! Expected: {expected}")

    # Send a masked WebSocket text frame (register)
    msg = json.dumps({"type": "auth_register", "username": "testuser", "password": "testpass"})
    payload = msg.encode()
    mask_key = os.urandom(4)

    frame = bytearray()
    frame.append(0x81)  # FIN=1, opcode=text
    if len(payload) < 126:
        frame.append(0x80 | len(payload))  # MASK=1
    frame.extend(mask_key)
    for i, b in enumerate(payload):
        frame.append(b ^ mask_key[i % 4])

    s.sendall(frame)
    print(f"Sent register message ({len(payload)} bytes)")

    # Read response frame
    s.settimeout(5)
    try:
        header = s.recv(2)
        if not header:
            print("Connection closed! No response frame.")
            return
        opcode = header[0] & 0x0F
        length = header[1] & 0x7F
        print(f"Response frame: opcode={opcode}, length={length}")
        if length == 126:
            ext = s.recv(2)
            length = struct.unpack('>H', ext)[0]
        data = b""
        while len(data) < length:
            chunk = s.recv(length - len(data))
            if not chunk:
                break
            data += chunk
        print(f"Response data: {data.decode()}")
    except socket.timeout:
        print("Timeout waiting for response")

    s.close()
    print("Done!")

if __name__ == "__main__":
    websocket_test()

import socket
import hashlib
import base64
import threading
import struct
import time
import sys

MAGIC = "258EAFA5-E914-47DA-95CA-5AB4AA29BE5E"

HTML = b"""<!DOCTYPE html><html><body>
<h1>WebSocket Test</h1>
<div id="status">Not connected</div>
<script>
const ws = new WebSocket('ws://' + location.host);
ws.onopen = () => { document.getElementById('status').textContent = 'CONNECTED!'; ws.send('hello'); };
ws.onmessage = (e) => { document.getElementById('status').textContent = 'Got: ' + e.data; };
ws.onclose = (e) => { document.getElementById('status').textContent = 'CLOSED code=' + e.code + ' clean=' + e.wasClean; };
ws.onerror = () => { document.getElementById('status').textContent = 'ERROR'; };
</script></body></html>"""

def handle_client(conn, addr):
    print(f"[{addr}] Connected")
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            print(f"[{addr}] Closed before handshake")
            conn.close()
            return
        data += chunk

    request = data.decode("utf-8", errors="replace")
    print(f"[{addr}] Request:\n{request[:200]}")

    if "Upgrade: websocket" in request or "upgrade: websocket" in request.lower():
        for line in request.split("\r\n"):
            if line.lower().startswith("sec-websocket-key:"):
                key = line.split(":", 1)[1].strip()
                break
        else:
            print(f"[{addr}] No key found!")
            conn.close()
            return

        accept = base64.b64encode(hashlib.sha1((key + MAGIC).encode()).digest()).decode()
        response = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept}\r\n"
            "\r\n"
        )
        conn.sendall(response.encode())
        print(f"[{addr}] WebSocket handshake sent (accept={accept})")

        while True:
            try:
                header = conn.recv(2)
                if not header or len(header) < 2:
                    print(f"[{addr}] Connection closed (recv returned {len(header) if header else 0})")
                    break

                opcode = header[0] & 0x0F
                masked = (header[1] & 0x80) != 0
                length = header[1] & 0x7F

                if length == 126:
                    ext = conn.recv(2)
                    length = struct.unpack(">H", ext)[0]
                elif length == 127:
                    ext = conn.recv(8)
                    length = struct.unpack(">Q", ext)[0]

                mask_key = conn.recv(4) if masked else b""
                payload = b""
                while len(payload) < length:
                    chunk = conn.recv(length - len(payload))
                    if not chunk:
                        break
                    payload += chunk

                if masked:
                    payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))

                if opcode == 1:
                    msg = payload.decode()
                    print(f"[{addr}] Message: {msg}")
                    reply = f"echo: {msg}"
                    reply_bytes = reply.encode()
                    frame = bytearray([0x81, len(reply_bytes)]) + reply_bytes
                    conn.sendall(frame)
                elif opcode == 8:
                    print(f"[{addr}] Close frame received")
                    conn.sendall(bytes([0x88, 0]))
                    break
            except Exception as e:
                print(f"[{addr}] Error: {e}")
                break
    else:
        response = (
            f"HTTP/1.1 200 OK\r\n"
            f"Content-Type: text/html\r\n"
            f"Content-Length: {len(HTML)}\r\n"
            f"Connection: close\r\n"
            f"\r\n"
        ).encode() + HTML
        conn.sendall(response)
        print(f"[{addr}] Served HTML")

    conn.close()
    print(f"[{addr}] Done")

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9091
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(5)
    print(f"Listening on port {port}")
    while True:
        conn, addr = srv.accept()
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

if __name__ == "__main__":
    main()

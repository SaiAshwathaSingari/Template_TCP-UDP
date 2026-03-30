/*
 * websocket.c - WebSocket protocol implementation over TCP
 *
 * Implements RFC 6455 WebSocket protocol handling:
 *  - HTTP/1.1 to WebSocket upgrade handshake
 *  - Frame encoding (server->client, unmasked per spec)
 *  - Frame decoding (client->server, masked per spec)
 *  - Partial TCP read buffering and frame reassembly
 *  - Control frame handling (ping/pong/close)
 *  - Static file serving for the web client
 */

#include "websocket.h"
#include "crypto.h"

int ws_parse_handshake(const char *request, int request_len,
                       char *ws_key_out, int key_max,
                       char *path_out, int path_max) {
    const char *p, *end;
    const char *key_header = "Sec-WebSocket-Key:";
    const char *upgrade_header = "Upgrade:";
    int has_upgrade = 0;

    (void)request_len;

    if (strncmp(request, "GET ", 4) != 0) return 0;
    p = request + 4;
    end = strchr(p, ' ');
    if (!end) return 0;
    {
        int plen = (int)(end - p);
        if (plen >= path_max) plen = path_max - 1;
        memcpy(path_out, p, plen);
        path_out[plen] = '\0';
    }

    p = strcasestr(request, upgrade_header);
    if (p) {
        p += strlen(upgrade_header);
        while (*p == ' ') p++;
        if (strncasecmp(p, "websocket", 9) == 0) has_upgrade = 1;
    }

    if (!has_upgrade) return 0;

    p = strcasestr(request, key_header);
    if (!p) return 0;
    p += strlen(key_header);
    while (*p == ' ') p++;
    end = strstr(p, "\r\n");
    if (!end) return 0;
    {
        int klen = (int)(end - p);
        if (klen >= key_max) klen = key_max - 1;
        memcpy(ws_key_out, p, klen);
        ws_key_out[klen] = '\0';
    }

    return 1;
}

int ws_build_handshake_response(const char *ws_key, char *response, int response_max) {
    char combined[256];
    uint8_t sha1_digest[20];
    char accept_key[64];

    snprintf(combined, sizeof(combined), "%s%s", ws_key, WS_MAGIC_STRING);
    sha1_hash((const uint8_t *)combined, strlen(combined), sha1_digest);
    base64_encode(sha1_digest, 20, accept_key, sizeof(accept_key));

    return snprintf(response, response_max,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Server: CodeCollab\r\n"
        "Cache-Control: no-cache\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);
}

int ws_decode_frame(const unsigned char *buffer, int buffer_len, int *buf_offset,
                    unsigned char *payload_out, int payload_max, int *opcode_out) {
    int offset = 0;
    int fin, mask_bit;
    uint64_t payload_len;
    unsigned char mask_key[4];
    uint64_t i;

    if (buffer_len < 2) return 0;

    fin = (buffer[0] >> 7) & 1;
    *opcode_out = buffer[0] & 0x0F;
    mask_bit = (buffer[1] >> 7) & 1;
    payload_len = buffer[1] & 0x7F;
    offset = 2;

    (void)fin;

    if (payload_len == 126) {
        if (buffer_len < 4) return 0;
        payload_len = ((uint64_t)buffer[2] << 8) | buffer[3];
        offset = 4;
    } else if (payload_len == 127) {
        if (buffer_len < 10) return 0;
        payload_len = 0;
        for (i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | buffer[2 + i];
        offset = 10;
    }

    if (mask_bit) {
        if (buffer_len < offset + 4) return 0;
        memcpy(mask_key, buffer + offset, 4);
        offset += 4;
    }

    if ((uint64_t)buffer_len < (uint64_t)offset + payload_len) return 0;
    if ((int64_t)payload_len > payload_max) return -1;

    memcpy(payload_out, buffer + offset, (size_t)payload_len);

    if (mask_bit) {
        for (i = 0; i < payload_len; i++)
            payload_out[i] ^= mask_key[i % 4];
    }

    *buf_offset = offset + (int)payload_len;
    return (int)payload_len;
}

int ws_send_text(socket_t socket_fd, const char *message, int message_len) {
    unsigned char header[10];
    int header_len = 0;
    int total_sent, sent;

    header[0] = 0x81;

    if (message_len < 126) {
        header[1] = (unsigned char)message_len;
        header_len = 2;
    } else if (message_len < 65536) {
        header[1] = 126;
        header[2] = (unsigned char)((message_len >> 8) & 0xFF);
        header[3] = (unsigned char)(message_len & 0xFF);
        header_len = 4;
    } else {
        header[1] = 127;
        memset(header + 2, 0, 4);
        header[6] = (unsigned char)((message_len >> 24) & 0xFF);
        header[7] = (unsigned char)((message_len >> 16) & 0xFF);
        header[8] = (unsigned char)((message_len >> 8) & 0xFF);
        header[9] = (unsigned char)(message_len & 0xFF);
        header_len = 10;
    }

    total_sent = 0;
    while (total_sent < header_len) {
        sent = send(socket_fd, (const char *)header + total_sent, header_len - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) return -1;
        total_sent += sent;
    }

    total_sent = 0;
    while (total_sent < message_len) {
        sent = send(socket_fd, message + total_sent, message_len - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) return -1;
        total_sent += sent;
    }

    return 0;
}

int ws_send_close(socket_t socket_fd, uint16_t status_code) {
    unsigned char frame[4];
    frame[0] = 0x88;
    frame[1] = 2;
    frame[2] = (status_code >> 8) & 0xFF;
    frame[3] = status_code & 0xFF;
    return (send(socket_fd, (const char *)frame, 4, MSG_NOSIGNAL) == 4) ? 0 : -1;
}

int ws_send_pong(socket_t socket_fd, const unsigned char *payload, int payload_len) {
    unsigned char header[2];
    header[0] = 0x8A;
    header[1] = (unsigned char)(payload_len & 0x7F);
    if (send(socket_fd, (const char *)header, 2, MSG_NOSIGNAL) != 2) return -1;
    if (payload_len > 0) {
        if (send(socket_fd, (const char *)payload, payload_len, MSG_NOSIGNAL) != payload_len) return -1;
    }
    return 0;
}

int ws_recv_buffered(socket_t socket_fd, unsigned char *buffer, int *buffer_len, int buffer_max) {
    int space = buffer_max - *buffer_len;
    int n;
    if (space <= 0) return -1;
    n = recv(socket_fd, (char *)buffer + *buffer_len, space, 0);
    if (n > 0) *buffer_len += n;
    return n;
}

int http_serve_file(socket_t socket_fd, const char *request, int request_len) {
    char path[512];
    const char *p;
    const char *end;
    char filepath[1024];
    FILE *fp;
    long fsize;
    char *body;
    char header[512];
    int hlen;
    const char *content_type = "text/html";

    (void)request_len;

    if (strncmp(request, "GET ", 4) != 0) return -1;
    p = request + 4;
    end = strchr(p, ' ');
    if (!end) return -1;
    {
        int plen = (int)(end - p);
        if (plen >= (int)sizeof(path)) plen = sizeof(path) - 1;
        memcpy(path, p, plen);
        path[plen] = '\0';
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
        snprintf(filepath, sizeof(filepath), "../client/index.html");
    else if (strcmp(path, "/style.css") == 0) {
        snprintf(filepath, sizeof(filepath), "../client/style.css");
        content_type = "text/css";
    } else if (strcmp(path, "/app.js") == 0) {
        snprintf(filepath, sizeof(filepath), "../client/app.js");
        content_type = "application/javascript";
    } else {
        const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
        send(socket_fd, resp, (int)strlen(resp), MSG_NOSIGNAL);
        return 0;
    }

    fp = fopen(filepath, "rb");
    if (!fp) {
        const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
        send(socket_fd, resp, (int)strlen(resp), MSG_NOSIGNAL);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    body = malloc(fsize + 1);
    if (!body) { fclose(fp); return -1; }
    (void)!fread(body, 1, fsize, fp);
    fclose(fp);

    hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "\r\n", content_type, fsize);

    send(socket_fd, header, hlen, MSG_NOSIGNAL);
    send(socket_fd, body, (int)fsize, MSG_NOSIGNAL);
    free(body);
    return 0;
}

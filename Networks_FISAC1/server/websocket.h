/*
 * websocket.h - WebSocket protocol interface
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "common.h"
#include <stdint.h>

int ws_parse_handshake(const char *request, int request_len,
                       char *ws_key_out, int key_max,
                       char *path_out, int path_max);

int ws_build_handshake_response(const char *ws_key, char *response, int response_max);

int ws_decode_frame(const unsigned char *buffer, int buffer_len, int *buf_offset,
                    unsigned char *payload_out, int payload_max, int *opcode_out);

int ws_send_text(socket_t socket_fd, const char *message, int message_len);
int ws_send_close(socket_t socket_fd, uint16_t status_code);
int ws_send_pong(socket_t socket_fd, const unsigned char *payload, int payload_len);
int ws_recv_buffered(socket_t socket_fd, unsigned char *buffer, int *buffer_len, int buffer_max);
int http_serve_file(socket_t socket_fd, const char *request, int request_len);

#endif /* WEBSOCKET_H */

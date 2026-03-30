/*
 * common.h - Shared definitions for the Collaborative Code Editor
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "platform.h"

#define SERVER_PORT         9090
#define MAX_CLIENTS         64
#define MAX_DOCUMENTS       32
#define BUFFER_SIZE         65536
#define MAX_FRAME_SIZE      131072
#define MAX_USERNAME        64
#define MAX_PASSWORD        128
#define SESSION_TOKEN_LEN   64
#define MAX_DOC_NAME        256
#define MAX_DOC_CONTENT     (1024 * 1024)
#define WS_MAGIC_STRING     "258EAFA5-E914-47DA-95CA-5AB4AA29BE5E"
#define HTTP_BUFFER_SIZE    8192
/* Defaults (can be overridden at runtime; see server.c) */
#define DB_DEFAULT_PATH     "collab_editor.db"
#define SESSION_TTL_DEFAULT 86400

#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT         0x1
#define WS_OPCODE_BINARY       0x2
#define WS_OPCODE_CLOSE        0x8
#define WS_OPCODE_PING         0x9
#define WS_OPCODE_PONG         0xA

#define MSG_AUTH_LOGIN      "auth_login"
#define MSG_AUTH_REGISTER   "auth_register"
#define MSG_AUTH_TOKEN      "auth_token"
#define MSG_AUTH_RESPONSE   "auth_response"
#define MSG_DOC_CREATE      "doc_create"
#define MSG_DOC_JOIN        "doc_join"
#define MSG_DOC_LEAVE       "doc_leave"
#define MSG_DOC_LIST        "doc_list"
#define MSG_DOC_EDIT        "doc_edit"
#define MSG_DOC_SYNC        "doc_sync"
#define MSG_DOC_CURSOR      "doc_cursor"
#define MSG_USER_LIST       "user_list"
#define MSG_ERROR           "error"
#define MSG_CHAT            "chat"

typedef struct client_s client_t;
typedef struct document_s document_t;

struct client_s {
    socket_t            socket_fd;
    int                 active;
    int                 authenticated;
    int                 user_id;
    char                username[MAX_USERNAME];
    char                session_token[SESSION_TOKEN_LEN + 1];
    int                 current_doc_id;
    pthread_t           thread;

    int                 ws_handshake_done;
    unsigned char       recv_buffer[BUFFER_SIZE];
    int                 recv_buffer_len;

    unsigned char       frame_buffer[MAX_FRAME_SIZE];
    int                 frame_buffer_len;

    struct sockaddr_in  addr;
};

struct document_s {
    int                 doc_id;
    char                name[MAX_DOC_NAME];
    char               *content;
    int                 content_len;
    int                 version;
    int                 owner_id;
    pthread_mutex_t     lock;
    socket_t            subscriber_fds[MAX_CLIENTS];
    int                 subscriber_count;
    int                 active;
};

extern client_t     g_clients[MAX_CLIENTS];
extern document_t   g_documents[MAX_DOCUMENTS];
extern pthread_mutex_t g_clients_lock;
extern pthread_mutex_t g_documents_lock;
extern volatile int g_running;

#define LOG_INFO(fmt, ...)  do { fprintf(stdout, "[INFO]  " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#define LOG_WARN(fmt, ...)  do { fprintf(stdout, "[WARN]  " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#define LOG_ERROR(fmt, ...) do { fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); fflush(stderr); } while(0)
#define LOG_DEBUG(fmt, ...) do { fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

#endif /* COMMON_H */

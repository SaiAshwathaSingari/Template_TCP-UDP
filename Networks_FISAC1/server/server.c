/*
 * server.c - Main entry point for the Collaborative Code Editor Server
 *
 * Architecture:
 *   Thread-per-connection concurrency model using POSIX pthreads.
 *   Each accepted TCP connection gets its own thread that handles:
 *     1) HTTP request / WebSocket upgrade handshake
 *     2) WebSocket frame I/O loop
 *     3) JSON message dispatch (auth, document ops, edits, chat)
 *
 * TCP Connection Lifecycle:
 *   1. Server creates a TCP socket, binds to port, listens.
 *   2. Client connects (TCP 3-way handshake: SYN, SYN-ACK, ACK).
 *   3. Client sends HTTP GET with "Upgrade: websocket" header.
 *   4. Server responds with HTTP 101 Switching Protocols.
 *   5. Connection is now a persistent, full-duplex WebSocket channel.
 *   6. Both sides exchange WebSocket frames until close.
 *   7. TCP connection teardown (FIN/ACK sequence).
 *
 * Socket Options Applied:
 *   SO_REUSEADDR  - Allows immediate server restart by reusing TIME_WAIT addresses
 *   SO_KEEPALIVE  - Detects dead clients via TCP keepalive probes
 *   TCP_NODELAY   - Disables Nagle's algorithm for low-latency small messages
 *   SO_SNDBUF     - Tuned send buffer for burst edit broadcasts
 *   SO_RCVBUF     - Tuned receive buffer for high-throughput input
 *
 * Concurrency Model Justification:
 *   Thread-per-connection was chosen because:
 *   - Simplifies per-client state management (each thread has its own stack)
 *   - Natural fit for blocking I/O on WebSocket frames
 *   - MAX_CLIENTS=64 keeps thread count manageable
 *   - Shared document state is protected by fine-grained mutexes
 *   - Alternative (epoll/select) would add complexity without benefit at this scale
 */

#include "common.h"
#include "crypto.h"
#include "database.h"
#include "websocket.h"

/* ===== Global State ===== */

client_t            g_clients[MAX_CLIENTS];
document_t          g_documents[MAX_DOCUMENTS];
pthread_mutex_t     g_clients_lock  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t     g_documents_lock = PTHREAD_MUTEX_INITIALIZER;
volatile int g_running = 1;

/* Forward declarations */
static void notify_all_clients_doc_list(void);

/* ===== JSON Utilities (minimal, no external dependencies) ===== */

static int json_escape_string(const char *input, char *output, int output_max) {
    int i = 0, o = 0;
    while (input[i] && o < output_max - 2) {
        switch (input[i]) {
            case '"':  if (o+2 >= output_max) goto done; output[o++]='\\'; output[o++]='"'; break;
            case '\\': if (o+2 >= output_max) goto done; output[o++]='\\'; output[o++]='\\'; break;
            case '\n': if (o+2 >= output_max) goto done; output[o++]='\\'; output[o++]='n'; break;
            case '\r': if (o+2 >= output_max) goto done; output[o++]='\\'; output[o++]='r'; break;
            case '\t': if (o+2 >= output_max) goto done; output[o++]='\\'; output[o++]='t'; break;
            default:   output[o++] = input[i]; break;
        }
        i++;
    }
done:
    output[o] = '\0';
    return o;
}

static int json_unescape_string(const char *input, char *output, int output_max) {
    int i = 0, o = 0;
    while (input[i] && o < output_max - 1) {
        if (input[i] == '\\' && input[i+1]) {
            i++;
            switch (input[i]) {
                case '"':  output[o++] = '"'; break;
                case '\\': output[o++] = '\\'; break;
                case 'n':  output[o++] = '\n'; break;
                case 'r':  output[o++] = '\r'; break;
                case 't':  output[o++] = '\t'; break;
                default:   output[o++] = input[i]; break;
            }
        } else {
            output[o++] = input[i];
        }
        i++;
    }
    output[o] = '\0';
    return o;
}

/*
 * Extract a JSON string value for "key". Handles \" \\ \/ \b \f \n \r \t escapes
 * so values from JSON.stringify (e.g. passwords with quotes) parse correctly.
 */
static int json_get_string(const char *json, const char *key, char *out, int out_max) {
    char search[256];
    const char *p;
    int o = 0;

    snprintf(search, sizeof(search), "\"%s\"", key);
    p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;

    while (*p) {
        if (*p == '"') {
            out[o] = '\0';
            return 1;
        }
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case '"':  out[o++] = '"'; break;
                case '\\': out[o++] = '\\'; break;
                case '/':  out[o++] = '/'; break;
                case 'b':  out[o++] = '\b'; break;
                case 'f':  out[o++] = '\f'; break;
                case 'n':  out[o++] = '\n'; break;
                case 'r':  out[o++] = '\r'; break;
                case 't':  out[o++] = '\t'; break;
                default:   out[o++] = *p; break;
            }
            p++;
        } else {
            out[o++] = *p++;
        }
        if (o >= out_max - 1) {
            out[out_max - 1] = '\0';
            return 0;
        }
    }
    return 0;
}

static int json_get_int(const char *json, const char *key, int *out) {
    char search[256];
    const char *p;

    snprintf(search, sizeof(search), "\"%s\"", key);
    p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    errno = 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p) return 0;              /* no digits */
    if (errno != 0) return 0;            /* overflow/underflow */
    if (v > INT_MAX || v < INT_MIN) return 0;
    *out = (int)v;
    return 1;
}

/* ===== Minimal HTTP helpers (fallback when WebSocket is blocked) ===== */

static int http_send_all(socket_t fd, const char *buf, int len) {
    int off = 0;
    while (off < len) {
        int n = send(fd, buf + off, len - off, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        off += n;
    }
    return 0;
}

static const char *http_find_header(const char *req, const char *name) {
    /* Case-insensitive search for "Name:" */
    const char *p = req;
    size_t nlen = strlen(name);
    while ((p = strcasestr(p, name)) != NULL) {
        if ((p == req || p[-1] == '\n') && strncasecmp(p, name, nlen) == 0 && p[nlen] == ':')
            return p + nlen + 1;
        p += nlen;
    }
    return NULL;
}

static int http_get_content_length(const char *req) {
    const char *p = http_find_header(req, "Content-Length");
    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    errno = 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p || errno != 0) return 0;
    if (v < 0 || v > INT_MAX) return 0;
    return (int)v;
}

static int http_send_json(socket_t fd, int status, const char *json_body) {
    char hdr[512];
    int blen = (int)strlen(json_body);
    const char *st = (status == 200) ? "200 OK" :
                     (status == 400) ? "400 Bad Request" :
                     (status == 401) ? "401 Unauthorized" :
                     (status == 404) ? "404 Not Found" : "500 Internal Server Error";
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n", st, blen);
    if (hlen <= 0 || hlen >= (int)sizeof(hdr)) return -1;
    if (http_send_all(fd, hdr, hlen) < 0) return -1;
    return http_send_all(fd, json_body, blen);
}

static int http_get_bearer_token(const char *req, char *token_out, int token_max) {
    const char *p = http_find_header(req, "Authorization");
    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "Bearer", 6) != 0) return 0;
    p += 6;
    while (*p == ' ' || *p == '\t') p++;
    const char *end = strstr(p, "\r\n");
    if (!end) return 0;
    int len = (int)(end - p);
    if (len <= 0) return 0;
    if (len >= token_max) len = token_max - 1;
    memcpy(token_out, p, len);
    token_out[len] = '\0';
    return 1;
}

static int http_require_auth(const char *req, int *user_id_out, char *username_out, int uname_max) {
    char token[SESSION_TOKEN_LEN + 1];
    if (!http_get_bearer_token(req, token, sizeof(token))) return 0;
    return db_validate_session(token, user_id_out, username_out, uname_max);
}

/* ===== Chat (HTTP polling fallback) ===== */
typedef struct {
    int id;
    int doc_id;
    char user[MAX_USERNAME];
    char message[512];
    time_t ts;
} chat_msg_t;

static pthread_mutex_t g_chat_lock = PTHREAD_MUTEX_INITIALIZER;
static chat_msg_t g_chat[512];
static int g_chat_count = 0;
static int g_chat_next_id = 1;

static void chat_add(int doc_id, const char *user, const char *message) {
    pthread_mutex_lock(&g_chat_lock);
    int idx = g_chat_count % (int)(sizeof(g_chat)/sizeof(g_chat[0]));
    g_chat[idx].id = g_chat_next_id++;
    g_chat[idx].doc_id = doc_id;
    snprintf(g_chat[idx].user, sizeof(g_chat[idx].user), "%s", user);
    snprintf(g_chat[idx].message, sizeof(g_chat[idx].message), "%s", message);
    g_chat[idx].ts = time(NULL);
    if (g_chat_count < (int)(sizeof(g_chat)/sizeof(g_chat[0]))) g_chat_count++;
    pthread_mutex_unlock(&g_chat_lock);
}

static int chat_list_json(int doc_id, int since_id, char *out, int out_max) {
    int off = 0;
    off += snprintf(out + off, out_max - off, "[");
    int first = 1;
    pthread_mutex_lock(&g_chat_lock);
    int max = (g_chat_count < (int)(sizeof(g_chat)/sizeof(g_chat[0]))) ? g_chat_count : (int)(sizeof(g_chat)/sizeof(g_chat[0]));
    for (int i = 0; i < max && off < out_max - 200; i++) {
        chat_msg_t *m = &g_chat[i];
        if (m->doc_id != doc_id) continue;
        if (m->id <= since_id) continue;
        if (!first) off += snprintf(out + off, out_max - off, ",");
        first = 0;
        char eu[128], em[1024];
        json_escape_string(m->user, eu, (int)sizeof(eu));
        json_escape_string(m->message, em, (int)sizeof(em));
        off += snprintf(out + off, out_max - off,
            "{\"id\":%d,\"user\":\"%s\",\"message\":\"%s\",\"ts\":%lld}",
            m->id, eu, em, (long long)m->ts);
    }
    pthread_mutex_unlock(&g_chat_lock);
    off += snprintf(out + off, out_max - off, "]");
    return off;
}

static int http_handle_api(socket_t fd, const char *req, int req_len) {
    /* Supports:
     * POST /api/register  {"username":"...","password":"..."}
     * POST /api/login     {"username":"...","password":"..."}
     * GET  /api/docs
     * POST /api/docs      {"name":"..."}
     * GET  /api/docs/<id>
     * PUT  /api/docs/<id> {"content":"...","version":<int>}
     * GET  /api/chat?doc_id=<id>&since=<id>
     * POST /api/chat      {"doc_id":<id>,"message":"..."}
     */
    const char *line_end = strstr(req, "\r\n");
    if (!line_end) return 0;

    char method[8] = {0};
    char path[128] = {0};
    if (sscanf(req, "%7s %127s", method, path) != 2) return 0;

    int is_docs = (strncmp(path, "/api/docs", 8) == 0);
    int is_chat = (strncmp(path, "/api/chat", 9) == 0);
    if (strcmp(path, "/api/register") != 0 && strcmp(path, "/api/login") != 0 && !is_docs && !is_chat)
        return 0;

    /* Docs/chat need auth; auth endpoints do not. */
    int authed_user_id = 0;
    char authed_username[MAX_USERNAME] = {0};
    if (is_docs || is_chat) {
        if (!http_require_auth(req, &authed_user_id, authed_username, sizeof(authed_username))) {
            http_send_json(fd, 401, "{\"ok\":false,\"error\":\"Unauthorized\"}");
            return 1;
        }
    }

    const char *hdr_end = strstr(req, "\r\n\r\n");
    if (!hdr_end) {
        http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Bad request\"}");
        return 1;
    }
    int header_bytes = (int)(hdr_end - req) + 4;
    int clen = http_get_content_length(req);
    const char *body = req + header_bytes;

    /* For methods with bodies, require full body. */
    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        if (clen <= 0 || header_bytes + clen > req_len) {
            http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Missing or incomplete body\"}");
            return 1;
        }
    }

    if (strcmp(path, "/api/register") == 0) {
        if (strcmp(method, "POST") != 0) { http_send_json(fd, 400, "{\"ok\":false,\"error\":\"POST required\"}"); return 1; }
        char username[MAX_USERNAME], password[MAX_PASSWORD];
        char salt[33], hash[65], token[SESSION_TOKEN_LEN + 1];
        if (!json_get_string(body, "username", username, sizeof(username)) ||
            !json_get_string(body, "password", password, sizeof(password))) {
            http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Missing fields\"}");
            return 1;
        }
        if (strlen(username) < 3 || strlen(password) < 4) {
            http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Username min 3, password min 4\"}");
            return 1;
        }
        int user_id;
        generate_salt(salt, 32);
        hash_password(password, salt, hash);
        user_id = db_create_user(username, hash, salt);
        if (user_id < 0) {
            http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Username already taken\"}");
            return 1;
        }
        generate_random_token(token, SESSION_TOKEN_LEN);
        db_create_session(user_id, token);
        {
            char out[512];
            snprintf(out, sizeof(out),
                "{\"ok\":true,\"user_id\":%d,\"username\":\"%s\",\"token\":\"%s\"}",
                user_id, username, token);
            http_send_json(fd, 200, out);
        }
        return 1;
    } else {
        if (strcmp(path, "/api/login") == 0) {
            if (strcmp(method, "POST") != 0) { http_send_json(fd, 400, "{\"ok\":false,\"error\":\"POST required\"}"); return 1; }
            char username[MAX_USERNAME], password[MAX_PASSWORD];
            char salt[33], hash[65], token[SESSION_TOKEN_LEN + 1];
            int user_id;
            char stored_hash[65];
            if (!json_get_string(body, "username", username, sizeof(username)) ||
                !json_get_string(body, "password", password, sizeof(password))) {
                http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Missing fields\"}");
                return 1;
            }
            if (!db_get_user(username, &user_id, stored_hash, sizeof(stored_hash), salt, sizeof(salt))) {
                http_send_json(fd, 401, "{\"ok\":false,\"error\":\"Invalid credentials\"}");
                return 1;
            }
            hash_password(password, salt, hash);
            if (strcmp(stored_hash, hash) != 0) {
                http_send_json(fd, 401, "{\"ok\":false,\"error\":\"Invalid credentials\"}");
                return 1;
            }
            generate_random_token(token, SESSION_TOKEN_LEN);
            db_create_session(user_id, token);
            {
                char out[512];
                snprintf(out, sizeof(out),
                    "{\"ok\":true,\"user_id\":%d,\"username\":\"%s\",\"token\":\"%s\"}",
                    user_id, username, token);
                http_send_json(fd, 200, out);
            }
            return 1;
        }

        /* Docs endpoints */
        if (strncmp(path, "/api/docs", 8) == 0) {
            if (strcmp(path, "/api/docs") == 0) {
                if (strcmp(method, "GET") == 0) {
                    char docs[8192];
                    int n = db_list_documents(docs, (int)sizeof(docs));
                    if (n < 0) { http_send_json(fd, 500, "{\"ok\":false,\"error\":\"DB error\"}"); return 1; }
                    char out[9000];
                    snprintf(out, sizeof(out), "{\"ok\":true,\"documents\":%s}", docs);
                    http_send_json(fd, 200, out);
                    return 1;
                }
                if (strcmp(method, "POST") == 0) {
                    char name[MAX_DOC_NAME];
                    if (!json_get_string(body, "name", name, sizeof(name))) {
                        http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Missing name\"}");
                        return 1;
                    }
                    int doc_id = db_create_document(name, authed_user_id);
                    if (doc_id < 0) { http_send_json(fd, 500, "{\"ok\":false,\"error\":\"Create failed\"}"); return 1; }
                    char out[512];
                    snprintf(out, sizeof(out), "{\"ok\":true,\"doc_id\":%d,\"name\":\"%s\"}", doc_id, name);
                    http_send_json(fd, 200, out);

                    /* Ensure other WS clients see it immediately. */
                    notify_all_clients_doc_list();
                    return 1;
                }
                http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Bad method\"}");
                return 1;
            } else {
                int doc_id = 0;
                if (sscanf(path, "/api/docs/%d", &doc_id) == 1 && doc_id > 0) {
                    if (strcmp(method, "GET") == 0) {
                        char name[MAX_DOC_NAME];
                        char *content = malloc(MAX_DOC_CONTENT);
                        int version = 0, owner = 0;
                        if (!content) { http_send_json(fd, 500, "{\"ok\":false,\"error\":\"OOM\"}"); return 1; }
                        int found = db_get_document(doc_id, name, sizeof(name), content, MAX_DOC_CONTENT, &version, &owner);
                        if (!found) { free(content); http_send_json(fd, 404, "{\"ok\":false,\"error\":\"Not found\"}"); return 1; }
                        char *esc = malloc(MAX_DOC_CONTENT * 2);
                        char out_hdr[512];
                        if (!esc) { free(content); http_send_json(fd, 500, "{\"ok\":false,\"error\":\"OOM\"}"); return 1; }
                        json_escape_string(content, esc, MAX_DOC_CONTENT * 2);
                        snprintf(out_hdr, sizeof(out_hdr), "{\"ok\":true,\"doc_id\":%d,\"name\":\"%s\",\"version\":%d,\"content\":\"", doc_id, name, version);
                        /* stream-ish build */
                        char *out = malloc(strlen(out_hdr) + strlen(esc) + 4);
                        if (!out) { free(content); free(esc); http_send_json(fd, 500, "{\"ok\":false,\"error\":\"OOM\"}"); return 1; }
                        sprintf(out, "%s%s\"}", out_hdr, esc);
                        http_send_json(fd, 200, out);
                        free(out); free(content); free(esc);
                        return 1;
                    }
                    if (strcmp(method, "PUT") == 0) {
                        char content[MAX_DOC_CONTENT];
                        int client_version = 0;

                        if (!json_get_string(body, "content", content, sizeof(content)) ||
                            !json_get_int(body, "version", &client_version)) {
                            http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Missing fields\"}");
                            return 1;
                        }

                        /* Optimistic concurrency: reject stale writes to keep clients synchronized. */
                        {
                            char name_tmp[MAX_DOC_NAME];
                            char *cur_content = malloc(MAX_DOC_CONTENT);
                            int cur_version = 0, owner = 0;
                            if (!cur_content) { http_send_json(fd, 500, "{\"ok\":false,\"error\":\"OOM\"}"); return 1; }

                            if (!db_get_document(doc_id, name_tmp, sizeof(name_tmp), cur_content, MAX_DOC_CONTENT, &cur_version, &owner)) {
                                free(cur_content);
                                http_send_json(fd, 404, "{\"ok\":false,\"error\":\"Not found\"}");
                                return 1;
                            }

                            if (client_version != cur_version) {
                                char *esc = malloc(MAX_DOC_CONTENT * 2);
                                if (!esc) { free(cur_content); http_send_json(fd, 500, "{\"ok\":false,\"error\":\"OOM\"}"); return 1; }
                                json_escape_string(cur_content, esc, MAX_DOC_CONTENT * 2);

                                /* 409 Conflict with latest content/version */
                                char *out = malloc(MAX_DOC_CONTENT * 2 + 512);
                                if (!out) { free(esc); free(cur_content); http_send_json(fd, 500, "{\"ok\":false,\"error\":\"OOM\"}"); return 1; }
                                snprintf(out, MAX_DOC_CONTENT * 2 + 512,
                                    "{\"ok\":false,\"error\":\"Version conflict\",\"code\":409,"
                                    "\"version\":%d,\"content\":\"%s\"}",
                                    cur_version, esc);
                                http_send_json(fd, 409, out);
                                free(out);
                                free(esc);
                                free(cur_content);
                                return 1;
                            }

                            free(cur_content);
                        }

                        /* Apply update */
                        int new_version = client_version + 1;
                        if (db_update_document(doc_id, content, new_version) < 0) {
                            http_send_json(fd, 500, "{\"ok\":false,\"error\":\"Update failed\"}");
                            return 1;
                        }

                        /* Save snapshot every 10 versions (same policy as WebSocket path) */
                        if (new_version % 10 == 0) {
                            db_save_version(doc_id, content, new_version, authed_user_id);
                        }

                        {
                            char out[128];
                            snprintf(out, sizeof(out), "{\"ok\":true,\"version\":%d}", new_version);
                            http_send_json(fd, 200, out);
                        }
                        return 1;
                    }
                    if (strcmp(method, "DELETE") == 0) {
                        int owner = 0;
                        if (!db_get_document_owner(doc_id, &owner)) {
                            http_send_json(fd, 404, "{\"ok\":false,\"error\":\"Not found\"}");
                            return 1;
                        }
                        if (owner != authed_user_id) {
                            http_send_json(fd, 401, "{\"ok\":false,\"error\":\"Only owner can delete\"}");
                            return 1;
                        }
                        if (db_delete_document(doc_id) < 0) {
                            http_send_json(fd, 500, "{\"ok\":false,\"error\":\"Delete failed\"}");
                            return 1;
                        }
                        http_send_json(fd, 200, "{\"ok\":true}");

                        /* Ensure other WS clients remove it immediately. */
                        notify_all_clients_doc_list();
                        return 1;
                    }
                    http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Bad method\"}");
                    return 1;
                }
            }
            http_send_json(fd, 404, "{\"ok\":false,\"error\":\"Not found\"}");
            return 1;
        }

        /* Chat endpoints */
        if (strncmp(path, "/api/chat", 9) == 0) {
            if (strcmp(method, "GET") == 0) {
                /* parse query */
                int doc_id = 0, since = 0;
                const char *q = strchr(path, '?');
                if (q) {
                    const char *d = strstr(q, "doc_id=");
                    const char *s = strstr(q, "since=");
                    if (d) {
                        errno = 0;
                        char *end = NULL;
                        long v = strtol(d + 7, &end, 10);
                        if (end != (d + 7) && errno == 0 && v > 0 && v <= INT_MAX) doc_id = (int)v;
                    }
                    if (s) {
                        errno = 0;
                        char *end = NULL;
                        long v = strtol(s + 6, &end, 10);
                        if (end != (s + 6) && errno == 0 && v >= 0 && v <= INT_MAX) since = (int)v;
                    }
                }
                if (doc_id <= 0) { http_send_json(fd, 400, "{\"ok\":false,\"error\":\"doc_id required\"}"); return 1; }
                char msgs[8192];
                chat_list_json(doc_id, since, msgs, (int)sizeof(msgs));
                char out[9000];
                snprintf(out, sizeof(out), "{\"ok\":true,\"messages\":%s}", msgs);
                http_send_json(fd, 200, out);
                return 1;
            }
            if (strcmp(method, "POST") == 0) {
                int doc_id = 0;
                char message[512];
                if (!json_get_int(body, "doc_id", &doc_id) || !json_get_string(body, "message", message, sizeof(message))) {
                    http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Missing fields\"}");
                    return 1;
                }
                chat_add(doc_id, authed_username, message);
                http_send_json(fd, 200, "{\"ok\":true}");
                return 1;
            }
            http_send_json(fd, 400, "{\"ok\":false,\"error\":\"Bad method\"}");
            return 1;
        }

        http_send_json(fd, 404, "{\"ok\":false,\"error\":\"Not found\"}");
        return 1;
    }
}

/* ===== Notifications ===== */

static void notify_all_clients_doc_list(void) {
    char *docs = malloc(BUFFER_SIZE);
    char *msg = NULL;
    if (!docs) return;

    int docs_len = db_list_documents(docs, BUFFER_SIZE);
    if (docs_len < 0) { free(docs); return; }

    size_t need = (size_t)docs_len + 64;
    msg = malloc(need);
    if (!msg) { free(docs); return; }

    int len = snprintf(msg, need, "{\"type\":\"doc_list\",\"documents\":%s}", docs);
    if (len <= 0) { free(msg); free(docs); return; }

    pthread_mutex_lock(&g_clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i].active) continue;
        if (!g_clients[i].authenticated) continue;
        if (!g_clients[i].ws_handshake_done) continue; /* only WS clients */
        (void)ws_send_text(g_clients[i].socket_fd, msg, len);
    }
    pthread_mutex_unlock(&g_clients_lock);

    free(msg);
    free(docs);
}

/* ===== Document Management ===== */

static document_t *find_document(int doc_id) {
    int i;
    for (i = 0; i < MAX_DOCUMENTS; i++) {
        if (g_documents[i].active && g_documents[i].doc_id == doc_id)
            return &g_documents[i];
    }
    return NULL;
}

static document_t *load_document(int doc_id) {
    document_t *doc;
    int i;
    char name[MAX_DOC_NAME];
    char *content;
    int version, owner;

    pthread_mutex_lock(&g_documents_lock);

    doc = find_document(doc_id);
    if (doc) {
        pthread_mutex_unlock(&g_documents_lock);
        return doc;
    }

    content = malloc(MAX_DOC_CONTENT);
    if (!content) { pthread_mutex_unlock(&g_documents_lock); return NULL; }

    if (!db_get_document(doc_id, name, sizeof(name), content, MAX_DOC_CONTENT, &version, &owner)) {
        free(content);
        pthread_mutex_unlock(&g_documents_lock);
        return NULL;
    }

    for (i = 0; i < MAX_DOCUMENTS; i++) {
        if (!g_documents[i].active) {
            doc = &g_documents[i];
            doc->doc_id = doc_id;
            strncpy(doc->name, name, MAX_DOC_NAME - 1);
            doc->content = content;
            doc->content_len = strlen(content);
            doc->version = version;
            doc->owner_id = owner;
            doc->subscriber_count = 0;
            doc->active = 1;
            pthread_mutex_init(&doc->lock, NULL);
            break;
        }
    }

    pthread_mutex_unlock(&g_documents_lock);
    return doc;
}

static void doc_add_subscriber(document_t *doc, int fd) {
    int i;
    pthread_mutex_lock(&doc->lock);
    for (i = 0; i < doc->subscriber_count; i++) {
        if (doc->subscriber_fds[i] == fd) {
            pthread_mutex_unlock(&doc->lock);
            return;
        }
    }
    if (doc->subscriber_count < MAX_CLIENTS)
        doc->subscriber_fds[doc->subscriber_count++] = fd;
    pthread_mutex_unlock(&doc->lock);
}

static void doc_remove_subscriber(document_t *doc, int fd) {
    int i;
    pthread_mutex_lock(&doc->lock);
    for (i = 0; i < doc->subscriber_count; i++) {
        if (doc->subscriber_fds[i] == fd) {
            doc->subscriber_fds[i] = doc->subscriber_fds[--doc->subscriber_count];
            break;
        }
    }
    pthread_mutex_unlock(&doc->lock);
}

static void broadcast_to_doc(document_t *doc, const char *msg, int msg_len, int exclude_fd) {
    int i;
    pthread_mutex_lock(&doc->lock);
    for (i = 0; i < doc->subscriber_count; i++) {
        if (doc->subscriber_fds[i] != exclude_fd)
            ws_send_text(doc->subscriber_fds[i], msg, msg_len);
    }
    pthread_mutex_unlock(&doc->lock);
}

static void send_user_list(document_t *doc) {
    char msg[4096];
    int offset = 0, i, j;
    int first = 1;

    offset += snprintf(msg + offset, sizeof(msg) - offset,
        "{\"type\":\"user_list\",\"doc_id\":%d,\"users\":[", doc->doc_id);

    pthread_mutex_lock(&doc->lock);
    pthread_mutex_lock(&g_clients_lock);

    for (i = 0; i < doc->subscriber_count; i++) {
        for (j = 0; j < MAX_CLIENTS; j++) {
            if (g_clients[j].active && g_clients[j].socket_fd == doc->subscriber_fds[i]) {
                if (!first) offset += snprintf(msg + offset, sizeof(msg) - offset, ",");
                first = 0;
                offset += snprintf(msg + offset, sizeof(msg) - offset,
                    "{\"id\":%d,\"username\":\"%s\"}", g_clients[j].user_id, g_clients[j].username);
                break;
            }
        }
    }

    pthread_mutex_unlock(&g_clients_lock);
    pthread_mutex_unlock(&doc->lock);

    offset += snprintf(msg + offset, sizeof(msg) - offset, "]}");
    broadcast_to_doc(doc, msg, offset, -1);
}

/* ===== Message Handlers ===== */

static void handle_auth_register(client_t *client, const char *json) {
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    char salt[33], hash[65], token[SESSION_TOKEN_LEN + 1];
    char response[512];
    int user_id, len;

    if (!json_get_string(json, "username", username, sizeof(username)) ||
        !json_get_string(json, "password", password, sizeof(password))) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"auth_response\",\"success\":false,\"error\":\"Missing fields\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    if (strlen(username) < 3 || strlen(password) < 4) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"auth_response\",\"success\":false,\"error\":\"Username min 3, password min 4 chars\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    generate_salt(salt, 32);
    hash_password(password, salt, hash);

    user_id = db_create_user(username, hash, salt);
    if (user_id < 0) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"auth_response\",\"success\":false,\"error\":\"Username already taken\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    generate_random_token(token, SESSION_TOKEN_LEN);
    db_create_session(user_id, token);

    client->authenticated = 1;
    client->user_id = user_id;
    strncpy(client->username, username, MAX_USERNAME - 1);
    client->username[MAX_USERNAME - 1] = '\0';
    strncpy(client->session_token, token, SESSION_TOKEN_LEN);
    client->session_token[SESSION_TOKEN_LEN] = '\0';

    len = snprintf(response, sizeof(response),
        "{\"type\":\"auth_response\",\"success\":true,\"user_id\":%d,"
        "\"username\":\"%s\",\"token\":\"%s\"}", user_id, username, token);
    ws_send_text(client->socket_fd, response, len);
    LOG_INFO("User registered: %s (id=%d)", username, user_id);
}

static void handle_auth_login(client_t *client, const char *json) {
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    char stored_hash[65], salt[33], computed_hash[65], token[SESSION_TOKEN_LEN + 1];
    char response[512];
    int user_id, len;

    if (!json_get_string(json, "username", username, sizeof(username)) ||
        !json_get_string(json, "password", password, sizeof(password))) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"auth_response\",\"success\":false,\"error\":\"Missing fields\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    if (!db_get_user(username, &user_id, stored_hash, sizeof(stored_hash),
                     salt, sizeof(salt))) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"auth_response\",\"success\":false,\"error\":\"Invalid credentials\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    hash_password(password, salt, computed_hash);
    if (strcmp(stored_hash, computed_hash) != 0) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"auth_response\",\"success\":false,\"error\":\"Invalid credentials\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    generate_random_token(token, SESSION_TOKEN_LEN);
    db_create_session(user_id, token);

    client->authenticated = 1;
    client->user_id = user_id;
    strncpy(client->username, username, MAX_USERNAME - 1);
    client->username[MAX_USERNAME - 1] = '\0';
    strncpy(client->session_token, token, SESSION_TOKEN_LEN);
    client->session_token[SESSION_TOKEN_LEN] = '\0';

    len = snprintf(response, sizeof(response),
        "{\"type\":\"auth_response\",\"success\":true,\"user_id\":%d,"
        "\"username\":\"%s\",\"token\":\"%s\"}", user_id, username, token);
    ws_send_text(client->socket_fd, response, len);
    LOG_INFO("User logged in: %s (id=%d)", username, user_id);
}

static void handle_doc_create(client_t *client, const char *json) {
    char name[MAX_DOC_NAME], response[512];
    int doc_id, len;

    if (!json_get_string(json, "name", name, sizeof(name))) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"error\",\"message\":\"Missing document name\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    doc_id = db_create_document(name, client->user_id);
    if (doc_id < 0) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"error\",\"message\":\"Failed to create document\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    len = snprintf(response, sizeof(response),
        "{\"type\":\"doc_created\",\"doc_id\":%d,\"name\":\"%s\"}", doc_id, name);
    ws_send_text(client->socket_fd, response, len);
    LOG_INFO("Document created: '%s' (id=%d) by %s", name, doc_id, client->username);

    /* Push updated list to all other connected clients (no refresh needed). */
    notify_all_clients_doc_list();
}

static void handle_doc_list(client_t *client) {
    char *json_buf = malloc(BUFFER_SIZE);
    char *response;
    int list_len, len;

    if (!json_buf) return;
    response = malloc(BUFFER_SIZE + 256);
    if (!response) { free(json_buf); return; }

    list_len = db_list_documents(json_buf, BUFFER_SIZE);
    if (list_len < 0) {
        len = snprintf(response, BUFFER_SIZE + 256,
            "{\"type\":\"error\",\"message\":\"Failed to list documents\"}");
    } else {
        len = snprintf(response, BUFFER_SIZE + 256,
            "{\"type\":\"doc_list\",\"documents\":%s}", json_buf);
    }

    ws_send_text(client->socket_fd, response, len);
    free(json_buf);
    free(response);
}

static void handle_doc_join(client_t *client, const char *json) {
    int doc_id, len;
    char response[BUFFER_SIZE];
    document_t *doc;

    if (!json_get_int(json, "doc_id", &doc_id)) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"error\",\"message\":\"Missing doc_id\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    /* Leave current document if subscribed */
    if (client->current_doc_id > 0) {
        doc = find_document(client->current_doc_id);
        if (doc) {
            doc_remove_subscriber(doc, client->socket_fd);
            send_user_list(doc);
        }
    }

    doc = load_document(doc_id);
    if (!doc) {
        len = snprintf(response, sizeof(response),
            "{\"type\":\"error\",\"message\":\"Document not found\"}");
        ws_send_text(client->socket_fd, response, len);
        return;
    }

    client->current_doc_id = doc_id;
    doc_add_subscriber(doc, client->socket_fd);

    /* Send current document content to joining client (with escaped JSON) */
    {
        char *escaped = malloc(MAX_DOC_CONTENT * 2);
        char *big_response = malloc(MAX_DOC_CONTENT * 2 + 512);
        if (escaped && big_response) {
            pthread_mutex_lock(&doc->lock);
            json_escape_string(doc->content ? doc->content : "", escaped, MAX_DOC_CONTENT * 2);
            len = snprintf(big_response, MAX_DOC_CONTENT * 2 + 512,
                "{\"type\":\"doc_sync\",\"doc_id\":%d,\"name\":\"%s\","
                "\"content\":\"%s\",\"version\":%d}",
                doc->doc_id, doc->name, escaped, doc->version);
            pthread_mutex_unlock(&doc->lock);
            ws_send_text(client->socket_fd, big_response, len);
        } else {
            pthread_mutex_lock(&doc->lock);
            pthread_mutex_unlock(&doc->lock);
        }
        free(escaped);
        free(big_response);
    }
    send_user_list(doc);
    LOG_INFO("User %s joined document %d (%s)", client->username, doc_id, doc->name);
}

static void handle_doc_edit(client_t *client, const char *json) {
    int doc_id, version, cursor_pos = 0;
    char *escaped_content;
    char *raw_content;
    document_t *doc;
    char *broadcast;
    int len;

    if (!json_get_int(json, "doc_id", &doc_id)) return;

    escaped_content = malloc(MAX_DOC_CONTENT);
    if (!escaped_content) return;

    if (!json_get_string(json, "content", escaped_content, MAX_DOC_CONTENT)) {
        free(escaped_content);
        return;
    }

    /* Unescape the JSON-escaped content from the client */
    raw_content = malloc(MAX_DOC_CONTENT);
    if (!raw_content) { free(escaped_content); return; }
    json_unescape_string(escaped_content, raw_content, MAX_DOC_CONTENT);
    free(escaped_content);

    json_get_int(json, "cursor", &cursor_pos);

    doc = find_document(doc_id);
    if (!doc) { free(raw_content); return; }

    pthread_mutex_lock(&doc->lock);

    free(doc->content);
    doc->content = raw_content;
    doc->content_len = strlen(raw_content);
    doc->version++;
    version = doc->version;

    pthread_mutex_unlock(&doc->lock);

    /* Persist to database (async-safe: db has its own lock) */
    db_update_document(doc_id, raw_content, version);

    /* Save version snapshot every 10 edits */
    if (version % 10 == 0)
        db_save_version(doc_id, raw_content, version, client->user_id);

    /* Re-escape for broadcasting to other clients */
    broadcast = malloc(MAX_DOC_CONTENT * 2 + 512);
    if (broadcast) {
        char *esc = malloc(MAX_DOC_CONTENT * 2);
        if (esc) {
            json_escape_string(raw_content, esc, MAX_DOC_CONTENT * 2);
            len = snprintf(broadcast, MAX_DOC_CONTENT * 2 + 512,
                "{\"type\":\"doc_edit\",\"doc_id\":%d,\"content\":\"%s\","
                "\"version\":%d,\"editor\":\"%s\",\"cursor\":%d}",
                doc_id, esc, version, client->username, cursor_pos);
            broadcast_to_doc(doc, broadcast, len, client->socket_fd);
            free(esc);
        }
        free(broadcast);
    }
}

static void handle_doc_cursor(client_t *client, const char *json) {
    int doc_id, cursor_pos;
    document_t *doc;
    char msg[256];
    int len;

    if (!json_get_int(json, "doc_id", &doc_id) ||
        !json_get_int(json, "cursor", &cursor_pos)) return;

    doc = find_document(doc_id);
    if (!doc) return;

    len = snprintf(msg, sizeof(msg),
        "{\"type\":\"doc_cursor\",\"doc_id\":%d,\"user\":\"%s\",\"cursor\":%d}",
        doc_id, client->username, cursor_pos);
    broadcast_to_doc(doc, msg, len, client->socket_fd);
}

static void handle_chat(client_t *client, const char *json) {
    char message[1024], broadcast_msg[2048];
    int doc_id, len;
    document_t *doc;

    if (!json_get_string(json, "message", message, sizeof(message)) ||
        !json_get_int(json, "doc_id", &doc_id)) return;

    doc = find_document(doc_id);
    if (!doc) return;

    len = snprintf(broadcast_msg, sizeof(broadcast_msg),
        "{\"type\":\"chat\",\"doc_id\":%d,\"user\":\"%s\",\"message\":\"%s\"}",
        doc_id, client->username, message);
    broadcast_to_doc(doc, broadcast_msg, len, -1);
}

static void handle_doc_leave(client_t *client) {
    document_t *doc;
    if (client->current_doc_id <= 0) return;

    doc = find_document(client->current_doc_id);
    if (doc) {
        doc_remove_subscriber(doc, client->socket_fd);
        send_user_list(doc);
    }
    client->current_doc_id = 0;
}

/* ===== Message Dispatch ===== */

static void dispatch_message(client_t *client, const char *payload, int payload_len) {
    char type[64];
    char msg_copy[BUFFER_SIZE];

    if (payload_len >= BUFFER_SIZE) return;
    memcpy(msg_copy, payload, payload_len);
    msg_copy[payload_len] = '\0';

    if (!json_get_string(msg_copy, "type", type, sizeof(type))) {
        LOG_WARN("Message without type from fd=%lld", (long long)client->socket_fd);
        return;
    }

    /* Authentication messages don't require prior auth */
    if (strcmp(type, MSG_AUTH_REGISTER) == 0) {
        handle_auth_register(client, msg_copy);
        return;
    }
    if (strcmp(type, MSG_AUTH_LOGIN) == 0) {
        handle_auth_login(client, msg_copy);
        return;
    }

    /* All other messages require authentication */
    if (!client->authenticated) {
        char err[128];
        int len = snprintf(err, sizeof(err),
            "{\"type\":\"error\",\"message\":\"Not authenticated\"}");
        ws_send_text(client->socket_fd, err, len);
        return;
    }

    if (strcmp(type, MSG_DOC_CREATE) == 0)       handle_doc_create(client, msg_copy);
    else if (strcmp(type, MSG_DOC_LIST) == 0)     handle_doc_list(client);
    else if (strcmp(type, MSG_DOC_JOIN) == 0)     handle_doc_join(client, msg_copy);
    else if (strcmp(type, MSG_DOC_LEAVE) == 0)    handle_doc_leave(client);
    else if (strcmp(type, MSG_DOC_EDIT) == 0)     handle_doc_edit(client, msg_copy);
    else if (strcmp(type, MSG_DOC_CURSOR) == 0)   handle_doc_cursor(client, msg_copy);
    else if (strcmp(type, MSG_CHAT) == 0)         handle_chat(client, msg_copy);
    else LOG_WARN("Unknown message type: %s", type);
}

/* ===== Client Thread ===== */

static void cleanup_client(client_t *client) {
    if (client->current_doc_id > 0) {
        document_t *doc = find_document(client->current_doc_id);
        if (doc) {
            doc_remove_subscriber(doc, client->socket_fd);
            send_user_list(doc);
        }
    }

    /*
     * IMPORTANT:
     * Do NOT delete sessions on socket disconnect.
     * Browsers (and some environments) may drop WebSockets frequently, and the
     * HTTP API relies on the session token surviving refreshes/reconnects.
     * Sessions are cleaned up by db_cleanup_expired_sessions() on startup.
     */

    platform_close(client->socket_fd);

    pthread_mutex_lock(&g_clients_lock);
    client->active = 0;
    client->socket_fd = SOCKET_INVALID;
    client->authenticated = 0;
    client->current_doc_id = 0;
    client->ws_handshake_done = 0;
    client->recv_buffer_len = 0;
    client->frame_buffer_len = 0;
    client->session_token[0] = '\0';
    client->username[0] = '\0';
    pthread_mutex_unlock(&g_clients_lock);

    LOG_INFO("Client disconnected and cleaned up");
}

static void *client_thread(void *arg) {
    client_t *client = (client_t *)arg;
    unsigned char *payload;
    int n, opcode, consumed;

    payload = malloc(MAX_FRAME_SIZE);
    if (!payload) { cleanup_client(client); return NULL; }

    LOG_INFO("Client connected from %s:%d (fd=%lld)",
        inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port),
        (long long)client->socket_fd);

    while (g_running && client->active) {
        n = ws_recv_buffered(client->socket_fd, client->recv_buffer,
                             &client->recv_buffer_len, BUFFER_SIZE);
        if (n <= 0) {
            if (n == 0) LOG_INFO("Client fd=%lld disconnected (recv=0, ws_done=%d, buf_len=%d)",
                (long long)client->socket_fd, client->ws_handshake_done, client->recv_buffer_len);
            else LOG_WARN("recv error fd=%lld (errno=%d)",
                (long long)client->socket_fd, platform_errno());
            break;
        }
        LOG_DEBUG("fd=%lld recv %d bytes (total buf=%d, ws_done=%d)",
            (long long)client->socket_fd, n, client->recv_buffer_len, client->ws_handshake_done);

        /* Phase 1: HTTP → WebSocket handshake */
        if (!client->ws_handshake_done) {
            char *header_end = strstr((char *)client->recv_buffer, "\r\n\r\n");
            if (!header_end) continue;

            char ws_key[128] = {0}, path[256] = {0};
            char response[1024];

            LOG_DEBUG("fd=%lld HTTP request (%d bytes):\n%.*s",
                (long long)client->socket_fd, client->recv_buffer_len,
                client->recv_buffer_len, client->recv_buffer);

            if (ws_parse_handshake((char *)client->recv_buffer, client->recv_buffer_len,
                                   ws_key, sizeof(ws_key), path, sizeof(path))) {
                int rlen = ws_build_handshake_response(ws_key, response, sizeof(response));
                LOG_DEBUG("WS key=[%s] response_len=%d", ws_key, rlen);
                LOG_DEBUG("WS response:\n%.*s", rlen, response);
                int sent = send(client->socket_fd, response, rlen, MSG_NOSIGNAL);
                LOG_DEBUG("WS handshake send() returned %d (expected %d)", sent, rlen);
                client->ws_handshake_done = 1;
                LOG_INFO("WebSocket handshake complete for fd=%lld (path=%s)", (long long)client->socket_fd, path);

                /* Remove consumed HTTP data from buffer */
                int http_len = (int)(header_end - (char *)client->recv_buffer) + 4;
                int remaining = client->recv_buffer_len - http_len;
                if (remaining > 0)
                    memmove(client->recv_buffer, client->recv_buffer + http_len, remaining);
                client->recv_buffer_len = remaining;
            } else {
                /* Not a WebSocket request: ensure full HTTP body (if any) is buffered */
                {
                    int header_bytes = (int)(header_end - (char *)client->recv_buffer) + 4;
                    int clen = http_get_content_length((char *)client->recv_buffer);
                    if (clen > 0 && client->recv_buffer_len < header_bytes + clen) {
                        /* Need more data (body) */
                        continue;
                    }
                }

                /* Try API, else serve static files */
                if (!http_handle_api(client->socket_fd, (char *)client->recv_buffer, client->recv_buffer_len)) {
                    http_serve_file(client->socket_fd, (char *)client->recv_buffer, client->recv_buffer_len);
                }
                break;
            }
            continue;
        }

        /* Phase 2: Process WebSocket frames */
        while (client->recv_buffer_len > 0) {
            consumed = 0;
            int plen = ws_decode_frame(client->recv_buffer, client->recv_buffer_len,
                                       &consumed, payload, MAX_FRAME_SIZE, &opcode);
            if (plen == 0) break;   /* Incomplete frame, wait for more data */
            if (plen < 0) {
                LOG_WARN("Frame decode error fd=%lld", (long long)client->socket_fd);
                goto disconnect;
            }

            /* Shift buffer past consumed frame */
            int remaining = client->recv_buffer_len - consumed;
            if (remaining > 0)
                memmove(client->recv_buffer, client->recv_buffer + consumed, remaining);
            client->recv_buffer_len = remaining;

            switch (opcode) {
                case WS_OPCODE_TEXT:
                    payload[plen] = '\0';
                    dispatch_message(client, (char *)payload, plen);
                    break;
                case WS_OPCODE_PING:
                    ws_send_pong(client->socket_fd, payload, plen);
                    break;
                case WS_OPCODE_CLOSE:
                    ws_send_close(client->socket_fd, 1000);
                    goto disconnect;
                case WS_OPCODE_PONG:
                    break;
                default:
                    LOG_WARN("Unhandled opcode 0x%X from fd=%lld", opcode, (long long)client->socket_fd);
            }
        }
    }

disconnect:
    free(payload);
    cleanup_client(client);
    return NULL;
}

/* ===== Socket Option Configuration ===== */

static void configure_server_socket(socket_t server_fd) {
    int opt = 1;
    int sndbuf = 65536;
    int rcvbuf = 65536;

    /*
     * SO_REUSEADDR: Permits the server to bind to a port still in TIME_WAIT state
     * from a previous instance. Essential for rapid server restarts during development
     * and production recovery. Without this, bind() would fail with EADDRINUSE for
     * up to 2*MSL (typically 60 seconds) after a server restart.
     */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0)
        LOG_WARN("SO_REUSEADDR failed: %s", strerror(errno));
    else
        LOG_INFO("SO_REUSEADDR enabled - allows immediate server restart");

    /*
     * SO_KEEPALIVE: Enables TCP keepalive probes to detect dead connections.
     * If a client disconnects without sending a FIN (e.g., network failure),
     * keepalive probes will eventually detect the dead connection and trigger
     * an error on the socket, allowing cleanup of stale client resources.
     */
    if (setsockopt(server_fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&opt, sizeof(opt)) < 0)
        LOG_WARN("SO_KEEPALIVE failed: %s", strerror(errno));
    else
        LOG_INFO("SO_KEEPALIVE enabled - detects dead client connections");

    /*
     * SO_SNDBUF: Sets the kernel send buffer size. A larger send buffer allows
     * the server to queue more outgoing data before blocking, which is beneficial
     * when broadcasting edits to many clients simultaneously. 64KB provides
     * headroom for burst transmissions.
     */
    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, sizeof(sndbuf)) < 0)
        LOG_WARN("SO_SNDBUF failed: %s", strerror(errno));
    else
        LOG_INFO("SO_SNDBUF set to %d bytes - optimized for burst broadcasts", sndbuf);

    /*
     * SO_RCVBUF: Sets the kernel receive buffer size. A larger receive buffer
     * prevents data loss when the server thread is busy processing and can't
     * call recv() immediately. Important for bursty typing input.
     */
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(rcvbuf)) < 0)
        LOG_WARN("SO_RCVBUF failed: %s", strerror(errno));
    else
        LOG_INFO("SO_RCVBUF set to %d bytes - prevents input data loss", rcvbuf);
}

static void configure_client_socket(socket_t client_fd) {
    int opt = 1;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt)) < 0)
        LOG_WARN("TCP_NODELAY failed on fd=%lld", (long long)client_fd);
    setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&opt, sizeof(opt));
}

/* ===== Signal Handling ===== */

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    LOG_INFO("Shutdown signal received");
}

static int parse_port(const char *s, int *port_out) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (errno != 0) return 0;
    if (v < 1 || v > 65535) return 0;
    *port_out = (int)v;
    return 1;
}

static int parse_ttl(const char *s, int *ttl_out) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (errno != 0) return 0;
    if (v < 60 || v > 365L * 24L * 3600L) return 0; /* 1 min .. 1 year */
    *ttl_out = (int)v;
    return 1;
}

/* ===== Main ===== */

int main(int argc, char *argv[]) {
    socket_t server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int port = SERVER_PORT;
    const char *db_path = DB_DEFAULT_PATH;
    int session_ttl = SESSION_TTL_DEFAULT;
    int i;

    /* Disable stdout/stderr buffering for reliable log output */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /*
     * Runtime configuration (no hard-coded operational values):
     *   collab_server [port] [db_path] [session_ttl_seconds]
     * Environment overrides:
     *   CODECOLLAB_PORT, CODECOLLAB_DB, CODECOLLAB_SESSION_TTL
     */
    {
        const char *ep = getenv("CODECOLLAB_PORT");
        if (ep && *ep) {
            int ptmp = 0;
            if (parse_port(ep, &ptmp)) port = ptmp;
            else LOG_WARN("Invalid CODECOLLAB_PORT='%s' (using %d)", ep, port);
        }
        const char *edb = getenv("CODECOLLAB_DB");
        if (edb && *edb) db_path = edb;
        const char *ettl = getenv("CODECOLLAB_SESSION_TTL");
        if (ettl && *ettl) {
            int ttmp = 0;
            if (parse_ttl(ettl, &ttmp)) session_ttl = ttmp;
            else LOG_WARN("Invalid CODECOLLAB_SESSION_TTL='%s' (using %d)", ettl, session_ttl);
        }
    }

    if (argc > 1) {
        int ptmp = 0;
        if (parse_port(argv[1], &ptmp)) port = ptmp;
        else LOG_WARN("Invalid port arg '%s' (using %d)", argv[1], port);
    }
    if (argc > 2 && argv[2] && argv[2][0]) db_path = argv[2];
    if (argc > 3) {
        int ttmp = 0;
        if (parse_ttl(argv[3], &ttmp)) session_ttl = ttmp;
        else LOG_WARN("Invalid session_ttl arg '%s' (using %d)", argv[3], session_ttl);
    }

    if (platform_init() != 0) {
        LOG_ERROR("Platform initialization failed");
        return 1;
    }

    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    memset(g_clients, 0, sizeof(g_clients));
    memset(g_documents, 0, sizeof(g_documents));
    for (i = 0; i < MAX_CLIENTS; i++) g_clients[i].socket_fd = SOCKET_INVALID;

    if (db_init(db_path) < 0) {
        LOG_ERROR("Database initialization failed");
        return 1;
    }

    db_cleanup_expired_sessions(session_ttl);

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == SOCKET_INVALID) {
        LOG_ERROR("socket() failed");
        return 1;
    }

    configure_server_socket(server_fd);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("bind() failed on port %d", port);
        platform_close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        LOG_ERROR("listen() failed");
        platform_close(server_fd);
        return 1;
    }

    LOG_INFO("=================================================");
    LOG_INFO("  Collaborative Code Editor Server");
    LOG_INFO("  Listening on port %d", port);
    LOG_INFO("  Open http://localhost:%d in your browser", port);
    LOG_INFO("  DB: %s", db_path);
    LOG_INFO("  Session TTL: %d seconds", session_ttl);
    LOG_INFO("=================================================");

    while (g_running) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == SOCKET_INVALID) {
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            LOG_ERROR("accept() failed");
            continue;
        }

        configure_client_socket(client_fd);

        pthread_mutex_lock(&g_clients_lock);
        client_t *slot = NULL;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (!g_clients[i].active) {
                slot = &g_clients[i];
                memset(slot, 0, sizeof(client_t));
                slot->socket_fd = client_fd;
                slot->active = 1;
                slot->addr = client_addr;
                break;
            }
        }
        pthread_mutex_unlock(&g_clients_lock);

        if (!slot) {
            LOG_WARN("Max clients reached, rejecting connection");
            const char *msg = "HTTP/1.1 503 Service Unavailable\r\n\r\n";
            send(client_fd, msg, (int)strlen(msg), MSG_NOSIGNAL);
            platform_close(client_fd);
            continue;
        }

        if (pthread_create(&slot->thread, NULL, client_thread, slot) != 0) {
            LOG_ERROR("pthread_create failed");
            slot->active = 0;
            platform_close(client_fd);
            continue;
        }
        pthread_detach(slot->thread);
    }

    LOG_INFO("Shutting down server...");
    platform_close(server_fd);

    pthread_mutex_lock(&g_clients_lock);
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active) {
            ws_send_close(g_clients[i].socket_fd, 1001);
            platform_close(g_clients[i].socket_fd);
            g_clients[i].active = 0;
        }
    }
    pthread_mutex_unlock(&g_clients_lock);

    for (i = 0; i < MAX_DOCUMENTS; i++) {
        if (g_documents[i].active && g_documents[i].content) {
            free(g_documents[i].content);
            pthread_mutex_destroy(&g_documents[i].lock);
        }
    }

    db_close();
    platform_cleanup();
    LOG_INFO("Server stopped.");
    return 0;
}

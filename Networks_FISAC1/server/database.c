/*
 * database.c - SQLite database operations
 *
 * All database access is serialized through a single mutex to prevent
 * concurrent SQLite write conflicts. Read operations during active WebSocket
 * sessions use short-lived transactions to minimize lock contention and
 * avoid degrading real-time latency.
 */

#include "common.h"
#include "database.h"

static sqlite3 *g_db = NULL;
static pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT UNIQUE NOT NULL,"
    "  password_hash TEXT NOT NULL,"
    "  salt TEXT NOT NULL,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER NOT NULL,"
    "  token TEXT UNIQUE NOT NULL,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  FOREIGN KEY (user_id) REFERENCES users(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS documents ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  content TEXT DEFAULT '',"
    "  version INTEGER DEFAULT 1,"
    "  owner_id INTEGER NOT NULL,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  FOREIGN KEY (owner_id) REFERENCES users(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS document_versions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  doc_id INTEGER NOT NULL,"
    "  content TEXT NOT NULL,"
    "  version INTEGER NOT NULL,"
    "  editor_id INTEGER NOT NULL,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  FOREIGN KEY (doc_id) REFERENCES documents(id),"
    "  FOREIGN KEY (editor_id) REFERENCES users(id)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);"
    "CREATE INDEX IF NOT EXISTS idx_doc_versions ON document_versions(doc_id, version);";

int db_init(const char *db_path) {
    char *err_msg = NULL;
    int rc;

    pthread_mutex_lock(&g_db_lock);

    rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open database %s: %s", db_path, sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    /* WAL mode reduces lock contention for concurrent reads during real-time edits */
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA busy_timeout=5000;", NULL, NULL, NULL);

    rc = sqlite3_exec(g_db, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Schema creation failed: %s", err_msg);
        sqlite3_free(err_msg);
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    LOG_INFO("Database initialized at %s", db_path);
    pthread_mutex_unlock(&g_db_lock);
    return 0;
}

void db_close(void) {
    pthread_mutex_lock(&g_db_lock);
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    pthread_mutex_unlock(&g_db_lock);
}

int db_create_user(const char *username, const char *password_hash, const char *salt) {
    sqlite3_stmt *stmt;
    int rc, user_id = -1;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO users (username, password_hash, salt) VALUES (?, ?, ?)", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE)
        user_id = (int)sqlite3_last_insert_rowid(g_db);
    else
        LOG_WARN("User creation failed for '%s': %s", username, sqlite3_errmsg(g_db));

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return user_id;
}

int db_get_user(const char *username, int *user_id_out,
                char *hash_out, int hash_max, char *salt_out, int salt_max) {
    sqlite3_stmt *stmt;
    int rc, found = 0;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "SELECT id, password_hash, salt FROM users WHERE username = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return 0; }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *user_id_out = sqlite3_column_int(stmt, 0);
        snprintf(hash_out, hash_max, "%s", (const char *)sqlite3_column_text(stmt, 1));
        snprintf(salt_out, salt_max, "%s", (const char *)sqlite3_column_text(stmt, 2));
        found = 1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return found;
}

int db_create_session(int user_id, const char *token) {
    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO sessions (user_id, token) VALUES (?, ?)", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, token, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_validate_session(const char *token, int *user_id_out, char *username_out, int uname_max) {
    sqlite3_stmt *stmt;
    int rc, found = 0;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "SELECT s.user_id, u.username FROM sessions s "
        "JOIN users u ON s.user_id = u.id WHERE s.token = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return 0; }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *user_id_out = sqlite3_column_int(stmt, 0);
        snprintf(username_out, uname_max, "%s", (const char *)sqlite3_column_text(stmt, 1));
        found = 1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return found;
}

int db_delete_session(const char *token) {
    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db, "DELETE FROM sessions WHERE token = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

void db_cleanup_expired_sessions(int max_age_seconds) {
    char sql[256];
    pthread_mutex_lock(&g_db_lock);
    snprintf(sql, sizeof(sql),
        "DELETE FROM sessions WHERE created_at < datetime('now', '-%d seconds')", max_age_seconds);
    sqlite3_exec(g_db, sql, NULL, NULL, NULL);
    pthread_mutex_unlock(&g_db_lock);
}

int db_create_document(const char *name, int owner_id) {
    sqlite3_stmt *stmt;
    int rc, doc_id = -1;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO documents (name, content, version, owner_id) VALUES (?, '', 1, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, owner_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE)
        doc_id = (int)sqlite3_last_insert_rowid(g_db);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return doc_id;
}

int db_get_document(int doc_id, char *name_out, int name_max,
                    char *content_out, int content_max, int *version_out, int *owner_out) {
    sqlite3_stmt *stmt;
    int rc, found = 0;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "SELECT name, content, version, owner_id FROM documents WHERE id = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return 0; }

    sqlite3_bind_int(stmt, 1, doc_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *n = (const char *)sqlite3_column_text(stmt, 0);
        const char *c = (const char *)sqlite3_column_text(stmt, 1);
        snprintf(name_out, name_max, "%s", n ? n : "");
        snprintf(content_out, content_max, "%s", c ? c : "");
        *version_out = sqlite3_column_int(stmt, 2);
        *owner_out = sqlite3_column_int(stmt, 3);
        found = 1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return found;
}

int db_update_document(int doc_id, const char *content, int version) {
    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "UPDATE documents SET content = ?, version = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, version);
    sqlite3_bind_int(stmt, 3, doc_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_list_documents(char *json_out, int json_max) {
    sqlite3_stmt *stmt;
    int rc, offset = 0;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "SELECT d.id, d.name, d.version, u.username, d.updated_at "
        "FROM documents d JOIN users u ON d.owner_id = u.id ORDER BY d.updated_at DESC",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    offset += snprintf(json_out + offset, json_max - offset, "[");
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW && offset < json_max - 200) {
        if (!first) offset += snprintf(json_out + offset, json_max - offset, ",");
        first = 0;
        offset += snprintf(json_out + offset, json_max - offset,
            "{\"id\":%d,\"name\":\"%s\",\"version\":%d,\"owner\":\"%s\",\"updated\":\"%s\"}",
            sqlite3_column_int(stmt, 0),
            (const char *)sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            (const char *)sqlite3_column_text(stmt, 3),
            (const char *)sqlite3_column_text(stmt, 4));
    }
    offset += snprintf(json_out + offset, json_max - offset, "]");

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return offset;
}

int db_get_document_owner(int doc_id, int *owner_out) {
    sqlite3_stmt *stmt;
    int rc, found = 0;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db, "SELECT owner_id FROM documents WHERE id = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return 0; }
    sqlite3_bind_int(stmt, 1, doc_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *owner_out = sqlite3_column_int(stmt, 0);
        found = 1;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return found;
}

int db_delete_document(int doc_id) {
    sqlite3_stmt *stmt;
    int rc1, rc2;

    pthread_mutex_lock(&g_db_lock);

    /* delete versions first to avoid orphans */
    rc1 = sqlite3_prepare_v2(g_db, "DELETE FROM document_versions WHERE doc_id = ?", -1, &stmt, NULL);
    if (rc1 != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }
    sqlite3_bind_int(stmt, 1, doc_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    rc2 = sqlite3_prepare_v2(g_db, "DELETE FROM documents WHERE id = ?", -1, &stmt, NULL);
    if (rc2 != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }
    sqlite3_bind_int(stmt, 1, doc_id);
    rc2 = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    pthread_mutex_unlock(&g_db_lock);
    return (rc2 == SQLITE_DONE) ? 0 : -1;
}

int db_save_version(int doc_id, const char *content, int version, int editor_id) {
    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO document_versions (doc_id, content, version, editor_id) VALUES (?, ?, ?, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    sqlite3_bind_int(stmt, 1, doc_id);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, version);
    sqlite3_bind_int(stmt, 4, editor_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_version_history(int doc_id, char *json_out, int json_max) {
    sqlite3_stmt *stmt;
    int rc, offset = 0;

    pthread_mutex_lock(&g_db_lock);
    rc = sqlite3_prepare_v2(g_db,
        "SELECT dv.version, u.username, dv.created_at "
        "FROM document_versions dv JOIN users u ON dv.editor_id = u.id "
        "WHERE dv.doc_id = ? ORDER BY dv.version DESC LIMIT 50",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_lock); return -1; }

    sqlite3_bind_int(stmt, 1, doc_id);
    offset += snprintf(json_out + offset, json_max - offset, "[");
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW && offset < json_max - 200) {
        if (!first) offset += snprintf(json_out + offset, json_max - offset, ",");
        first = 0;
        offset += snprintf(json_out + offset, json_max - offset,
            "{\"version\":%d,\"editor\":\"%s\",\"time\":\"%s\"}",
            sqlite3_column_int(stmt, 0),
            (const char *)sqlite3_column_text(stmt, 1),
            (const char *)sqlite3_column_text(stmt, 2));
    }
    offset += snprintf(json_out + offset, json_max - offset, "]");

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return offset;
}

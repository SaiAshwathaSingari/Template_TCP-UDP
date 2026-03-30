/*
 * database.h - SQLite database interface for the collaborative editor.
 *
 * Schema:
 *   users            - Registered user accounts with salted SHA-256 passwords
 *   sessions         - Active login sessions tied to users
 *   documents        - Collaborative documents with owner tracking
 *   document_versions - Versioned snapshots for history / rollback
 */

#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"

int  db_init(const char *db_path);
void db_close(void);

/* Users */
int  db_create_user(const char *username, const char *password_hash, const char *salt);
int  db_get_user(const char *username, int *user_id_out,
                 char *hash_out, int hash_max, char *salt_out, int salt_max);

/* Sessions */
int  db_create_session(int user_id, const char *token);
int  db_validate_session(const char *token, int *user_id_out, char *username_out, int uname_max);
int  db_delete_session(const char *token);
void db_cleanup_expired_sessions(int max_age_seconds);

/* Documents */
int  db_create_document(const char *name, int owner_id);
int  db_get_document(int doc_id, char *name_out, int name_max,
                     char *content_out, int content_max, int *version_out, int *owner_out);
int  db_update_document(int doc_id, const char *content, int version);
int  db_list_documents(char *json_out, int json_max);
int  db_get_document_owner(int doc_id, int *owner_out);
int  db_delete_document(int doc_id);

/* Document Versions */
int  db_save_version(int doc_id, const char *content, int version, int editor_id);
int  db_get_version_history(int doc_id, char *json_out, int json_max);

#endif /* DATABASE_H */

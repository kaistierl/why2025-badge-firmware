#pragma once

#include <stdbool.h>

typedef struct ota_session_t ota_session_t;
typedef ota_session_t       *ota_handle_t;

ota_handle_t ota_session_open();
bool         ota_write(ota_handle_t session, void *buffer, int block_size);
bool         ota_session_commit(ota_handle_t session);
bool         ota_session_abort(ota_handle_t session);

bool ota_get_running_version(char *version);
bool ota_get_invalid_version(char *version);
#pragma once

#include <stdbool.h>

typedef struct ota_session_t ota_session_t;
typedef ota_session_t* ota_handle_t;

ota_handle_t badgevms_ota_session_open();
bool badgevms_ota_write(ota_handle_t session, void *buffer, int block_size);
bool badgevms_ota_session_commit(ota_handle_t session);
bool badgevms_ota_session_abort(ota_handle_t session);
/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CURL_GLOBAL_DEFAULT 0

typedef void CURL;
typedef int  CURLcode;
typedef int  CURLoption;

typedef enum {
    CURLE_OK = 0,
    CURLE_UNSUPPORTED_PROTOCOL,
    CURLE_FAILED_INIT,
    CURLE_URL_MALFORMAT,
    CURLE_COULDNT_RESOLVE_PROXY,
    CURLE_COULDNT_RESOLVE_HOST,
    CURLE_COULDNT_CONNECT,
    CURLE_WEIRD_SERVER_REPLY,
    CURLE_HTTP_RETURNED_ERROR,
    CURLE_WRITE_ERROR,
    CURLE_OUT_OF_MEMORY,
    CURLE_OPERATION_TIMEDOUT,
    CURLE_SSL_CONNECT_ERROR,
    CURLE_SSL_PEER_CERTIFICATE,
    CURLE_SSL_ENGINE_NOTFOUND,
    CURLE_SSL_ENGINE_SETFAILED,
    CURLE_SEND_ERROR,
    CURLE_RECV_ERROR,
    CURLE_SSL_CERTPROBLEM,
    CURLE_SSL_CIPHER,
    CURLE_SSL_CACERT,
    CURLE_BAD_CONTENT_ENCODING,
    CURLE_FILESIZE_EXCEEDED,
    CURLE_USE_SSL_FAILED,
    CURLE_SEND_FAIL_REWIND,
    CURLE_SSL_ENGINE_INITFAILED,
    CURLE_LOGIN_DENIED,
    CURLE_ABORTED_BY_CALLBACK
} curl_easy_error_t;

typedef enum {
    CURLOPT_URL               = 10002,
    CURLOPT_PROXY             = 10004,
    CURLOPT_USERPWD           = 10005,
    CURLOPT_PROXYUSERPWD      = 10006,
    CURLOPT_RANGE             = 10007,
    CURLOPT_POSTFIELDS        = 10015,
    CURLOPT_REFERER           = 10016,
    CURLOPT_USERAGENT         = 10018,
    CURLOPT_HTTPHEADER        = 10023,
    CURLOPT_COOKIE            = 10022,
    CURLOPT_COOKIEFILE        = 10031,
    CURLOPT_COOKIEJAR         = 10082,
    CURLOPT_CUSTOMREQUEST     = 10036,
    CURLOPT_POSTFIELDSIZE     = 60,
    CURLOPT_TIMEOUT           = 78,
    CURLOPT_TIMEOUT_MS        = 155,
    CURLOPT_CONNECTTIMEOUT    = 78,
    CURLOPT_CONNECTTIMEOUT_MS = 156,
    CURLOPT_SSL_VERIFYPEER    = 64,
    CURLOPT_SSL_VERIFYHOST    = 81,
    CURLOPT_CAINFO            = 10065,
    CURLOPT_CAPATH            = 10097,
    CURLOPT_WRITEFUNCTION     = 20011,
    CURLOPT_WRITEDATA         = 10001,
    CURLOPT_HEADERFUNCTION    = 20079,
    CURLOPT_HEADERDATA        = 10029,
    CURLOPT_FOLLOWLOCATION    = 52,
    CURLOPT_MAXREDIRS         = 68,
    CURLOPT_HTTPGET           = 80,
    CURLOPT_POST              = 47,
    CURLOPT_PUT               = 54,
    CURLOPT_NOBODY            = 44,
    CURLOPT_VERBOSE           = 41,
    CURLOPT_PROXYTYPE         = 101,
    CURLOPT_PROXYPORT         = 59,
    CURLOPT_HTTPAUTH          = 107,
    CURLOPT_PROXYAUTH         = 111,
    CURLOPT_BUFFERSIZE        = 98,
} curl_easy_option_t;

typedef enum {
    CURLINFO_RESPONSE_CODE           = 0x200002,
    CURLINFO_CONTENT_LENGTH_DOWNLOAD = 0x300003,
    CURLINFO_CONTENT_TYPE            = 0x100012,
    CURLINFO_EFFECTIVE_URL           = 0x100001
} curl_easy_info_t;

// Proxy types (for CURLOPT_PROXYTYPE)
typedef enum {
    CURLPROXY_HTTP            = 0,
    CURLPROXY_HTTP_1_0        = 1,
    CURLPROXY_SOCKS4          = 4,
    CURLPROXY_SOCKS5          = 5,
    CURLPROXY_SOCKS4A         = 6,
    CURLPROXY_SOCKS5_HOSTNAME = 7
} curl_proxytype;

// Auth types (for CURLOPT_HTTPAUTH, CURLOPT_PROXYAUTH)
#define CURLAUTH_NONE      0
#define CURLAUTH_BASIC     (1 << 0)
#define CURLAUTH_DIGEST    (1 << 1)
#define CURLAUTH_NEGOTIATE (1 << 2)
#define CURLAUTH_NTLM      (1 << 3)
#define CURLAUTH_DIGEST_IE (1 << 4)
#define CURLAUTH_NTLM_WB   (1 << 5)
#define CURLAUTH_BEARER    (1 << 6)
#define CURLAUTH_ANY       (~CURLAUTH_DIGEST_IE)
#define CURLAUTH_ANYSAFE   (~(CURLAUTH_BASIC | CURLAUTH_DIGEST_IE))

typedef size_t (*curl_write_callback)(void *contents, size_t size, size_t nmemb, void *userp);
typedef size_t (*curl_header_callback)(void *contents, size_t size, size_t nmemb, void *userp);

typedef struct curl_handle curl_handle_t;

struct curl_slist {
    char              *data;
    struct curl_slist *next;
};

CURL       *curl_easy_init(void);
CURLcode    curl_easy_setopt(CURL *curl, CURLoption option, ...);
CURLcode    curl_easy_perform(CURL *curl);
void        curl_easy_cleanup(CURL *curl);
CURLcode    curl_easy_getinfo(CURL *curl, curl_easy_info_t info, ...);
char const *curl_easy_strerror(CURLcode error);

struct curl_slist *curl_slist_append(struct curl_slist *list, char const *string);
void               curl_slist_free_all(struct curl_slist *list);

CURLcode curl_global_init(long flags);
void     curl_global_cleanup(void);

#ifdef __cplusplus
}
#endif

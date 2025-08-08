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

#include "curl/curl.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "task.h"
#include "thirdparty/dlmalloc.h"
#include "why_io.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <time.h>

static char const *TAG = "ESP_CURL";

typedef enum { CURL_SAMESITE_NONE = 0, CURL_SAMESITE_LAX, CURL_SAMESITE_STRICT } curl_samesite;

typedef struct cookie_entry {
    char                *name;
    char                *value;
    char                *domain;
    char                *path;
    time_t               expires;
    bool                 secure;
    bool                 http_only;
    curl_samesite        samesite;
    struct cookie_entry *next;
} cookie_entry_t;

struct curl_handle {
    esp_http_client_handle_t esp_client;
    esp_http_client_config_t config;

    curl_write_callback  write_function;
    void                *write_data;
    curl_header_callback header_function;
    void                *header_data;

    char              *post_data;
    size_t             post_data_size;
    struct curl_slist *headers;

    cookie_entry_t *cookies;
    char           *cookie_file;
    char           *cookie_jar;
    char           *manual_cookies;

    char          *proxy_url;
    char          *proxy_userpwd;
    curl_proxytype proxy_type;
    long           proxy_port;
    long           proxy_auth;

    int     response_code;
    int64_t content_length;
    char   *content_type;
    char   *effective_url;

    bool configured;
    bool verbose;
    bool ssl_verify_peer;
    long http_auth;
};

static size_t default_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    printf("%.*s", (int)realsize, (char *)contents);
    return realsize;
}

static size_t default_header_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    why_fprintf(stderr, "%.*s", (int)realsize, (char *)contents);
    return realsize;
}

static void free_cookie(cookie_entry_t *cookie) {
    if (!cookie)
        return;

    dlfree(cookie->name);
    dlfree(cookie->value);
    dlfree(cookie->domain);
    dlfree(cookie->path);
    dlfree(cookie);
}

static void free_all_cookies(cookie_entry_t *cookies) {
    while (cookies) {
        cookie_entry_t *next = cookies->next;
        free_cookie(cookies);
        cookies = next;
    }
}

static cookie_entry_t *parse_set_cookie(char const *set_cookie_header) {
    if (!set_cookie_header) {
        ESP_LOGW(TAG, "No cookie header");
        return NULL;
    }

    ESP_LOGW(TAG, "Parsing header '%s'", set_cookie_header);

    cookie_entry_t *cookie = dlcalloc(1, sizeof(cookie_entry_t));
    if (!cookie) {
        ESP_LOGW(TAG, "Cookie dlmalloc failed (cookie)");
        return NULL;
    }

    char *header_copy = why_strdup(set_cookie_header);
    if (!header_copy) {
        ESP_LOGW(TAG, "Cookie dlmalloc failed (header_copy)");
        free_cookie(cookie);
        return NULL;
    }

    char *first_semicolon = strchr(header_copy, ';');
    char *name_value_part = header_copy;

    if (first_semicolon) {
        *first_semicolon = '\0';
    }

    char *equals = strchr(name_value_part, '=');
    if (equals) {
        *equals = '\0';

        char *name  = name_value_part;
        char *value = equals + 1;

        while (*name == ' ' || *name == '\t') name++;
        char *name_end = name + strlen(name) - 1;
        while (name_end > name && (*name_end == ' ' || *name_end == '\t')) {
            *name_end = '\0';
            name_end--;
        }

        while (*value == ' ' || *value == '\t') value++;
        char *value_end = value + strlen(value) - 1;
        while (value_end > value && (*value_end == ' ' || *value_end == '\t')) {
            *value_end = '\0';
            value_end--;
        }

        cookie->name  = why_strdup(name);
        cookie->value = why_strdup(value);

        if (!cookie->name || !cookie->value) {
            ESP_LOGW(TAG, "No name or no value for '%s'", header_copy);
            dlfree(header_copy);
            free_cookie(cookie);
            return NULL;
        }
    } else {
        ESP_LOGW(TAG, "No = for '%s'", header_copy);
        dlfree(header_copy);
        free_cookie(cookie);
        return NULL;
    }

    cookie->domain    = why_strdup("example.com");
    cookie->path      = why_strdup("/");
    cookie->expires   = 0;
    cookie->secure    = false;
    cookie->http_only = false;
    cookie->samesite  = CURL_SAMESITE_NONE; // Default per RFC

    if (!cookie->domain || !cookie->path) {
        ESP_LOGW(TAG, "No domain or cookie path for '%s'", header_copy);
        dlfree(header_copy);
        free_cookie(cookie);
        return NULL;
    }

    if (first_semicolon) {
        char *attr_start = first_semicolon + 1;

        while (attr_start && *attr_start) {
            while (*attr_start == ' ' || *attr_start == '\t') {
                attr_start++;
            }

            if (!*attr_start)
                break;

            char *attr_end = strchr(attr_start, ';');
            if (attr_end) {
                *attr_end = '\0';
            }

            char *attr_equals = strchr(attr_start, '=');
            if (attr_equals) {
                *attr_equals     = '\0';
                char *attr_name  = attr_start;
                char *attr_value = attr_equals + 1;

                while (*attr_name == ' ' || *attr_name == '\t') attr_name++;
                char *name_end = attr_name + strlen(attr_name) - 1;
                while (name_end > attr_name && (*name_end == ' ' || *name_end == '\t')) {
                    *name_end = '\0';
                    name_end--;
                }

                while (*attr_value == ' ' || *attr_value == '\t') {
                    attr_value++;
                }
                char *value_end = attr_value + strlen(attr_value) - 1;
                while (value_end > attr_value && (*value_end == ' ' || *value_end == '\t')) {
                    *value_end = '\0';
                    value_end--;
                }

                if (strcasecmp(attr_name, "domain") == 0) {
                    dlfree(cookie->domain);
                    cookie->domain = why_strdup(attr_value);
                    if (!cookie->domain) {
                        ESP_LOGW(TAG, "No domain for '%s'", header_copy);
                        dlfree(header_copy);
                        free_cookie(cookie);
                        return NULL;
                    }
                } else if (strcasecmp(attr_name, "path") == 0) {
                    dlfree(cookie->path);
                    cookie->path = why_strdup(attr_value);
                    if (!cookie->path) {
                        ESP_LOGW(TAG, "No path for '%s'", header_copy);
                        dlfree(header_copy);
                        free_cookie(cookie);
                        return NULL;
                    }
                } else if (strcasecmp(attr_name, "expires") == 0) {
                    // RFC 1123 date format...
                    cookie->expires = 2147483647; // Max time_t for 32-bit (year 2038)
                } else if (strcasecmp(attr_name, "max-age") == 0) {
                    long max_age = atol(attr_value);
                    if (max_age > 0) {
                        time_t current_time = time(NULL);
                        if (current_time != (time_t)-1) {
                            cookie->expires = current_time + max_age;
                        }
                    } else if (max_age == 0) {
                        // max-age=0 means delete cookie immediately
                        cookie->expires = 1; // Past time
                    }
                } else if (strcasecmp(attr_name, "samesite") == 0) {
                    if (strcasecmp(attr_value, "strict") == 0) {
                        cookie->samesite = CURL_SAMESITE_STRICT;
                    } else if (strcasecmp(attr_value, "lax") == 0) {
                        cookie->samesite = CURL_SAMESITE_LAX;
                    } else if (strcasecmp(attr_value, "none") == 0) {
                        cookie->samesite = CURL_SAMESITE_NONE;
                    } else {
                        // Invalid SameSite value, default to None per spec
                        cookie->samesite = CURL_SAMESITE_NONE;
                        ESP_LOGW(TAG, "Invalid SameSite value: %s, defaulting to None", attr_value);
                    }
                }
                // More attributes... maybe

            } else {
                char *attr_name = attr_start;

                while (*attr_name == ' ' || *attr_name == '\t') attr_name++;
                char *name_end = attr_name + strlen(attr_name) - 1;
                while (name_end > attr_name && (*name_end == ' ' || *name_end == '\t')) {
                    *name_end = '\0';
                    name_end--;
                }

                if (strcasecmp(attr_name, "secure") == 0) {
                    cookie->secure = true;
                } else if (strcasecmp(attr_name, "httponly") == 0) {
                    cookie->http_only = true;
                }
            }

            if (attr_end) {
                attr_start = attr_end + 1;
            } else {
                break;
            }
        }
    }

    dlfree(header_copy);
    return cookie;
}

static void add_cookie(curl_handle_t *curl, cookie_entry_t *new_cookie) {
    if (!curl || !new_cookie)
        return;

    cookie_entry_t **current = &curl->cookies;
    while (*current) {
        if (strcmp((*current)->name, new_cookie->name) == 0) {
            cookie_entry_t *to_remove = *current;
            *current                  = (*current)->next;
            free_cookie(to_remove);
            break;
        }
        current = &(*current)->next;
    }

    new_cookie->next = curl->cookies;
    curl->cookies    = new_cookie;
}

static char *build_cookie_header(curl_handle_t *curl) {
    if (!curl->cookies && !curl->manual_cookies)
        return NULL;

    size_t total_len = 0;
    char  *result    = NULL;

    if (curl->manual_cookies) {
        total_len += strlen(curl->manual_cookies);
    }

    cookie_entry_t *cookie = curl->cookies;
    while (cookie) {
        if (total_len > 0)
            total_len += 2;                                            // "; "
        total_len += strlen(cookie->name) + 1 + strlen(cookie->value); // name=value
        cookie     = cookie->next;
    }

    if (total_len == 0)
        return NULL;

    result = dlmalloc(total_len + 1);
    if (!result)
        return NULL;

    result[0] = '\0';

    if (curl->manual_cookies) {
        strcat(result, curl->manual_cookies);
    }

    cookie = curl->cookies;
    while (cookie) {
        if (strlen(result) > 0)
            strcat(result, "; ");
        strcat(result, cookie->name);
        strcat(result, "=");
        strcat(result, cookie->value);
        cookie = cookie->next;
    }

    return result;
}

static int load_cookies_from_file(curl_handle_t *curl, char const *filename) {
    if (!curl || !filename)
        return -1;

    FILE *file = why_fopen(filename, "r");
    if (!file) {
        ESP_LOGW(TAG, "Cookie file not found or can't be opened: %s", filename);
        return 0;
    }

    char line[512];
    int  cookies_loaded = 0;

    while (why_fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '#' || line[0] == '\0') {
            continue;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        char *name          = strtok(line, "\t");
        char *value         = strtok(NULL, "\t");
        char *domain        = strtok(NULL, "\t");
        char *path          = strtok(NULL, "\t");
        char *expires_str   = strtok(NULL, "\t");
        char *secure_str    = strtok(NULL, "\t");
        char *http_only_str = strtok(NULL, "\t");
        char *samesite_str  = strtok(NULL, "\t");

        if (!name || !value || !domain || !path || !expires_str || !secure_str || !http_only_str || !samesite_str) {
            ESP_LOGW(TAG, "Malformed cookie line in file: %s", filename);
            continue;
        }

        cookie_entry_t *cookie = dlcalloc(1, sizeof(cookie_entry_t));
        if (!cookie) {
            ESP_LOGE(TAG, "Failed to allocate memory for cookie");
            continue;
        }

        cookie->name      = why_strdup(name);
        cookie->value     = why_strdup(value);
        cookie->domain    = why_strdup(domain);
        cookie->path      = why_strdup(path);
        cookie->expires   = (time_t)atol(expires_str);
        cookie->secure    = (atoi(secure_str) != 0);
        cookie->http_only = (atoi(http_only_str) != 0);
        cookie->samesite  = (atoi(samesite_str) != 0);

        time_t current_time = time(NULL);
        if (cookie->expires > 0 && cookie->expires < current_time) {
            ESP_LOGW(TAG, "Skipping expired cookie: %s", cookie->name);
            free_cookie(cookie);
            continue;
        }

        add_cookie(curl, cookie);
        cookies_loaded++;
    }

    why_fclose(file);
    ESP_LOGW(TAG, "Loaded %d cookies from file: %s", cookies_loaded, filename);
    return cookies_loaded;
}

static int save_cookies_to_file(curl_handle_t *curl, char const *filename) {
    if (!curl || !filename)
        return -1;

    FILE *file = why_fopen(filename, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open cookie file for writing: %s", filename);
        return -1;
    }

    why_fprintf(file, "# BadgeVMS cookie jar file\n");
    why_fprintf(file, "# Format: name\\tvalue\\tdomain\\tpath\\texpires\\tsecure\\thttp_only\\tsamesite\t\n");

    int    cookies_saved = 0;
    time_t current_time  = time(NULL);

    cookie_entry_t *cookie = curl->cookies;
    while (cookie) {
        if (cookie->expires > 0 && cookie->expires < current_time) {
            ESP_LOGW(TAG, "Skipping expired cookie when saving: %s", cookie->name);
            cookie = cookie->next;
            continue;
        }
        ESP_LOGW(TAG, "Saving cookie: %s", cookie->name);

        why_fprintf(
            file,
            "%s\t%s\t%s\t%s\t%ld\t%d\t%d\t%d\t\n",
            cookie->name ? cookie->name : "",
            cookie->value ? cookie->value : "",
            cookie->domain ? cookie->domain : "",
            cookie->path ? cookie->path : "",
            (long)cookie->expires,
            cookie->secure ? 1 : 0,
            cookie->http_only ? 1 : 0,
            cookie->samesite ? 1 : 0
        );

        cookies_saved++;
        cookie = cookie->next;
    }

    ESP_LOGW(TAG, "Closing jar");
    why_fclose(file);
    ESP_LOGW(TAG, "Saved %d cookies to file: %s", cookies_saved, filename);
    return cookies_saved;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    curl_handle_t *curl = (curl_handle_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            if (curl->verbose) {
                ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            }
            break;

        case HTTP_EVENT_ON_CONNECTED:
            if (curl->verbose) {
                ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            }
            break;

        case HTTP_EVENT_HEADER_SENT:
            if (curl->verbose) {
                ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            }
            break;

        case HTTP_EVENT_ON_HEADER:
            if (evt->header_key) {
                if (strncasecmp(evt->header_key, "Set-Cookie", 10) == 0) {
                    if (evt->header_value) {
                        cookie_entry_t *cookie = parse_set_cookie(evt->header_value);
                        if (cookie) {
                            add_cookie(curl, cookie);
                        }
                    }
                }
            }

            if (curl->header_function) {
                size_t key_len = strlen(evt->header_key);
                size_t val_len = strlen(evt->header_value);

                char *tmp_header = dlmalloc(key_len + val_len + 2);
                memcpy(tmp_header, evt->header_key, key_len);
                tmp_header[key_len]     = ':';
                tmp_header[key_len + 1] = ' ';
                memcpy(&tmp_header[key_len + 2], evt->header_value, val_len);

                curl->header_function(tmp_header, 1, key_len + val_len + 2, curl->header_data);
                dlfree(tmp_header);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            if (curl->write_function) {
                curl->write_function(evt->data, 1, evt->data_len, curl->write_data);
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            curl->response_code  = esp_http_client_get_status_code(curl->esp_client);
            curl->content_length = esp_http_client_get_content_length(curl->esp_client);
            if (curl->verbose) {
                ESP_LOGI(
                    TAG,
                    "HTTP_EVENT_ON_FINISH, status=%d, content_length=%lld",
                    curl->response_code,
                    curl->content_length
                );
            }
            break;

        case HTTP_EVENT_DISCONNECTED:
            if (curl->verbose) {
                ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            }
            break;

        case HTTP_EVENT_REDIRECT:
            if (curl->verbose) {
                ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            }
            break;
    }
    return ESP_OK;
}

CURL *curl_easy_init(void) {
    curl_handle_t *curl = dlcalloc(1, sizeof(curl_handle_t));
    if (!curl) {
        return NULL;
    }

    curl->write_function  = default_write_callback;
    curl->header_function = default_header_callback;

    memset(&curl->config, 0, sizeof(esp_http_client_config_t));
    curl->config.event_handler = http_event_handler;
    curl->config.user_data     = curl;
    curl->config.timeout_ms    = 30000;

    curl->ssl_verify_peer          = true;
    curl->config.crt_bundle_attach = esp_crt_bundle_attach;

    // Initialize proxy settings
    curl->proxy_type = CURLPROXY_HTTP;
    curl->proxy_port = 0;
    curl->proxy_auth = CURLAUTH_BASIC;
    curl->http_auth  = CURLAUTH_BASIC;

    return (CURL *)curl;
}

CURLcode curl_easy_setopt(CURL *curl_handle, CURLoption option, ...) {
    if (!curl_handle) {
        return CURLE_FAILED_INIT;
    }

    curl_handle_t *curl = (curl_handle_t *)curl_handle;
    va_list        args;
    va_start(args, option);

    switch (option) {
        case CURLOPT_URL: {
            char const *url = va_arg(args, char const *);
            dlfree((void *)curl->config.url);
            curl->config.url = why_strdup(url);
            break;
        }

        case CURLOPT_USERAGENT: {
            char const *agent = va_arg(args, char const *);
            dlfree((void *)curl->config.user_agent);
            curl->config.user_agent = why_strdup(agent);
            break;
        }

        case CURLOPT_TIMEOUT: {
            long timeout            = va_arg(args, long);
            curl->config.timeout_ms = timeout * 1000;
            break;
        }

        case CURLOPT_TIMEOUT_MS: {
            long timeout_ms         = va_arg(args, long);
            curl->config.timeout_ms = timeout_ms;
            break;
        }

        case CURLOPT_SSL_VERIFYPEER: {
            long verify           = va_arg(args, long);
            curl->ssl_verify_peer = (verify != 0);
            if (verify) {
                curl->config.crt_bundle_attach           = esp_crt_bundle_attach;
                curl->config.skip_cert_common_name_check = false;
            } else {
                curl->config.crt_bundle_attach           = NULL;
                curl->config.skip_cert_common_name_check = true;
            }
            break;
        }

        case CURLOPT_SSL_VERIFYHOST: {
            long verify = va_arg(args, long);
            if (verify == 0) {
                curl->config.skip_cert_common_name_check = true;
            } else {
                curl->config.skip_cert_common_name_check = false;
            }
            break;
        }

        case CURLOPT_CAINFO: {
            char const *ca_file = va_arg(args, char const *);
            // In ESP-IDF, this should be the actual certificate content, not a file path
            dlfree((void *)curl->config.cert_pem);
            curl->config.cert_pem          = why_strdup(ca_file);
            curl->config.crt_bundle_attach = NULL;
            break;
        }

        case CURLOPT_USERPWD: {
            char const *userpwd      = va_arg(args, char const *);
            char       *userpwd_copy = why_strdup(userpwd);
            char       *colon        = strchr(userpwd_copy, ':');
            if (colon) {
                *colon = '\0';
                dlfree((void *)curl->config.username);
                dlfree((void *)curl->config.password);
                curl->config.username = why_strdup(userpwd_copy);
                curl->config.password = why_strdup(colon + 1);
            }
            dlfree(userpwd_copy);
            break;
        }

        case CURLOPT_POSTFIELDS: {
            char const *data = va_arg(args, char const *);
            dlfree(curl->post_data);
            curl->post_data      = why_strdup(data);
            curl->post_data_size = strlen(data);
            break;
        }

        case CURLOPT_POSTFIELDSIZE: {
            long size            = va_arg(args, long);
            curl->post_data_size = size;
            break;
        }

        case CURLOPT_HTTPHEADER: {
            struct curl_slist *headers = va_arg(args, struct curl_slist *);
            curl->headers              = headers;
            break;
        }

        case CURLOPT_WRITEFUNCTION: {
            curl_write_callback func = va_arg(args, curl_write_callback);
            curl->write_function     = func;
            break;
        }

        case CURLOPT_WRITEDATA: {
            void *data       = va_arg(args, void *);
            curl->write_data = data;
            break;
        }

        case CURLOPT_HEADERFUNCTION: {
            curl_header_callback func = va_arg(args, curl_header_callback);
            curl->header_function     = func;
            break;
        }

        case CURLOPT_HEADERDATA: {
            void *data        = va_arg(args, void *);
            curl->header_data = data;
            break;
        }

        case CURLOPT_FOLLOWLOCATION: {
            long follow                        = va_arg(args, long);
            curl->config.max_redirection_count = follow ? 10 : 0;
            break;
        }

        case CURLOPT_MAXREDIRS: {
            long max_redirs                    = va_arg(args, long);
            curl->config.max_redirection_count = max_redirs;
            break;
        }

        case CURLOPT_VERBOSE: {
            long verbose  = va_arg(args, long);
            curl->verbose = verbose != 0;
            break;
        }

        case CURLOPT_CUSTOMREQUEST: {
            char const *method  = va_arg(args, char const *);
            curl->config.method = HTTP_METHOD_GET;
            if (strcmp(method, "GET") == 0)
                curl->config.method = HTTP_METHOD_GET;
            else if (strcmp(method, "POST") == 0)
                curl->config.method = HTTP_METHOD_POST;
            else if (strcmp(method, "PUT") == 0)
                curl->config.method = HTTP_METHOD_PUT;
            else if (strcmp(method, "DELETE") == 0)
                curl->config.method = HTTP_METHOD_DELETE;
            else if (strcmp(method, "HEAD") == 0)
                curl->config.method = HTTP_METHOD_HEAD;
            else if (strcmp(method, "PATCH") == 0)
                curl->config.method = HTTP_METHOD_PATCH;
            break;
        }

        case CURLOPT_POST: {
            long post = va_arg(args, long);
            if (post) {
                curl->config.method = HTTP_METHOD_POST;
            }
            break;
        }

        case CURLOPT_PUT: {
            long put = va_arg(args, long);
            if (put) {
                curl->config.method = HTTP_METHOD_PUT;
            }
            break;
        }

        case CURLOPT_HTTPGET: {
            long get = va_arg(args, long);
            if (get) {
                curl->config.method = HTTP_METHOD_GET;
            }
            break;
        }

        case CURLOPT_NOBODY: {
            long nobody = va_arg(args, long);
            if (nobody) {
                curl->config.method = HTTP_METHOD_HEAD;
            }
            break;
        }

        case CURLOPT_COOKIE: {
            char const *cookies = va_arg(args, char const *);
            dlfree(curl->manual_cookies);
            curl->manual_cookies = why_strdup(cookies);
            break;
        }

        case CURLOPT_COOKIEFILE: {
            char const *filename = va_arg(args, char const *);
            dlfree(curl->cookie_file);
            curl->cookie_file = why_strdup(filename);
            load_cookies_from_file(curl, filename);
            break;
        }

        case CURLOPT_COOKIEJAR: {
            char const *filename = va_arg(args, char const *);
            dlfree(curl->cookie_jar);
            curl->cookie_jar = why_strdup(filename);
            break;
        }

        case CURLOPT_BUFFERSIZE: {
            long size                = va_arg(args, long);
            curl->config.buffer_size = size;
            va_end(args);
            break;
        }

        // Proxy options (stubs - not implemented in esp_http_client)
        case CURLOPT_PROXY: {
            char const *proxy = va_arg(args, char const *);
            dlfree(curl->proxy_url);
            curl->proxy_url = why_strdup(proxy);
            ESP_LOGW(TAG, "Proxy support not implemented in BadgeVMS HTTP client: %s", proxy);
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        case CURLOPT_PROXYUSERPWD: {
            char const *userpwd = va_arg(args, char const *);
            dlfree(curl->proxy_userpwd);
            curl->proxy_userpwd = why_strdup(userpwd);
            ESP_LOGW(TAG, "Proxy authentication not implemented in BadgeVMS HTTP client");
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        case CURLOPT_PROXYTYPE: {
            long type        = va_arg(args, long);
            curl->proxy_type = (curl_proxytype)type;
            ESP_LOGW(TAG, "Proxy type configuration not implemented in BadgeVMS HTTP client");
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        case CURLOPT_PROXYPORT: {
            long port        = va_arg(args, long);
            curl->proxy_port = port;
            ESP_LOGW(TAG, "Proxy port configuration not implemented in BadgeVMS HTTP client");
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        case CURLOPT_PROXYAUTH: {
            long auth        = va_arg(args, long);
            curl->proxy_auth = auth;
            ESP_LOGW(TAG, "Proxy authentication type not implemented in BadgeVMS HTTP client");
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        case CURLOPT_HTTPAUTH: {
            long auth       = va_arg(args, long);
            curl->http_auth = auth;
            // The actual HTTP auth is handled through username/password
            break;
        }

        case CURLOPT_RANGE: {
            char const *range = va_arg(args, char const *);
            ESP_LOGW(TAG, "CURLOPT_RANGE not implemented: %s", range);
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        case CURLOPT_REFERER: {
            char const *referer = va_arg(args, char const *);
            ESP_LOGW(TAG, "CURLOPT_REFERER not implemented: %s", referer);
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
        }

        default:
            ESP_LOGW(TAG, "Unsupported curl option: %d", option);
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
    }

    va_end(args);
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *curl_handle) {
    if (!curl_handle) {
        return CURLE_FAILED_INIT;
    }

    curl_handle_t *curl = (curl_handle_t *)curl_handle;

    curl->esp_client = esp_http_client_init(&curl->config);
    if (!curl->esp_client) {
        return CURLE_FAILED_INIT;
    }

    if (curl->headers) {
        struct curl_slist *header = curl->headers;
        while (header) {
            char *colon = strchr(header->data, ':');
            if (colon) {
                char *header_copy = why_strdup(header->data);
                char *colon_copy  = strchr(header_copy, ':');
                *colon_copy       = '\0';

                char *value = colon_copy + 1;
                while (*value && (*value == ' ' || *value == '\t')) {
                    value++;
                }

                esp_http_client_set_header(curl->esp_client, header_copy, value);
                dlfree(header_copy);
            }
            header = header->next;
        }
    }

    char *cookie_header = build_cookie_header(curl);
    if (cookie_header) {
        esp_http_client_set_header(curl->esp_client, "Cookie", cookie_header);
        dlfree(cookie_header);
    }

    if (curl->post_data && curl->config.method == HTTP_METHOD_POST) {
        esp_http_client_set_post_field(curl->esp_client, curl->post_data, curl->post_data_size);
    }

    esp_err_t err = esp_http_client_perform(curl->esp_client);

    if (curl->cookie_jar) {
        save_cookies_to_file(curl, curl->cookie_jar);
    }

    esp_http_client_cleanup(curl->esp_client);
    curl->esp_client = NULL;

    if (err == ESP_OK) {
        return CURLE_OK;
    } else if (err == ESP_ERR_TIMEOUT) {
        return CURLE_OPERATION_TIMEDOUT;
    } else if (err == ESP_ERR_HTTP_CONNECT) {
        return CURLE_COULDNT_CONNECT;
    } else {
        return CURLE_HTTP_RETURNED_ERROR;
    }
}

void curl_easy_cleanup(CURL *curl_handle) {
    if (!curl_handle) {
        return;
    }

    curl_handle_t *curl = (curl_handle_t *)curl_handle;

    dlfree((void *)curl->config.url);
    dlfree((void *)curl->config.user_agent);
    dlfree((void *)curl->config.username);
    dlfree((void *)curl->config.password);
    dlfree((void *)curl->config.cert_pem);
    dlfree(curl->post_data);
    dlfree(curl->content_type);
    dlfree(curl->effective_url);

    free_all_cookies(curl->cookies);
    dlfree(curl->cookie_file);
    dlfree(curl->cookie_jar);
    dlfree(curl->manual_cookies);

    dlfree(curl->proxy_url);
    dlfree(curl->proxy_userpwd);

    if (curl->esp_client) {
        esp_http_client_cleanup(curl->esp_client);
    }

    dlfree(curl);
}

CURLcode curl_easy_getinfo(CURL *curl_handle, curl_easy_info_t info, ...) {
    if (!curl_handle) {
        return CURLE_FAILED_INIT;
    }

    curl_handle_t *curl = (curl_handle_t *)curl_handle;
    va_list        args;
    va_start(args, info);

    switch (info) {
        case CURLINFO_RESPONSE_CODE: {
            long *code = va_arg(args, long *);
            *code      = curl->response_code;
            break;
        }

        case CURLINFO_CONTENT_LENGTH_DOWNLOAD: {
            double *length = va_arg(args, double *);
            *length        = (double)curl->content_length;
            break;
        }

        case CURLINFO_CONTENT_TYPE: {
            char **type = va_arg(args, char **);
            *type       = curl->content_type;
            break;
        }

        case CURLINFO_EFFECTIVE_URL: {
            char **url = va_arg(args, char **);
            *url       = curl->effective_url ? curl->effective_url : (char *)curl->config.url;
            break;
        }

        default: va_end(args); return CURLE_UNSUPPORTED_PROTOCOL;
    }

    va_end(args);
    return CURLE_OK;
}

char const *curl_easy_strerror(CURLcode error) {
    switch (error) {
        case CURLE_OK: return "No error";
        case CURLE_UNSUPPORTED_PROTOCOL: return "Unsupported protocol";
        case CURLE_FAILED_INIT: return "Failed initialization";
        case CURLE_URL_MALFORMAT: return "URL malformat";
        case CURLE_COULDNT_RESOLVE_HOST: return "Couldn't resolve host";
        case CURLE_COULDNT_CONNECT: return "Couldn't connect";
        case CURLE_HTTP_RETURNED_ERROR: return "HTTP returned error";
        case CURLE_OPERATION_TIMEDOUT: return "Operation timed out";
        case CURLE_SSL_CONNECT_ERROR: return "SSL connect error";
        default: return "Unknown error";
    }
}

struct curl_slist *curl_slist_append(struct curl_slist *list, char const *string) {
    struct curl_slist *new_node = dlmalloc(sizeof(struct curl_slist));
    if (!new_node) {
        return list;
    }

    new_node->data = why_strdup(string);
    new_node->next = NULL;

    if (!list) {
        return new_node;
    }

    struct curl_slist *current = list;
    while (current->next) {
        current = current->next;
    }
    current->next = new_node;

    return list;
}

void curl_slist_free_all(struct curl_slist *list) {
    while (list) {
        struct curl_slist *next = list->next;
        dlfree(list->data);
        dlfree(list);
        list = next;
    }
}

CURLcode curl_global_init(long flags) {
    return CURLE_OK;
}

void curl_global_cleanup(void) {
}

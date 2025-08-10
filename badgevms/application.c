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

#include "badgevms/application.h"

#include "badgevms/pathfuncs.h"
#include "badgevms/process.h"
#include "esp_log.h"
#include "task.h"
#include "thirdparty/cJSON.h"
#include "why_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAG "application"

#define APPLICATION_MAGIC 0xDEADBEEF
#define MAX_PATH_LEN      512

static char applications_base_dir[MAX_PATH_LEN] = "";

typedef struct application_list {
    application_t **applications;
    size_t          count;
    size_t          current_index;
} application_list_t;

static bool validate_path(application_t *app, char const *path) {
    if (!path || !app)
        return false;

    bool  success  = false;
    char *new_path = path_concat(app->installed_path, path);

    path_t parsed_path;
    if (parse_path(new_path, &parsed_path) == PATH_PARSE_OK) {
        success = true;
        path_free(&parsed_path);
    }

    free(new_path);
    return success;
}

static char *get_metadata_path(char const *unique_identifier) {
    if (!unique_identifier || !applications_base_dir[0])
        return NULL;

    path_t parsed_path;
    char  *filename;
    why_asprintf(&filename, "%s.json", unique_identifier);
    char *path = path_fileconcat(applications_base_dir, filename);

    if (parse_path(path, &parsed_path) != PATH_PARSE_OK) {
        free(path);
        path = NULL;
    } else {
        path_free(&parsed_path);
    }

    why_free(filename);
    return path;
}

static char *get_application_dir(char const *unique_identifier) {
    if (!unique_identifier || !applications_base_dir[0])
        return NULL;

    path_t parsed_path;
    char  *path = path_dirconcat(applications_base_dir, unique_identifier);

    if (parse_path(path, &parsed_path) != PATH_PARSE_OK) {
        free(path);
        path = NULL;
    } else {
        path_free(&parsed_path);
    }

    return path;
}

static cJSON *application_to_json(application_t const *app) {
    cJSON *json = cJSON_CreateObject();
    if (!json)
        return NULL;

    cJSON_AddStringToObject(json, "unique_identifier", app->unique_identifier ?: "");
    cJSON_AddStringToObject(json, "name", app->name ?: "");
    cJSON_AddStringToObject(json, "author", app->author ?: "");
    cJSON_AddStringToObject(json, "version", app->version ?: "");
    cJSON_AddStringToObject(json, "interpreter", app->interpreter ?: "");
    cJSON_AddStringToObject(json, "metadata_file", app->metadata_file ?: "");
    cJSON_AddStringToObject(json, "binary_path", app->binary_path ?: "");
    cJSON_AddNumberToObject(json, "source", app->source);

    return json;
}

static application_t *json_to_application(cJSON *json) {
    application_t *app = why_calloc(1, sizeof(application_t));
    if (!app)
        return NULL;

    // Cast away const for internal modification

    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "unique_identifier")) && cJSON_IsString(item)) {
        app->unique_identifier = why_strdup(item->valuestring);
        app->installed_path    = get_application_dir(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "name")) && cJSON_IsString(item)) {
        app->name = why_strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "author")) && cJSON_IsString(item)) {
        app->author = why_strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "version")) && cJSON_IsString(item)) {
        app->version = why_strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "interpreter")) && cJSON_IsString(item)) {
        app->interpreter = why_strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "metadata_file")) && cJSON_IsString(item)) {
        app->metadata_file = why_strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "binary_path")) && cJSON_IsString(item)) {
        app->binary_path = why_strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "source")) && cJSON_IsNumber(item)) {
        *((application_source_t *)&app->source) = (application_source_t)item->valueint;
    }

    return app;
}

static bool save_application_metadata(application_t const *app) {
    if (!app || !app->unique_identifier)
        return false;

    char *metadata_path = get_metadata_path(app->unique_identifier);
    if (!metadata_path)
        return false;

    cJSON *json = application_to_json(app);
    if (!json) {
        why_free(metadata_path);
        return false;
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        why_free(metadata_path);
        return false;
    }

    FILE *fp = why_fopen(metadata_path, "w");
    if (!fp) {
        why_free(json_string);
        why_free(metadata_path);
        return false;
    }

    why_fputs(json_string, fp);
    why_fclose(fp);
    why_free(json_string);
    why_free(metadata_path);

    return true;
}

static application_t *load_application_metadata(char const *unique_identifier) {
    if (!unique_identifier)
        return NULL;

    char *metadata_path = get_metadata_path(unique_identifier);
    if (!metadata_path)
        return NULL;

    FILE *fp = why_fopen(metadata_path, "r");
    if (!fp) {
        why_free(metadata_path);
        return NULL;
    }

    why_fseek(fp, 0, SEEK_END);
    long file_size = why_ftell(fp);
    why_fseek(fp, 0, SEEK_SET);

    char *content = why_malloc(file_size + 1);
    if (!content) {
        why_fclose(fp);
        why_free(metadata_path);
        return NULL;
    }

    why_fread(content, 1, file_size, fp);
    content[file_size] = '\0';
    why_fclose(fp);
    why_free(metadata_path);

    cJSON *json = cJSON_Parse(content);
    why_free(content);

    if (!json)
        return NULL;

    application_t *app = json_to_application(json);
    cJSON_Delete(json);

    return app;
}

bool application_init(char const *applications_dir, char const *flash_dir, char const *sd_dir) {
    if (!applications_dir || strlen(applications_dir) >= MAX_PATH_LEN) {
        return false;
    }

    strncpy(applications_base_dir, applications_dir, MAX_PATH_LEN - 1);
    applications_base_dir[MAX_PATH_LEN - 1] = '\0';

    if (flash_dir) {
        mkdir_p(flash_dir);
    }

    if (sd_dir) {
        mkdir_p(sd_dir);
    }

    return mkdir_p(applications_base_dir);
}

application_t *application_create(
    char const          *unique_identifier,
    char const          *name,
    char const          *author,
    char const          *version,
    char const          *interpreter,
    application_source_t source
) {
    if (!unique_identifier || !applications_base_dir[0]) {
        return NULL;
    }

    char *metadata_path = get_metadata_path(unique_identifier);
    if (!metadata_path) {
        ESP_LOGW(TAG, "Illegal application name %s", unique_identifier);
        return NULL;
    }

    FILE *fp = why_fopen(metadata_path, "r");
    if (fp) {
        // Already exists
        why_fclose(fp);
        why_free(metadata_path);
        return NULL;
    }
    why_free(metadata_path);

    char *app_dir = get_application_dir(unique_identifier);
    if (!app_dir)
        return NULL;

    if (!mkdir_p(app_dir)) {
        why_free(app_dir);
        return NULL;
    }

    application_t *app = why_calloc(1, sizeof(application_t));
    if (!app) {
        why_free(app_dir);
        return NULL;
    }

    // Cast away const for internal modification
    app->unique_identifier                  = why_strdup(unique_identifier);
    app->name                               = why_strdup(name);
    app->author                             = why_strdup(author);
    app->version                            = why_strdup(version);
    app->interpreter                        = why_strdup(interpreter);
    app->installed_path                     = app_dir;
    *((application_source_t *)&app->source) = source;

    if (!save_application_metadata(app)) {
        application_free(app);
        return NULL;
    }

    return app;
}

bool application_set_metadata(application_t *app, char const *metadata_file) {
    if (!app)
        return false;

    if (metadata_file && !validate_path(app, metadata_file)) {
        return false;
    }

    why_free((void *)app->metadata_file);
    app->metadata_file = why_strdup(metadata_file);

    return save_application_metadata(app);
}

bool application_set_binary_path(application_t *app, char const *binary_path) {
    if (!app)
        return false;

    if (binary_path && !validate_path(app, binary_path)) {
        return false;
    }

    why_free((void *)app->binary_path);
    app->binary_path = why_strdup(binary_path);

    return save_application_metadata(app);
}

bool application_set_version(application_t *app, char const *version) {
    if (!app)
        return false;

    why_free((void *)app->version);
    app->version = why_strdup(version);

    return save_application_metadata(app);
}

bool application_set_author(application_t *app, char const *author) {
    if (!app)
        return false;

    why_free((void *)app->author);
    app->author = why_strdup(author);

    return save_application_metadata(app);
}

bool application_set_name(application_t *app, char const *name) {
    if (!app)
        return false;

    why_free((void *)app->name);
    app->name = why_strdup(name);

    return save_application_metadata(app);
}

bool application_set_interpreter(application_t *app, char const *interpreter) {
    if (!app)
        return false;

    why_free((void *)app->interpreter);
    app->interpreter = why_strdup(interpreter);

    return save_application_metadata(app);
}

bool application_destroy(application_t *app) {
    if (!app)
        return false;

    char const *unique_id = app->unique_identifier;
    if (!unique_id)
        return false;

    bool  success = false;
    char *app_dir = get_application_dir(unique_id);
    if (app_dir) {
        ESP_LOGI(TAG, "Attempting to recursively delete %s\n", app_dir);
        success = rm_rf(app_dir);
        why_free(app_dir);
    } else {
        ESP_LOGW(TAG, "No valid app_dir for %s\n", app->unique_identifier);
    }

    return success;
}

char *application_create_file_string(application_t *app, char const *file_path) {
    if (!app) {
        return NULL;
    }

    if (!file_path || !app->installed_path) {
        ESP_LOGW(TAG, "Cannot create file in %s, no installed path, or no file_path given", app->unique_identifier);
        return NULL;
    }

    char *absolute_file_path = path_concat(app->installed_path, file_path);
    if (!absolute_file_path) {
        ESP_LOGW(TAG, "Could not create a sane path out of %s and %s", app->installed_path, file_path);
        return NULL;
    }

    ESP_LOGI(TAG, "Attempting to create %s", absolute_file_path);
    char *dirname = path_dirname(absolute_file_path);
    if (!dirname) {
        ESP_LOGW(TAG, "Couldn't determine dirname for %s", absolute_file_path);
        why_free(absolute_file_path);
        return NULL;
    }

    ESP_LOGW(TAG, "Creating directory %s", dirname);
    if (!mkdir_p(dirname)) {
        ESP_LOGW(TAG, "Couldn't create directory %s", dirname);
        why_free(dirname);
        why_free(absolute_file_path);
        return NULL;
    }

    return absolute_file_path;
}

FILE *application_create_file(application_t *app, char const *file_path) {
    char *absolute_file_path = application_create_file_string(app, file_path);
    if (!absolute_file_path) {
        return NULL;
    }

    FILE *ret = why_fopen(absolute_file_path, "w");
    why_free(absolute_file_path);
    return ret;
}

application_list_handle application_list(application_t **out) {
    if (!applications_base_dir[0])
        return NULL;

    DIR *dir = why_opendir(applications_base_dir);
    if (!dir) {
        ESP_LOGW(TAG, "Unable to opendir(%s)", applications_base_dir);
        return NULL;
    }

    application_list_t *list = why_calloc(1, sizeof(application_list_t));
    if (!list) {
        why_closedir(dir);
        return NULL;
    }

    // Count .json files first
    struct dirent *entry;
    size_t         json_count = 0;

    while ((entry = why_readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
            json_count++;
        }
    }

    if (json_count == 0) {
        why_closedir(dir);
        if (out)
            *out = NULL;
        return list;
    }

    list->applications = why_calloc(json_count, sizeof(application_t *));
    if (!list->applications) {
        why_closedir(dir);
        why_free(list);
        return NULL;
    }

    why_rewinddir(dir);
    while ((entry = why_readdir(dir)) != NULL && list->count < json_count) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
            char *unique_id = why_malloc(len - 4);
            if (unique_id) {
                strncpy(unique_id, entry->d_name, len - 5);
                unique_id[len - 5] = '\0';

                application_t *app = load_application_metadata(unique_id);
                if (app) {
                    list->applications[list->count++] = app;
                }
                why_free(unique_id);
            }
        }
    }

    why_closedir(dir);

    if (out && list->count > 0) {
        *out = list->applications[0];
    } else if (out) {
        *out = NULL;
    }

    return list;
}

application_t *application_list_get_next(application_list_handle list) {
    if (!list)
        return NULL;

    list->current_index++;

    if (list->current_index >= list->count) {
        return NULL;
    }

    return list->applications[list->current_index];
}

void application_list_close(application_list_handle list) {
    if (!list)
        return;

    // Free all loaded applications
    for (size_t i = 0; i < list->count; i++) {
        if (list->applications[i]) {
            application_t *app = (application_t *)list->applications[i];
            application_free(app);
        }
    }

    why_free(list->applications);
    why_free(list);
}

application_t *application_get(char const *unique_identifier) {
    if (!unique_identifier)
        return NULL;

    application_t *app = load_application_metadata(unique_identifier);
    return app;
}

void application_free(application_t *app) {
    if (!app)
        return;

    // Cast away const to free the strings
    why_free((void *)app->unique_identifier);
    why_free((void *)app->name);
    why_free((void *)app->author);
    why_free((void *)app->version);
    why_free((void *)app->interpreter);
    why_free((void *)app->metadata_file);
    why_free((void *)app->installed_path);
    why_free((void *)app->binary_path);

    why_free(app);
}

pid_t application_launch(char const *unique_identifier) {
    pid_t ret         = -1;
    char *binary_path = NULL;

    if (!unique_identifier) {
        return -1;
    }

    application_t *app = application_get(unique_identifier);
    if (!app) {
        ESP_LOGW(TAG, "Cannot launch %s not installed", unique_identifier);
        return -1;
    }

    if (!app->binary_path || !app->installed_path) {
        ESP_LOGW(TAG, "Cannot launch %s, no binary path or installed_path?", unique_identifier);
        goto out;
    }

    binary_path = path_concat(app->installed_path, app->binary_path);
    if (!binary_path) {
        ESP_LOGW(TAG, "Could not create a sane path out of %s and %s\n", app->installed_path, app->binary_path);
        goto out;
    }

    ESP_LOGI(TAG, "Attempting to launch %s", binary_path);
    ret = process_create(binary_path, 0, 0, NULL);
    if (ret != -1) {
        task_set_application_uid(ret, unique_identifier);
    }

out:
    application_free(app);
    why_free(binary_path);
    return ret;
}

bool application_is_running(char const *unique_identifier) {
    if (!unique_identifier) {
        return false;
    }

    application_t *app = application_get(unique_identifier);
    bool           ret = false;

    if (!app) {
        return false;
    }

    if (!app->binary_path || !app->installed_path) {
        ESP_LOGW(TAG, "Application %s does not exist, no binary path or installed_path?", unique_identifier);
        goto out;
    }

    ret = task_application_is_running(unique_identifier);

out:
    application_free(app);
    return ret;
}

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

#include "badgevms/pathfuncs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

static inline bool is_valid_device_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '$';
}

static inline bool is_valid_path_char(char c) {
    return is_valid_device_char(c) || c == '.';
}

// Parse a path in the form of DEVICE:[DIR.SUBDIR]FILENAME.EXT
path_parse_result_t parse_path(char const *path, path_t *result) {
    result->buffer    = NULL;
    result->device    = NULL;
    result->directory = NULL;
    result->filename  = NULL;
    result->unixpath  = NULL;
    result->len       = strlen(path);

    if (!path || !*path) {
        return PATH_PARSE_EMPTY_PATH;
    }

    result->buffer = strdup(path);
    if (!result->buffer) {
        return PATH_PARSE_EMPTY_PATH;
    }

    char *p     = result->buffer;
    char *start = p;

    // Everything up to the ':' is the device part
    while (*p && *p != ':') {
        if (!is_valid_device_char(*p)) {
            return PATH_PARSE_INVALID_DEVICE_CHAR;
        }
        p++;
    }

    if (*p != ':') {
        return PATH_PARSE_NO_DEVICE;
    }

    if (p == start) {
        return PATH_PARSE_EMPTY_DEVICE;
    }

    *p             = '\0';
    result->device = start;
    p++; // Skip ':'

    // Directory comes next [dir.name]
    if (*p == '[') {
        p++; // Skip '['
        start = p;

        // Find closing ']'
        while (*p && *p != ']') {
            if (!is_valid_path_char(*p)) {
                return PATH_PARSE_INVALID_DIR_CHAR;
            }
            p++;
        }

        if (*p != ']') {
            return PATH_PARSE_UNCLOSED_DIRECTORY;
        }

        if (p > start) {
            *p                = '\0';
            result->directory = start;
        }

        p++; // Skip ']'
    }

    if (*p != '\0') {
        start = p;
        while (*p) {
            if (!is_valid_path_char(*p)) {
                return PATH_PARSE_INVALID_FILE_CHAR;
            }
            p++;
        }
        result->filename = start;
    }

    return PATH_PARSE_OK;
}

char *path_to_unix(path_t *path) {
    if (path->unixpath) {
        return path->unixpath;
    }

    // Most unix paths are shorter than BadgeVMS paths except for directory-less paths.
    char  *unixpath      = malloc(path->len + 3);
    size_t device_len    = strlen(path->device);
    size_t directory_len = 0;
    size_t filename_len  = 0;

    if (path->directory) {
        directory_len = strlen(path->directory);
    }

    if (path->filename) {
        filename_len = strlen(path->filename);
    }

    size_t o      = 0;
    unixpath[o++] = '/';
    memcpy(&unixpath[o], path->device, device_len);
    o             += device_len;
    unixpath[o++]  = '/';

    if (directory_len) {
        for (size_t i = 0; i < directory_len; ++i) {
            char c = path->directory[i];
            if (c == '.')
                c = '/';
            unixpath[o++] = c;
        }
        unixpath[o++] = '/';
    }

    if (filename_len) {
        memcpy(&unixpath[o], path->filename, filename_len);
        o += filename_len;
    }

    unixpath[o]    = 0;
    path->unixpath = unixpath;
    return unixpath;
}

void path_free(path_t *path) {
    free(path->buffer);
    free(path->unixpath);
}

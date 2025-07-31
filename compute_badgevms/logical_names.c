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

#define TAG logical_names

#ifndef ESP_LOGE
#define ESP_LOGE(tag, ...)                                                                                             \
    do {                                                                                                               \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        fprintf(stderr, "\n");                                                                                         \
    } while (0)
#endif

#include "logical_names.h"

#include "hash_helper.h"
#include "khash.h"

#include <stdio.h>

#include <ctype.h>

#define MAX_DIR_DEPTH     25
#define RESOLVE_MAX_DEPTH 15

typedef struct {
    char  *pointer;
    size_t len;
    bool   terminal;
    size_t count;
    size_t idx;
} raw_string_t;

static raw_string_t const raw_null = {
    .pointer = NULL,
    .len     = 0,
};

typedef struct {
    raw_string_t unparsable;
    raw_string_t device;
    raw_string_t dir_components[MAX_DIR_DEPTH];
    int          dir_count;
    raw_string_t filename;
    size_t       count;
} parsed_components_t;

static parsed_components_t const parsed_components_null = {
    .unparsable     = raw_null,
    .device         = raw_null,
    .dir_components = {raw_null},
    .dir_count      = 0,
    .filename       = raw_null,
    .count          = 0
};

KHASH_MAP_INIT_STR(lnametable, logical_name_target_t);
static khash_t(lnametable) * logical_name_table;

static inline bool raw_cmp(raw_string_t *l, raw_string_t *r) {
    if (l->pointer != r->pointer)
        return false;
    if (l->len != r->len)
        return false;

    return true;
}

static inline raw_string_t raw_from_cstr(char *cstr, bool terminal) {
    raw_string_t res = {
        .pointer  = cstr,
        .len      = strlen(cstr),
        .terminal = terminal,
        .count    = 1,
        .idx      = 0,
    };

    return res;
}

static inline raw_string_t raw_from_ptr(char *pointer, int len, bool terminal) {
    raw_string_t res = {
        .pointer  = pointer,
        .len      = len,
        .terminal = terminal,
        .count    = 1,
        .idx      = 0,
    };

    return res;
}

#ifndef RUN_TEST
static inline void raw_print(raw_string_t r) {
    if (r.len == 0 || r.pointer == NULL) {
        printf("len: %zi '(null)'", r.len);
        return;
    }
    char b           = r.pointer[r.len];
    r.pointer[r.len] = '\0';
    printf("len: %zi '%s'", r.len, r.pointer);
    r.pointer[r.len] = b;
}

static void parsed_components_dump(parsed_components_t components) {
    printf("Components dump: \n");
    printf("    Unparsable: ");
    raw_print(components.unparsable);
    printf("\n");
    printf("    Device    : ");
    raw_print(components.device);
    printf("\n");
    printf("    Filename  : ");
    raw_print(components.filename);
    printf("\n");
    for (int i = 0; i < components.dir_count; ++i) {
        printf("    Dir[%i]   : ", i);
        raw_print(components.dir_components[i]);
        printf("\n");
    }
}
#endif

static char *parsed_components_serialize(parsed_components_t components) {
    if (components.unparsable.len == 0 && components.device.len == 0) {
        // Null value
        return NULL;
    }

    if (components.unparsable.len != 0) {
        // Just copy the junk into a fresh c string
        char *res = malloc(components.unparsable.len + 1);
        memcpy(res, components.unparsable.pointer, components.unparsable.len);
        res[components.unparsable.len] = '\0';
        return res;
    }

    size_t result_len = components.device.len;
    // If our device name doesn't already have a ':' we will need to add it
    if (components.device.pointer[components.device.len - 1] != ':')
        result_len++;

    result_len += components.filename.len;
    for (int i = 0; i < components.dir_count; ++i) {
        result_len += components.dir_components[i].len;
    }

    // Directories + '[]'
    if (components.dir_count)
        result_len += 2;
    // One '.' per subdir
    if (components.dir_count > 1)
        result_len += components.dir_count - 1;

    char *res       = malloc(result_len + 1);
    res[result_len] = '\0';

    size_t offset = 0;
    memcpy(res, components.device.pointer, components.device.len);
    offset += components.device.len;

    // Our device doesn't already have a ':', add it.
    if (components.device.pointer[components.device.len - 1] != ':')
        res[offset++] = ':';

    if (components.dir_count) {
        res[offset++] = '[';
        for (int i = 0; i < components.dir_count; ++i) {
            // Separator dot for subdirs
            if (i)
                res[offset++] = '.';
            memcpy(res + offset, components.dir_components[i].pointer, components.dir_components[i].len);
            offset += components.dir_components[i].len;
        }
        res[offset++] = ']';
    }

    if (components.filename.len) {
        memcpy(res + offset, components.filename.pointer, components.filename.len);
    }

    return res;
}

static inline bool path_cmp(parsed_components_t *l, parsed_components_t *r) {
    if (l->count != r->count)
        return false;

    if (l->unparsable.len != r->unparsable.len)
        return false;
    if (l->unparsable.len && r->unparsable.len) {
        if (raw_cmp(&l->unparsable, &r->unparsable))
            return true;
    }

    if (l->dir_count != r->dir_count)
        return false;
    if (!raw_cmp(&l->device, &r->device))
        return false;
    if (!raw_cmp(&l->filename, &r->filename))
        return false;

    for (int i = 0; i < l->dir_count; ++i) {
        if (!raw_cmp(&l->dir_components[i], &r->dir_components[i]))
            return false;
    }

    return true;
}

static inline parsed_components_t parse_string(raw_string_t str) {
    parsed_components_t res = parsed_components_null;
    // We always have something, even if it is just unparsable
    res.count               = 1;

    char *device_separator = NULL;
    char *dir_start        = NULL;
    char *dir_end          = NULL;
    char *last_dir         = NULL;

    size_t string_size = str.len;
    for (size_t i = 0; i < string_size; ++i) {
        char c = str.pointer[i];

        if (c == ':') {
            // Device separator
            if (device_separator)
                goto error;
            device_separator = str.pointer + i;
            res.device       = raw_from_ptr(str.pointer, i, false);
            continue;
        }

        if (c == '[') {
            // Dir start
            if (dir_start)
                goto error;
            dir_start = str.pointer + i;
            last_dir  = str.pointer + i + 1;
            continue;
        }

        if (c == ']') {
            // Dir end
            if (dir_end)
                goto error;
            dir_end = str.pointer + i;
            res.dir_components[res.dir_count] =
                raw_from_ptr(last_dir, (uintptr_t)(str.pointer + i) - (uintptr_t)last_dir, false);
            res.dir_count++;
            continue;
        }

        if (c == '.' && dir_start && !dir_end) {
            // We're parsing directories
            res.dir_components[res.dir_count] =
                raw_from_ptr(last_dir, (uintptr_t)(str.pointer + i) - (uintptr_t)last_dir, false);
            res.dir_count++;
            last_dir = str.pointer + i + 1;
        }
    }

    // No device separator
    if (!device_separator)
        goto error;
    // Opening '[' but no closing ']'
    if (dir_start && !dir_end)
        goto error;

    if (dir_end) {
        if (dir_end + 1 < str.pointer + string_size) {
            // We have a filename after a directory
            res.filename =
                raw_from_ptr(dir_end + 1, string_size - ((uintptr_t)(dir_end + 1) - (uintptr_t)str.pointer), false);
        }
        // There was no file part
    } else if (device_separator + 1 < str.pointer + string_size) {
        // We have a filename directly after the device
        res.filename = raw_from_ptr(
            device_separator + 1,
            string_size - ((uintptr_t)(device_separator + 1) - (uintptr_t)str.pointer),
            false
        );
    }

    return res;

error:
    res.unparsable = str;
    return res;
}

static inline parsed_components_t parse_cstring(char *cstr) {
    raw_string_t str = raw_from_cstr(cstr, false);
    return parse_string(str);
}

static raw_string_t resolve_string(raw_string_t string, size_t idx, int depth) {
    if (string.terminal)
        return string;

    if (depth > RESOLVE_MAX_DEPTH || string.len == 0) {
        return raw_null;
    }

    // This is safe because we are always pointing into a cstr
    // the next character is, at worst, already a '\0'
    char char_bak              = string.pointer[string.len];
    string.pointer[string.len] = '\0';

    logical_name_target_t *name;
    khint_t                k = kh_get(lnametable, logical_name_table, string.pointer);
    if (k == kh_end(logical_name_table)) {
        string.pointer[string.len] = char_bak; // Restore character
        return string;
    } else {
        string.pointer[string.len] = char_bak; // Restore character
        name                       = &kh_val(logical_name_table, k);
        raw_string_t new_string;
        if (name->target_count > 1) {
            // If we see an invalid index just get the first one
            size_t i         = idx > name->target_count - 1 ? 0 : idx;
            new_string       = raw_from_cstr((char *)name->target[i], name->terminal);
            new_string.count = name->target_count;
            new_string.idx   = idx;
        } else {
            new_string = raw_from_cstr((char *)name->target[0], name->terminal);
        }
        return resolve_string(new_string, idx, ++depth);
    }
}

static raw_string_t resolve_device_string(raw_string_t string, size_t idx, int depth) {
    if (string.terminal)
        return string;

    if (depth > RESOLVE_MAX_DEPTH || string.len == 0) {
        return raw_null;
    }

    // Special case for devices. Once we have a valid device path we don't
    // actually know whether the logical name is for DEVICE or DEVICE:
    // so we need to try both. We can rely on the fact that the input string
    // must have contained a ':' at some point, so there is definitely enough
    // space.

    char char_bak               = string.pointer[string.len];
    string.pointer[string.len]  = ':';
    string.len                 += 1;

    raw_string_t new_string = resolve_string(string, idx, depth);

    if (raw_cmp(&string, &new_string)) {
        // This didn't work. Try without the ':'
        string.len                 -= 1;
        string.pointer[string.len]  = char_bak;
        return resolve_string(string, idx, depth);
    }

    // Don't strip any trailing ':', this avoids trouble if both DEVICE
    // and DEVICE: are defined
    return new_string;
}

static parsed_components_t _logical_name_resolve(parsed_components_t path, size_t list_idx, int depth) {
    if (depth > RESOLVE_MAX_DEPTH) {
        return parsed_components_null;
    }

    if (path.unparsable.len) {
        // Just a string
        raw_string_t res = resolve_string(path.unparsable, 0, depth + 1);
        if (res.count > 1) {
            if (path.count == 1) {
                // Set the result count to our first list
                path.count = res.count;
                res        = resolve_string(path.unparsable, list_idx, depth + 1);
            }
        }

        // 1) If we looped or otherwise hit the max depth don't do anything
        // 2) If there was no change we are done
        if (!res.len || raw_cmp(&res, &path.unparsable)) {
            return path;
        }
        // We might have a path now. Try again.
        parsed_components_t new_path = parse_string(res);
        // Make sure we don't lose our result count
        new_path.count               = path.count;
        return (_logical_name_resolve(new_path, 0, depth + 1));
    }

    parsed_components_t orig_path = path;

    // Actual path of some kind
    raw_string_t new_device = resolve_device_string(path.device, 0, depth + 1);
    if (new_device.count > 1) {
        if (path.count == 1) {
            // Set the result count to our first list
            path.count = new_device.count;
            // Re-resolve using the first list, only the first time
            new_device = resolve_device_string(path.device, list_idx, depth + 1);
        }
    }

    if (!raw_cmp(&new_device, &path.device)) {
        parsed_components_t device_path = parse_string(new_device);
        if (device_path.unparsable.len) {
            // Still just a plain string
            path.device = new_device;
        } else {
            // Our device might have expanded into a bigger path
            if (device_path.dir_count) {
                // Our device expanded to something with directories
                // insert them to the left of our existing directories
                if (path.dir_count) {
                    // Move our existing directories to the right
                    memmove(
                        &path.dir_components[device_path.dir_count],
                        path.dir_components,
                        path.dir_count * sizeof(raw_string_t)
                    );
                }
                path.dir_count += device_path.dir_count;

                // Insert our new dir components
                memcpy(path.dir_components, device_path.dir_components, device_path.dir_count * sizeof(raw_string_t));
            }

            if (device_path.filename.len) {
                path.filename = device_path.filename;
            }

            if (device_path.device.len) {
                path.device = device_path.device;
            }
        }
    }

    path.filename = resolve_string(path.filename, 0, depth + 1);
    for (int i = 0; i < path.dir_count; ++i) {
        path.dir_components[i] = resolve_string(path.dir_components[i], 0, depth + 1);
    }

    if (path_cmp(&orig_path, &path)) {
        return path;
    }

    return _logical_name_resolve(path, list_idx, depth + 1);
}

void logical_names_system_init() {
    logical_name_table = kh_init(lnametable);
}

int logical_name_set(char const *logical_name, char const *target, bool is_terminal) {
    logical_name_target_t name;
    name.target_count  = 0;
    size_t name_size   = strlen(target);
    size_t last_name   = 0;
    size_t num_targets = 1;

    // Count elements
    for (size_t i = 0; i < name_size; ++i) {
        char c = target[i];
        if (c == ',' || c == '\0')
            ++num_targets;
    }

    name.target = malloc(num_targets * sizeof(char *));

    // See if we have a list
    for (size_t i = 0; i <= name_size; ++i) {
        char c = target[i];

        if (c == ',' || c == '\0') {
            // We found a component
            // Strip whitespace
            size_t end = i;
            for (size_t k = last_name; k < i; ++k) {
                if (isspace((int)target[k])) {
                    last_name++;
                    continue;
                }
                break;
            }
            for (size_t k = i; k > last_name; --k) {
                if (isspace((int)target[k])) {
                    --end;
                    continue;
                }
                break;
            }
            size_t component_size = end - last_name;
            if (component_size && component_size <= name_size) {
                // Copy the string so we won't have to copy it later when resolving
                // and add 1 more byte because we might need to resolve it as a device
                char *t = malloc(component_size + 2);
                memcpy(t, target + last_name, component_size);
                t[component_size]              = '\0';
                name.target[name.target_count] = t;
                name.target_count++;
                last_name = i + 1; // Skip ','
            }
        }
    }

    name.terminal = is_terminal;

    if (name.target_count) {
        khash_insert_str(lnametable, logical_name_table, logical_name, name, char const *);
        return 0;
    }

    free(name.target);
    return 1;
}

logical_name_target_t logical_name_get(char const *logical_name) {
    khint_t k = kh_get(lnametable, logical_name_table, logical_name);
    if (k == kh_end(logical_name_table)) {
        logical_name_target_t res = {NULL, 0, false};
        return res;
    } else {
        return kh_val(logical_name_table, k);
    }
}

void logical_name_del(char const *logical_name) {
    khash_del_str(lnametable, logical_name_table, logical_name, "Logical name did not exist");
}

logical_name_result_t logical_name_resolve(char *logical_name, size_t idx) {
    logical_name_result_t result;
    if (!logical_name || !strlen(logical_name)) {
        // Don't try and parse empty strings or NULL
        result.result_count = 0;
        result.result       = NULL;
        goto out;
    }
    parsed_components_t parsed = _logical_name_resolve(parse_cstring(logical_name), idx, 0);
    result.result              = parsed_components_serialize(parsed);
    result.result_count        = parsed.count;

out:
    return result;
}

logical_name_result_t logical_name_resolve_const(char const *logical_name, size_t idx) {
    char                 *tmp    = strdup(logical_name);
    logical_name_result_t result = logical_name_resolve(tmp, idx);
    free(tmp);
    return result;
}

void logical_name_result_free(logical_name_result_t result) {
    free(result.result);
}

#ifdef RUN_TEST

typedef struct {
    char const *in;
    char const *expect;
    size_t      expect_count;
    size_t      idx;
} test_t;

test_t tests[] = {
    // Undefined strings should pass right through
    {"STRING", "STRING", 1, 0},
    {"DEVICE:", "DEVICE:", 1, 0},
    {"DEVICE:filename.ext", "DEVICE:filename.ext", 1, 0},
    {"DEVICE:[dira]filename.ext", "DEVICE:[dira]filename.ext", 1, 0},
    {"DEVICE:[dira.dirb.dirc]filename.ext", "DEVICE:[dira.dirb.dirc]filename.ext", 1, 0},

    // Simple substitutions
    {"SIMPLE", "STRING", 1, 0},
    {"SIMPLEDEV:", "MY_SIMPLEDEV:", 1, 0},

    {"USER:", "MYFLASH:[dira]", 1, 0},
    {"FLASH0:", "MYFLASH:", 1, 0},
    {"FLASH0", "MYFLASH", 1, 0},
    {"USER:file.txt", "MYFLASH:[dira]file.txt", 1, 0},
    {"USER:[dirb.dirc]file.txt", "MYFLASH:[dira.dirb.dirc]file.txt", 1, 0},
    {"TEST1:", "DRIVE0:", 1, 0},
    {"TEST2:", "DRIVE0:", 1, 0},
    {"TEST3:", "DRIVE0:[dira]", 1, 0},
    {"TEST4:", "DRIVE0:[dira.dirb]", 1, 0},
    {"TEST5", "DRIVE0:[dira.dirb]filename.ext", 1, 0},

    // Directory name substitions
    {"USER:[DIR1]", "MYFLASH:[dira.SUBST1]", 1, 0},
    {"USER:[DIR1.DIR2]", "MYFLASH:[dira.SUBST1.SUBST2]", 1, 0},
    {"USER:[DIR1.DIR2.DIR3]", "MYFLASH:[dira.SUBST1.SUBST2.STRING]", 1, 0},

    // File name substititions
    {"USER:[DIR1]FILE", "MYFLASH:[dira.SUBST1]FILE", 1, 0},
    {"USER:[DIR1]FILE1", "MYFLASH:[dira.SUBST1]FILENAME.EXT", 1, 0},
    {"USER:[DIR1]FILE2", "MYFLASH:[dira.SUBST1]INDIRECT.EXT", 1, 0},

    // Terminals
    {"CIRC3", "CIRC3", 1, 0},   // CIRC4 is terminal
    {"CIRC4", "CIRC3", 1, 0},   // CIRC4 is terminal
    {"USER2:", "TERM3:", 1, 0}, // TERM2 is terminal

    // Lists
    {"LIST1", "ONE", 3, 0},
    {"LIST1", "TWO", 3, 1},
    {"LIST1", "THREE", 3, 2},

    {"LIST2", "MYFLASH:[dira]", 1, 0},
    {"SEARCH:", "DRIVE0:[SUBDIR]", 2, 0},
    {"SEARCH:", "DRIVE0:[SUBDIR.ANOTHER]", 2, 1},

    // Error checks
    // Return original for loops
    {"CIRC1", "CIRC1", 1, 0},
    {"CIRC2", "CIRC2", 1, 0},

    // Bad path, treat as string
    {"BAD:[unclosed", "BAD:[unclosed", 1, 0},
    {"DOUBLE::COLON", "DOUBLE::COLON", 1, 0},

    // Nested lists
    {"LIST3", "ONE", 3, 0},
    {"LIST3", "MYFLASH", 3, 1}, // Nested lists get kinda screwy
    {"LIST3", "THREE", 3, 2},

    //{"", NULL, 0, 0},

    {NULL, NULL, 0, 0},
};

int main() {
    logical_names_system_init();

    logical_name_set("SIMPLE", "STRING", false);
    logical_name_set("DIR1", "SUBST1", false);
    logical_name_set("DIR2", "SUBST2", false);
    logical_name_set("DIR3", "SIMPLE", false);
    logical_name_set("FILE1", "FILENAME.EXT", false);
    logical_name_set("FILE2", "FILE3", false);
    logical_name_set("FILE3", "INDIRECT.EXT", false);
    logical_name_set("SIMPLEDEV:", "MY_SIMPLEDEV:", false);
    logical_name_set("USER", "FLASH0:[dira]", false);
    logical_name_set("FLASH0", "MYFLASH", false);
    logical_name_set("TEST1", "DRIVE0", false);
    logical_name_set("TEST2", "TEST1:", false);
    logical_name_set("TEST3", "TEST2:[dira]", false);
    logical_name_set("TEST4", "TEST3:[dirb]", false);
    logical_name_set("TEST5", "TEST4:filename.ext", false);

    logical_name_set("CIRC1", "CIRC2", false);
    logical_name_set("CIRC2", "CIRC1", false);

    logical_name_set("CIRC3", "CIRC4", false);
    logical_name_set("CIRC4", "CIRC3", true);

    logical_name_set("USER2:", "TERM1", false);
    logical_name_set("TERM1", "TERM2", false);
    logical_name_set("TERM2", "TERM3", true);
    logical_name_set("TERM3", "UNREACHABLE", false);

    logical_name_set("LIST1", "ONE, TWO, THREE", false);
    logical_name_set("LIST2", "USER, FLASH0", false);
    logical_name_set("LIST3", "LIST1, LIST2", false);

    logical_name_set("SEARCH", "DRIVE0:[SUBDIR], DRIVE0:[SUBDIR.ANOTHER]", false);

    bool                  error = false;
    // Test getting
    logical_name_target_t t;
    t = logical_name_get("SIMPLE");
    if (t.target_count != 1) {
        printf("\033[31mlogical_name_get(\"SIMPLE\") target_count != 1\033[0m\n");
        error = true;
    }
    if (strcmp(t.target[0], "STRING") != 0) {
        printf("\033[31mlogical_name_get(\"SIMPLE\") result string invalid, got '%s'\033[0m\n", t.target[0]);
        error = true;
    }
    t = logical_name_get("SEARCH");
    if (t.target_count != 2) {
        printf("\033[31mlogical_name_get(\"SEARCH\") target_count != 2\033[0m\n");
        error = true;
    }
    if (strcmp(t.target[0], "DRIVE0:[SUBDIR]") != 0) {
        printf("\033[31mlogical_name_get(\"SEARCH\") result string invalid, got '%s'\033[0m\n", t.target[0]);
        error = true;
    }
    if (strcmp(t.target[1], "DRIVE0:[SUBDIR.ANOTHER]") != 0) {
        printf("\033[31mlogical_name_get(\"SEARCH\") result string invalid, got '%s'\033[0m\n", t.target[0]);
        error = true;
    }

    logical_name_result_t res;

    test_t *test = tests;
    while (test->in) {
        printf("=== Running test for '%s' ('%s') === \n", test->in, test->expect);
        res       = logical_name_resolve_const(test->in, test->idx);
        bool fail = false;

        if (res.result_count != test->expect_count)
            fail = true;
        if (res.result_count && test->expect && strcmp(test->expect, res.result) != 0)
            fail = true;
        if (res.result_count && !test->expect)
            fail = true;
        if (!res.result_count && test->expect)
            fail = true;

        if (fail) {
            printf(
                "\033[31mTestcase '%s' failed.\n\tExpected: '%s'\n\tActual:   '%s'\n\tCount:    %zi\n\tActual:   "
                "%zi\033[0m\n",
                test->in,
                test->expect,
                res.result,
                test->expect_count,
                res.result_count
            );
            error = true;
        }
        printf("=== End     test for %s === \n\n", test->in);
        free(res.result);
        ++test;
    }

    if (!error) {
        printf("\033[32mAll tests passed\033[0m\n");
    }

    logical_name_target_t x;
    kh_foreach_value(logical_name_table, x, {
        for (size_t i = 0; i < x.target_count; ++i) {
            free((void *)x.target[i]);
        }
        free((void *)x.target);
    });
    kh_destroy(lnametable, logical_name_table);

    if (error)
        return 1;
    return 0;
}
#endif

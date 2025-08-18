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
#include "logical_names.h"
#include "task.h"
#include "why_io.h"

#include <dirent.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define kcalloc(N, Z)  why_calloc(N, Z)
#define kmalloc(Z)     why_malloc(Z)
#define krealloc(P, Z) why_realloc(P, Z)
#define kfree(P)       why_free(P)

#include "thirdparty/khash.h"

KHASH_SET_INIT_STR(seen);
typedef khash_t(seen) * seen_names_t;

typedef int (*fs_operation_func)(filesystem_device_t *fs_dev, path_t *path, void *extra_data);
typedef int (*why_helper_func)(char const *resolved_path, void *extra_data);

typedef struct why_dirent_entry {
    struct dirent            dirent;        // Standard dirent structure
    char                    *badgevms_name; // BadgeVMS name
    struct why_dirent_entry *next;
} why_dirent_entry_t;

typedef struct why_dir {
    char               *original_badgevms_path; // Original BadgeVMS path requested
    why_dirent_entry_t *entries;
    why_dirent_entry_t *current;
    size_t              entry_count;
} why_dir_t;

static int _why_filesystem_op(char const *resolved_path, fs_operation_func operation, void *extra_data) {
    path_t parsed_path;
    int    res = parse_path(resolved_path, &parsed_path);

    if (res != 0) {
        return -1;
    }

    device_t *device = device_get(parsed_path.device);
    if (!device || device->type != DEVICE_TYPE_FILESYSTEM) {
        path_free(&parsed_path);
        return -1;
    }

    filesystem_device_t *fs_device = (filesystem_device_t *)device;

    int result = operation(fs_device, &parsed_path, extra_data);

    path_free(&parsed_path);
    return result;
}

static int _why_searchlist_first_found(
    char const *pathname, char const *operation_name, why_helper_func helper_func, void *extra_data
) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI(operation_name, "Calling %s from task %p for path %s", operation_name, task_info->handle, pathname);

    logical_name_result_t lname        = logical_name_resolve_const(pathname, 0);
    size_t                result_count = lname.result_count;

    ESP_LOGI(operation_name, "Processing %s, %zi options", pathname, result_count);
    for (int i = 0; i < result_count; ++i) {
        ESP_LOGI(operation_name, "Trying location: %s", lname.result);

        int result = helper_func(lname.result, extra_data);
        if (result == 0) {
            ESP_LOGI(operation_name, "Success at %s", lname.result);
            logical_name_result_free(lname);
            return 0;
        }

        logical_name_result_free(lname);
        ESP_LOGI(operation_name, "Resolving at index %i", i + 1);
        lname = logical_name_resolve_const(pathname, i + 1);
    }

    task_info->_errno = ENOENT;
    logical_name_result_free(lname);
    ESP_LOGI(operation_name, "%s failed in all search locations", pathname);
    return -1;
}

static int _why_searchlist_try_all(
    char const *pathname, char const *operation_name, why_helper_func helper_func, void *extra_data
) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI(operation_name, "Calling %s from task %p for path %s", operation_name, task_info->handle, pathname);

    logical_name_result_t lname        = logical_name_resolve_const(pathname, 0);
    size_t                result_count = lname.result_count;
    bool                  any_success  = false;

    ESP_LOGI(operation_name, "Processing %s in all %zi locations", pathname, result_count);
    for (int i = 0; i < result_count; ++i) {
        ESP_LOGI(operation_name, "Trying location: %s", lname.result);

        int result = helper_func(lname.result, extra_data);
        if (result == 0) {
            ESP_LOGI(operation_name, "Success at %s", lname.result);
            any_success = true;
        }

        logical_name_result_free(lname);
        ESP_LOGI(operation_name, "Resolving at index %i", i + 1);
        lname = logical_name_resolve_const(pathname, i + 1);
    }

    logical_name_result_free(lname);

    if (any_success) {
        ESP_LOGI(operation_name, "%s succeeded in at least one location", pathname);
        return 0;
    } else {
        task_info->_errno = ENOENT;
        ESP_LOGI(operation_name, "%s failed in all search locations", pathname);
        return -1;
    }
}

static int _why_rename_single(char const *oldpath_resolved, char const *newpath_resolved) {
    path_t parsed_oldpath, parsed_newpath;
    int    res_old = parse_path(oldpath_resolved, &parsed_oldpath);
    int    res_new = parse_path(newpath_resolved, &parsed_newpath);

    if (res_old != 0 || res_new != 0) {
        if (res_old == 0)
            path_free(&parsed_oldpath);
        if (res_new == 0)
            path_free(&parsed_newpath);
        return -1;
    }

    device_t *device = device_get(parsed_oldpath.device);
    if (!device || device->type != DEVICE_TYPE_FILESYSTEM ||
        strcmp(parsed_oldpath.device, parsed_newpath.device) != 0) {
        ESP_LOGW(
            "_why_rename_single",
            "Unable to open device for %s -> %s, dev: %s -> %s",
            oldpath_resolved,
            newpath_resolved,
            parsed_oldpath.device,
            parsed_newpath.device
        );
        path_free(&parsed_oldpath);
        path_free(&parsed_newpath);
        return -1;
    }

    filesystem_device_t *fs_device = (filesystem_device_t *)device;
    if (!fs_device->_rename) {
        path_free(&parsed_oldpath);
        path_free(&parsed_newpath);
        return -1;
    }

    int result = fs_device->_rename(fs_device, &parsed_oldpath, &parsed_newpath);

    path_free(&parsed_oldpath);
    path_free(&parsed_newpath);
    return result;
}

static int _stat_operation(filesystem_device_t *fs_dev, path_t *path, void *extra_data) {
    if (!fs_dev->_stat)
        return -1;
    struct stat *statbuf = (struct stat *)extra_data;
    return fs_dev->_stat(fs_dev, path, statbuf);
}

static int _unlink_operation(filesystem_device_t *fs_dev, path_t *path, void *extra_data) {
    if (!fs_dev->_unlink)
        return -1;
    return fs_dev->_unlink(fs_dev, path);
}

static int _mkdir_operation(filesystem_device_t *fs_dev, path_t *path, void *extra_data) {
    if (!fs_dev->_mkdir)
        return -1;
    mode_t mode = *(mode_t *)extra_data;
    return fs_dev->_mkdir(fs_dev, path, mode);
}

static int _rmdir_operation(filesystem_device_t *fs_dev, path_t *path, void *extra_data) {
    if (!fs_dev->_rmdir)
        return -1;
    return fs_dev->_rmdir(fs_dev, path);
}

static int _why_stat_wrapper(char const *resolved_path, void *extra_data) {
    struct stat *statbuf = (struct stat *)extra_data;
    return _why_filesystem_op(resolved_path, _stat_operation, statbuf);
}

static int _why_unlink_wrapper(char const *resolved_path, void *extra_data) {
    return _why_filesystem_op(resolved_path, _unlink_operation, NULL);
}

static int _why_mkdir_wrapper(char const *resolved_path, void *extra_data) {
    mode_t mode = *(mode_t *)extra_data;
    return _why_filesystem_op(resolved_path, _mkdir_operation, &mode);
}

static int _why_rmdir_wrapper(char const *resolved_path, void *extra_data) {
    return _why_filesystem_op(resolved_path, _rmdir_operation, NULL);
}

int why_stat(char const *restrict pathname, struct stat *restrict statbuf) {
    if (!pathname || !statbuf) {
        get_task_info()->_errno = EINVAL;
        return -1;
    }
    return _why_searchlist_first_found(pathname, "why_stat", _why_stat_wrapper, statbuf);
}

int why_unlink(char const *pathname) {
    if (!pathname) {
        get_task_info()->_errno = EINVAL;
        return -1;
    }
    return _why_searchlist_first_found(pathname, "why_unlink", _why_unlink_wrapper, NULL);
}

int why_mkdir(char const *pathname, mode_t mode) {
    if (!pathname) {
        get_task_info()->_errno = EINVAL;
        return -1;
    }
    return _why_searchlist_first_found(pathname, "why_mkdir", _why_mkdir_wrapper, &mode);
}

int why_rmdir(char const *pathname) {
    if (!pathname) {
        get_task_info()->_errno = EINVAL;
        return -1;
    }
    return _why_searchlist_try_all(pathname, "why_rmdir", _why_rmdir_wrapper, NULL);
}

int why_fstat(int fd, struct stat *restrict statbuf) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_fstat", "Calling fstat from task %p for fd %d", task_info->handle, fd);

    if (!statbuf) {
        task_info->_errno = EINVAL;
        return -1;
    }

    if (fd < 0 || fd >= MAXFD || !task_info->thread->file_handles[fd].is_open) {
        task_info->_errno = EBADF;
        return -1;
    }

    device_t *device = task_info->thread->file_handles[fd].device;
    if (!device || device->type != DEVICE_TYPE_FILESYSTEM) {
        task_info->_errno = ENOTTY;
        return -1;
    }

    filesystem_device_t *fs_device = (filesystem_device_t *)device;
    if (!fs_device->_fstat) {
        task_info->_errno = ENOSYS;
        return -1;
    }

    return fs_device->_fstat(fs_device, task_info->thread->file_handles[fd].dev_fd, statbuf);
}

int why_rename(char const *oldpath, char const *newpath) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_rename", "Calling rename from task %p: %s -> %s", task_info->handle, oldpath, newpath);

    if (!oldpath || !newpath) {
        task_info->_errno = EINVAL;
        return -1;
    }

    // For rename, we use first-found semantics on oldpath
    // and assume newpath should be on the same filesystem
    logical_name_result_t oldname   = logical_name_resolve_const(oldpath, 0);
    logical_name_result_t newname   = logical_name_resolve_const(newpath, 0);
    size_t                old_count = oldname.result_count;

    ESP_LOGI("why_rename", "Finding oldpath %s, %zi options", oldpath, old_count);
    for (int i = 0; i < old_count; ++i) {
        ESP_LOGI("why_rename", "Trying oldpath location: %s", oldname.result);

        int rename_result = _why_rename_single(oldname.result, newname.result);
        if (rename_result == 0) {
            ESP_LOGI("why_rename", "Successfully renamed %s to %s", oldname.result, newname.result);
            logical_name_result_free(oldname);
            logical_name_result_free(newname);
            return 0;
        }

        logical_name_result_free(oldname);
        logical_name_result_free(newname);
        ESP_LOGI("why_rename", "Resolving oldpath at index %i", i + 1);
        oldname = logical_name_resolve_const(oldpath, i + 1);
        newname = logical_name_resolve_const(newpath, i + 1);
    }

    task_info->_errno = ENOENT;
    logical_name_result_free(oldname);
    logical_name_result_free(newname);
    ESP_LOGI("why_rename", "Rename %s -> %s failed in all search locations", oldpath, newpath);
    return -1;
}

int why_remove(char const *pathname) {
    if (!pathname) {
        get_task_info()->_errno = EINVAL;
        return -1;
    }

    struct stat statbuf;
    if (why_stat(pathname, &statbuf) != 0) {
        // why_stat already set errno
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        return why_rmdir(pathname);
    } else {
        return why_unlink(pathname);
    }
}

static void seen_names_init(seen_names_t *seen) {
    *seen = kh_init(seen);
}

static bool seen_names_contains(seen_names_t seen, char const *name) {
    khiter_t k = kh_get(seen, seen, name);
    return (k != kh_end(seen));
}

static bool seen_names_add(seen_names_t seen, char const *name) {
    if (seen_names_contains(seen, name)) {
        return false; // Already seen (shadowed)
    }

    int   ret;
    char *name_copy = why_strdup(name);
    if (!name_copy)
        return false;

    kh_put(seen, seen, name_copy, &ret);
    if (ret < 0) {
        why_free(name_copy); // Put failed
        return false;
    }
    return true;
}

static void seen_names_cleanup(seen_names_t seen) {
    khiter_t k;
    // Free all the strdup'd keys
    for (k = kh_begin(seen); k != kh_end(seen); ++k) {
        if (kh_exist(seen, k)) {
            why_free((void *)kh_key(seen, k));
        }
    }
    kh_destroy(seen, seen);
}

static why_dirent_entry_t *create_dirent_entry(const struct dirent *original_dirent, char const *badgevms_name) {
    why_dirent_entry_t *entry = why_malloc(sizeof(why_dirent_entry_t));
    if (!entry)
        return NULL;

    memcpy(&entry->dirent, original_dirent, sizeof(struct dirent));

    entry->badgevms_name = why_strdup(badgevms_name);
    if (!entry->badgevms_name) {
        why_free(entry);
        return NULL;
    }

    strncpy(entry->dirent.d_name, badgevms_name, sizeof(entry->dirent.d_name) - 1);
    entry->dirent.d_name[sizeof(entry->dirent.d_name) - 1] = '\0';

    entry->next = NULL;
    return entry;
}

static void free_dirent_entry(why_dirent_entry_t *entry) {
    if (entry) {
        why_free(entry->badgevms_name);
        why_free(entry);
    }
}

static void add_entry_to_list(why_dir_t *dir, why_dirent_entry_t *entry) {
    entry->next  = dir->entries;
    dir->entries = entry;
    dir->entry_count++;
}

static bool read_directory_location(char const *resolved_path, why_dir_t *merged_dir, seen_names_t seen) {
    path_t parsed_path;
    int    res = parse_path(resolved_path, &parsed_path);

    if (res != 0) {
        return false;
    }

    device_t *device = device_get(parsed_path.device);
    if (!device || device->type != DEVICE_TYPE_FILESYSTEM) {
        path_free(&parsed_path);
        return false;
    }

    filesystem_device_t *fs_device = (filesystem_device_t *)device;
    if (!fs_device->_opendir || !fs_device->_readdir || !fs_device->_closedir) {
        path_free(&parsed_path);
        return false;
    }

    DIR *device_dir = fs_device->_opendir(fs_device, &parsed_path);
    if (!device_dir) {
        path_free(&parsed_path);
        return false;
    }

    struct dirent *entry;
    while ((entry = fs_device->_readdir(fs_device, device_dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (seen_names_add(seen, entry->d_name)) {
            why_dirent_entry_t *new_entry = create_dirent_entry(entry, entry->d_name);
            if (new_entry) {
                add_entry_to_list(merged_dir, new_entry);
            }
        }
    }

    fs_device->_closedir(fs_device, device_dir);
    path_free(&parsed_path);

    return true;
}

DIR *why_opendir(char const *name) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_opendir", "Calling opendir from task %p for path %s", task_info->handle, name);

    if (!name) {
        task_info->_errno = EINVAL;
        return NULL;
    }

    why_dir_t *merged_dir = why_malloc(sizeof(why_dir_t));
    if (!merged_dir) {
        task_info->_errno = ENOMEM;
        return NULL;
    }

    merged_dir->original_badgevms_path = why_strdup(name);
    merged_dir->entries                = NULL;
    merged_dir->current                = NULL;
    merged_dir->entry_count            = 0;

    if (!merged_dir->original_badgevms_path) {
        why_free(merged_dir);
        task_info->_errno = ENOMEM;
        return NULL;
    }

    seen_names_t seen;
    seen_names_init(&seen);

    logical_name_result_t lname               = logical_name_resolve_const(name, 0);
    size_t                result_count        = lname.result_count;
    bool                  found_any_directory = false;

    ESP_LOGI("why_opendir", "Reading directory %s from %zi locations", name, result_count);
    for (int i = 0; i < result_count; ++i) {
        ESP_LOGI("why_opendir", "Trying location: %s", lname.result);

        if (read_directory_location(lname.result, merged_dir, seen)) {
            ESP_LOGI("why_opendir", "Successfully read entries from %s", lname.result);
            found_any_directory = true;
        }

        logical_name_result_free(lname);
        ESP_LOGI("why_opendir", "Resolving at index %i", i + 1);
        lname = logical_name_resolve_const(name, i + 1);
    }

    logical_name_result_free(lname);
    seen_names_cleanup(seen);

    if (!found_any_directory) {
        ESP_LOGI("why_opendir", "No readable directories found for %s", name);
        why_free(merged_dir->original_badgevms_path);
        why_free(merged_dir);
        task_info->_errno = ENOENT;
        return NULL;
    }

    merged_dir->current = merged_dir->entries;

    ESP_LOGI("why_opendir", "Successfully opened merged directory %s with %zi entries", name, merged_dir->entry_count);

    return (DIR *)merged_dir;
}

struct dirent *why_readdir(DIR *dirp) {
    if (!dirp) {
        get_task_info()->_errno = EBADF;
        return NULL;
    }

    why_dir_t *merged_dir = (why_dir_t *)dirp;

    if (!merged_dir->current) {
        return NULL;
    }

    why_dirent_entry_t *current_entry = merged_dir->current;
    merged_dir->current               = current_entry->next;

    return &current_entry->dirent;
}

int why_closedir(DIR *dirp) {
    task_info_t *task_info = get_task_info();
    ESP_LOGI("why_closedir", "Calling closedir from task %p", task_info->handle);

    if (!dirp) {
        task_info->_errno = EBADF;
        return -1;
    }

    why_dir_t *merged_dir = (why_dir_t *)dirp;

    why_dirent_entry_t *current = merged_dir->entries;
    while (current) {
        why_dirent_entry_t *next = current->next;
        free_dirent_entry(current);
        current = next;
    }

    why_free(merged_dir->original_badgevms_path);
    why_free(merged_dir);

    return 0;
}

void why_rewinddir(DIR *dirp) {
    if (!dirp) {
        get_task_info()->_errno = EBADF;
        return;
    }

    why_dir_t *merged_dir = (why_dir_t *)dirp;

    merged_dir->current = merged_dir->entries;

    get_task_info()->_errno = 0;
}

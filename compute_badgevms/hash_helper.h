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

#define khash_get_str(_type, _val, _table_type, _table_ptr, _key, _error)                                              \
    _type _val = 0;                                                                                                    \
    do {                                                                                                               \
        khint_t k = kh_get(_table_type, _table_ptr, _key);                                                             \
        if (k == kh_end(_table_ptr)) {                                                                                 \
            ESP_LOGE(TAG, _error " %s", _key);                                                                         \
        } else {                                                                                                       \
            _val = (_type)kh_val(_table_ptr, k);                                                                       \
        }                                                                                                              \
    } while (0);

#define khash_insert_unique_str(_table_type, _table_ptr, _key, _value, _error)                                         \
    do {                                                                                                               \
        int     r;                                                                                                     \
        khint_t k = kh_get(_table_type, _table_ptr, _key);                                                             \
        if (k == kh_end(_table_ptr)) {                                                                                 \
            k = kh_put(_table_type, _table_ptr, _key, &r);                                                             \
            if (r >= 0) {                                                                                              \
                kh_value(_table_ptr, k) = _value;                                                                      \
            } else {                                                                                                   \
                ESP_LOGE(TAG, "Unable to create %s", _key);                                                            \
                abort();                                                                                               \
            }                                                                                                          \
        } else {                                                                                                       \
            ESP_LOGE(TAG, _error ": %s", _key);                                                                        \
        }                                                                                                              \
    } while (0);


#define khash_insert_str(_table_type, _table_ptr, _key, _value, _type)                                                 \
    do {                                                                                                               \
        int     r;                                                                                                     \
        khint_t k = kh_put(_table_type, _table_ptr, _key, &r);                                                         \
        if (r >= 0) {                                                                                                  \
            kh_value(_table_ptr, k) = _value;                                                                          \
        } else {                                                                                                       \
            ESP_LOGE(TAG, "Unable to create %s", _key);                                                                \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0);

#define khash_del_str(_table_type, _table_ptr, _key, _error)                                                           \
    do {                                                                                                               \
        khint_t k = kh_get(_table_type, _table_ptr, _key);                                                             \
        if (k != kh_end(_table_ptr)) {                                                                                 \
            kh_del(_table_type, _table_ptr, k);                                                                        \
        } else {                                                                                                       \
            ESP_LOGE(TAG, _error ": %s", _key);                                                                        \
        }                                                                                                              \
    } while (0);


#define khash_get_ptr(_type, _val, _table_type, _table_ptr, _key, _error)                                              \
    _type _val = 0;                                                                                                    \
    do {                                                                                                               \
        khint_t k = kh_get(_table_type, _table_ptr, (uintptr_t)_key);                                                  \
        if (k == kh_end(_table_ptr)) {                                                                                 \
            ESP_LOGE(TAG, _error " %p", _key);                                                                         \
        } else {                                                                                                       \
            _val = (_type)kh_val(_table_ptr, k);                                                                       \
        }                                                                                                              \
    } while (0);

#define khash_insert_unique_ptr(_table_type, _table_ptr, _key, _value, _error)                                         \
    do {                                                                                                               \
        int     r;                                                                                                     \
        khint_t k = kh_get(_table_type, _table_ptr, (uintptr_t)_key);                                                  \
        if (k == kh_end(_table_ptr)) {                                                                                 \
            k = kh_put(_table_type, _table_ptr, (uintptr_t)_key, &r);                                                  \
            if (r >= 0) {                                                                                              \
                kh_value(_table_ptr, k) = _value;                                                                      \
            } else {                                                                                                   \
                ESP_LOGE(TAG, "Unable to create %p", _key);                                                            \
                abort();                                                                                               \
            }                                                                                                          \
        } else {                                                                                                       \
            ESP_LOGE(TAG, _error ": %p", _key);                                                                        \
        }                                                                                                              \
    } while (0);


#define khash_insert_ptr(_table_type, _table_ptr, _key, _value, _type)                                                 \
    do {                                                                                                               \
        int     r;                                                                                                     \
        khint_t k = kh_put(_table_type, _table_ptr, (uintptr_t)_key, &r);                                              \
        if (r >= 0) {                                                                                                  \
            kh_value(_table_ptr, k) = _value;                                                                          \
        } else {                                                                                                       \
            ESP_LOGE(TAG, "Unable to create %p", _key);                                                                \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0);

#define khash_del_ptr(_table_type, _table_ptr, _key, _error)                                                           \
    do {                                                                                                               \
        khint_t k = kh_get(_table_type, _table_ptr, (uintptr_t)_key);                                                  \
        if (k != kh_end(_table_ptr)) {                                                                                 \
            kh_del(_table_type, _table_ptr, k);                                                                        \
        } else {                                                                                                       \
            ESP_LOGE(TAG, _error ": %p", _key);                                                                        \
        }                                                                                                              \
    } while (0);

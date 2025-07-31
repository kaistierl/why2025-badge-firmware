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

#include "khash.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char  *result;
    size_t result_count;
} logical_name_result_t;

typedef struct {
    char **target;
    size_t target_count;
    bool   terminal;
} logical_name_target_t;

void                  logical_names_system_init();
int                   logical_name_set(char const *logical_name, char const *target, bool is_terminal);
logical_name_target_t logical_name_get(char const *logical_name);
void                  logical_name_del(char const *logical_name);
void                  logical_name_result_free(logical_name_result_t result);
logical_name_result_t logical_name_resolve(char *logical_name, size_t idx);
logical_name_result_t logical_name_resolve_const(char const *logical_name, size_t idx);

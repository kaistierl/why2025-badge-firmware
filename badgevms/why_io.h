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

#include "thirdparty/dlmalloc.h"

#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>

ssize_t why_write(int fd, void const *buf, size_t count);
ssize_t why_read(int fd, void *buf, size_t count);
off_t   why_lseek(int fd, off_t offset, int whence);
int     why_open(char const *pathname, int flags, mode_t mode);
int     why_close(int fd);

FILE *why_fopen(char const *restrict pathname, char const *restrict mode);
int   why_fclose(FILE *stream);

int   why_fgetc(FILE *stream);
int   why_getc(FILE *stream);
int   why_getchar(void);
char *why_fgets(char *s, int size, FILE *restrict stream);
int   why_ungetc(int c, FILE *stream);

int why_printf(char const *restrict format, ...);
int why_fprintf(FILE *restrict stream, char const *restrict format, ...);
int why_dprintf(int fd, char const *restrict format, ...);
int why_sprintf(char *restrict str, char const *restrict format, ...);
int why_snprintf(char *str, size_t size, char const *restrict format, ...);

int why_vprintf(char const *restrict format, va_list ap);
int why_vfprintf(FILE *restrict stream, char const *restrict format, va_list ap);
int why_vdprintf(int fd, char const *restrict format, va_list ap);
int why_vsprintf(char *restrict str, char const *restrict format, va_list ap);
int why_vsnprintf(char *str, size_t size, char const *restrict format, va_list ap);

int why_asprintf(char **restrict strp, char const *restrict fmt, ...);
int why_vasprintf(char **restrict strp, char const *restrict fmt, va_list ap);

char *why_strdup(char const *s);

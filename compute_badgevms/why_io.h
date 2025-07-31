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

#include <fcntl.h>
#include <unistd.h>

ssize_t why_write(int fd, void const *buf, size_t count);
ssize_t why_read(int fd, void *buf, size_t count);
off_t   why_lseek(int fd, off_t offset, int whence);
int     why_open(char const *pathname, int flags, mode_t mode);
int     why_close(int fd);

char *why_strdup(char const *s);

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

#define MIN_STACK_SIZE 16384

#define FRAMEBUFFER_MAX_W       720
#define FRAMEBUFFER_MAX_H       720
#define FRAMEBUFFER_MAX_REFRESH 60
#define FRAMEBUFFER_BPP         2 // 16 bits per pixel

#define FRAMEBUFFER_BYTES (FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_W * FRAMEBUFFER_BPP)

// Maximum number of pending events for a window
#define WINDOW_MAX_EVENTS 10

// Maximum windows allowed on the screen
#define MAX_WINDOWS 10

#define DISPLAY_FRAMEBUFFERS 3

#define I2C0_MASTER_FREQ_HZ 100 * 1000 // i2c bus speed for the i2c bus on the carrier board, being I2C_NUM_0

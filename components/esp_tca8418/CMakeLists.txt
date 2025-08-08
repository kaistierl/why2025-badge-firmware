###############################################################################
#
# TCA8418 Keypad driver
#
# SPDX-FileCopyrightText: 2024 Mike Dunston (atanisoft)
# SPDX-License-Identifier: BSD-2-Clause
#
###############################################################################

idf_component_register(SRCS "esp_tca8418.c"
                       INCLUDE_DIRS "include"  "${CMAKE_CURRENT_SOURCE_DIR}/../../badgevms"
                       REQUIRES "driver" "i2c_bus")

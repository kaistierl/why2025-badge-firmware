###############################################################################
#
# TCA8418 Keypad driver
#
# SPDX-FileCopyrightText: 2024 Mike Dunston (atanisoft)
# SPDX-License-Identifier: BSD-2-Clause
#
###############################################################################

idf_component_register(SRCS "esp_tca8418.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "driver" "i2c_bus")

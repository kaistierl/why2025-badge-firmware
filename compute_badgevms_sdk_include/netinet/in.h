/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IN_H_
#define IN_H_

#include <stdint.h>

typedef uint32_t in_addr_t;

struct in_addr {
  in_addr_t s_addr;
};

struct in6_addr {
  union {
    uint32_t u32_addr[4];
    uint8_t  u8_addr[16];
  } un;
};

#endif /* IN_H_ */

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

#include "badgevms/device.h"

#define wifi_status_t                  badgevms_wifi_status_t
#define wifi_connection_status_t       badgevms_wifi_connection_status_t
#define wifi_cipher_type_t             badgevms_wifi_cipher_type_t
#define wifi_auth_mode_t               badgevms_wifi_auth_mode_t
#define wifi_connection_mode_t         badgevms_wifi_connection_mode_t
#define WIFI_CIPHER_TYPE_NONE          BADGEVMS_WIFI_CIPHER_TYPE_NONE
#define WIFI_CIPHER_TYPE_WEP40         BADGEVMS_WIFI_CIPHER_TYPE_WEP40
#define WIFI_CIPHER_TYPE_WEP104        BADGEVMS_WIFI_CIPHER_TYPE_WEP104
#define WIFI_CIPHER_TYPE_TKIP          BADGEVMS_WIFI_CIPHER_TYPE_TKIP
#define WIFI_CIPHER_TYPE_CCMP          BADGEVMS_WIFI_CIPHER_TYPE_CCMP
#define WIFI_CIPHER_TYPE_TKIP_CCMP     BADGEVMS_WIFI_CIPHER_TYPE_TKIP_CCMP
#define WIFI_CIPHER_TYPE_AES_CMAC128   BADGEVMS_WIFI_CIPHER_TYPE_AES_CMAC128
#define WIFI_CIPHER_TYPE_SMS4          BADGEVMS_WIFI_CIPHER_TYPE_SMS4
#define WIFI_CIPHER_TYPE_GCMP          BADGEVMS_WIFI_CIPHER_TYPE_GCMP
#define WIFI_CIPHER_TYPE_GCMP256       BADGEVMS_WIFI_CIPHER_TYPE_GCMP256
#define WIFI_CIPHER_TYPE_AES_GMAC128   BADGEVMS_WIFI_CIPHER_TYPE_AES_GMAC128
#define WIFI_CIPHER_TYPE_AES_GMAC256   BADGEVMS_WIFI_CIPHER_TYPE_AES_GMAC256
#define WIFI_CIPHER_TYPE_UNKNOWN       BADGEVMS_WIFI_CIPHER_TYPE_UNKNOWN
#define WIFI_AUTH_NONE                 BADGEVMS_WIFI_AUTH_NONE
#define WIFI_AUTH_OPEN                 BADGEVMS_WIFI_AUTH_OPEN
#define WIFI_AUTH_WEP                  BADGEVMS_WIFI_AUTH_WEP
#define WIFI_AUTH_WPA_PSK              BADGEVMS_WIFI_AUTH_WPA_PSK
#define WIFI_AUTH_WPA2_PSK             BADGEVMS_WIFI_AUTH_WPA2_PSK
#define WIFI_AUTH_WPA_WPA2_PSK         BADGEVMS_WIFI_AUTH_WPA_WPA2_PSK
#define WIFI_AUTH_WPA2_ENTERPRISE      BADGEVMS_WIFI_AUTH_WPA2_ENTERPRISE
#define WIFI_AUTH_WPA3_PSK             BADGEVMS_WIFI_AUTH_WPA3_PSK
#define WIFI_AUTH_WPA2_WPA3_PSK        BADGEVMS_WIFI_AUTH_WPA2_WPA3_PSK
#define WIFI_AUTH_WAPI_PSK             BADGEVMS_WIFI_AUTH_WAPI_PSK
#define WIFI_AUTH_OWE                  BADGEVMS_WIFI_AUTH_OWE
#define WIFI_AUTH_WPA3_ENT_192         BADGEVMS_WIFI_AUTH_WPA3_ENT_192
#define WIFI_AUTH_DPP                  BADGEVMS_WIFI_AUTH_DPP
#define WIFI_AUTH_WPA3_ENTERPRISE      BADGEVMS_WIFI_AUTH_WPA3_ENTERPRISE
#define WIFI_AUTH_WPA2_WPA3_ENTERPRISE BADGEVMS_WIFI_AUTH_WPA2_WPA3_ENTERPRISE
#define WIFI_AUTH_WPA_ENTERPRISE       BADGEVMS_WIFI_AUTH_WPA_ENTERPRISE

#include "badgevms/wifi.h"

#undef WIFI_CIPHER_TYPE_NONE
#undef WIFI_CIPHER_TYPE_WEP40
#undef WIFI_CIPHER_TYPE_WEP104
#undef WIFI_CIPHER_TYPE_TKIP
#undef WIFI_CIPHER_TYPE_CCMP
#undef WIFI_CIPHER_TYPE_TKIP_CCMP
#undef WIFI_CIPHER_TYPE_AES_CMAC128
#undef WIFI_CIPHER_TYPE_SMS4
#undef WIFI_CIPHER_TYPE_GCMP
#undef WIFI_CIPHER_TYPE_GCMP256
#undef WIFI_CIPHER_TYPE_AES_GMAC128
#undef WIFI_CIPHER_TYPE_AES_GMAC256
#undef WIFI_CIPHER_TYPE_UNKNOWN
#undef WIFI_AUTH_NONE
#undef WIFI_AUTH_OPEN
#undef WIFI_AUTH_WEP
#undef WIFI_AUTH_WPA_PSK
#undef WIFI_AUTH_WPA2_PSK
#undef WIFI_AUTH_WPA_WPA2_PSK
#undef WIFI_AUTH_WPA2_ENTERPRISE
#undef WIFI_AUTH_WPA3_PSK
#undef WIFI_AUTH_WPA2_WPA3_PSK
#undef WIFI_AUTH_WAPI_PSK
#undef WIFI_AUTH_OWE
#undef WIFI_AUTH_WPA3_ENT_192
#undef WIFI_AUTH_DPP
#undef WIFI_AUTH_WPA3_ENTERPRISE
#undef WIFI_AUTH_WPA2_WPA3_ENTERPRISE
#undef WIFI_AUTH_WPA_ENTERPRISE

#undef wifi_connection_mode_t
#undef wifi_auth_mode_t
#undef wifi_cipher_type_t
#undef wifi_connection_status_t
#undef wifi_status_t

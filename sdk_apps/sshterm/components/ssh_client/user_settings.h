/* user_settings.h — wolfSSL/wolfSSH build configuration for sshterm
 *
 * SSH-client-only build targeting the WHY2025 badge (ESP32-P4 + BadgeVMS).
 * TLS, certificate issuance, PKCS7, and all server-side features are
 * disabled. HW crypto acceleration is also disabled on ESP32-P4 until
 * BadgeVMS exposes the relevant APIs.
 *
 * Copyright (C) 2006-2025 wolfSSL Inc. — GPLv3
 */

/* ── Build mode ─────────────────────────────────────────────────────────── */

#undef  WOLFCRYPT_ONLY
#define WOLFCRYPT_ONLY

#undef  NO_TLS
#define NO_TLS

#undef  WOLFSSL_WOLFSSH
#define WOLFSSL_WOLFSSH

/* ── Threading: single-threaded for BadgeVMS ────────────────────────────── */

#undef  SINGLE_THREADED
#define SINGLE_THREADED
#define NO_WOLFSSL_MULTITHREADING
#define WOLFSSL_NO_MULTITHREAD

/* ── I/O: custom blocking callbacks (custom_io.c) ───────────────────────── */

#define WOLFSSL_USER_IO     /* custom wolfSSL I/O callbacks */
#define WOLFSSH_USER_IO     /* custom wolfSSH I/O callbacks */
#define WOLFSSH_TERM        /* terminal/pty support in wolfSSH */
#define NO_WOLFSSL_SERVER   /* client only */
#define NO_SESSION_CACHE

/* ── Embedded / BadgeVMS constraints ────────────────────────────────────── */

#define WOLFSSL_SMALL_STACK
#define WC_NO_HARDEN
#define NO_WRITEV
#define NO_DEV_RANDOM       /* no /dev/random on badge */
#define NO_DEV_URANDOM
#define BENCH_EMBEDDED
#define NO_TERMIOS
/* NOTE: NO_FILESYSTEM is intentionally NOT set — wolfSSH terminal needs it */
#define NO_MAIN_DRIVER
#define NO_INLINE

/* ── Custom RNG (badge_crypto_port.c) ───────────────────────────────────── */

extern int badge_generate_seed(unsigned char* output, unsigned int sz);
#define CUSTOM_RAND_GENERATE_SEED badge_generate_seed

/* ── Crypto algorithms required for SSH ─────────────────────────────────── */

#define HAVE_ECC
#define HAVE_RSA
#define HAVE_DH
#define HAVE_SHA256
#define HAVE_HMAC
#define HAVE_AES
#define HAVE_AESGCM
#define HAVE_AES_CBC
#define WOLFSSL_AES_DIRECT
#define WOLFSSL_AES_COUNTER

/* FFDHE groups for DH key exchange */
#define HAVE_FFDHE_2048
#define HAVE_FFDHE_3072
#define HAVE_FFDHE_4096

/* SHA variants (SHA512 required for Ed25519) */
#define WOLFSSL_SHA224
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512

/* Elliptic curves */
#define HAVE_CURVE25519
#define CURVE25519_SMALL
#define HAVE_ED25519
#define HAVE_ECC_NISTP256
#define HAVE_ECC_NISTP384
#define HAVE_ECC_NISTP521

/* OpenSSL compatibility layer (used by wolfSSH internals) */
#define OPENSSL_EXTRA

/* ── Disabled features (save flash / reduce attack surface) ─────────────── */

#define NO_DSA
#define NO_PSK
#define NO_MD4
#define NO_RC4
#define NO_OLD_TLS

/* ── RSA configuration ──────────────────────────────────────────────────── */

#define RSA_LOW_MEM
#define FP_MAX_BITS 8192
#define RSA_MAX_SIZE 4096
#define USE_FAST_MATH

/* ESP32 RSA hardware primitive (only active when WOLFSSL_ESP32 is defined
 * by the build system, which does not apply to local/host builds) */
#if defined(WOLFSSL_ESP32) || defined(WOLFSSL_ESPWROOM32SE)
    #define ESP32_USE_RSA_PRIMITIVE
    #if defined(CONFIG_IDF_TARGET_ESP32)
        #undef  ESP_RSA_EXPT_XBITS
        #define ESP_RSA_EXPT_XBITS 32
        #undef  ESP_RSA_MULM_BITS
        #define ESP_RSA_MULM_BITS  16
    #endif
#endif

/* ── ASN.1 / key handling ───────────────────────────────────────────────── */

#define WOLFSSL_KEY_GEN
#define WOLFSSL_ASN_TEMPLATE

/* ── Hardware acceleration: disabled on ESP32-P4 (BadgeVMS APIs pending) ── */

#if defined(CONFIG_IDF_TARGET_ESP32P4)
    #define NO_ESP32_CRYPT
    #define NO_WOLFSSL_ESP32_CRYPT_HASH
    #define NO_WOLFSSL_ESP32_CRYPT_AES
    #define NO_WOLFSSL_ESP32_CRYPT_RSA_PRI
#else
    /* Local / host builds and any other target: all SW */
    #define NO_ESP32_CRYPT
    #define NO_WOLFSSL_ESP32_CRYPT_HASH
    #define NO_WOLFSSL_ESP32_CRYPT_AES
    #define NO_WOLFSSL_ESP32_CRYPT_RSA_PRI
#endif

/* ── Debug options (uncomment as needed) ────────────────────────────────── */

/* #define DEBUG_WOLFSSL        */
/* #define DEBUG_WOLFSSL_VERBOSE */
/* #define DEBUG_WOLFSSH        */

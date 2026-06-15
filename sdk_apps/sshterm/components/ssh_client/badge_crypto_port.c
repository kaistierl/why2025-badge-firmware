/* badge_crypto_port.c
 * Badge-specific crypto implementations for wolfSSL/wolfSSH
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "user_settings.h"

#ifdef SSHTERM_LOCAL_BUILD

#include <fcntl.h>
#include <unistd.h>

/* Badge-specific random seed function
 * This function is called by wolfSSL via the CUSTOM_RAND_GENERATE_SEED macro.
 * Signature must match wolfSSL expectations: int func(byte* output, word32 sz)
 */
int badge_generate_seed(unsigned char *output, unsigned int sz)
{
    if (!output || sz == 0) {
        printf("BADGE_RNG: ERROR - invalid parameters\n");
        return 1;
    }

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        printf("BADGE_RNG: ERROR - cannot open /dev/urandom\n");
        return 1;
    }
    ssize_t n = read(fd, output, (size_t)sz);
    close(fd);
    if (n != (ssize_t)sz) {
        printf("BADGE_RNG: ERROR - short read from /dev/urandom (%zd of %u)\n", n, sz);
        return 1;
    }
    return 0;
}

#else /* Hardware / BadgeVMS build */

#include <wolfssl/wolfcrypt/sha256.h>
#include <badgevms/wifi.h>
#include <badgevms/device.h>
#include <badgevms/misc_funcs.h>

/* 32-byte entropy pool, updated on every seed call (SHA-256 output size) */
static byte g_entropy_pool[WC_SHA256_DIGEST_SIZE];

static void pool_secure_wipe(void *ptr, size_t n)
{
    memset(ptr, 0, n);
    __asm__ __volatile__("" ::: "memory");
}

/* Collect entropy from hardware sources and update the pool:
 *   new_pool = SHA256(old_pool || time || mac || rssi || sensor_readings)
 *
 * Chaining the old pool value provides forward security: an attacker who
 * observes one seed output cannot predict future outputs without knowing all
 * subsequent entropy inputs.
 */
static void reseed_pool(void)
{
    wc_Sha256 sha;
    wc_InitSha256(&sha);

    /* Chain previous pool state */
    wc_Sha256Update(&sha, g_entropy_pool, WC_SHA256_DIGEST_SIZE);

    /* Wall-clock time — provides good variation after NTP sync, and some
     * variation even without it (ESP32 timer increments continuously) */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    wc_Sha256Update(&sha, (byte *)&tv, (word32)sizeof(tv));

    /* Device MAC address — not secret, but unique per badge, preventing
     * an attacker who knows approximate boot time from applying one pre-computed
     * key reconstruction to all badges simultaneously */
    const char *mac = get_mac_address();
    if (mac) {
        wc_Sha256Update(&sha, (byte *)mac, (word32)strlen(mac));
    }

    /* WiFi RSSI of the associated AP — fluctuates with RF environment and
     * multipath, providing a few bits of physical-layer randomness */
    wifi_station_handle station = wifi_get_connection_station();
    if (station) {
        int rssi = wifi_station_get_rssi(station);
        wc_Sha256Update(&sha, (byte *)&rssi, (word32)sizeof(rssi));
    }

    /* BME690 environmental sensor — temperature, humidity, pressure, and gas
     * resistance each contribute device- and environment-specific variation.
     * These change slowly but are unpredictable to a remote attacker. */
    gas_device_t *gas = (gas_device_t *)device_get("GAS0");
    if (gas) {
        float vals[4] = {
            gas->_get_temperature(gas),
            gas->_get_humidity(gas),
            gas->_get_pressure(gas),
            gas->_get_gas_resistance(gas),
        };
        wc_Sha256Update(&sha, (byte *)vals, (word32)sizeof(vals));
    }

    wc_Sha256Final(&sha, g_entropy_pool);
    wc_Sha256Free(&sha);
}

/* Badge-specific random seed function
 * This function is called by wolfSSL via the CUSTOM_RAND_GENERATE_SEED macro.
 * Signature must match wolfSSL expectations: int func(byte* output, word32 sz)
 *
 * Entropy model: reseed_pool() gathers fresh hardware observations on every
 * call.  Output bytes are produced by counter-mode SHA-256:
 *   output_block_i = SHA256(pool || i)
 * which whitens any remaining bias in the collected entropy and provides
 * uniform output even when individual sources have low bit-rate randomness.
 */
int badge_generate_seed(unsigned char *output, unsigned int sz)
{
    if (!output || sz == 0) {
        printf("BADGE_RNG: ERROR - invalid parameters\n");
        return 1;
    }

    reseed_pool();

    wc_Sha256 sha;
    byte block[WC_SHA256_DIGEST_SIZE];
    unsigned int offset  = 0;
    unsigned int counter = 0;

    while (offset < sz) {
        wc_InitSha256(&sha);
        wc_Sha256Update(&sha, g_entropy_pool, WC_SHA256_DIGEST_SIZE);
        wc_Sha256Update(&sha, (byte *)&counter, (word32)sizeof(counter));
        wc_Sha256Final(&sha, block);
        wc_Sha256Free(&sha);

        unsigned int n = sz - offset;
        if (n > WC_SHA256_DIGEST_SIZE) n = WC_SHA256_DIGEST_SIZE;
        memcpy(output + offset, block, n);
        offset += n;
        counter++;
    }

    pool_secure_wipe(block, sizeof(block));
    printf("BADGE_RNG: seeded %u bytes from SHA-256 entropy pool\n", sz);
    return 0;
}

#endif /* SSHTERM_LOCAL_BUILD */

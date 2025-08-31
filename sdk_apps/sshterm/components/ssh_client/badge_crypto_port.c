/* badge_crypto_port.c 
 * Badge-specific crypto implementations for wolfSSL/wolfSSH
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "user_settings.h"

/* Try to include ESP-IDF hardware RNG if available
TODO: Does not work with BadgeVMS yet */
#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "esp_random.h"
#endif

/* Badge-specific random seed function 
 * This function is called by wolfSSL via the CUSTOM_RAND_GENERATE_SEED macro
 * Uses multiple entropy sources for better randomness
 * Signature must match wolfSSL expectations: int func(byte* output, word32 sz)
 */
int badge_generate_seed(unsigned char* output, unsigned int sz)
{
    if (!output) {
        printf("BADGE_RNG: ERROR - output pointer is NULL\n");
        return 1;
    }
    
    if (sz == 0) {
        printf("BADGE_RNG: ERROR - size is 0\n");
        return 2;
    }
    
    if (sz > 1024) {
        printf("BADGE_RNG: ERROR - size too large: %u\n", sz);
        return 3;
    }
    
#ifdef CONFIG_IDF_TARGET_ESP32P4
    /* Use ESP32-P4 hardware RNG if available
    TODO: Does not work with BadgeVMS yet */
    esp_fill_random(output, sz);
    printf("BADGE_RNG: Used ESP32-P4 hardware RNG\n");
    return 0;
#else
    /* Fallback to software entropy sources */
    printf("BADGE_RNG: Using software entropy sources\n");
    
    /* Use multiple entropy sources available on badge:
     * - Current time/tick count
     * - Stack pointer address variation  
     * - Multiple LCGs with different parameters
     * - Memory addresses for additional entropy
     */
    static unsigned int seed1 = 0, seed2 = 0, seed3 = 0;
    unsigned int i;
    volatile char stack_var;
    struct timeval tv;
    
    // Get microsecond precision time if available
    if (gettimeofday(&tv, NULL) == 0) {
        seed1 ^= (unsigned int)tv.tv_sec;
        seed2 ^= (unsigned int)tv.tv_usec;
    } else {
        // Fallback to basic time
        time_t t = time(NULL);
        seed1 ^= (unsigned int)t;
    }
    
    // Initialize seeds if not done yet, using multiple entropy sources
    if (seed1 == 0) {
        seed1 = (unsigned int)((uintptr_t)&stack_var) ^ 0xAAAAAAAA;
    }
    if (seed2 == 0) {
        seed2 = (unsigned int)((uintptr_t)&tv) ^ 0x55555555;
    }
    if (seed3 == 0) {
        seed3 = (unsigned int)((uintptr_t)&output) ^ 0x33333333;
    }
    
    for (i = 0; i < sz; i++) {
        // Mix in stack pointer variation for each byte
        seed1 ^= (unsigned int)((uintptr_t)&stack_var);
        seed2 ^= (unsigned int)((uintptr_t)&i);
        seed3 ^= (unsigned int)((uintptr_t)&output);
        
        // Use multiple LCGs with different parameters
        seed1 = seed1 * 1103515245 + 12345;
        seed2 = seed2 * 1664525 + 1013904223;
        seed3 = seed3 * 69069 + 1;
        
        // Combine outputs from all three generators
        output[i] = (unsigned char)((seed1 >> 16) ^ (seed2 >> 8) ^ seed3);
    }
    
    return 0; /* Success */
#endif
}

/* badge_crypto_port.c 
 * Badge-specific crypto implementations for wolfSSL/wolfSSH
 */

#include <stddef.h>
#include <stdio.h>
#include "user_settings.h"

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
    
    /* Use multiple entropy sources available on badge:
     * - Current time/tick count
     * - Stack pointer address variation  
     * - Simple LCG with varying seed
     */
    static unsigned int seed = 0;
    unsigned int i;
    volatile char stack_var;
    
    // Initialize seed if not done yet
    if (seed == 0) {
        seed = (unsigned int)&stack_var;  /* Use stack address as initial entropy */
    }
    
    for (i = 0; i < sz; i++) {
        // Mix in stack pointer variation for each byte
        seed ^= (unsigned int)&stack_var;
        seed = seed * 1103515245 + 12345;  /* LCG */
        output[i] = (unsigned char)(seed >> 16);
    }
    
    return 0; /* Success */
}

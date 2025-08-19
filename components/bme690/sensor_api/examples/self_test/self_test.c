/**
 * Copyright (C) 2024 Bosch Sensortec GmbH
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>

#include "bme69x.h"
#include "common.h"
#include "coines.h"

/***********************************************************************/
/*                         Test code                                   */
/***********************************************************************/

int main(void)
{
    struct bme69x_dev bme;
    int8_t rslt;

    /* Interface preference is updated as a parameter
     * For I2C : BME69X_I2C_INTF
     * For SPI : BME69X_SPI_INTF
     */
    rslt = bme69x_interface_init(&bme, BME69X_SPI_INTF);
    bme69x_check_rslt("bme69x_interface_init", rslt);

    rslt = bme69x_init(&bme);
    bme69x_check_rslt("bme69x_init", rslt);

    rslt = bme69x_selftest_check(&bme);
    bme69x_check_rslt("bme69x_selftest_check", rslt);

    if (rslt == BME69X_OK)
    {
        printf("Self-test passed\n");
    }

    if (rslt == BME69X_E_SELF_TEST)
    {
        printf("Self-test failed\n");
    }

    bme69x_coines_deinit();

    return rslt;
}

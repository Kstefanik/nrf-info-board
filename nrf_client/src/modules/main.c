/*****************************************************************************
 * @file    main.c
 * @author  Karol Stefanik
 * @brief   Main entry point for nrf-info-board client application
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <zephyr/kernel.h>
#include "utils/logging_control.h"

int main(void)
{
    int err;

    err = logging_control_init();
    if (err != 0) {
        printk("Failed to initialize logging control: %d\n", err);
    }

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
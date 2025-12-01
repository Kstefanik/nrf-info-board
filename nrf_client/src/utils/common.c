/*****************************************************************************
 * @file    common.c
 * @author  Karol Stefanik
 * @brief   Common utility implementation for nrf-info-board client
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.
 *****************************************************************************/

#include <zephyr/logging/log.h>
#include "common.h"

LOG_MODULE_REGISTER(common, LOG_LEVEL_INF);

K_MSGQ_DEFINE(display_msgq, sizeof(struct display_message), 20, 4);

K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(mqtt_connected, 0, 1);

char* get_status(void)
{
    static char status_buf[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
    int err;

    err = snprintf(status_buf, sizeof(status_buf), "Displaying:\n-------------------------\n%s\n-------------------------\n", currently_displaying);
    if (err < 0) {
        LOG_ERR("Snprintf failed");
        return "Snprintf failed";
    }

    return status_buf;
}


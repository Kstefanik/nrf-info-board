/*****************************************************************************
 * @file    common.c
 * @author  Karol Stefanik
 * @brief   Common utility implementation for nrf-info-board client
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.
 *****************************************************************************/

#include "common.h"

K_MSGQ_DEFINE(display_msgq, sizeof(struct display_message), 20, 4);

K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(mqtt_connected, 0, 1);

K_WORK_DEFINE(wakeup_workq, wakeup_workq_handler);

void wakeup_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&wakeup_workq);
}

K_TIMER_DEFINE(wakeup_timer, wakeup_timer_handler, NULL);


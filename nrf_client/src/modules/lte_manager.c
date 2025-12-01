/*****************************************************************************
 * @file    lte_manager.c
 * @author  Karol Stefanik
 * @brief   LTE manager thread implementation for nrf-info-board client
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 ****************************************************************************/

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#include "utils/common.h"

LOG_MODULE_REGISTER(lte_manager, LOG_LEVEL_INF);

static void lte_handler(const struct lte_lc_evt *const evt) 
{
    switch (evt->type) {
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) || (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {
            LOG_INF("Network registration status: %s", evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");
            k_sem_give(&lte_connected);
        }
        break;
    case LTE_LC_EVT_RRC_UPDATE:
        LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
        break;
    default:
        break;
    }
}

static void lte_manager_entry(void) {
    int err;

    err = nrf_modem_lib_init();
    if (err) {
        LOG_ERR("Failed to initialize the modem library, error: %d", err);
        return;
    }

    err = lte_lc_reduced_mobility_set(LTE_LC_REDUCED_MOBILITY_NORDIC);
    if (err) {
        LOG_ERR("lte_lc_reduced_mobility_set, error: %d\n", err);
        return;
    }

    err = lte_lc_connect_async(lte_handler);
    if (err) {
        LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
        return;
    }

    while (1) {
        k_sleep(K_FOREVER);
    }
}

K_THREAD_DEFINE(lte_manager_thread_id, CONFIG_LTE_THREAD_STACK_SIZE, lte_manager_entry, NULL, NULL, NULL, CONFIG_LTE_THREAD_PRIORITY, 0, 0);

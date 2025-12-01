/*****************************************************************************
 * @file    mqtt_manager.c
 * @author  Karol Stefanik
 * @brief   MQTT manager thread implementation for nrf-info-board client
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 ****************************************************************************/

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/sys/reboot.h>

#include "utils/mqtt_utils.h"
#include "utils/common.h"

LOG_MODULE_REGISTER(mqtt_manager, LOG_LEVEL_INF);

static struct mqtt_client client;
static struct pollfd fds;

static void mqtt_manager_entry(void) {
    int err;
    uint32_t connect_attempt = 0;

    while (1) {
        LOG_INF("MQTT waiting for LTE connection...");

        err = k_sem_take(&lte_connected, K_SECONDS(CONFIG_LTE_CONNECT_TIMEOUT_S));
        if (err) {
            LOG_ERR("Timeout waiting for LTE connection: %d", err);
            LOG_INF("Rebooting system to recover...");
            sys_reboot(SYS_REBOOT_COLD);
        }

    do_connect:
        if (connect_attempt++ > 0) {
            LOG_INF("Reconnecting in %d seconds...", CONFIG_MQTT_RECONNECT_DELAY_S);
            k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
        }

        err = client_init(&client);
        if (err) {
            LOG_ERR("Failed to initialize MQTT client: %d", err);
            continue;
        }

        err = mqtt_connect(&client);
        if (err) {
            LOG_ERR("Error in mqtt_connect: %d", err);
            goto do_connect;
        }

        err = fds_init(&client, &fds);
        if (err) {
            LOG_ERR("Error in fds_init: %d", err);
            mqtt_disconnect(&client);
            continue;
        }

        k_sem_give(&mqtt_connected);

        while (1) {
            err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
            if (err < 0) {
                LOG_ERR("Error in poll(): %d", errno);
                break;
            }

            err = mqtt_live(&client);
            if ((err != 0) && (err != -EAGAIN)) {
                LOG_ERR("Error in mqtt_live: %d", err);
                break;
            }

            if ((fds.revents & POLLIN) == POLLIN) {
                err = mqtt_input(&client);
                if (err != 0) {
                    LOG_ERR("Error in mqtt_input: %d", err);
                    break;
                }
            }

            if ((fds.revents & POLLERR) == POLLERR) {
                LOG_ERR("POLLERR");
                break;
            }

            if ((fds.revents & POLLNVAL) == POLLNVAL) {
                LOG_ERR("POLLNVAL");
                break;
            }
        }

        LOG_INF("Disconnecting MQTT client");
        mqtt_disconnect(&client);

        goto do_connect;
    }
}

K_THREAD_DEFINE(mqtt_manager_thread_id, CONFIG_MQTT_THREAD_STACK_SIZE, mqtt_manager_entry, NULL, NULL, NULL, CONFIG_MQTT_THREAD_PRIORITY, 0, 0);

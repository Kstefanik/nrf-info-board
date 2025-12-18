/*****************************************************************************
 * @file    mqtt_utils.h
 * @author  Karol Stefanik
 * @brief   MQTT utility functions and macros for nrf-info-board client
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 ****************************************************************************/

#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>

#ifndef _MQTT_UTILS_H_
#define _MQTT_UTILS_H_

/* IMEI length for client ID generation */
#define IMEI_LEN 15
/* CGSN AT command response length */
#define CGSN_RESPONSE_LENGTH (IMEI_LEN + 6 + 1)
/* Maximum client ID length */
#define CLIENT_ID_LEN sizeof("nrf-") + IMEI_LEN

/* Supported command IDs for MQTT messages */
typedef enum {
    CMD_REBOOT = 1,
    CMD_UNKNOWN = 255
} command_id_t;

/* Command structure for parsing MQTT commands */
typedef struct {
    command_id_t id;
    const char *name;
} command_t;

/**
 * @brief Initialize MQTT client structure.
 *
 * @param client Pointer to MQTT client structure.
 * @return 0 on success, negative error code otherwise.
 */
int client_init(struct mqtt_client *client);

/**
 * @brief Initialize pollfd structure for MQTT client socket.
 *
 * @param client Pointer to MQTT client structure.
 * @param fds Pointer to pollfd structure.
 * @return 0 on success, negative error code otherwise.
 */
int fds_init(struct mqtt_client *client, struct pollfd *fds);

/**
 * @brief Publish data to MQTT topic.
 *
 * @param c Pointer to MQTT client structure.
 * @param qos MQTT QoS level.
 * @param topic Topic string.
 * @param data Data buffer.
 * @param len Length of data.
 * @return 0 on success, negative error code otherwise.
 */
int data_publish(struct mqtt_client *c, enum mqtt_qos qos, const char* topic, uint8_t *data, size_t len);

#endif /* _MQTT_UTILS_H_ */
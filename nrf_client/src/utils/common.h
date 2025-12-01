/*****************************************************************************
 * @file    common.h
 * @author  Karol Stefanik
 * @brief   Common definitions for nrf-info-board client
 *
 * Copyright (c) 2025  Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <zephyr/kernel.h>

/* Buffer for currently displayed text */
extern char currently_displaying[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

/* Semaphores for LTE and MQTT connection status */
extern struct k_sem lte_connected;
extern struct k_sem mqtt_connected;

/* Message structure for display queue */
struct display_message {
    char text[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
};

/* Message structure for MQTT queue */
struct mqtt_message {
    char text[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
};

/* Message queue for display messages */
extern struct k_msgq display_msgq;

/* Returns current display status string */
char* get_status(void);

#endif /* _COMMON_H_ */
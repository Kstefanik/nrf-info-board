/*****************************************************************************
 * @file    logging_control.h
 * @author  Karol Stefanik
 * @brief   API for runtime control of UART logging with button and LED indicator.
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef ZEPHYR_LOGGING_CONTROL_H
#define ZEPHYR_LOGGING_CONTROL_H

#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * @brief Initialize logging control (button/LED, UART backend).
 *
 * @return 0 on success, negative error code otherwise.
 */
int logging_control_init(void);

/**
 * @brief Toggle UART logging and LED indicator.
 */
void logging_toggle_uart(void);

#endif /* ZEPHYR_LOGGING_CONTROL_H */
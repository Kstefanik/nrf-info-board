/*****************************************************************************
 * @file    logging_control.c
 * @author  Karol Stefanik
 * @brief   UART logging control utility for nrf-info-board client
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/drivers/gpio.h>

#include "logging_control.h"

LOG_MODULE_REGISTER(logging_control, LOG_LEVEL_INF);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);

static const struct log_backend *uart_backend = NULL;

static bool logging_enabled = false;
static bool logging_initialized = false;

static struct gpio_callback button_cb;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static int64_t last_press_time = 0;
    int64_t current_time = k_uptime_get();
    if ((current_time - last_press_time) > 50) {
        logging_toggle_uart();
        last_press_time = current_time;
    }
}

int logging_control_init(void)
{
    int ret;

    if (logging_initialized) {
        return 0;
    }

    uart_backend = log_backend_get_by_name("log_backend_uart");
    if (uart_backend == NULL) {
        LOG_ERR("UART log backend not found");
        return -ENODEV;
    }

    log_backend_disable(uart_backend);

    if (device_is_ready(led.port)) {
        ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        if (ret == 0) {
            gpio_pin_set_dt(&led, 0);
        } else {
            LOG_WRN("Failed to configure LED: %d", ret);
        }
    } else {
        LOG_WRN("LED device not ready");
    }

    if (device_is_ready(button.port)) {
        ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
        if (ret == 0) {
            ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
            if (ret == 0) {
                gpio_init_callback(&button_cb, button_pressed, BIT(button.pin));
                gpio_add_callback(button.port, &button_cb);
                LOG_INF("Button interrupt configured");
            } else {
                LOG_WRN("Failed to configure button interrupt: %d", ret);
            }
        } else {
            LOG_WRN("Failed to configure button: %d", ret);
        }
    } else {
        LOG_WRN("Button device not ready");
    }

    logging_initialized = true;
    LOG_INF("Logging control initialized - UART logging disabled by default");
    return 0;
}

void logging_toggle_uart(void)
{
    if (!logging_initialized || uart_backend == NULL) {
        LOG_ERR("Logging control not initialized");
        return;
    }
    
    if (logging_enabled) {
        log_backend_disable(uart_backend);
        logging_enabled = false;
        if (device_is_ready(led.port)) {
            gpio_pin_set_dt(&led, 0);
        }
        printk("UART logging DISABLED\n");
    } else {
        log_backend_enable(uart_backend, uart_backend->cb->ctx, CONFIG_LOG_MAX_LEVEL);
        logging_enabled = true;
        if (device_is_ready(led.port)) {
            gpio_pin_set_dt(&led, 1);
        }
        printk("UART logging ENABLED\n");
    }
}
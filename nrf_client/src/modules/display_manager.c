/*****************************************************************************
 * @file    display_manager.c
 * @author  Karol Stefanik
 * @brief   Display manager implementation for nrf-info-board client
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>

#include "utils/common.h"

LOG_MODULE_REGISTER(display_manager, LOG_LEVEL_INF);

static const struct device *display_dev;
static uint8_t font_height, font_width;
char currently_displaying[CONFIG_MQTT_MESSAGE_BUFFER_SIZE] = "None";

static int display_init(void)
{
    int err;
    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display not ready");
        return -ENODEV;
    }
    err = display_set_pixel_format(display_dev, PIXEL_FORMAT_MONO10);
    if (err) {
        LOG_ERR("Pixel format set failed");
        return err;
    }
    err = cfb_framebuffer_init(display_dev);
    if (err) {
        LOG_ERR("CFB framebuffer init failed");
        return err;
    }
    err = cfb_framebuffer_set_font(display_dev, 0);
    if (err) {
        LOG_ERR("CFB set font failed");
        return err;
    }
    err = cfb_get_font_size(display_dev, 0, &font_width, &font_height);
    if (err) {
        LOG_ERR("CFB get font size failed");
        return err;
    }
    err = display_blanking_off(display_dev);
    if (err) {
        LOG_ERR("Display blanking off failed");
        return err;
    }
    return 0;
}

static int print_str_to_display(char *message, int x, int y)
{
    int err;
    err = cfb_framebuffer_clear(display_dev, true);
    if (err) {
        LOG_ERR("CFB framebuffer clear failed");
        return err;
    }
    err = cfb_print(display_dev, message, x, y * font_height);
    if (err) {
        LOG_ERR("CFB print failed");
        return err;
    }
    err = cfb_framebuffer_finalize(display_dev);
    if (err) {
        LOG_ERR("CFB framebuffer finalize failed");
        return err;
    }
    return 0;
}

static int print_display_msg(struct display_message msg)
{
    int err;
    err = cfb_framebuffer_clear(display_dev, true);
    if (err) {
        LOG_ERR("CFB framebuffer clear failed");
        return err;
    }
    char *start = msg.text;
    int row = 0;
    while (*start) {
        char *end = strchr(start, '\n');
        int len = end ? (end - start) : strlen(start);
        char buf[26];
        if (len > 25) len = 25;
        strncpy(buf, start, len);
        buf[len] = '\0';
        cfb_print(display_dev, buf, 0, row * font_height);
        row++;
        if (!end) {
            break;
        }
        start = end + 1;
    }
    err = cfb_framebuffer_finalize(display_dev);
    if (err) {
        LOG_ERR("CFB framebuffer finalize failed");
        return err;
    }
    return 0;
}

static void display_manager_entry(void)
{
    int err;
    err = display_init();
    if (err) {
        LOG_ERR("Display initialization failed");
        return;
    }
    LOG_INF("Waiting for MQTT connection");
    err = print_str_to_display("Waiting for connection...", 0, 3);
    if (err) {
        LOG_ERR("Print to display failed");
    }
    k_sem_take(&mqtt_connected, K_FOREVER);
    k_sleep(K_SECONDS(2));
    err = print_str_to_display("Welcome", 90, 3);
    if (err) {
        LOG_ERR("Print to display failed");
    }
    err = snprintf(currently_displaying, sizeof(currently_displaying), "Welcome");
    if (err < 0) {
        LOG_ERR("Snprintf failed");
    }
    while (1) {
        struct display_message msg;
        LOG_INF("Display thread waiting for message...");
        k_msgq_get(&display_msgq, &msg, K_FOREVER);
        err = display_blanking_off(display_dev);
        if (err) {
            LOG_ERR("Display blanking off failed");
        }
        err = print_display_msg(msg);
        if (err) {
            LOG_ERR("Print display message failed");
            continue;
        }
        err = snprintf(currently_displaying, sizeof(currently_displaying), "%s", msg.text);
        if (err < 0) {
            LOG_ERR("Snprintf failed");
        }
        err = display_blanking_on(display_dev);
        if (err) {
            LOG_ERR("Display blanking on failed");
        }
    }
}

K_THREAD_DEFINE(display_manager_thread_id, CONFIG_DISPLAY_THREAD_STACK_SIZE, display_manager_entry, NULL, NULL, NULL, CONFIG_DISPLAY_THREAD_PRIORITY, 0, 0);
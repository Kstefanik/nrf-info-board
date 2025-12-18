/*****************************************************************************
 * @file    mqtt_utils.c
 * @author  Karol Stefanik
 * @brief   MQTT utility implementation for nrf-info-board client
 *
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <nrf_modem_at.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/reboot.h>

#include "mqtt_utils.h"
#include "common.h"

LOG_MODULE_REGISTER(mqtt_utils, LOG_LEVEL_INF);

static bool first_connect = true;

static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

static struct sockaddr_storage broker;

static const command_t commands[] = {
    { CMD_REBOOT, "reboot" },
    { CMD_UNKNOWN, NULL }
};

static command_id_t parse_command(const uint8_t *payload, size_t len, size_t *cmd_len_out)
{
    for (size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
        size_t cmd_len = strlen(commands[i].name);
        if (len >= cmd_len && memcmp(payload, commands[i].name, cmd_len) == 0) {
            if (cmd_len_out) *cmd_len_out = cmd_len;
            return commands[i].id;
        }
    }
    if (cmd_len_out) *cmd_len_out = 0;
    return CMD_UNKNOWN;
}

static int get_received_payload(struct mqtt_client *c, size_t length)
{
    int ret;
    int err = 0;
    size_t read_len = length;

    if (read_len > sizeof(payload_buf)) {
        read_len = sizeof(payload_buf);
        err = -EMSGSIZE;
    }

    // Read the payload that fits into the buffer
    ret = mqtt_readall_publish_payload(c, payload_buf, read_len);
    if (ret) {
        return ret;
    }

    // Discard the remaining payload if any
    size_t remaining = length - read_len;
    while (remaining > 0) {
        uint8_t dummy[32];
        size_t chunk = remaining > sizeof(dummy) ? sizeof(dummy) : remaining;
        ret = mqtt_read_publish_payload_blocking(c, dummy, chunk);
        if (ret == 0) {
            return -EIO;
        } else if (ret < 0) {
            return ret;
        }
        remaining -= ret;
    }

    return err;
}

static int subscribe(struct mqtt_client *const c)
{
    struct mqtt_topic subscribe_topics[] = {
        {
            .topic = {
                .utf8 = CONFIG_MQTT_DISPLAY_TOPIC,
                .size = strlen(CONFIG_MQTT_DISPLAY_TOPIC)
            },
            .qos = MQTT_QOS_1_AT_LEAST_ONCE
        },
        {
            .topic = {
                .utf8 = CONFIG_MQTT_CMD_TOPIC,
                .size = strlen(CONFIG_MQTT_CMD_TOPIC)
            },
            .qos = MQTT_QOS_1_AT_LEAST_ONCE
        }
    };

    const struct mqtt_subscription_list subscription_list = {
        .list = subscribe_topics,
        .list_count = ARRAY_SIZE(subscribe_topics),
        .message_id = 1234
    };

    LOG_INF("Subscribing to multiple topics");
    return mqtt_subscribe(c, &subscription_list);
}

static void data_print(uint8_t *prefix, uint8_t *data, size_t len)
{
    char buf[len + 1];

    memcpy(buf, data, len);
    buf[len] = 0;
    LOG_INF("%s%s", (char *)prefix, (char *)buf);
}

int data_publish(struct mqtt_client *c, enum mqtt_qos qos, const char* topic, uint8_t *data, size_t len)
{
    struct mqtt_publish_param param;

    param.message.topic.qos = qos;
    param.message.topic.topic.utf8 = topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = data;
    param.message.payload.len = len;
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    data_print("Publishing: ", data, len);
    LOG_INF("to topic: %s len: %u", topic, (unsigned int)strlen(topic));

    return mqtt_publish(c, &param);
}

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
    int err;

    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT connect failed: %d", evt->result);
            break;
        }

        LOG_INF("MQTT client connected");
        err = subscribe(c);
        if (err) {
            LOG_ERR("Could not subscribe to topics: %d", err);
        }

        if (first_connect) {
            // Send status message upon first connection
            k_work_submit(&wakeup_workq);
            first_connect = false;
        }
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected: %d", evt->result);
        break;

    case MQTT_EVT_PUBLISH:
        const struct mqtt_publish_param *p = &evt->param.publish;
        LOG_INF("MQTT message received result=%d len=%d", evt->result, p->message.payload.len);

        err = get_received_payload(c, p->message.payload.len);

        if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            const struct mqtt_puback_param ack = {
                .message_id = p->message_id
            };

            err = mqtt_publish_qos1_ack(c, &ack);
            if (err) {
                LOG_ERR("Failed to send PUBACK: %d", err);
            }
        }

        if (err >= 0) {
            // Handling display topic messages
            if (strncmp(p->message.topic.topic.utf8, CONFIG_MQTT_DISPLAY_TOPIC, p->message.topic.topic.size) == 0) {
                struct display_message msg = {0};
                size_t copy_len = p->message.payload.len < sizeof(msg.text) - 1 ? p->message.payload.len : sizeof(msg.text) - 1;
                memcpy(msg.text, payload_buf, copy_len);
                msg.text[copy_len] = '\0';

                int put_err = k_msgq_put(&display_msgq, &msg, K_NO_WAIT);
                if (put_err) {
                    LOG_ERR("Failed to send display message: %d", put_err);
                } 
                else {
                    LOG_INF("Display message queued: %s", msg.text);
                }
                return;
            }
            // Handling command topic messages
            else if (strncmp(p->message.topic.topic.utf8, CONFIG_MQTT_CMD_TOPIC, p->message.topic.topic.size) == 0) {

                size_t cmd_len = 0;
                command_id_t cmd_id = parse_command(payload_buf, p->message.payload.len, &cmd_len);

                switch (cmd_id) {           
                    case CMD_REBOOT:
                        LOG_INF("Rebooting system...");
                        k_sleep(K_MSEC(5000));
                        sys_reboot(SYS_REBOOT_COLD);
                        break;

                    default:
                        const char *reply = "Wrong command";
                        err = data_publish(c, MQTT_QOS_1_AT_LEAST_ONCE, CONFIG_MQTT_STATUS_TOPIC, (uint8_t *)reply, strlen(reply));
                        if (err) {
                            LOG_ERR("Failed to publish wrong command reply: %d", err);
                        } 
                        else {
                            LOG_INF("Unknown command received, replied with 'Wrong command'");
                        }
                        break;
                }
            }
        }
        else if (err == -EMSGSIZE) {
            LOG_ERR("Received payload (%d bytes) is larger than the payload buffer size (%d bytes).",
                p->message.payload.len, sizeof(payload_buf));
        } 
        else {
            LOG_ERR("get_received_payload failed: %d", err);
            LOG_INF("Disconnecting MQTT client...");

            err = mqtt_disconnect(c);
            if (err) {
                LOG_ERR("Could not disconnect: %d", err);
            }
        }
        break;

    case MQTT_EVT_PUBACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT PUBACK error: %d", evt->result);
            break;
        }

        LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);
        break;

    case MQTT_EVT_SUBACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT SUBACK error: %d", evt->result);
            break;
        }

        LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
        break;

    case MQTT_EVT_PINGRESP:
        if (evt->result != 0) {
            LOG_ERR("MQTT PINGRESP error: %d", evt->result);
        }
        break;

    default:
        LOG_INF("Unhandled MQTT event type: %d", evt->type);
        break;
    }
}

static int broker_init(void)
{
    int err;
    struct addrinfo *result;
    struct addrinfo *addr;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
    if (err) {
        LOG_ERR("getaddrinfo failed: %d", err);
        return -ECHILD;
    }

    addr = result;

    while (addr != NULL) {
        if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
            struct sockaddr_in *broker4 =
                ((struct sockaddr_in *)&broker);
            char ipv4_addr[NET_IPV4_ADDR_LEN];

            broker4->sin_addr.s_addr =
                ((struct sockaddr_in *)addr->ai_addr)
                ->sin_addr.s_addr;
            broker4->sin_family = AF_INET;
            broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

            inet_ntop(AF_INET, &broker4->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
            LOG_INF("IPv4 Address found %s", (char *)(ipv4_addr));

            break;
        } else {
            LOG_ERR("ai_addrlen = %u should be %u or %u",
                (unsigned int)addr->ai_addrlen,
                (unsigned int)sizeof(struct sockaddr_in),
                (unsigned int)sizeof(struct sockaddr_in6));
        }

        addr = addr->ai_next;
    }

    freeaddrinfo(result);

    return err;
}

static const uint8_t* client_id_get(void)
{
    int err;

    static uint8_t client_id[MAX(sizeof(CONFIG_MQTT_CLIENT_ID),
                     CLIENT_ID_LEN)];

    if (strlen(CONFIG_MQTT_CLIENT_ID) > 0) {
        err  = snprintf(client_id, sizeof(client_id), "%s", CONFIG_MQTT_CLIENT_ID);
        if (err < 0) {
            LOG_ERR("Failed to format client ID, error: %d", err);
        }
        goto exit;
    }

    char imei_buf[CGSN_RESPONSE_LENGTH + 1];

    err = nrf_modem_at_cmd(imei_buf, sizeof(imei_buf), "AT+CGSN");
    if (err) {
        LOG_ERR("Failed to obtain IMEI, error: %d", err);
        goto exit;
    }

    imei_buf[IMEI_LEN] = '\0';

    err = snprintf(client_id, sizeof(client_id), "nrf-%.*s", IMEI_LEN, imei_buf);
    if (err < 0) {
        LOG_ERR("Failed to format client ID, error: %d", err);
        goto exit;
    }

exit:
    LOG_INF("client_id = %s", (char *)(client_id));

    return client_id;
}

int client_init(struct mqtt_client *client)
{
    int err;
    mqtt_client_init(client);

    err = broker_init();
    if (err) {
        LOG_ERR("Failed to initialize broker connection");
        return err;
    }

    client->broker = &broker;
    client->evt_cb = mqtt_evt_handler;
    client->client_id.utf8 = client_id_get();
    client->client_id.size = strlen(client->client_id.utf8);
    client->password = NULL;
    client->user_name = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;

    client->rx_buf = rx_buffer;
    client->rx_buf_size = sizeof(rx_buffer);
    client->tx_buf = tx_buffer;
    client->tx_buf_size = sizeof(tx_buffer);

    client->transport.type = MQTT_TRANSPORT_NON_SECURE;

    return err;
}

int fds_init(struct mqtt_client *c, struct pollfd *fds)
{
    if (c->transport.type == MQTT_TRANSPORT_NON_SECURE) {
        fds->fd = c->transport.tcp.sock;
    } else {
        return -ENOTSUP;
    }

    fds->events = POLLIN;

    return 0;
}
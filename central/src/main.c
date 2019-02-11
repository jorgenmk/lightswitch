/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <misc/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <misc/byteorder.h>

#include <gpio.h>
#include <pwm.h>

static struct bt_conn *default_conn;
static struct device *pwm;

static struct k_delayed_work work;
static u8_t target_name[] = "Lightswitch";


static void stop(struct k_work *item)
{
	pwm_pin_set_usec(pwm, 8, 20000, 1300);
}

static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		if (memcmp(data->data, target_name, data->data_len) == 0) {
			printk("Match\n");
			default_conn = bt_conn_create_le(addr,
							 BT_LE_CONN_PARAM_DEFAULT);
			return false;
		}
        else {
            printk("name: %s\n", data->data);
        }
	}
	return true;
}

void connection_device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	/* We're only interested in connectable events */
	if (type != BT_LE_ADV_IND && type != BT_LE_ADV_DIRECT_IND) {
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
	bt_data_parse(ad, eir_found, (void *)addr);
}

static void connected(struct bt_conn *conn, u8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);
		return;
	}
	printk("Switch\n");
	err = pwm_pin_set_usec(pwm, 8,
			   20000, 1800);
	if (err < 0) {
		printk("Failed to pwm_pin_set_usec\n");
		return;
	}
	k_delayed_work_submit(&work, 500);
	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason %u)\n", addr, reason);

	bt_conn_unref(conn);
	conn = NULL;
}

static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);
	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, connection_device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}
	printk("Scanning successfully started\n");
}

void main(void)
{
	int err;
	k_delayed_work_init(&work, stop);
	pwm = device_get_binding("PWM_0");
	if (pwm) {
		err = pwm_pin_set_usec(pwm, 8,
				20000, 1300);
	}
	if (err < 0) {
		printk("pwm_pin_set_usec failed (err %d)\n", err);
		return;
	}
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");
	

	printk("gpio setup\n");
	

	
	while(1) {
		k_sleep(2000);
	}
}

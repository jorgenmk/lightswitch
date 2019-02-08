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

static struct gpio_callback gpio_cb;
static struct bt_conn *default_conn;
static struct bt_gatt_write_params wp = {0};
static struct bt_gatt_discover_params discover_params;
static u8_t uuuu[] = {
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12};
static struct bt_uuid_128 uuid_s = BT_UUID_INIT_128(
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static struct bt_uuid_128 uuid_c = BT_UUID_INIT_128(
	0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;

	switch (data->type) {
	case BT_DATA_UUID128_ALL:
		for (int i = 0; i < 8; i++) {
			printk("%x,", data->data[i]);
		}
		printk(" len: %d\n", data->data_len);
		if (memcmp(data->data, uuuu, sizeof(uuuu)) == 0) {
			printk("Match\n");
			bt_le_scan_stop();
			default_conn = bt_conn_create_le(addr,
							 BT_LE_CONN_PARAM_DEFAULT);
		}
		//memcpy(buf, data->data, data->data_len);
		//printk("name: %s\n", buf);
/*		
			default_conn = bt_conn_create_le(addr,
							 BT_LE_CONN_PARAM_DEFAULT);
			return false;
			*/
	}

	return true;
}



static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	if (default_conn) {
		return;
	}

	/* We're only interested in connectable events */
	if (type != BT_LE_ADV_IND && type != BT_LE_ADV_DIRECT_IND) {
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	//printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
	bt_data_parse(ad, eir_found, (void *)addr);
	/* connect only to devices in close proximity */
	if (rssi < -40) {
		return;
	}

	if (bt_le_scan_stop()) {
		return;

	}
	
	//default_conn = bt_conn_create_le(addr, BT_LE_CONN_PARAM_DEFAULT);
}

static u8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		printk("Discover complete\n");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	printk("[ATTRIBUTE] handle %u\n", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, (struct bt_uuid*)&uuid_c)) {
		printk("HIT\n");
		//memcpy(&uuid, BT_UUID_HRS_MEASUREMENT, sizeof(uuid));
		discover_params.uuid = &uuid_c.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}

	}else{

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, u8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);
		return;
	}

	printk("Connected: %s\n", addr);
	if (conn == default_conn) {
		//memcpy(&uuid, BT_UUID_HRS, sizeof(uuid));
		discover_params.uuid = &uuid_s.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = 0x0001;
		discover_params.end_handle = 0xffff;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			printk("Discover failed(err %d)\n", err);
			return;
		}
	}
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

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
};


void callback(struct bt_conn *conn, u8_t err, struct bt_gatt_write_params *params)
{
	printk("Wrote\n");
}

static void button_pressed(struct device *gpiob, struct gpio_callback *cb,
		    u32_t pins)
{
	printk("Button pressed at %d\n", k_cycle_get_32());
	if (default_conn) {
		bt_gatt_write(default_conn, &wp);
	}
}

void main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}
	printk("Scanning successfully started\n");

	struct device *gpiob = device_get_binding("GPIO_0");
	if (!gpiob) {
		printk("error\n");
		return;
	}

	static u8_t data[] = {0x01, 0x02};
	wp.handle = 21;
	wp.func = callback;
	wp.data = data;
	wp.length = 2;

	gpio_pin_configure(gpiob, 11,
			   GPIO_DIR_IN | GPIO_INT |  GPIO_PUD_PULL_UP | GPIO_INT_EDGE);

	gpio_init_callback(&gpio_cb, button_pressed, BIT(11));

	gpio_add_callback(gpiob, &gpio_cb);
	gpio_pin_enable_callback(gpiob, 11);
	/*
	gpio_pin_configure(gpiob, 5,
			   GPIO_DIR_OUT);
	gpio_pin_write(gpiob, 5, 0);

	gpio_pin_configure(gpiob, 6,
			   GPIO_DIR_OUT);
	gpio_pin_write(gpiob, 6, 0);

	gpio_pin_configure(gpiob, 7,
			   GPIO_DIR_OUT);
	gpio_pin_write(gpiob, 7, 0);
	*/
	printk("gpio setup\n");
	

	
	while(1) {
		k_sleep(2000);
	}
}

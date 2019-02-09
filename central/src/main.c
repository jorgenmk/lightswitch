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

#include "connection.h"

static struct device *pwm;
static struct gpio_callback gpio_cb;

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

static const char target[] = "Switch";

static ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, u16_t len, u16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static void stop(struct k_work *item)
{
	pwm_pin_set_usec(pwm, 8, 20000, 1300);
}

static struct k_delayed_work work;

static ssize_t write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, u16_t len, u16_t offset,
			 u8_t flags)
{
	u8_t *value = attr->user_data;
	u8_t *pointer = (u8_t*)buf;
	u32_t pwm_value = pointer[0] | (pointer[1] << 8);
	printk("written: %d\n", pwm_value);
	int err = pwm_pin_set_usec(pwm, 8,
			   20000, 1800);
	k_delayed_work_submit(&work, 500);
	printk("err: %d\n", err);

	return len;
}


static struct bt_gatt_attr vnd_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(&uuid_s),
	BT_GATT_CHARACTERISTIC(&uuid_c.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_vnd, write_vnd, vnd_value)
};

static struct bt_gatt_service vnd_svc = BT_GATT_SERVICE(vnd_attrs);


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

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_gatt_service_register(&vnd_svc);

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
	init_connection();
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	

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

	printk("gpio setup\n");
	

	
	while(1) {
		k_sleep(2000);
	}
}

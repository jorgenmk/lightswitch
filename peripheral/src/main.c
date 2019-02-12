/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <gpio.h>

static struct gpio_callback gpio_cb;

/* Custom Service Variables */
static struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static struct bt_uuid_128 vnd_enc_uuid = BT_UUID_INIT_128(
	0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static u8_t vnd_value[] = { 'V', 'e', 'n', 'd', 'o', 'r' };

static ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, u16_t len, u16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
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
	
	if (offset + len > sizeof(vnd_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	
	memcpy(value + offset, buf, len);

	return len;
}


/* Vendor Primary Service Declaration */
static struct bt_gatt_attr vnd_attrs[] = {
	/* Vendor Primary Service Declaration */
	BT_GATT_PRIMARY_SERVICE(&vnd_uuid),
	BT_GATT_CHARACTERISTIC(&vnd_enc_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_vnd, write_vnd, NULL)
};

static struct bt_gatt_service vnd_svc = BT_GATT_SERVICE(vnd_attrs);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      0x0d, 0x18, 0x0f, 0x18, 0x05, 0x18),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
		      0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12),
};

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	} else {
		printk("Connected\n");
	}
	bt_le_adv_stop();
	struct device *dev = device_get_binding(LED0_GPIO_CONTROLLER);
	gpio_pin_write(dev, LED0_GPIO_PIN, 0);
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
	k_cpu_idle();
	/*int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}
	*/
	struct device *dev = device_get_binding(LED0_GPIO_CONTROLLER);
	gpio_pin_write(dev, LED0_GPIO_PIN, 1);
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
	bt_gatt_service_register(&vnd_svc);
	
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
	/*
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
	*/
	//bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);

}

void button_pressed(struct device *port,
					struct gpio_callback *cb,
					u32_t pins)
{
	//struct device *dev = device_get_binding(LED0_GPIO_CONTROLLER);
	///gpio_pin_write(dev, LED0_GPIO_PIN, 0);
	bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	struct device *dev = device_get_binding(LED0_GPIO_CONTROLLER);
	gpio_pin_write(dev, LED0_GPIO_PIN, 1);

}

void main(void)
{
	int err;
	struct device *gpiob = device_get_binding(SW0_GPIO_CONTROLLER);
	if (!gpiob) {
		printk("error\n");
		return;
	}
	gpio_pin_configure(gpiob, SW0_GPIO_PIN,
			   GPIO_DIR_IN | GPIO_INT | GPIO_PUD_PULL_UP | GPIO_INT_EDGE);

	gpio_init_callback(&gpio_cb, button_pressed, BIT(SW0_GPIO_PIN));

	gpio_add_callback(gpiob, &gpio_cb);
	gpio_pin_enable_callback(gpiob, SW0_GPIO_PIN);

	struct device *dev = device_get_binding(LED0_GPIO_CONTROLLER);

	gpio_pin_configure(dev, LED0_GPIO_PIN, GPIO_DIR_OUT);
	gpio_pin_write(dev, LED0_GPIO_PIN, 0);

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	

	while (1) {
		k_sleep(MSEC_PER_SEC);
		//gpio_pin_write(dev, LED0_GPIO_PIN, 0);
		k_sleep(MSEC_PER_SEC);
		//gpio_pin_write(dev, LED0_GPIO_PIN, 0);
	}
}

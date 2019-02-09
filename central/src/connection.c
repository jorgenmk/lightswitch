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

#define MAX_CONNS 4

static struct bt_conn *connections[MAX_CONNS];
static u8_t target_name[64];

static bool is_connected(struct bt_conn *conn)
{
    for(int i = 0; i < MAX_CONNS; i++) {
        if (conn == connections[i]) {
            return true;
        }
    }
    return false;
}

struct bt_conn *new_conn(void)
{
    for(int i = 0; i < MAX_CONNS; i++) {
        if (connections[i] == NULL) {
            return &connections[i];
        }
    }
    return NULL;
}

int delete_conn(struct bt_conn *conn)
{
    for(int i = 0; i < MAX_CONNS; i++) {
        if (conn == connections[i]) {
            bt_conn_unref(conn);
	        connections[i] = NULL;
            return 0;
        }
    }
    return -ENODEV;
}

static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		if (memcmp(data->data, target_name, data->data_len) == 0) {
			printk("Match\n");
			
            struct bt_conn *new = new_conn();
            if (new == NULL) {
                /* Max connections reached */
                bt_le_scan_stop();
                return false;
            }
			new = bt_conn_create_le(addr,
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
	//printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
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

	printk("Connected: %s\n", addr);
	
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

	err = delete_conn(conn);
    if(err < 0) {
        printk("delete conn error %d\n");
    }

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, connection_device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
};

void init_connection(void)
{
    bt_conn_cb_register(&conn_callbacks);
}
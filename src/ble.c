/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief LED Button Service (LBS) sample
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "ble.h"

bool                   notify_enabled;
static uint32_t                   padlock_state;
static struct bt_padlock_cb       padlock_cb;

//const struct bt_gatt_attr attr_padlock_svc;
//const struct bt_gatt_service_static padlock_svc = {.attrs = &attr_padlock_svc, .attr_count = 6U};

static void padlocklc_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				  uint16_t value)
{
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	printk("Notify Enable: %d", notify_enabled);
}

static ssize_t write_padlock_key(struct bt_conn *conn,
			 const struct bt_gatt_attr *attr,
			 const void *buf,
			 uint16_t len, uint16_t offset, uint8_t flags)
{
	printk("Attribute write, handle: %u, conn: %p", attr->handle,
		(void *)conn);

	if (len != 8U) {
		printk("Write led: Incorrect data length");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (offset != 0) {
		printk("Write led: Incorrect data offset");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (padlock_cb.key_cb) {
		uint8_t* val = (uint8_t *)buf;
		padlock_cb.key_cb(val, len);
	}

	return len;
}

static ssize_t read_padlock_status(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr,
			  void *buf,
			  uint16_t len,
			  uint16_t offset)
{
	const char *value = attr->user_data;

	printk("Attribute read, handle: %u, conn: %p", attr->handle,
		(void *)conn);

	if (padlock_cb.status_cb) {
		padlock_state = padlock_cb.status_cb();
		return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
					 sizeof(*value));
	}

	return 0;
}

/* LED Button Service Declaration */
BT_GATT_SERVICE_DEFINE(padlock_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_PADLOCK),
	BT_GATT_CHARACTERISTIC(BT_UUID_PADLOCK_STATUS,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_padlock_status, NULL,
			       &padlock_state),
	BT_GATT_CCC(padlocklc_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_PADLOCK_KEY,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, write_padlock_key, NULL),
);

int bt_padlock_init(struct bt_padlock_cb *callbacks)
{
	if (callbacks) {
		padlock_cb.status_cb    = callbacks->status_cb;
		padlock_cb.key_cb = callbacks->key_cb;
	}

	return 0;
}

int bt_padlock_send_button_state(uint32_t button_state)
{
	if (!notify_enabled) {
		return -EACCES;
	}

	return bt_gatt_notify(NULL, &padlock_svc.attrs[2],
			      &button_state,
			      sizeof(button_state));
}

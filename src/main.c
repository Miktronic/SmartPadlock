/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "ble.h"

#include <zephyr/settings/settings.h>

#include "led_buttons.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include "adc.h"

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)
				
#define RUN_LED_BLINK_INTERVAL  500

#define KEY_ID 			1
#define AUTO_CLOSE_ID	2
extern bool notify_enabled;

uint16_t battery_level = 0x00;
uint32_t lock_status = 0x00;
uint8_t pre_lock_status = 0x00;
uint8_t cmd_status = 0x00;
uint8_t timeout = 0x00;
uint8_t bt_buf[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t key_array[6] = {0x01, 0x02, 0x03, 0x04, 0x01, 0x02};
uint8_t xor_array[6] = {0x73, 0x74, 0x65, 0x76, 0x65, 0x65};

uint8_t input_idx = 0;
uint8_t key_buf[6] = {0x00};

uint32_t device_status = 0;
uint32_t pre_device_status = 0;
struct nvs_fs fs;

uint8_t button_input = 0;
uint8_t bt_connected = 0;
uint8_t led_blink = 0;
uint8_t scanned = 0;
uint8_t scanned_timeout = 0;
uint8_t charging = 0;
uint8_t pressed_key = 0x00;
uint8_t usb_detect = 0;
uint8_t auto_closed_en = 0;

#define KEY_LEN			(sizeof(key_array) - 2)
#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
		      0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}
	bt_connected = 1;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
	bt_connected = 0;
}

#ifdef CONFIG_BT_LBS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d\n", addr, level,
			err);
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
#ifdef CONFIG_BT_LBS_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_LBS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

static void app_key_cb(uint8_t* buf, uint16_t len)
{
	memcpy(bt_buf, buf, 8);
}

static uint32_t app_status_cb(void)
{
	return device_status;
}

static struct bt_padlock_cb padlock_callbacs = {
	.key_cb    = app_key_cb,
	.status_cb = app_status_cb,
};

static int init_nvs(void){
	int rc = 0;
	struct flash_pages_info info;

	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		printk("Flash device %s is not ready\n", fs.flash_device->name);
		return -EINVAL;
	}
	fs.offset = NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
		return  -EINVAL;
	}
	fs.sector_size = info.size;
	fs.sector_count = 3U;

	rc = nvs_mount(&fs);
	if (rc) {
		printk("Flash Init failed\n");
		return  -EINVAL;
	}

	return 0;
}

void button_scan(void)
{
	if(button_input == 1)
	{
		user_set_led(BLUE_LED3, 1);
		if(pressed_key == ENTER_BTN1){		
			input_idx = 0;
		}
		else{
			key_buf[input_idx] = pressed_key;
			input_idx = input_idx + 1;	
		}
		button_input = 0;
	}

	if (input_idx == 6){
		if ((key_buf[0] = key_array[0]) && (key_buf[1] == key_array[1]) && (key_buf[2] == key_array[2]) && 
			(key_buf[3] == key_array[3]) && (key_buf[4] == key_array[4]) && (key_buf[5] == key_array[5]))
		{
			user_open_lock();
			cmd_status = 1;
		}
		else{
			user_set_led(RED_LED1, 1);
			k_sleep(K_MSEC(300));
			user_set_led(RED_LED1, 0);
		}
		input_idx = 0;
		button_input = 0;
		key_buf[0] = 0x00;
		key_buf[1] = 0x00;
		key_buf[2] = 0x00;
		key_buf[3] = 0x00;
		key_buf[4] = 0x00; 
		key_buf[5] = 0x00;	
	}

	lock_status =  get_lock_status();
}

int main(void)
{
	int err;

	err = init_nvs();	
	err = battery_setup();
	err = battery_measure_enable(true);
	// read the KEY
	err = nvs_read(&fs, KEY_ID, &key_array, sizeof(key_array));
	if (err > 0) { /* item was found, show it */
		printk("Id: %d, Address: %s\n", KEY_ID, key_array);
	} else   {/* item was not found, add it */
		(void)nvs_write(&fs, KEY_ID, &key_array, strlen(key_array));
	}

	err = nvs_read(&fs, AUTO_CLOSE_ID, &auto_closed_en, sizeof(auto_closed_en));
	if (err > 0) { /* item was found, show it */
		printk("Id: %d, Address: %s\n", AUTO_CLOSE_ID, auto_closed_en);
	} else   {/* item was not found, add it */
		(void)nvs_write(&fs, AUTO_CLOSE_ID, &auto_closed_en, strlen(auto_closed_en));
	}

	user_leds_init();
	user_buttons_init();

	if (IS_ENABLED(CONFIG_BT_LBS_SECURITY_ENABLED)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			printk("Failed to register authorization callbacks.\n");
			return 0;
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			printk("Failed to register authorization info callbacks.\n");
			return 0;
		}
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_padlock_init(&padlock_callbacs);
	if (err) {
		printk("Failed to init LBS (err:%d)\n", err);
		return 0;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return 0;
	}

	printk("Advertising successfully started\n");

	pre_lock_status = get_lock_status();
	
	for (;;) {

		// process the user commands

		// If opening command ...
		button_scan();

		if(bt_connected){
			if((bt_buf[0] == 0x55) && (bt_buf[7] == 0xAA))
			{
				if ((bt_buf[1] == key_array[0]) && (bt_buf[2] == key_array[1]) && (bt_buf[3] == key_array[2]) && 
					(bt_buf[4] == key_array[3]) && (bt_buf[5] == key_array[4]) && (bt_buf[6] == key_array[5]))
				{
					user_open_lock();
					cmd_status = 1;
				}
				else{
					user_set_led(RED_LED1, 1);
				}
				bt_buf[0] = 0x00;
				bt_buf[1] = 0x00;
				bt_buf[2] = 0x00;
				bt_buf[3] = 0x00;
				bt_buf[4] = 0x00;
				bt_buf[5] = 0x00;
				bt_buf[6] = 0x00;
				bt_buf[7] = 0x00;
			}
			//If updating the key...
			else if((bt_buf[0] == 0x55) && (bt_buf[7] == 0xBB)){

				bt_buf[1] = bt_buf[1] ^ xor_array[0];
				bt_buf[2] = bt_buf[2] ^ xor_array[1];
				bt_buf[3] = bt_buf[3] ^ xor_array[2];
				bt_buf[4] = bt_buf[4] ^ xor_array[3];
				bt_buf[5] = bt_buf[5] ^ xor_array[4];
				bt_buf[6] = bt_buf[6] ^ xor_array[5];

				err = nvs_write(&fs, KEY_ID, &bt_buf[1], KEY_LEN);

				if((err == KEY_LEN) || (err == 0))
				{	
					user_set_led(BLUE_LED3, 1);
					key_array[0] = bt_buf[1];
					key_array[1] = bt_buf[2];
					key_array[2] = bt_buf[3];
					key_array[3] = bt_buf[4];
					key_array[4] = bt_buf[5];
					key_array[5] = bt_buf[6];
				}
				else{
					user_set_led(RED_LED1, 1);
				}
				bt_buf[0] = 0x00;
				bt_buf[1] = 0x00;
				bt_buf[2] = 0x00;
				bt_buf[3] = 0x00;
				bt_buf[4] = 0x00;
				bt_buf[5] = 0x00;
				bt_buf[6] = 0x00;
				bt_buf[7] = 0x00;
			}
			else if ((bt_buf[0] == 0x55) && (bt_buf[7] == 0xCC)){
				auto_closed_en = bt_buf[6];

				(void)nvs_write(&fs, AUTO_CLOSE_ID, &auto_closed_en, strlen(auto_closed_en));
			}
			else if((bt_buf[0] == 0x55) && (bt_buf[7] == 0xAB) && (auto_closed_en == 1))
			{
				if ((bt_buf[1] == key_array[0]) && (bt_buf[2] == key_array[1]) && (bt_buf[3] == key_array[2]) && 
					(bt_buf[4] == key_array[3]) && (bt_buf[5] == key_array[4]) && (bt_buf[6] == key_array[5]))
				{
					user_close_lock();
					cmd_status = 0;
				}
				else{
					user_set_led(RED_LED1, 1);
				}
				bt_buf[0] = 0x00;
				bt_buf[1] = 0x00;
				bt_buf[2] = 0x00;
				bt_buf[3] = 0x00;
				bt_buf[4] = 0x00;
				bt_buf[5] = 0x00;
				bt_buf[6] = 0x00;
				bt_buf[7] = 0x00;
			}
			

			// update the lock status
			battery_level = battery_sample() * 1.403;

			device_status = (lock_status & 0x0000FFFF) + (uint32_t)(battery_level << 16);

			if ((notify_enabled == true) && (pre_device_status != device_status)){
				bt_padlock_send_button_state(device_status);
			}
			pre_device_status = device_status;
		}
		
		// if open command...
		if(cmd_status == 1){ // 10 second timeout to open a lock 
			timeout = timeout + 1;
			if(timeout > 5){
				timeout = 0;
				if((lock_status == 1) && (auto_closed_en == 0)){ // after 5s, close a lock again
					user_close_lock();
					cmd_status = 0;
				}
			}
		}
		if((timeout == 0) && (cmd_status == 1) && (auto_closed_en == 0) && (pre_lock_status == 0) && (lock_status == 1)){ // if lock is detected, run a motor
			user_close_lock();
			cmd_status = 0;
		}

		pre_lock_status = lock_status;		
		//check charging
		
		usb_detect = get_usb_status();
		if( usb_detect == 1){
			if (bt_connected == 0){
				battery_level = battery_sample() * 1.403;
			}
			if(battery_level > 0x1060){
				user_set_led(WHITE_LED4, (led_blink % 2));
			}
			else{
				user_set_led(WHITE_LED4, 1);
			}
			led_blink++;
		}

		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
		user_set_led_all_off();
	}	
}	

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_H_
#define BLE_H_

/**@file
 * @defgroup bt_lbs LED Button Service API
 * @{
 * @brief API for the LED Button Service (LBS).
 */

#include <zephyr/types.h>

/** @brief LBS Service UUID. */
#define BT_UUID_PADLOCK_VAL \
	BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief Button Characteristic UUID. */
#define BT_UUID_PADLOCK_STATUS_VAL \
	BT_UUID_128_ENCODE(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief LED Characteristic UUID. */
#define BT_UUID_PADLOCK_KEY_VAL \
	BT_UUID_128_ENCODE(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd123)


#define BT_UUID_PADLOCK           BT_UUID_DECLARE_128(BT_UUID_PADLOCK_VAL)
#define BT_UUID_PADLOCK_STATUS    BT_UUID_DECLARE_128(BT_UUID_PADLOCK_STATUS_VAL)
#define BT_UUID_PADLOCK_KEY       BT_UUID_DECLARE_128(BT_UUID_PADLOCK_KEY_VAL)

/** @brief Callback type for when an LED state change is received. */
typedef void (*key_cb_t)(uint8_t* buf, uint16_t length);

/** @brief Callback type for when the button state is pulled. */
typedef uint32_t (*status_cb_t)(void);

/** @brief Callback struct used by the LBS Service. */
struct bt_padlock_cb {
	/** key send callback. */
	key_cb_t    key_cb;
	/** lock status read callback. */
	status_cb_t status_cb;
};

/** @brief Initialize the LBS Service.
 *
 * This function registers a GATT service with two characteristics: Button
 * and LED.
 * Send notifications for the Button Characteristic to let connected peers know
 * when the button state changes.
 * Write to the LED Characteristic to change the state of the LED on the
 * board.
 *
 * @param[in] callbacks Struct containing pointers to callback functions
 *			used by the service. This pointer can be NULL
 *			if no callback functions are defined.
 *
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_padlock_init(struct bt_padlock_cb *callbacks);

/** @brief Send the button state.
 *
 * This function sends a binary state, typically the state of a
 * button, to all connected peers.
 *
 * @param[in] button_state The state of the button.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_padlock_send_button_state(uint32_t button_state);

/**
 * @}
 */

#endif /* BT_LBS_H_ */

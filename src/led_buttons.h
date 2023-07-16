/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef LED_BUTTONS_H_
#define LED_BUTTONS_H_

/** @file dk_buttons_and_leds.h
 * @brief Module for handling buttons and LEDs on Nordic DKs.
 * @defgroup dk_buttons_and_leds DK buttons and LEDs
 * @{
 */

#include <zephyr/types.h>
#include <zephyr/sys/slist.h>

#define USER_NO_LEDS_MSK        (0)
#define RED_LED1                0
#define GREEN_LED2              1
#define BLUE_LED3               2
#define WHITE_LED4              3
#define AIN_GPIO				4
#define BIN_GPIO				5

#define RED_LED1_MSK        BIT(RED_LED1)
#define GREEN_LED2_MSK      BIT(GREEN_LED2)
#define BLUE_LED3_MSK       BIT(BLUE_LED3)
#define WHITE_LED4_MSK      BIT(WHITE_LED4)
#define AIN_GPIO_MSK		BIT(AIN_GPIO)
#define BIN_GPIO_MSK		BIT(BIN_GPIO)

#define USER_ALL_LEDS_MSK   (RED_LED1_MSK | GREEN_LED2_MSK | BLUE_LED3_MSK | WHITE_LED4_MSK | AIN_GPIO_MSK | BIN_GPIO_MSK)

#define USER_NO_BTNS_MSK   (0)
#define ENTER_BTN1          0		// ENTER
#define UP_BTN2          	1		// UP
#define DOWN_BTN3          	2		// DOWN
#define RIGHT_BTN4          3		// RIGHT
#define LEFT_BTN5          	4		// LEFT
#define LOCK_BTN6			5		// LOCKDETECT
#define USB_BTN7			6		// USBDETECT

#define ENTER_BTN1_MSK      BIT(ENTER_BTN1)
#define UP_BTN2_MSK      	BIT(UP_BTN2)
#define DOWN_BTN3_MSK      	BIT(DOWN_BTN3)
#define RIGHT_BTN4_MSK      BIT(RIGHT_BTN4)
#define LEFT_BTN5_MSK      	BIT(LEFT_BTN5)
#define LOCK_BTN6_MSK		BIT(LOCK_BTN6)
#define USB_BTN7_MSK		BIT(USB_BTN7)

#define USER_ALL_BTNS_MSK  (ENTER_BTN1_MSK | UP_BTN2_MSK | \
			  DOWN_BTN3_MSK | RIGHT_BTN4_MSK | LEFT_BTN5_MSK | LOCK_BTN6_MSK | USB_BTN7_MSK)

uint32_t get_padlock_buttons(void);
int user_leds_init(void);
int user_buttons_init(void);
int user_set_leds(uint32_t leds);
int user_set_leds_state(uint32_t leds_on_mask, uint32_t leds_off_mask);
int user_set_led(uint8_t led_idx, uint32_t val);
int user_set_led_on(uint8_t led_idx);
int user_set_led_off(uint8_t led_idx);
uint8_t get_lock_status(void);
void user_open_lock(void);
void user_close_lock(void);
uint8_t get_usb_status(void);
uint8_t get_enter_status(void);
void user_set_led_all_off(void);
/** @} */

#endif /* DK_BUTTON_AND_LEDS_H__ */

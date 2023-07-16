/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <soc.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <nrfx.h>
#include "led_buttons.h"

#define CONFIG_BUTTON_SCAN_INTERVAL 1
#define BUTTONS_NODE DT_PATH(buttons)
#define LEDS_NODE DT_PATH(leds)

#define GPIO0_DEV DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define GPIO1_DEV DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio1))

/* GPIO0, GPIO1 and GPIO expander devices require different interrupt flags. */
#define FLAGS_GPIO_0_1_ACTIVE GPIO_INT_LEVEL_ACTIVE
#define FLAGS_GPIO_EXP_ACTIVE (GPIO_INT_EDGE | GPIO_INT_HIGH_1 | GPIO_INT_LOW_0 | GPIO_INT_ENABLE)

#define GPIO_SPEC_AND_COMMA(button_or_led) GPIO_DT_SPEC_GET(button_or_led, gpios),

static const struct gpio_dt_spec padlock_buttons[] = {
#if DT_NODE_EXISTS(BUTTONS_NODE)
	DT_FOREACH_CHILD(BUTTONS_NODE, GPIO_SPEC_AND_COMMA)
#endif
};

static const struct gpio_dt_spec padlock_leds[] = {
#if DT_NODE_EXISTS(LEDS_NODE)
	DT_FOREACH_CHILD(LEDS_NODE, GPIO_SPEC_AND_COMMA)
#endif
};

static struct gpio_callback button_cb_data;
extern uint8_t scanned;
extern uint8_t input_idx;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	scanned = 1;
}

uint32_t get_padlock_buttons(void)
{
	uint32_t ret = 0;
	for (size_t i = 0; i < ARRAY_SIZE(padlock_buttons); i++) {
		int val;

		val = gpio_pin_get_dt(&padlock_buttons[i]);
		if (val < 0) {
			printk("Cannot read gpio pin");
			return 0;
		}
		if (val) {
			ret |= 1U << i;
		}
	}

	return ret;
}
uint8_t get_lock_status(void)
{
	return gpio_pin_get_dt(&padlock_buttons[LOCK_BTN6]);
}
uint8_t get_usb_status(void)
{
	return gpio_pin_get_dt(&padlock_buttons[USB_BTN7]);
}
uint8_t get_enter_status(void)
{
	return gpio_pin_get_dt(&padlock_buttons[ENTER_BTN1]);
}
int user_leds_init(void)
{
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(padlock_leds); i++) {
		err = gpio_pin_configure_dt(&padlock_leds[i], GPIO_OUTPUT);
		if (err) {
			printk("Cannot configure LED gpio");
			return err;
		}
	}

	return user_set_leds_state(USER_NO_LEDS_MSK, USER_ALL_LEDS_MSK);
}

int user_buttons_init(void)
{
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(padlock_buttons); i++) {
		/* Enable pull resistor towards the inactive voltage. */
		gpio_flags_t flags =
			padlock_buttons[i].dt_flags & GPIO_ACTIVE_LOW ?
			GPIO_PULL_UP : GPIO_PULL_DOWN;

		if (i == USB_BTN7){
			err = gpio_pin_configure_dt(&padlock_buttons[i], GPIO_INPUT);
		}else{
			err = gpio_pin_configure_dt(&padlock_buttons[i], GPIO_INPUT | flags);
		}

		if (err) {
			printk("Cannot configure button gpio");
			return err;
		}
	}

	uint32_t pin_mask = 0;

	for (size_t i = 0; i < ARRAY_SIZE(padlock_buttons); i++) {
		/* Module starts in scanning mode and will switch to
		 * callback mode if no button is pressed.
		 */
		/*if (i == ENTER_BTN1){
			err = gpio_pin_interrupt_configure_dt(&padlock_buttons[i], GPIO_INT_LEVEL_INACTIVE);

			if (err != 0) {
				printk("Error %d: failed to configure interrupt on %s pin %d\n",
				err, padlock_buttons[i].port->name, padlock_buttons[i].pin);
			}


			gpio_init_callback(&button_cb_data, button_pressed, BIT(padlock_buttons[i].pin));
			gpio_add_callback(padlock_buttons[i].port, &button_cb_data);
		}*/
		//else{
			err = gpio_pin_interrupt_configure_dt(&padlock_buttons[i],
						      GPIO_INT_DISABLE);
			if (err) {
				printk("Cannot disable callbacks()");
				return err;
			}
		//}

		pin_mask |= BIT(padlock_buttons[i].pin);
		
	}
	return 0;
}
int user_set_leds(uint32_t leds)
{
	return user_set_leds_state(leds, USER_ALL_LEDS_MSK);
}

int user_set_leds_state(uint32_t leds_on_mask, uint32_t leds_off_mask)
{
	if ((leds_on_mask & ~USER_ALL_LEDS_MSK) != 0 ||
	   (leds_off_mask & ~USER_ALL_LEDS_MSK) != 0) {
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(padlock_leds); i++) {
		int val, err;

		if (BIT(i) & leds_on_mask) {
			val = 1;
		} else if (BIT(i) & leds_off_mask) {
			val = 0;
		} else {
			continue;
		}

		err = gpio_pin_set_dt(&padlock_leds[i], val);
		if (err) {
			printk("Cannot write LED gpio");
			return err;
		}
	}

	return 0;
}

int user_set_led(uint8_t led_idx, uint32_t val)
{
	int err;

	if (led_idx >= ARRAY_SIZE(padlock_leds)) {
		printk("LED index out of the range");
		return -EINVAL;
	}
	err = gpio_pin_set_dt(&padlock_leds[led_idx], val);
	if (err) {
		printk("Cannot write LED gpio");
	}
	return err;
}

int user_set_led_on(uint8_t led_idx)
{
	return user_set_led(led_idx, 1);
}

int user_set_led_off(uint8_t led_idx)
{
	return user_set_led(led_idx, 0);
}
void user_set_led_all_off(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(padlock_leds); i++) {
		gpio_pin_set_dt(&padlock_leds[i], 0);
	}
}
void user_close_lock(void)
{
	int err;
	user_set_led(GREEN_LED2, 1);
	err = gpio_pin_set_dt(&padlock_leds[AIN_GPIO], 1);
	if (err) {
		printk("Cannot write AIN gpio");
	}
	err = gpio_pin_set_dt(&padlock_leds[BIN_GPIO], 0);
	if (err) {
		printk("Cannot write AIN gpio");
	}
	k_sleep(K_MSEC(500));
	err = gpio_pin_set_dt(&padlock_leds[AIN_GPIO], 0);
	if (err) {
		printk("Cannot write AIN gpio");
	}
	user_set_led(GREEN_LED2, 0);
}
void user_open_lock(void)
{
	int err;
	user_set_led(GREEN_LED2, 1);
	err = gpio_pin_set_dt(&padlock_leds[AIN_GPIO], 0);
	if (err) {
		printk("Cannot write AIN gpio");
	}
	err = gpio_pin_set_dt(&padlock_leds[BIN_GPIO], 1);
	if (err) {
		printk("Cannot write AIN gpio");
	}
	k_sleep(K_MSEC(500));
	err = gpio_pin_set_dt(&padlock_leds[BIN_GPIO], 0);
	if (err) {
		printk("Cannot write AIN gpio");
	}
	user_set_led(GREEN_LED2, 0);
}

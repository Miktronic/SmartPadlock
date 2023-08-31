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
extern uint8_t button_input;
extern uint8_t pressed_key;

static void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	button_input = 1;

	for (size_t i = 0; i < 5; i++) {
		if ((1 << padlock_buttons[i].pin) == pins){
			pressed_key = i;
		}
	}

}

uint32_t get_padlock_buttons(void)
{
	uint32_t ret = 0;
	for (size_t i = 0; i < ARRAY_SIZE(padlock_buttons); i++) {
		int val;

		val = gpio_pin_get_dt(&padlock_buttons[i]);
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
void user_leds_init(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(padlock_leds); i++) {
		gpio_pin_configure_dt(&padlock_leds[i], GPIO_OUTPUT);
	}
}

void user_buttons_init(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(padlock_buttons); i++) {
		/* Enable pull resistor towards the inactive voltage. */
		gpio_flags_t flags =
			padlock_buttons[i].dt_flags & GPIO_ACTIVE_LOW ?
			GPIO_PULL_UP : GPIO_PULL_DOWN;

		if (i == USB_BTN7){
			gpio_pin_configure_dt(&padlock_buttons[i], GPIO_INPUT);
		}else{
			gpio_pin_configure_dt(&padlock_buttons[i], GPIO_INPUT | flags);
		}
	}

	uint32_t pin_mask = 0;

	for (size_t i = 0; i < 5; i++) {
		/* Module starts in scanning mode and will switch to
		 * callback mode if no button is pressed.
		 */

		gpio_pin_interrupt_configure_dt(&padlock_buttons[i], GPIO_INT_EDGE_RISING);
		
		
		/*err = gpio_pin_interrupt_configure_dt(&padlock_buttons[i],
					      GPIO_INT_DISABLE);
		if (err) {
			printk("Cannot disable callbacks()");
			return err;
		}
		*/
		pin_mask |= BIT(padlock_buttons[i].pin);
		
	}
	gpio_init_callback(&button_cb_data, button_pressed, pin_mask);
	gpio_add_callback(padlock_buttons[0].port, &button_cb_data);

	gpio_pin_interrupt_configure_dt(&padlock_buttons[5], GPIO_INT_DISABLE);
	gpio_pin_interrupt_configure_dt(&padlock_buttons[6], GPIO_INT_DISABLE);
}

void user_set_led(uint8_t led_idx, uint32_t val)
{
	gpio_pin_set_dt(&padlock_leds[led_idx], val);
}

void user_close_lock(void)
{
	int err;
	user_set_led(GREEN_LED2, 1);
	gpio_pin_set_dt(&padlock_leds[AIN_GPIO], 1);
	gpio_pin_set_dt(&padlock_leds[BIN_GPIO], 0);
	k_sleep(K_MSEC(500));
	gpio_pin_set_dt(&padlock_leds[AIN_GPIO], 0);
	user_set_led(GREEN_LED2, 0);
}

void user_open_lock(void)
{
	int err;
	user_set_led(GREEN_LED2, 1);
	gpio_pin_set_dt(&padlock_leds[AIN_GPIO], 0);
	gpio_pin_set_dt(&padlock_leds[BIN_GPIO], 1);
	k_sleep(K_MSEC(500));
	gpio_pin_set_dt(&padlock_leds[BIN_GPIO], 0);
	user_set_led(GREEN_LED2, 0);
}

void user_set_led_all_off(void)
{
	user_set_led(RED_LED1, 0);
	user_set_led(GREEN_LED2, 0);
	user_set_led(BLUE_LED3, 0);
	user_set_led(WHITE_LED4, 0);

}
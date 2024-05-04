/*
 * Copyright (c) 2024 Clunky Machines - julien@vermillard.com
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <stdio.h>

#define LOG_MODULE_NAME m5stack_rover_sample
#define LOG_LEVEL LOG_LEVEL_DBG

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

static const struct i2c_dt_spec i2c_rover = I2C_DT_SPEC_GET(DT_NODELABEL(rover));

struct k_event onOff_event;

#define ROVER_OFF 0
#define ROVER_FRONT 1
#define ROVER_REVERSE 2
#define ROVER_LEFT 3
#define ROVER_RIGHT 4

// look at the m5stack rover doc for the protocol but in a nutshell
// 0x00: front left motor
// 0x01: front right motor
// 0x02: back left motor
// 0x03: back right motor
// and the value is the speed (negative for reverse)
void setRover(int roverMode)
{
    switch (roverMode)
    {
    case ROVER_FRONT:
        i2c_reg_write_byte_dt(&i2c_rover, 0x00, 0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x01, 0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x02, 0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x03, 0x20);
        break;
    case ROVER_REVERSE:
        i2c_reg_write_byte_dt(&i2c_rover, 0x00, -0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x01, -0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x02, -0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x03, -0x20);
        break;
    case ROVER_LEFT:
        i2c_reg_write_byte_dt(&i2c_rover, 0x00, -0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x01, 0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x02, 0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x03, -0x20);
        break;
    case ROVER_RIGHT:
        i2c_reg_write_byte_dt(&i2c_rover, 0x00, 0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x01, -0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x02, -0x20);
        i2c_reg_write_byte_dt(&i2c_rover, 0x03, 0x20);
        break;
    case ROVER_OFF:
    default:
        i2c_reg_write_byte_dt(&i2c_rover, 0x00, 0x00);
        i2c_reg_write_byte_dt(&i2c_rover, 0x01, 0x00);
        i2c_reg_write_byte_dt(&i2c_rover, 0x02, 0x00);
        i2c_reg_write_byte_dt(&i2c_rover, 0x03, 0x00);
    }
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
    // we send back an event to the main thread, because a blocking i2c write
    // in the button interrupt handler will crash
    k_event_set(&onOff_event, 0x001);
}

int main(void)
{
    k_event_init(&onOff_event);

    printk("Hello World\n");

    if (!device_is_ready(i2c_rover.bus))
    {
        printk("Rover I2C not ready.\n");
        return 0;
    }
    else
    {
        printk("Rover I2C ready.\n");
        printk("bus: '%s' address: 0x%x\n", i2c_rover.bus->name, i2c_rover.addr);
    }

    int ret;

    if (!gpio_is_ready_dt(&button))
    {
        printk("Error: button device %s is not ready\n",
               button.port->name);
        return 0;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0)
    {
        printk("Error %d: failed to configure %s pin %d\n",
               ret, button.port->name, button.pin);
        return 0;
    }

    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0)
    {
        printk("Error %d: failed to configure interrupt on %s pin %d\n",
               ret, button.port->name, button.pin);
        return 0;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);
    printk("Set up button at %s pin %d\n", button.port->name, button.pin);

    int roverMode = ROVER_OFF;
    while (1)
    {
        uint32_t events = k_event_wait(&onOff_event, 0x1, true, K_FOREVER);
        if (events == 0)
        {
            printk("No input event are available!\n");
        }
        else
        {
            printk("Button event!\n");
            roverMode = (++roverMode) % 5;
            setRover(roverMode);
        }
        k_sleep(K_MSEC(10));
    }
    return -1;
}
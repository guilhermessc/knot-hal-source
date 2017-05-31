/*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */


#ifndef __HAL_GPIO_H__
#define __HAL_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INPUT 0
#define OUTPUT 1

#define LOW 0
#define HIGH 1

int hal_gpio_setup(void);
void hal_gpio_unmap(void);
void hal_gpio_pin_mode(uint8_t, uint8_t);
void hal_gpio_digital_write(uint8_t, uint8_t);
int hal_gpio_digital_read(uint8_t);
int hal_gpio_analog_read(uint8_t);
void hal_gpio_analog_reference(uint8_t mode);
void hal_gpio_analog_write(uint8_t, int);

#ifdef __cplusplus
}
#endif

#endif
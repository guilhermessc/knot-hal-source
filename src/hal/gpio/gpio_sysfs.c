/*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include "hal/gpio_sysfs.h"

#define BUFFER_MAX 3
#define DIRECTION_MAX 35
#define VALUE_MAX 30
#define HIGHEST_GPIO 26;

static uint8_t initialized_gpio[HIGHEST_GPIO];
static int ngpio = 0;

static int GPIOExport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return -1;
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return 0;
}

static int GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return -1;
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return 0;
}

static int GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";

	char path[DIRECTION_MAX];
	int fd;

	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return -1;
	}

	if (-1 == write(fd, &s_directions_str[INPUT == dir ? 0 : 3],
		INPUT == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return -1;
	}

	close(fd);
	return 0;
}

static int GPIORead(int pin)
{
	char path[VALUE_MAX];
	char value_str[3];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return -1;
	}

	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return -1;
	}

	close(fd);

	return atoi(value_str);
}

static int GPIOWrite(int pin, int value)
{
	static const char s_values_str[] = "01";

	char path[VALUE_MAX];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return -1;
	}

	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return -1;
	}

	close(fd);
	return 0;
}

int hal_gpio_setup(void)
{
	ngpio = 0;
	return 0;
}

void hal_gpio_unmap(void)
{
	while (ngpio)
		GPIOUnexport((int) initialized_gpio[--ngpio]);
}

void hal_gpio_pin_mode(uint8_t gpio, uint8_t mode)
{
	if (ngpio > HIGHEST_GPIO) {
		fprintf(stderr, "Cannot initialize gpio: maximum number exceeded\n");
		return;
	}

	if (GPIOExport((int) gpio) == 0)
		initialized_gpio[ngpio++] = gpio;

	GPIODirection((int) gpio, (int) mode);
}

void hal_gpio_digital_write(uint8_t gpio, uint8_t value)
{
	GPIOWrite(gpio, value);
}

int hal_gpio_digital_read(uint8_t gpio)
{
	return (GPIORead((int) gpio) == LOW ? LOW : HIGH);
}

int hal_gpio_analog_read(uint8_t)
{
	return 0;
}

void hal_gpio_analog_reference(uint8_t mode)
{

}

void hal_gpio_analog_write(uint8_t, int)
{

}
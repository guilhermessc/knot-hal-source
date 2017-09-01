/*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

GMainLoop * mainloop;

#define INPUT_PIN	20
#define OUTPUT_PIN	16





#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <poll.h>


#define HAL_GPIO_INPUT 0
#define HAL_GPIO_OUTPUT 1

#define HAL_GPIO_LOW 0
#define HAL_GPIO_HIGH 1

#define HAL_GPIO_NONE 0
#define HAL_GPIO_RISING 1
#define HAL_GPIO_FALLING 2
#define HAL_GPIO_BOTH 3

#define HIGHEST_GPIO 28

/* Bit Operation */
#define SET_BIT(data, idx)		((data) |= 1 << (idx))
#define CLR_BIT(data, idx)		((data) &= ~(1 << (idx)))
#define CHK_BIT(data, idx)		((data) & (1 << (idx)))

/* 0 = not initialized, 1 = initialized */
#define BIT_INITIALIZED	0
/* 0 = input, 1 = output */
#define BIT_DIRECTION	1
/* 0 = not an interrupt, 1 = interrupt pin*/
#define BIT_INTERRUPT	2


static uint8_t gpio_map[HIGHEST_GPIO];

static int gpio_export(int pin)
{
	char buffer[3], path[35];
	ssize_t bytes_written;
	int fd, err = 0;

	/* GPIO already exported */
	snprintf(path, 35, "/sys/class/gpio/gpio%2d", pin);
	if (access(path, F_OK) == 0)
		return 0;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd == -1)
		/* Failed to open export for writing! */
		return -errno;

	bytes_written = snprintf(buffer, 3, "%2d", pin);

	if (write(fd, buffer, bytes_written) == -1)
		err = errno;

	close(fd);

	return -err;
}

static int gpio_unexport(int pin)
{
	char buffer[3];
	ssize_t bytes_written;
	int fd, err = 0;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (fd == -1)
		/* Failed to open unexport for writing! */
		return -errno;

	bytes_written = snprintf(buffer, 3, "%2d", pin);

	if (write(fd, buffer, bytes_written) == -1)
		err = errno;

	close(fd);

	return -err;
}

static int gpio_direction(int pin, int dir)
{
	char path[35];
	int fd, err = 0, delay_ms;

	/*
	 * Rigth after the GPIO is exported the system have a small
	 * latency before it grants permission to write.
	 */
	snprintf(path, 35, "/sys/class/gpio/gpio%2d/direction", pin);
	for (delay_ms = 0; delay_ms < 200; delay_ms += 10) {
		if (access(path, W_OK) == 0)
			break;
		usleep(delay_ms * 1000);
	}

	snprintf(path, 35, "/sys/class/gpio/gpio%2d/direction", pin);
	fd = open(path, O_WRONLY);
	if (fd == -1)
		/* Failed to open gpio direction for writing! */
		return -errno;

	if (write(fd, dir == HAL_GPIO_INPUT ? "in" : "out",
		dir == HAL_GPIO_INPUT ? 2 : 3) == -1)
		/* Failed to set direction! */
		err = errno;

	close(fd);

	return -err;
}

static int gpio_read(int pin)
{
	char path[30];
	char value_str[3];
	int fd;

	snprintf(path, 30, "/sys/class/gpio/gpio%2d/value", pin);
	fd = open(path, O_RDONLY);
	if (fd == -1)
		/* Failed to open gpio value for reading! */
		return -errno;

	/*
	 * This reading operation returns
	 * 3 characters wich can be:
	 * '0', '\n' and '\0' or
	 * '1', '\n' and '\0'
	 */
	if (read(fd, value_str, 3) == -1) {
		/* Failed to read value! */
		close(fd);
		return -errno;
	}

	close(fd);

	if (value_str[0] == '0' || value_str[0] == '1')
		return value_str[0] - '0';
	return -EAGAIN;
}

static int gpio_write(int pin, int value)
{
	char path[30];
	int fd, err = 0;

	snprintf(path, 30, "/sys/class/gpio/gpio%2d/value", pin);
	fd = open(path, O_WRONLY);
	if (fd == -1)
		/* Failed to open gpio value for writing! */
		return -errno;

	if (write(fd, value == HAL_GPIO_LOW ? "0" : "1", 1) != 1)
		/* Failed to write value! */
		err = errno;

	close(fd);

	return -err;
}

static int gpio_edge(int pin, int edge)
{
	char path[30];
	int fd, err = 0, werr = 0;

	snprintf(path, 30, "/sys/class/gpio/gpio%2d/edge", pin);
	fd = open(path, O_WRONLY);

	if (fd == -1)
		/* Failed to open gpio value for writing! */
		return -errno;

	switch (edge) {

	case HAL_GPIO_NONE:
		werr = write(fd, "none", 4);
		break;

	case HAL_GPIO_RISING:
		werr = write(fd, "rising", 6);
		break;

	case HAL_GPIO_FALLING:
		werr = write(fd, "falling", 7);
		break;

	case HAL_GPIO_BOTH:
		werr = write(fd, "both", 4);
		break;
	}

	if (werr == -1)
		err = errno;

	close(fd);

	return -err;
}

static int get_gpio_fd(int gpio)
{
	char path[30], dummy_buffer[1024];
	int fd;
	unsigned int n;

	snprintf(path, 30, "/sys/class/gpio/gpio%2d/value", gpio);
	fd = open(path, O_RDONLY);

	if (fd == -1)
		/* Failed to open gpio value for reading! */
		return -errno;

	/* Clear the file before watching */
	ioctl(fd, FIONREAD, &n);
	while (n > 0) {
		if (n >= sizeof(dummy_buffer))
			read(fd, dummy_buffer, sizeof(dummy_buffer));
		else
			read(fd, dummy_buffer, n);
		n -= sizeof(dummy_buffer);
	}

	return fd;
}

int hal_gpio_setup(void)
{
	char *ret;

	ret = memset(gpio_map, 0, sizeof(gpio_map));
	return (ret == NULL) ? -EAGAIN : 0;
}

void hal_gpio_unmap(void)
{
	int i;

	for (i = 0; i < HIGHEST_GPIO; ++i)
		if (CHK_BIT(gpio_map[i], BIT_INITIALIZED))
			gpio_unexport(i);

	memset(gpio_map, 0, sizeof(gpio_map));
}

int hal_gpio_pin_mode(uint8_t gpio, uint8_t mode)
{
	int err;

	if (gpio > HIGHEST_GPIO)
		/* Cannot initialize gpio: maximum gpio exceeded */
		return -EINVAL;

	if (!CHK_BIT(gpio_map[gpio-1], BIT_INITIALIZED)) {

		err = gpio_export(gpio);
		if (err < 0)
			return err;

		SET_BIT(gpio_map[gpio-1], BIT_INITIALIZED);
	}

	err = gpio_direction(gpio, mode);
	if (err < 0)
		return err;

	if (mode == HAL_GPIO_INPUT)
		CLR_BIT(gpio_map[gpio-1], BIT_DIRECTION);
	else
		SET_BIT(gpio_map[gpio-1], BIT_DIRECTION);

	return 0;
}

void hal_gpio_digital_write(uint8_t gpio, uint8_t value)
{
	if (CHK_BIT(gpio_map[gpio-1], BIT_DIRECTION)
		&& CHK_BIT(gpio_map[gpio-1], BIT_INITIALIZED))
		gpio_write(gpio, value);
	else{
		/* Changing mode and writing */
		hal_gpio_pin_mode(gpio, HAL_GPIO_OUTPUT);
		hal_gpio_digital_write(gpio, value);
	}
}

int hal_gpio_digital_read(uint8_t gpio)
{
	int ret = 0;

	if (!CHK_BIT(gpio_map[gpio-1], BIT_DIRECTION)
		&& CHK_BIT(gpio_map[gpio-1], BIT_INITIALIZED)) {
		ret = gpio_read(gpio);
		if (ret < 0)
			return ret;
		return (ret == 0 ? HAL_GPIO_LOW : HAL_GPIO_HIGH);
	}

	/* Changing mode and reading */
	hal_gpio_pin_mode(gpio, HAL_GPIO_INPUT);
	return hal_gpio_digital_read(gpio);
}

int hal_gpio_analog_read(uint8_t gpio)
{
	return 0;
}

void hal_gpio_analog_reference(uint8_t mode)
{

}

void hal_gpio_analog_write(uint8_t gpio, int value)
{

}

/*
 * Warning: According to the linux documentation you must
 * follow a set of rules when watching a GPIO pin to trigger
 * the interruption properly:
 *
 * "If you use poll(2), set the events POLLPRI and POLLERR. If you
 * use select(2), set the file descriptor in exceptfds. After
 * poll(2) returns, either lseek(2) to the beginning of the sysfs
 * file and read the new value or close the file and re-open it
 * to read the value.".
 *
 * In case you are using GLIB you must set the conditions
 * G_IO_PRI and G_IO_ERR.
 */
int hal_gpio_get_fd(uint8_t gpio, int edge)
{
	int err;

	if (!CHK_BIT(gpio_map[gpio-1], BIT_INITIALIZED) ||
		CHK_BIT(gpio_map[gpio-1], BIT_DIRECTION))
		return -EIO;

	err = gpio_edge(gpio, edge);
	if (err < 0)
		return err;

	return get_gpio_fd(gpio);
}







/* TODO: check for duplicate */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

GMainLoop * mainloop;

/*
 * In this example we trigger an interrupt by using
 * another pin as output.
 *
 * To test this example you must connect the pins to
 * to be used and you may change the parameters below.
 *
 * Now run the example and, to control the output pin,
 * open the terminal and run the following commands:
 *
 *	echo 1 > /sys/class/gpio/gpio16/value
 *	echo 0 > /sys/class/gpio/gpio16/value
 *
 * Note: These paths depends on the output pin number
 * so if you change the output pin to N change the
 * path to /sys/class/gpio/gpioN/value.
 *
 * Note: We DO NOT use the physical pin numbers here,
 * we use the BCM pin numbers.
 */

#define INPUT_PIN	20
#define OUTPUT_PIN	16
#define EDGE HAL_GPIO_RISING


static void gpio_watch_destroy(gpointer user_data)
{
	int fd = *((int *)user_data);

	close(fd);
	hal_gpio_unmap();
}

static gboolean gpio_watch(GIOChannel *io, GIOCondition cond,
						gpointer user_data)
{
	char data_dummy[10];
	GError *gerr = NULL;
	GIOStatus status_rwnd, status_read;
	gsize rbytes;

	/* MUST clean up the file, you cin simply rewind and read some chars */
	status_rwnd = g_io_channel_seek_position(io, 0, G_SEEK_SET, &gerr);
	status_read = g_io_channel_read_chars(io, data_dummy,
					sizeof(data_dummy)-1, &rbytes, &gerr);
	/* TODO: Add some safety checks on status after cleaning the file */

	printf("Interrupt = %c\n", data_dummy[0]);

	return TRUE;
}

static gboolean tick_tock(gpointer user_data)
{
	static time;

	printf("time: \t%d\n", ++time);
	return TRUE;
}

static void sig_term(int sig)
{
	g_main_loop_quit(mainloop);
}

int main(int argc, char *argv[])
{
	GIOChannel *io;
	GIOCondition cond = G_IO_PRI | G_IO_ERR;
	int fd, err;

	hal_gpio_setup();
	hal_gpio_pin_mode(OUTPUT_PIN, HAL_GPIO_OUTPUT);
	hal_gpio_pin_mode(INPUT_PIN, HAL_GPIO_INPUT);

	fd = hal_gpio_get_fd(INPUT_PIN, EDGE);
	/* TODO: check if fd returned correctly ( > 0) */

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	mainloop = g_main_loop_new(NULL, FALSE);

	io = g_io_channel_unix_new(fd);
	g_io_add_watch_full(io, G_PRIORITY_HIGH, cond, gpio_watch, &fd,
							gpio_watch_destroy);
	g_io_channel_set_close_on_unref(io, FALSE);
	/*
	 * Adds a timer to the main loop for tests purposes.
	 * You may remove this line and also the function tick_tock()
	 * later, if you want to use this code for any application.
	 */
	g_timeout_add_seconds(1, tick_tock, NULL);

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);

	return 0;
}

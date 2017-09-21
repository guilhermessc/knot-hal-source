/*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include "../hal/gpio_sysfs.h"

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

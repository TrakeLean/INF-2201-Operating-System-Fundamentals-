#include "scheduler.h"
#include "screen.h"
#include "thread.h"
#include "util.h"

/* Dining philosphers threads. */

enum
{
	THINK_TIME = 9999,
	EAT_TIME = THINK_TIME,
};
volatile int conds_initialized = 0;
condition_t cond[2];
lock_t lock;
int num_eating = 0;
int scroll_eating = 0;
int caps_eating = 0;
char *times_used[3];
/* Set to true if status should be printed to screen */
int print_to_screen;

enum
{
	LED_NONE = 0x00,
	LED_SCROLL = 0x01,
	LED_NUM = 0x02,
	LED_CAPS = 0x04,
	LED_ALL = 0x07
};

/* Turns keyboard LEDs on or off according to bitmask.
 *
 * Bitmask is composed of the following three flags:
 * 0x01 -- SCROLL LOCK LED enable flag
 * 0x02 -- NUM LOCK LED enable flag
 * 0x04 -- CAPS LOCK LED enable flag
 *
 * Bitmask = 0x00 thus disables all LEDS, while 0x07
 * enables all LEDS.
 *
 * See http://www.computer-engineering.org/ps2keyboard/
 * and http://forum.osdev.org/viewtopic.php?t=10053
 */
static void update_keyboard_LED(unsigned char bitmask)
{
	/* Make sure that bitmask only contains bits for status LEDs  */
	bitmask &= 0x07;

	/* Wait for keyboard buffer to be empty */
	while (inb(0x64) & 0x02)
		;
	/* Tells the keyboard to update LEDs */
	outb(0x60, 0xed);
	/* Wait for the keyboard to acknowledge LED change message */
	while (inb(0x60) != 0xfa)
		;
	/* Write bitmask to keyboard */
	outb(0x60, bitmask);

	ms_delay(100);
}

static void think_for_a_random_time(void)
{
	volatile int foo;
	int i, n;

	n = rand() % THINK_TIME;
	for (i = 0; i < n; i++)
		if (foo % 2 == 0)
			foo++;
}

static void eat_for_a_random_time(void)
{
	volatile int foo;
	int i, n;

	n = rand() % EAT_TIME;
	for (i = 0; i < n; i++)
		if (foo % 2 == 0)
			foo++;
}

/* Odd philosopher */
void num(void)
{
	print_to_screen = 1;

	/* Initialize conditions */
	condition_init(&cond[0]);
	condition_init(&cond[1]);
	condition_init(&cond[2]);
	lock_init(&lock);
	conds_initialized = 1;

	if (print_to_screen)
	{
		print_str(PHIL_LINE, PHIL_COL, "Phil.");
		print_str(PHIL_LINE + 1, PHIL_COL, "Running");
	}

	while (1)
	{
		think_for_a_random_time();

		while (scroll_eating + caps_eating > 0)
		{
			condition_wait(&cond[0], &lock);
		}

		/* Enable NUM-LOCK LED and disable the others */
		update_keyboard_LED(LED_NUM);

		num_eating = 1;
		/* With three forks only one philosopher at a time can eat */
		ASSERT(scroll_eating + caps_eating == 0);

		times_used[0] += 1;
		if (print_to_screen)
		{
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Num    ");
			print_int(PHIL_LINE + 1, PHIL_COL + 7, times_used[0]);
		}

		eat_for_a_random_time();

		num_eating = 0;
		condition_signal(&cond[0]);
	}
}

void caps(void)
{
	while (conds_initialized == 0)
	{
		yield();
	}
	while (1)
	{
		think_for_a_random_time();

		while (scroll_eating + num_eating > 0)
		{
			condition_wait(&cond[1], &lock);
		}

		/* Enable CAPS-LOCK LED and disable the others */
		update_keyboard_LED(LED_CAPS);

		caps_eating = 1;
		/* With three forks only one philosopher at a time can eat */
		ASSERT(scroll_eating + num_eating == 0);

		times_used[1] += 1;
		if (print_to_screen)
		{
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 2, PHIL_COL, "Caps   ");
			print_int(PHIL_LINE + 2, PHIL_COL + 7, times_used[1]);
		}

		eat_for_a_random_time();

		caps_eating = 0;

		condition_signal(&cond[1]);
	}
}

void scroll_th(void)
{
	while (conds_initialized == 0)
	{
		yield();
	}
	while (1)
	{
		think_for_a_random_time();

		while (num_eating + caps_eating > 0)
		{
			condition_wait(&cond[2], &lock);
		}

		/* Enable SCROLL-LOCK LED and disable the others */
		update_keyboard_LED(LED_SCROLL);

		scroll_eating = 1;
		/* With three forks only one philosopher at a time can eat */
		ASSERT(caps_eating + num_eating == 0);

		times_used[2] += 1;
		if (print_to_screen)
		{
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 3, PHIL_COL, "Scroll ");
			print_int(PHIL_LINE + 3, PHIL_COL + 7, times_used[2]);
		}

		eat_for_a_random_time();

		scroll_eating = 0;

		condition_signal(&cond[2]);
	}
}

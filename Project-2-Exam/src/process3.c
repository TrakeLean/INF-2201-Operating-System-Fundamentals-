/*
 * Simple counter program, to check the functionality of yield().
 * Each process prints their invocation counter on a separate line
 */

#include "common.h"
#include "syslib.h"
#include "util.h"

#define ROWS 9
#define COLUMNS 20

// https://www.asciiart.eu/space/stars
static char picture[ROWS][COLUMNS + 1] = {
"        .       ",
"       ,O,      ",
"      ,OOO,     ",
" oooooOOOOOooooo",
"  `OOOOOOOOOOO` ",
"    `OOOOOOO`   ",
"    OOOO OOOO   ",
"   OOO     OOO  ",
"  O           O ",
};

static void draw(int locx, int locy, int plane);
static void print_counter(void);

void _start(void) {
	int locx = 0, locy = 1;
    int way = 1;
	while (1) {
		print_counter();
		/* erase star */
		draw(locx, locy, FALSE);
        locx += way;
		if (locx < 0) {
		    way = 1;
		}
        else if (locx > 20){
            way = -1;
        }
		/* draw star */
		draw(locx, locy, TRUE);
		print_counter();
		delay(DELAY_VAL);
		yield();
	}
}

/* print counter */
static void print_counter(void) {
	static int counter = 0;

	print_str(24, 0, "Process 3 (star)       : ");
	print_int(24, 25, counter++);
	if (counter > 50000)
	{
		exit();
	}
}

/* draw star */
static void draw(int locx, int locy, int plane) {
	int i, j;

	for (i = 0; i < COLUMNS; i++) {
		for (j = 0; j < ROWS; j++) {
			/* draw star */
			if (plane == TRUE) {
				print_char(locy + j, locx + i, picture[j][i]);
			}
			/* erase star */
			else {
				print_char(locy + j, locx + i, ' ');
			}
		}
	}
}

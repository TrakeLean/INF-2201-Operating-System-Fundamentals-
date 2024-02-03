#include "common.h"
#include "screen.h"
#include "syslib.h"
#include "util.h"

#define COMMAND_MBOX 5

static void create_board(int *board_size, char **board);
static void draw_board(int *board_size, char **board);

int main(void)
{
    // Vertical, Horizontal
	int board_size[] = {8, 12};
	char board[board_size[0]][board_size[1]];
    int *board_ptr = board;
	int board_init = FALSE;
	msg_t m;	       /* "fire bullet" message */
	uint32_t count, space; /* number of messages, free space in buffer */
	int c, q;

	/* open command mailbox, to communicate with the shell */
	q = mbox_open(COMMAND_MBOX);
	while (1) {
		/* create board */
		if (board == FALSE) {
			// create_board(board_size, board);
			board_init = TRUE;
		}

		/* draw board */
		// draw_board(board_size, board);

		ms_delay(500);
	} /* end forever loop */

	if (q >= 0) { /* should not be reached */
		mbox_close(q);
	}

	return -1;
}

// static void draw_board(int *board_size, char **board)
// {
// 	for (int i = 0; i <= board_size[0]; i++) {
// 		for (int j = 0; j <= board_size[1]; j++) {
// 			// if (board[i][j] == "*")
// 			// {
// 			scrprintf(i, j, "*");
// 			// }
// 		}
// 	}
// }
// static void create_board(int *board_size, char **board)
// {
// 	for (int i = 0; i <= board_size[0]; i++) {
// 		for (int j = 0; j <= board_size[1]; j++) {
// 			if (board_size[0] % i == 0) {
// 				board[i][j] = "*";
// 			} else {
// 				board[i][j] = ".";
// 			}
// 		}
// 	}
// }

// static void pieces(int loc_x, int loc_y)
// {
// }

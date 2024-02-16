#include <stdio.h>
#include <math.h>
#include "GLCD/GLCD.h" 
#include "TouchPanel/TouchPanel.h"

#define BOARD_DIMENSION 7
#define BOARD_DIM 13

int board[BOARD_DIM][BOARD_DIM];
int start_match, start_turn1, start_turn2, end_turn1, end_turn2;
int row_player1, row_player2, col_player1, col_player2;		/* RATHER THAN A STRUCT */
int f2f_down, f2f_left, f2f_right, f2f_up;	/* TOKENS FACE TO FACE */
int posx_wall, posy_wall, horizontal, vertical, is_overlapped, is_previous_overlapped, is_out, trap1, trap2;
volatile int possible_down = 1, possible_left = 1, possible_right = 1, possible_up = 1;
extern int wall_mode;
extern uint32_t mossa;

/* ***************   DRAWING FUNCTIONS   *************** */
/* *****   JUMP TO LINE 240 FOR GAMING FUNCTIONS   ***** */
/**
 * @brief Clean (color black) a specific dispaly area.
 *	
 * @param	start_x  The abscissa of the starting point area to be cleaned.
 * @param start_y  The ordinate of the starting point area to be cleaned.	
 * @param	length	 The length, in pixels, of the area to be cleaned.
 *
 * @return Nothing
 */
void clean_zone(int start_x, int start_y, int length) {
	int i;
	for(i = 0; i < 30; i++) 	/* area 30 pixels high */
		LCD_DrawLine(start_x, start_y + i, start_x + length, start_y + i, Black);
}

/**
 * @brief Initialization of the Quoridor board.
 *
 * @details The board is modeled as a matrix 13x13 (on one row, 7 cells for
 * tokens, 6 cells for spaces). Tokens can occupy only cells with even row
 * and even column indexes. The remaining cells are occupied by walls. At
 * the beginning of the game, the entire board is initialized to 0. Subse-
 * quently the cells can take one of the following values (0 if free):
 * 		1 	if player1 is present
 *		2 	if player2 is present
 * 		3 	if a wall of player1 is present
 * 		4 	if a wall of player2 is present
 * The distinction between walls does not appear necessary immediately, but
 * can be useful in future. It gives a precise representation of the situation.
 *
 * @param	No params
 *
 * @return Nothing
 */
void initialize_board(void) {
	int i, j;
	for(i = 0; i < BOARD_DIM; i++)
		for(j = 0; j < BOARD_DIM; j++)
			board[i][j] = 0;
}

/**
 * @brief Draw white square contours, given the row and column indexes.
 *	
 * @details The squares are 28x28, divided by a space of 5 pixels. The
 * 240 - 7*28 - 6*5 pixels are used for the spaces at the outermost
 * edges of the board (7 pixels on the right, 7 pixels on the left).
 * The row and column indexes vary from 0 to the size of the real 
 * board (7) minus 1. The 13x13 matrix is used for implementations
 * reasons. For drawing, indexes from 0 to 7-1 are preferred. 
 *
 * @param	row  The row index in which to draw square countours.
 * @param col  The column index in which to draw square countours.
 *
 * @return Nothing
 */
void draw_square_edge(int row, int col) {
		LCD_DrawLine( 7+33*col,  7+33*row, 35+33*col,  7+33*row, White);
		LCD_DrawLine(35+33*col,  7+33*row, 35+33*col, 35+33*row, White);
		LCD_DrawLine(35+33*col, 35+33*row,  7+33*col, 35+33*row, White);
		LCD_DrawLine( 7+33*col, 35+33*row,  7+33*col,  7+33*row, White);
}

/**
 * @brief Draw Quoridor board 7x7. Square by square, row by row.
 *
 * @param	No params
 *
 * @return Nothing
 */
void draw_board(void) {
	int row, col;
	
	for(row = 0; row < BOARD_DIMENSION; row++) {
		for(col = 0; col < BOARD_DIMENSION; col++) {	
			draw_square_edge(row, col);
		}
	}	
}

/**
 * @brief Draw layout for the match (rectangles that will host
 * timers and number of wall for each player). 
 *
 * @param	No params
 *
 * @return Nothing
 */
void show_info_layout(void) {
	int col;
	
	/* DRAW 3 RECTANGLES (ONE NEXT TO THE OTHER) */
	for(col = 0; col < 3; col++) {
		LCD_DrawLine(10+75*col, 255, 80+75*col, 255, White);
		LCD_DrawLine(80+75*col, 255, 80+75*col, 300, White);
		LCD_DrawLine(80+75*col, 300, 10+75*col, 300, White);
		LCD_DrawLine(10+75*col, 300, 10+75*col, 255, White);
	}
	
	/* WRITE TITLES OF THE FIRST AND THIRD RECTANGLE */
	GUI_Text(20, 260, (uint8_t *) "P1 Wall", White, Black);
	GUI_Text(170, 260, (uint8_t *) "P2 Wall", Red, Black);
}

/**
 * @brief Write the number of walls still available for a player. 
 *
 * @param	id_player  The id of the player whose number of walls
 * is to be updated. In this phase, 1 and 2 for greater clarity.
 * @param walls  New number of walls of the player.
 *
 * @return Nothing
 */
void show_update_wall(int id_player, int walls) {
	char walls_str[2] = "";
	sprintf(walls_str, "%d", walls);
	if(id_player == 1)
		GUI_Text(45, 280, (uint8_t *) walls_str, White, Black);
	else
		GUI_Text(195, 280, (uint8_t *) walls_str, Red, Black);
}

/**
 * @brief Write seconds in the middle rectangle of the layout.
 *
 * @param	seconds  The number of seconds that have passed since
 * the start of the turn (from 1 to 20). Then turn of the opponent.
 *
 * @return Nothing
 */
void show_timer(int seconds) {
	char time_in_char[3] = "";
	sprintf(time_in_char, "%d s", seconds);
	if(seconds < 10)
		GUI_Text(110, 270, (uint8_t *) time_in_char, White, Black);
	else
		GUI_Text(105, 270, (uint8_t *) time_in_char, White, Black);
}

/**
 * @brief Draw token of a precise color in a precise position.
 *
 * @details The row and column indexes vary from 0 to the size 
 * of the real board (7) minus 1. The 13x13 matrix is used for 
 * implementations reasons. For drawing tokens, indexes from 0 
 * to 7-1 are preferred. Token like a circle plus a triangle.
 * 
 * @param	row  The row index in which to draw the token (player).
 * @param col  The column index in which to draw the token (player).
 * @param color  The color with which to draw the token (player).
 *
 * @return Nothing
 */
void draw_player(int row, int col, int color) {
	int i;				/* VARIABLE FOR LOOP */
	int x1, y1;		/* GENERIC COORDINATES OF CIRCLE */
	
	/* DRAW CIRCLE WITH HORIZONTAL LINES (RADIUS AND CENTER COORDINATES) */
	int radius = 5;
	double xc = 7 + 33*col + 14;
	double yc = 7 + 33*row + 9;
	
	for(i = 0; i < 2 * radius; i++) {
		y1 = yc - radius + i;
		x1 = sqrt(radius * radius - (y1 - yc) * (y1 - yc));	/* (y1 - yc) to center on y */ 
		LCD_DrawLine(xc - x1, y1, xc + x1, y1, color);
	}
	
	/* DRAW TRIANGLE WITH HORIZONTAL LINES */
	/* For aesthetic reasons, the tip of the triangle (token body) is hidden in the token
		 head (-3 in y coordinates of LCD_Drawline) */
	for(i = 0; i < 3 * radius; i++) {
		LCD_DrawLine(xc - i/2, yc + radius + i - 3, xc + i/2, yc + radius + i - 3, color);
	}
}

/**
 * @brief Draw wall of a precise color in a precise position.
 *
 * @details The posx and posy indexes vary from 0 to the size 
 * of the real board (7) minus 2. The 13x13 matrix is used for 
 * implementations reasons. For drawing walls, indexes from 0 
 * to 7-2 are preferred. If posx = 2 and posy = 2, the wall is
 * drawn under the row of index 2 and right to the column of
 * index 2. Different implementation for HORIZONTAL and VERTICAL
 * walls. Anyway, walls occupy two cells and one space (28*2 + 5).
 * 
 * @param	posx  The posx index in which to draw the wall.
 * @param posy  The posy index in which to draw the wall.
 * @param color  The color with which to draw the wall.
 *
 * @return Nothing
 */
void draw_wall(int posx, int posy, int color) {
	int line;
	if(horizontal) {
		for(line = 0; line <= 3; line++)	/* wall width */
			/* posx = 2 -> 3 squares + 3 spaces, posy = 3 -> 4 squares, 3 spaces */ 
			LCD_DrawLine(7 + (posx+1)*33, 8 + (posy+1)*28 + posy*5 + line, 7 + (posx+1)*33 + 61, 8 + (posy+1)*28 + posy*5 + line, color);
	} else {
		for(line = 0; line <= 28*2 + 5; line++)
			LCD_DrawLine(8 + (posx+1)*28 + posx*5, 7 + (posy+1)*33 + line, 8 + (posx+1)*28 + posx*5 + 3, 7 + (posy+1)*33 + line, color);
	}
} 

/**
 * @brief Draw full square, given the row and column indexes. 
 *
 * @param	row  The row index in which to draw the full square.
 * @param col  The column index in which to draw the full square.
 * @param color  The color with which to draw the full square.
 *
 * @return Nothing
 */
void draw_square(int row, int col, int color) {
	int line;

	for(line = 0; line < 29; line++)
		LCD_DrawLine(7 + 33*col, 7 + 33*row + line, 7 + 33*col + 28, 7 + 33*row + line, color);
}

/* ***************   GAMING FUNCTIONS   *************** */
/* *******   FOR TOKENS. JUMP TO 579 FOR WALLS   ****** */
/**
 * @brief Color possible moves of the player, given its position in terms of row and column.
 *
 * @param curr_row  The current row (0 to 6) where the player is.
 * @param curr_col  The current column (0 to 6) where the player is.
 * @param color			The color with which to highlight the possible moves.
 *
 * @return Nothing
 */
void possible_moves(int curr_row, int curr_col, int color) {
	/* CHECK FOR OUT OF BOARD, WALLS AND POSITION FREE (NO TOKEN) */
	/* --- DOWN --- */
	if (curr_row + 1 < BOARD_DIMENSION) {		/* if in board */
		if(board[(curr_row+1)*2][curr_col*2] == 0) {	/* if free (no opponent). Board 13x13 */
			if(board[(curr_row+1)*2-1][curr_col*2] == 0) {	/* and no wall */
				draw_square(curr_row + 1, curr_col, color);
				draw_square_edge(curr_row + 1, curr_col);
			} else	/* no opponent, but wall */
					possible_down = 0;
		} else {	/* there is the opponent */
			if (curr_row + 2 < BOARD_DIMENSION && 	/* IF NO OUT THE BOARD */
						board[(curr_row+1)*2-1][curr_col*2] == 0 &&		/* NO WALL BETWEEN THE TWO PLAYER */
						board[(curr_row+1)*2+1][curr_col*2] == 0) {		/* NO WALL BEHIND THE OPPONENT */
							draw_square(curr_row + 2, curr_col, color);
							draw_square_edge(curr_row + 2, curr_col);
							f2f_down = 1;
			} else
					possible_down = 0;
		}
	} else	/* out of board */
			possible_down = 0;
	/* --- LEFT --- */
	if (curr_col - 1 >= 0) {
		if(board[curr_row*2][(curr_col-1)*2] == 0) {
			if(board[curr_row*2][(curr_col-1)*2+1] == 0) {	
				draw_square(curr_row, curr_col - 1, color);
				draw_square_edge(curr_row, curr_col - 1);
			} else
					possible_left = 0;
		} else {
			if (curr_col - 2 >= 0 && 
					board[curr_row*2][(curr_col-1)*2+1] == 0 &&
					board[curr_row*2][(curr_col-1)*2-1] == 0) {
						draw_square(curr_row, curr_col - 2, color);
						draw_square_edge(curr_row, curr_col - 2);
						f2f_left = 1;
			} else
					possible_left = 0;
		}
	} else
			possible_left = 0;
	/* --- RIGHT --- */
	if (curr_col + 1 < BOARD_DIMENSION) {
		if(board[curr_row*2][(curr_col+1)*2] == 0) {
			if(board[curr_row*2][(curr_col+1)*2-1] == 0) {
				draw_square(curr_row, curr_col + 1, color);
				draw_square_edge(curr_row, curr_col + 1);
			} else
					possible_right = 0;
		} else {
			if (curr_col + 2 < BOARD_DIMENSION && 
					board[curr_row*2][(curr_col+1)*2-1] == 0 &&
					board[curr_row*2][(curr_col+1)*2+1] == 0) {
						draw_square(curr_row, curr_col + 2, color);
						draw_square_edge(curr_row, curr_col + 2);
						f2f_right = 1;
			} else
					possible_right = 0;
		}
	} else
			possible_right = 0;
	/* UP */
	if (curr_row - 1 >= 0) {
		if(board[(curr_row-1)*2][curr_col*2] == 0) {
			if(board[(curr_row-1)*2+1][curr_col*2] == 0) {
				draw_square(curr_row - 1, curr_col, color);
				draw_square_edge(curr_row - 1, curr_col);
			} else
					possible_up = 0;
		} else {
			if (curr_row - 2 >= 0 && 
					board[(curr_row-1)*2-1][curr_col*2] == 0 &&
					board[(curr_row-1)*2+1][curr_col*2] == 0) {
						draw_square(curr_row - 2, curr_col, color);
						draw_square_edge(curr_row - 2, curr_col);
						f2f_up = 1;
			} else
					possible_up = 0;
		}
	} else
			possible_up = 0;
	
}

/**
 * @brief Start a Quoridor match.
 *
 * @param No params
 *
 * @return Nothing
 */
void start_game(void) {
	/* #1 THE MATCH STARTS, IN GAME MODE */
	start_match = 1;
	wall_mode = 0;
	/* #2 PLAYER 1 IS THE FIRST (HIGHLIGHT ITS INFO RECTANGLE P1 Wall) */
	mossa = 0;
	start_turn1 = 1;
	LCD_DrawLine(10, 255, 80, 255, Lavanda);
	LCD_DrawLine(80, 255, 80, 300, Lavanda);
	LCD_DrawLine(80, 300, 10, 300, Lavanda);
	LCD_DrawLine(10, 300, 10, 255, Lavanda);
	/* #3 INITIALIZE POSITIONS OF PLAYERS */
	row_player1 = 0;
	row_player2 = 12;
	col_player1 = 6;
	col_player2 = 6;
	/* #4 INITIAL POSITION OF WALL (HORIZONTAL, IN THE CENTER OF THE BOARD) */
	posx_wall = 2;
	posy_wall = 2;
	horizontal = 1;
	vertical = 0;
	/* #5 COLOR POSSIBLE MOVES FOR PLAYER 1 */
	/* *** The curr_row and curr_col parameters of possible_moves take on values 
			between 0 and 6, while row_player and col_player take on values between 0 
			and 12. Ex. row_player2 equal to 12 corresponds to curr_row equal to 6 *** */
	possible_moves(row_player1/2, col_player1/2, Lavanda);
	/* #6 INITIAL POSITIONS OF PLAYERS IN MATRIX BOARD */
	board[row_player1][col_player1] = 1;	/* FIRST PLAYER */
	board[row_player2][col_player2] = 2;	/* SECOND PLAYER */
}

/**
 * @brief Move token down.
 *
 * @param id_player  The id of the player you intend to move down in the board.
 *
 * @return Nothing
 */
void move_down_token(int id_player) {
	/* PLAYER 1 (WHITE) */
	if (id_player == 1) {	
		/* #1 RECOLOR PLAYER'S POSSIBLE MOVES TO BLACK (BACKGROUND COLOR OF THE BOARD) */
		possible_moves(row_player1/2, col_player1/2, Black);
		
		/* #2 MANAGE THE MOVEMENT OF THE TOKEN */
		if(row_player1 < BOARD_DIM - 2) {
			/* #2.1 FREE THE CORRESPONDING POSITION IN BOARD 13X13 */
			board[row_player1][col_player1] = 0;	
			
			/* #2.2 CLEAN THE PREVIOUS POSITION OF THE TOKEN (BLACK SQUARE AND CONTOURS) */
			draw_square(row_player1/2, col_player1/2, Black);		
			draw_square_edge(row_player1/2, col_player1/2);	
			
			/* #2.3 CHECK IF THE TWO PLAYERS ARE FACE TO FACE */
			/* ALREADY CHECK THE POSSIBILITY OF GOING OUTSIDE THE BOARD IN possible_moves */
			if(!f2f_down)
				row_player1 = row_player1 + 2;	
			else
				row_player1 = row_player1 + 4;
			
			/* #2.4 NEW POSITION IN BOARD 13X13 */
			board[row_player1][col_player1] = 1;
			
			/* #2.5 DRAW PLAYER 1 IN THE NEW POSITION */
			draw_player(row_player1/2, col_player1/2, White);
			
			/* #2.6 END TURN 1. USEFUL FOR RESET THE TIMER */
			end_turn1 = 1;
			f2f_down = 0;
			}
	} else {	/* PLAYER 2 (RED) */
		possible_moves(row_player2/2, col_player2/2, Black);
		if(row_player2 < BOARD_DIM - 2) {
			board[row_player2][col_player2] = 0;	
			
			draw_square(row_player2/2, col_player2/2, Black);		
			draw_square_edge(row_player2/2, col_player2/2);			
			
			if(!f2f_down)
				row_player2 = row_player2 + 2;	
			else
				row_player2 = row_player2 + 4;
			
			board[row_player2][col_player2] = 2; 	/* POSITION PLAYER 2 IN BOARD */
			
			draw_player(row_player2/2, col_player2/2, Red);
			end_turn2 = 1;
		}
	}
}

/**
 * @brief Move token left.
 *
 * @param id_player  The id of the player you intend to move down in the board.
 *
 * @return Nothing
 */
void move_left_token(int id_player) {
	/* SIMILAR ACTIONS OF move_down_token. CHECK THIS FUNCTION FOR MORE EXPLANATIONS */
	if (id_player == 1) {
		possible_moves(row_player1/2, col_player1/2, Black);
		if(col_player1 >= 2) {
			board[row_player1][col_player1] = 0;	
			
			draw_square(row_player1/2, col_player1/2, Black);		
			draw_square_edge(row_player1/2, col_player1/2);			
			
			if(!f2f_left)
				col_player1 = col_player1 - 2;	
			else
				col_player1 = col_player1 - 4;
			
			board[row_player1][col_player1] = 1;
			
			draw_player(row_player1/2, col_player1/2, White);
			end_turn1 = 1;
			}
	} else {	
		possible_moves(row_player2/2, col_player2/2, Black);
		if(col_player2 >= 2) {
			board[row_player2][col_player2] = 0;	
			
			draw_square(row_player2/2, col_player2/2, Black);		
			draw_square_edge(row_player2/2, col_player2/2);			
			
			if(!f2f_left)
				col_player2 = col_player2 - 2;	
			else
				col_player2 = col_player2 - 4;
			
			board[row_player2][col_player2] = 2;
			
			draw_player(row_player2/2, col_player2/2, Red);
			end_turn2 = 1;
			}
	}
}

/**
 * @brief Move token right.
 *
 * @param id_player  The id of the player you intend to move down in the board.
 *
 * @return Nothing
 */
void move_right_token(int id_player) {
	/* SIMILAR ACTIONS OF move_down_token. CHECK THIS FUNCTION FOR MORE EXPLANATIONS */
	if (id_player == 1) {		
		possible_moves(row_player1/2, col_player1/2, Black);
		if(col_player1 < BOARD_DIM - 2) {
			board[row_player1][col_player1] = 0;	
			
			draw_square(row_player1/2, col_player1/2, Black);		
			draw_square_edge(row_player1/2, col_player1/2);			
			
			if(!f2f_right)
				col_player1 = col_player1 + 2;	
			else
				col_player1 = col_player1 + 4;
			
			board[row_player1][col_player1] = 1;
			
			draw_player(row_player1/2, col_player1/2, White);
			end_turn1 = 1;
			}
	} else {	
		possible_moves(row_player2/2, col_player2/2, Black);
		if(col_player2 < BOARD_DIM - 2) {
			board[row_player2][col_player2] = 0;	
			
			draw_square(row_player2/2, col_player2/2, Black);		
			draw_square_edge(row_player2/2, col_player2/2);			
			
			if(!f2f_right)
				col_player2 = col_player2 + 2;	
			else
				col_player2 = col_player2 + 4;
			
			board[row_player2][col_player2] = 2; 
			
			draw_player(row_player2/2, col_player2/2, Red);
			end_turn2 = 1;
			}
	}
}

/**
 * @brief Move token up.
 *
 * @param id_player  The id of the player you intend to move down in the board.
 *
 * @return Nothing
 */
void move_up_token(int id_player) {
	/* SIMILAR ACTIONS OF move_down_token. CHECK THIS FUNCTION FOR MORE EXPLANATIONS */
	if (id_player == 1) {		
		possible_moves(row_player1/2, col_player1/2, Black);
		if(row_player1 >= 2) {
			board[row_player1][col_player1] = 0;	
			
			draw_square(row_player1/2, col_player1/2, Black);		
			draw_square_edge(row_player1/2, col_player1/2);			
			
			if(!f2f_up) 
				row_player1 = row_player1 - 2;	
			else
				row_player1 = row_player1 - 4;
			
			board[row_player1][col_player1] = 1;	
			
			draw_player(row_player1/2, col_player1/2, White);
			end_turn1 = 1;
			}
	} else {	
		possible_moves(row_player2/2, col_player2/2, Black);
		if(row_player2 >= 2) {
			board[row_player2][col_player2] = 0;	
			
			draw_square(row_player2/2, col_player2/2, Black);		
			draw_square_edge(row_player2/2, col_player2/2);			
			
			if(!f2f_up)
				row_player2 = row_player2 - 2;	
			else
				row_player2 = row_player2 - 4;
			
			board[row_player2][col_player2] = 2; 
			
			draw_player(row_player2/2, col_player2/2, Red);
			end_turn2 = 1;
			}
	}
}

int overlap_pos[3][2];	/* MATRIX FOR COORDINATES OVERLAPPED */
int opponent_wall[3];		/* ARRAY FOR DISTINGUISHING PLAYER 1 OR PLAYER 2 WALLS */
/* *******   FOR WALLS. JUMP TO 241 FOR TOKENS   ****** */
/**
 * @brief Check if a wall is overlapped with other walls.
 *
 * @details The function returns 1 or 0 depending on whether or not the wall overlaps 
 * with others walls. It also stores within a 3x2 matrix 'overlap_pos' the values of 
 * the coordinates where the two walls are overlapped and in a vector of dimension 3 
 * 'opponent_wall' a value that allows distinguishing whether the wall already positioned 
 * and which is now overlapped belongs to player 1 or player 2. This it is useful for 
 * rebuilding walls (function 'color_spaces13x13').
 *
 * @param desidered_posx  The position in x (col) where you desire to place the wall.
 * @param desidered_posy  The position in y (row) where you desire to place the wall.
 *
 * @return 1 if it is overlapped. 0 otherwise
 */
int is_overlapped_wall(int desidered_posx, int desidered_posy) {
	int i, j, pos = 0, overlap = 0;
	/* #1 RESET ARRAY 'opponent_wall' AND MATRIX 'overlap_pos' */
	for(i = 0; i < 3; i++)
		opponent_wall[i] = 0;
	for(i = 0; i < 3; i++)
		for(j = 0; j < 2; j++)
			overlap_pos[i][j] = 0;
	/* #2 CHECK OVERLAP FOR HORIZONTAL WALLS */
	if(horizontal) {
		/* EACH WALL OCCUPIES 3 CELLS IN MATRIX BOARD - 2 SQUARES AND 1 SPACE */
		for(i = 0; i < 3; i++)
			/* #2.1 IF SOME THIS 3 CELLS ARE NOT FREE */
			if(board[desidered_posy*2+1][desidered_posx*2+2+i] != 0) {
				/* #2.2 STORE IN THE ARRAY THE TYPE OF WALL (3 FOR PLAYER1, 4 FOR PLAYER2) */
				if(board[desidered_posy*2+1][desidered_posx*2+2+i] == 3) 
					opponent_wall[pos] = 3;
				else
					opponent_wall[pos] = 4;
				/* #2.3 STORE IN THE MATRIX THE COORDINATES OF CELLS OVERLAPPED */
				overlap_pos[pos][0] = desidered_posy*2+1;
				overlap_pos[pos][1] = desidered_posx*2+2+i;
				/* #2.4 UPDATE INDEX pos AND SET THE VARIABILE overlap TO 1 */
				pos++;
				overlap = 1;
			}
	} else {	/* VERTICAL WALLS */
		for(i = 0; i < 3; i++)
			if(board[desidered_posy*2+2+i][desidered_posx*2+1] != 0) {
				if(board[desidered_posy*2+2+i][desidered_posx*2+1] == 3) 
					opponent_wall[pos] = 3;
				else
					opponent_wall[pos] = 4;
				overlap_pos[pos][0] = desidered_posy*2+2+i;
				overlap_pos[pos][1] = desidered_posx*2+1;
				pos++;
				overlap = 1;
			}
	}
	return overlap;
}

/**
 * @brief Color a specific space (between squares) on the board with a specific color.
 * 
 * @param i  The position in x of the space that you desire to color.
 * @param j  The position in y of the space that you desire to color.
 * @param color  The color to used.
 *
 * @return Nothing
 */
void color_spaces13x13(int i, int j, int color) {
	int line;
	/* EVEN COLUMN */
	if(j % 2 == 0) {
		for(line = 0; line <= 3; line++)
			LCD_DrawLine(7 + 33*j/2, 6 + 28*(i/2+1) + 5*i/2 + line, 7 + 33*j/2 + 28, 6 + 28*(i/2+1) + 5*i/2 + line, color);
	} else {	/* ODD COLUMN */
		if(i % 2 == 0)	/* EVEN ROW */
			for(line = 0; line <= 28; line++)
				LCD_DrawLine(6 + 28*(j/2+1) + 5*j/2, 7 + 33*i/2 + line, 6 + 28*(j/2+1)+ 5*j/2 + 3, 7 + 33*i/2 + line, color); 
		else	/* ODD ROW */
			for(line = 0; line <= 3; line++)
				LCD_DrawLine(6 + 28*(j/2+1) + 5*j/2, 6 + 28*(i/2+1) + 5*i/2 + line, 6 + 28*(j/2+1) + 5*j/2 + 3, 6 + 28*(i/2+1) + 5*i/2 + line, color);
	}
}

/* *******   BFS ALGORITHM FOR NOT TRAPPING TOKENS   ****** */

int neighbors[4][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};		/* MATRIX FOR IDENTIFING NEIGHBORS */
int visited[BOARD_DIMENSION][BOARD_DIMENSION];
int queue[BOARD_DIMENSION * BOARD_DIMENSION * 4][2];
/**
 * @brief Reset the two matrix useful for BFS (Breadth First Search) algorithm.
 * 
 * @param No params
 *
 * @return Nothing
 */
void reset_matrix(void) {
	int i, j;
	/* #1 RESET MATRIX OF VISITED NODES (7x7) */
	for(i = 0; i < BOARD_DIMENSION; i++)
		for(j = 0; j < BOARD_DIMENSION; j++)
			visited[i][j] = 0;
	/* #2 RESET QUEUE (7x7x4). IN GENERAL 4 NEIGHBORS */
	for(i = 0; i < BOARD_DIMENSION*BOARD_DIMENSION*4; i++) {
			queue[i][0] = 0;
			queue[i][1] = 0;
	}
}

/** 
 * @brief Tests if there are walls beetween two cells of the board.
 *
 * @param current_row  The current row of the token (from 0 to 6).
 * @param current_col  The current col of the token (from 0 to 6).
 * @param new_row  The new row of the token that moves vertically.
 * @param new_col  The new col of the token that moves horizontally.
 *
 * @return  1 if no wall between, so the road is free, 0 otherwise.
 */
int no_wall_between(int current_row, int current_col, int new_row, int new_col) {
	/* #1 IF THE TOKEN MOVES HORIZONTALLY (SAME ROW - CHANGE COLUMN) */
	if(current_row == new_row) {
		/* #1.1 MOVE RIGHT */
		if(current_col < new_col) {
			/* #1.2 IF THERE IS A WALL (VERTICAL WALL) BETWEEN */
			if(board[current_row*2][current_col*2+1] != 0)
				/* #1.3 RETURN */
				return 0;
		} else {	/* MOVE LEFT */
			if(board[current_row*2][current_col*2-1] != 0)
				return 0;
		}		
	}
	/* #2 IF THE TOKEN MOVES VERTICALLY (SAME COLUMN - CHANGE ROW) */
	if(current_col == new_col) {
		/* #2.1 MOVE DOWN */
		if(current_row < new_row) {
			/* #2.2 IF THERE IS A WALL (HORIZONTAL WALL) BETWEEN */
			if(board[current_row*2+1][current_col*2] != 0)
				/* #2.3 RETURN */
				return 0;
		} else {	/* MOVE UP */
			if(board[current_row*2-1][current_col*2] != 0)
				return 0;
		}		
	}
	/* #3 NO WALL BETWEEN */
	return 1;
}

/** 
 * @brief Tests whether adding a wall at a given position traps the player.
 *
 * @param id_player  The id of the player to be controlled is not trapped.
 * @param posx  The posx index in which to try to place the new wall. 
 * @param posy  The posy index in which to try to place the new wall.
 *
 * @return  1 if the player is trapped, the wall cannot be inserted, 0 otherwise.
 */
int is_trappola(int id_player, int posx, int posy) {
	int i, new_row, new_col, end, front = 0, rear = 0;
	reset_matrix();		/* RESET MATRIX OF VISITED NODES AND MATRIX FOR QUEUE */
	
	/* #1 INIZIALITATION FOR CHECKING TRAP OF PLAYER1 */
	if(id_player == 1) {
		/* #1.1 STORE IN QUEUE THE INITIAL POSITION OF PLAYER1 */
		queue[rear][0] = row_player1/2;
		queue[rear][1] = col_player1/2;
		/* #1.2 SET THE INITIAL POSITION OF PLAYER1 AS A VISITED NODE */
		visited[row_player1/2][col_player1/2] = 1;
		/* #1.3 INSERT WALL IN THE SPECIFIED POSITION */
		/* If, at the end, there is not a valid path for reaching the opposite part, 
			 TRAP. Delete the wall you just inserted from the matrix. */
		for(i = 0; i < 3; i++) 
			if(horizontal)
				board[posy*2+1][posx*2+2+i] = 3;
			else
				board[posy*2+2+i][posx*2+1] = 3;
		/* #1.4 OPPOSITE PART OF THE BOARD FRO PLAYER1 */
		end = 6;
	} else {	/* INIZIALITATION FOR CHECKING TRAP OF PLAYER2 */
		queue[rear][0] = row_player2/2;
		queue[rear][1] = col_player2/2;
		visited[row_player2/2][col_player2/2] = 1;
		for(i = 0; i < 3; i++) 
			if(horizontal)
				board[posy*2+1][posx*2+2+i] = 4;
			else
				board[posy*2+2+i][posx*2+1] = 4;
		end = 0;
	}
	
	/* #2 AS LONG AS THERE ARE NEW ELEMENTS IN THE QUEUE STILL TO EXPLORE */
	while(front <= rear) {
		/* #2.1 POP THE FIRST ELEMENT IN THE QUEUE NOT YET EXPLORED. THIS IS THE CURRENT POSITION OF THE PLAYER */
		int current_row = queue[front][0];
		int current_col = queue[front][1];
		front++;
		
		/* #2.2 IF CURRENT ROW IS EQUAL TO 6 (FOR PLAYER1), THERE IS A VALID PATH */
		if(current_row == end)
			return 0;
		
		/* #2.3 ANALYZE NEIGHBOR OF THE CURRENT POSITION */
		for(i = 0; i < 4; i++) {
			/* i = 0 -> UP NEIGHBOR, i = 1 -> DOWN NEIGHBOR, i = 2 -> LEFT NEIGHBOR, i = 3 -> RIGHT NEIGHBOR */
			new_row = current_row + neighbors[i][0];
			new_col = current_col + neighbors[i][1];
			
			/* #2.4 IF THE NEW POSITION IS VALID (NO OUT OF THE BOARD), IT IS NOT YET EXPLORED AND NO WALL BETWEEN THAT AVOID MOVE... */
			if(new_row >= 0 && new_row < BOARD_DIMENSION && new_col >= 0 && new_col < BOARD_DIMENSION &&
					!visited[new_row][new_col] && no_wall_between(current_row, current_col, new_row, new_col)) {
					/* #2.5 PUSH IN THE QUEUE THE NEW POSITION */
					queue[++rear][0] = new_row;
					queue[rear][1] = new_col;
					/* #2.6 SET AS VISITED */
					visited[new_row][new_col] = 1;
			}
		}
	} 
	
	/* #3. IF WE HAVE ANALYZED THE ENTIRE QUEUE (NO NEW LOCATIONS TO VISIT) AND HAVE NOT EXIT (no return 0), THE PLAYER IS TRAPPED */
	/* #3.1 DELETE WALL INSERTED IN 1.3 */
	for(i = 0; i < 3; i++) 
		if(horizontal)
			board[posy*2+1][posx*2+2+i] = 0;
		else
			board[posy*2+2+i][posx*2+1] = 0;
		/* #3.2 PLAYER TRAPPED :( */
	return 1;
}

/**
 * @brief Re-draw walls (a wall overlapped them and they need to be rebuilt).
 *
 * @param overlap_pos[][]  Matrix 3x2 that contains coordinates of cells overlapped.
 * 
 * @return Nothing
 */
void redraw_walls(int overlap_pos[][2]) {
	int i;
	for(i = 0; i < 3; i++)
		if(overlap_pos[i][0] != 0 && overlap_pos[i][1] != 0) {
			if(opponent_wall[i] == 3)
				color_spaces13x13(overlap_pos[i][0], overlap_pos[i][1], Beige);
			else
				color_spaces13x13(overlap_pos[i][0], overlap_pos[i][1], Red);
	}
}

/**
 * @brief Move down wall if it is possible (no out the board).
 *
 * @param id_player  The id of the player that positions the wall. Useful in future?
 * 
 * @return Nothing
 */
void move_down_wall(int id_player) {
	int overlap = 1;
	/* #1 CHECK IF THE WALL IN THE NEW DESIDERED POSITION WOULD BE OVERLAPPED */
	/* CHECK IT IF THE NEW POSITION IS CONTAINED IN THE BOARD. IF IT GOES OUT... */
	/* THE WALL IS HORIZONTAL OR VERTICAL (NEVER BOTH AT THE SAME TIME) */
	if((horizontal && posy_wall+1 < BOARD_DIMENSION-1) || (vertical && posy_wall+1 < BOARD_DIMENSION-2)) {
		/* #2 CHECK IF THE WALL IS OVERLAPPED IN THE PREVIOUS MOVEMENT */
		if(is_previous_overlapped) {
			draw_wall(posx_wall, posy_wall, Black);
			/* #2.1 RE-DRAW ALREADY POSITION WALLS */
			redraw_walls(overlap_pos);
		}
		overlap = is_overlapped_wall(posx_wall, posy_wall+1);
		if(!is_previous_overlapped)
			draw_wall(posx_wall, posy_wall, Black);
		posy_wall = posy_wall + 1;
	
		if(start_turn1)
			draw_wall(posx_wall, posy_wall, Hazelnut);
		else
			draw_wall(posx_wall, posy_wall, Violet);
			
		if(overlap) {
			is_overlapped = 1;
			is_previous_overlapped = 1;
		} else {
			is_overlapped = 0;
			is_previous_overlapped = 0;
		}
	} else
			is_out = 1;
}

/**
 * @brief Move left wall if it is possible (no out the board, no overlap)
 *
 * @param id_player  The id of the player that positions the wall. Useful in future?
 * 
 * @return Nothing
 */
void move_left_wall(int id_player) {
	int overlap = 1;
	/* SIMILAR ACTIONS OF move_down_wall. CHECK THIS FUNCTION FOR MORE EXPLANATIONS */
	if((horizontal && posx_wall-1 >= -1) || (vertical && posx_wall-1 >= 0)) {
		if(is_previous_overlapped) {
			draw_wall(posx_wall, posy_wall, Black);
			redraw_walls(overlap_pos);
		}
		overlap = is_overlapped_wall(posx_wall-1, posy_wall);
		if(!is_previous_overlapped)
			draw_wall(posx_wall, posy_wall, Black);
		posx_wall = posx_wall - 1;
		if(start_turn1)
			draw_wall(posx_wall, posy_wall, Hazelnut);
		else
			draw_wall(posx_wall, posy_wall, Violet);
		
		if(overlap) {
				is_overlapped = 1;
				is_previous_overlapped = 1;
		} else {
			is_overlapped = 0;
			is_previous_overlapped = 0;
		}
	} else
			is_out = 1;
}

/**
 * @brief Move right wall if it is possible (no out the board, no overlap)
 *
 * @param id_player  The id of the player that positions the wall. Useful in future?
 * 
 * @return Nothing
 */
void move_right_wall(int id_player) {
	int overlap = 1;
	/* SIMILAR ACTIONS OF move_down_wall. CHECK THIS FUNCTION FOR MORE EXPLANATIONS */
	if((horizontal && posx_wall+1 < BOARD_DIMENSION-2) || (vertical && posx_wall+1 < BOARD_DIMENSION-1)) {
		if(is_previous_overlapped) {
			draw_wall(posx_wall, posy_wall, Black);
			redraw_walls(overlap_pos);
		}
		overlap = is_overlapped_wall(posx_wall+1, posy_wall);
		if(!is_previous_overlapped)
			draw_wall(posx_wall, posy_wall, Black);
		posx_wall = posx_wall + 1;
		if(start_turn1)
			draw_wall(posx_wall, posy_wall, Hazelnut);
		else
			draw_wall(posx_wall, posy_wall, Violet);
		
		if(overlap) {
				is_overlapped = 1;
				is_previous_overlapped = 1;
		} else {
			is_overlapped = 0;
			is_previous_overlapped = 0;
		}
	} else
			is_out = 1;
}

/**
 * @brief Move up wall if it is possible (no out the board, no overlap)
 *
 * @param id_player  The id of the player that positions the wall. Useful in future?
 * 
 * @return Nothing
 */
void move_up_wall(int id_player) {
	int overlap = 1;
	/* SIMILAR ACTIONS OF move_down_wall. CHECK THIS FUNCTION FOR MORE EXPLANATIONS */
	if((horizontal && posy_wall-1 >= 0) || (vertical && posy_wall-1 >= -1)) {
		if(is_previous_overlapped) {
			draw_wall(posx_wall, posy_wall, Black);
			redraw_walls(overlap_pos);
		}
		overlap = is_overlapped_wall(posx_wall, posy_wall-1);
		if(!is_previous_overlapped)
			draw_wall(posx_wall, posy_wall, Black);
		posy_wall = posy_wall - 1;
		if(start_turn1)
			draw_wall(posx_wall, posy_wall, Hazelnut);
		else
			draw_wall(posx_wall, posy_wall, Violet);
		
		if(overlap) {
			is_overlapped = 1;
			is_previous_overlapped = 1;
		} else {
			is_overlapped = 0;
			is_previous_overlapped = 0;
		}
	} else
			is_out = 1;
}

/**
 * @brief Rotate if it is possible (no out the board, no overlap)
 *
 * @return Nothing
 */
void rotate_wall(void) {
	int overlap;
	/* DELETE PREVIOUS WALL (BEFORE CHANGE FROM HORIZONTAL TO VERTICAL OR VICEVERSA */
	draw_wall(posx_wall, posy_wall, Black);
	/* IF WALLS ARE PREVOIUS OVERLAPPED, REDRAW THEM */
	if(is_previous_overlapped)
		redraw_walls(overlap_pos);
	/* CHANGE WALL ORIENTATION AND NEW COORDINATES */
	if(horizontal) {
			horizontal = 0;
			vertical = 1;
			posx_wall = posx_wall + 1;
			posy_wall = posy_wall - 1;
	} else {
			vertical = 0;
			horizontal = 1;
			posx_wall = posx_wall - 1;
			posy_wall = posy_wall + 1;
	}
	/* IF WALL IN THE NEW ORIENTATION, OVERLAP OTHERS WALLS */
	overlap = is_overlapped_wall(posx_wall, posy_wall);
	if(overlap) {
			is_overlapped = 1;
			is_previous_overlapped = 1;
		} else {
			is_overlapped = 0;
			is_previous_overlapped = 0;
		}
	/* DRAW WALL IN THE NEW POSITION (ROTATE OF 90 DEGREES) */
	if(start_turn1)
		draw_wall(posx_wall, posy_wall, Hazelnut);
	else
		draw_wall(posx_wall, posy_wall, Violet);
}

void position_wall(int id_player) {
	int i;
	trap1 = is_trappola(1, posx_wall, posy_wall);
	trap2 = is_trappola(2, posx_wall, posy_wall);
	if(start_turn1 && !trap1 && !trap2) {
		draw_wall(posx_wall, posy_wall, Beige);
		/* WALL1 (3,2) -> board[2*2+1][3*2+2+i], il muro occupa 3 celle della matrice 13x13 */
		for(i = 0; i < 3; i++) 
			if(horizontal)
				board[posy_wall*2+1][posx_wall*2+2+i] = 3;
			else
				board[posy_wall*2+2+i][posx_wall*2+1] = 3;
		end_turn1 = 1;
	} else if(start_turn2 && !trap1 && !trap2){
		draw_wall(posx_wall, posy_wall, Red);
		for(i = 0; i < 3; i++)
			if(horizontal)
				board[posy_wall*2+1][posx_wall*2+2+i] = 4;
			else
				board[posy_wall*2+2+i][posx_wall*2+1] = 4;
		end_turn2 = 1;
	}
} 

#ifndef MAZE_H_INCLUDED
#define MAZE_H_INCLUDED

#define MAZE_W 20
#define MAZE_H 59

extern unsigned char maze_map[MAZE_H][MAZE_W];
extern unsigned char maze_row_lo[MAZE_H];
extern unsigned char maze_row_hi[MAZE_H];
extern unsigned char maze_exit_col;
extern unsigned char maze_exit_row;
extern unsigned char maze_exit_found;
extern unsigned char maze_exit_open;

void maze_load_level(unsigned char requested_level);
unsigned char maze_solid(unsigned char x, unsigned char y);
void maze_set_exit_open(unsigned char open);

#endif

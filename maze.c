#include <fcntl.h>
#include <unistd.h>
#include "maze.h"

#define LEVEL_BUFFER_SIZE 64

#pragma bss-name (push, "AUXBSS")
static unsigned char level_buffer[LEVEL_BUFFER_SIZE];
static int level_file;
static unsigned char level_buffer_pos;
static unsigned char level_buffer_len;
#pragma bss-name (pop)

#pragma bss-name (push, "HIGHBSS")
unsigned char maze_map[MAZE_H][MAZE_W];
unsigned char maze_row_lo[MAZE_H];
unsigned char maze_row_hi[MAZE_H];
#pragma bss-name (pop)

unsigned char maze_exit_col;
unsigned char maze_exit_row;
unsigned char maze_exit_found;
unsigned char maze_exit_open;

static const unsigned char level1_data[MAZE_H][MAZE_W] = {
    {1,1,1,1,1,1,1,1,5,5,5,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,4,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,1},
    {1,6,6,6,6,6,1,2,2,2,2,2,2,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,0,0,3,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,0,0,4,1},
    {1,0,0,0,0,0,1,2,2,1,1,1,1,1,0,0,0,0,0,1},
    {1,0,0,0,0,3,1,2,2,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,4,1,2,2,1,3,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,2,2,1,4,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,2,2,1,0,0,0,0,0,1,1,1,1,1},
    {1,0,0,0,0,0,1,2,2,1,0,0,0,0,0,1,2,2,2,1},
    {1,0,0,0,0,0,1,2,2,1,0,0,0,0,0,1,2,2,2,1},
    {1,0,0,0,0,0,1,2,2,1,6,6,6,6,6,1,2,2,2,1},
    {1,1,1,0,0,0,1,2,2,1,0,0,0,0,0,1,2,2,2,1},
    {1,2,1,0,0,0,0,1,2,1,0,0,0,0,0,1,2,2,2,1},
    {1,2,1,0,0,0,0,1,2,1,0,0,0,0,0,1,1,1,1,1},
    {1,2,1,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,3,1},
    {1,2,1,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,4,1},
    {1,1,1,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,0,1},
    {1,3,0,0,0,0,0,1,2,1,1,1,1,0,0,0,0,0,0,1},
    {1,4,0,0,0,0,0,1,2,2,2,2,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,2,2,2,2,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,2,2,2,2,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,2,2,2,1,0,0,0,0,1,1,1,1,1,1,0,0,0,0,1},
    {1,2,2,2,1,0,0,0,0,1,2,2,2,2,1,1,0,0,0,1},
    {1,2,2,1,0,0,0,0,1,2,2,2,2,2,2,1,0,0,0,1},
    {1,2,1,0,0,0,0,1,2,2,2,2,2,2,2,1,0,0,0,1},
    {1,1,0,0,0,0,1,2,2,2,2,2,2,2,2,1,0,0,0,1},
    {1,0,0,0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0,1},
    {1,3,0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0,0,1},
    {1,4,0,0,1,2,2,2,2,2,2,2,2,1,3,0,0,0,0,1},
    {1,0,0,0,1,1,2,2,2,2,2,2,1,0,4,0,0,0,0,1},
    {1,0,0,0,0,1,2,2,2,2,2,1,0,0,0,0,0,0,1,1},
    {1,0,0,0,0,1,2,2,2,2,2,1,0,0,0,0,0,1,2,1},
    {1,0,0,0,0,1,1,2,2,2,2,1,0,0,0,0,1,2,2,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,1,0,0,0,1,2,2,1},
    {1,6,6,6,6,6,1,2,2,2,2,2,2,1,0,0,1,2,2,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,1,2,2,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,1,2,2,1},
    {1,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,1,2,2,1},
    {1,0,0,0,0,0,1,1,1,1,1,1,1,1,6,6,6,1,2,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,3,1},
    {1,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,4,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static void clear_map(void)
{
    unsigned char row;
    unsigned char col;
    unsigned int addr;

    /* Split row addresses so the assembly DDA can index them without a multiply. */
    for (row = 0; row < MAZE_H; ++row) {
        addr = (unsigned int)maze_map[row];
        maze_row_lo[row] = (unsigned char)addr;
        maze_row_hi[row] = (unsigned char)(addr >> 8);
    }

    for (row = 0; row < MAZE_H; ++row) {
        for (col = 0; col < MAZE_W; ++col) {
            if (row == 0 || row == MAZE_H - 1 || col == 0 || col == MAZE_W - 1)
                maze_map[row][col] = 1;
            else
                maze_map[row][col] = 0;
        }
    }
}

static int read_level_byte(void)
{
    int count;

    if (level_buffer_pos >= level_buffer_len) {
        count = read(level_file, level_buffer, LEVEL_BUFFER_SIZE);
        if (count <= 0) return -1;
        level_buffer_pos = 0;
        level_buffer_len = (unsigned char)count;
    }
    return level_buffer[level_buffer_pos++];
}

void maze_load_level(unsigned char requested_level)
{
    const char *level_name;
    unsigned char row;
    unsigned char col;
    unsigned char tile;
    int ch;

    clear_map();
    maze_exit_open = 0;
    maze_exit_found = 0;
    if (requested_level <= 1) {
        for (row = 0; row < MAZE_H; ++row)
            for (col = 0; col < MAZE_W; ++col)
                maze_map[row][col] = level1_data[row][col];
    } else if (requested_level == 2) {
        /* Level 2 reuses level 1's data rotated 180 degrees, with target
         * tiles 3/4 swapped to keep the two-row glyph pointing the right
         * way up - same as chased1V3.BAS lines 19700-19709. */
        for (row = 0; row < MAZE_H; ++row) {
            for (col = 0; col < MAZE_W; ++col) {
                tile = level1_data[row][col];
                if (tile == 3) tile = 4;
                else if (tile == 4) tile = 3;
                maze_map[MAZE_H - 1 - row][MAZE_W - 1 - col] = tile;
            }
        }
    } else {
        if (requested_level == 3) level_name = "D:L2.CSV";
        else if (requested_level == 4) level_name = "D:L3.CSV";
        else if (requested_level == 5) level_name = "D:L4.CSV";
        else level_name = "D:L5.CSV";
        level_file = open(level_name, O_RDONLY);
        if (level_file < 0) return;
        level_buffer_pos = 0;
        level_buffer_len = 0;
        do { ch = read_level_byte(); } while (ch >= 0 && ch != '\n' && ch != '\r');
        if (ch == '\r') ch = read_level_byte();
        for (row = 0; row < MAZE_H; ++row) {
            do { ch = read_level_byte(); } while (ch >= 0 && ch != ';');
            if (ch < 0) { close(level_file); return; }
            for (col = 0; col < MAZE_W; ++col) {
                do { ch = read_level_byte(); } while (ch >= 0 && (ch < '0' || ch > '6'));
                if (ch < 0) { close(level_file); return; }
                maze_map[row][col] = (unsigned char)(ch - '0');
            }
        }
        close(level_file);
    }

    for (row = 0; row < MAZE_H && !maze_exit_found; ++row) {
        for (col = 0; col < MAZE_W; ++col) {
            if (maze_map[row][col] != 5) continue;
            maze_exit_col = col;
            maze_exit_row = row;
            maze_exit_found = 1;
            break;
        }
    }
}

void maze_set_exit_open(unsigned char open)
{
    maze_exit_open = open;
}

unsigned char maze_solid(unsigned char x, unsigned char y)
{
    unsigned char tile;

    if (x >= MAZE_W || y >= MAZE_H) return 1;
    tile = maze_map[y][x];
    if (tile == 5) return maze_exit_open ? 0 : 1;
    return (tile == 1 || tile == 2) ? 1 : 0;
}

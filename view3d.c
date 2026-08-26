#include <atari.h>
#include "maze.h"
#include "trig3d.h"
#include "view3d.h"

/* ANTIC mode D: 160x96, 4 colours, 2 bits per pixel, 40 bytes per row.
 * One ray column == one byte == 4 pixels, so no bit masking is ever needed. */
#define VIEW_STRIDE 40
#define HORIZON (VIEW_ROWS / 2)
#define WALL_SCALE ((unsigned int)VIEW_ROWS * 256u)   /* a wall one cell away fills the view */
#define MAX_STEPS 24
#define DIST_MIN 96u

/* Wall height by distance, indexed by dist >> 4, so the per-ray divide becomes
 * a table read. */
#define HEIGHT_STEPS 400
static unsigned char height_table[HEIGHT_STEPS];

unsigned int col_dist[VIEW_COLS];

#define PIX_CEILING 0x00u   /* colour 0 -> COLBK  */
#define PIX_FLOOR   0x55u   /* colour 1 -> COLPF0 */
#define PIX_WALL_Y  0xAAu   /* colour 2 -> COLPF1 */
#define PIX_WALL_X  0xFFu   /* colour 3 -> COLPF2 */

#define COLOR_FLOOR   0x26
#define COLOR_WALL_Y  0x08
#define COLOR_WALL_X  0x0E
#define COLOR_CEILING 0x92

/* Minimap: one pixel per maze cell, so the 20 cell wide map packs into 5 bytes.
 * It owns the left 5 screen bytes; the 3D view starts after it so that the
 * raycaster never overwrites the map. */
#define MINI_BYTES 5

extern unsigned char col3d_x;
extern unsigned char col3d_row;
extern unsigned char col3d_end;
extern unsigned char col3d_color;
extern void col3d_fill_down(void);
extern void col3d_fill_up(void);
extern void col3d_fill_span(void);

extern unsigned char dda_map_x;
extern unsigned char dda_map_y;
extern unsigned char dda_step_x;
extern unsigned char dda_step_y;
extern unsigned int dda_side_x;
extern unsigned int dda_side_y;
extern unsigned int dda_delta_x;
extern unsigned int dda_delta_y;
extern unsigned char dda_side;
extern unsigned char dda_hit;
extern unsigned char dda_steps;
extern void dda_cast(void);

/* cc65 puts function locals on a software stack reached through (sp),y, so the
 * per-ray values are file scope statics to get direct absolute addressing. */
static int ray_dir_x;
static int ray_dir_y;
static unsigned int ray_dist;
static unsigned int ray_height;
static unsigned int ray_mag;
static unsigned char ray_idx;
static unsigned char ray_top;
static unsigned char ray_bottom;
static unsigned char ray_wall;

#pragma bss-name (push, "SCREEN")
unsigned char view_buffer[VIEW_STRIDE * VIEW_ROWS];
static unsigned char hud_line[20];
#pragma bss-name (pop)

#pragma bss-name (push, "DLIST")
static unsigned char view_dlist[104];
#pragma bss-name (pop)

static unsigned char minimap_bits[MAZE_H][MINI_BYTES];
static unsigned char marker_row = 0xFF;
static unsigned char nose_row = 0xFF;

/* Facing quantised to 8 compass points; angle 0 is +X with +Y running down. */
static const signed char dir_dx[8] = {  1,  1,  0, -1, -1, -1,  0,  1 };
static const signed char dir_dy[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };

/* (frac * delta) >> 8. Splitting delta into bytes keeps this to two 8x8
 * multiplies, which cc65 does far more cheaply than a 16x16 one. */
static unsigned int scale_frac(unsigned int frac, unsigned int delta)
{
    unsigned char f;

    if (frac >= 256u) return delta;
    f = (unsigned char)frac;
    return ((unsigned int)f * (unsigned char)(delta >> 8))
         + (((unsigned int)f * (unsigned char)delta) >> 8);
}

static void draw_open_byte(unsigned char x)
{
    col3d_x = x;
    col3d_row = HORIZON;
    col3d_color = PIX_FLOOR;
    col3d_fill_down();
    col3d_row = HORIZON - 1;
    col3d_color = PIX_CEILING;
    col3d_fill_up();
}

static void draw_open_column(unsigned char col)
{
    unsigned char x;

    x = (unsigned char)(MINI_BYTES + (col << 1));
    draw_open_byte(x);
    if (x + 1 < VIEW_STRIDE) draw_open_byte(x + 1);
}

static void draw_wall_byte(unsigned char x, unsigned char top,
                           unsigned char bottom, unsigned char wall)
{
    col3d_x = x;
    col3d_row = top;
    col3d_end = bottom + 1;
    col3d_color = wall;
    col3d_fill_span();
    if (bottom < VIEW_ROWS - 1) {
        col3d_row = bottom + 1;
        col3d_color = PIX_FLOOR;
        col3d_fill_down();
    }
    if (top > 0) {
        col3d_row = top - 1;
        col3d_color = PIX_CEILING;
        col3d_fill_up();
    }
}

static void draw_wall_column(unsigned char col, unsigned char top,
                             unsigned char bottom, unsigned char wall)
{
    unsigned char x;

    x = (unsigned char)(MINI_BYTES + (col << 1));
    draw_wall_byte(x, top, bottom, wall);
    if (x + 1 < VIEW_STRIDE) draw_wall_byte(x + 1, top, bottom, wall);
}

static void cast_column(unsigned char col, unsigned int ray_angle,
                        unsigned int px, unsigned int py)
{
    ray_idx = (unsigned char)(ray_angle >> 8);
    ray_dir_x = sin3d[(unsigned char)(ray_idx + 64)];
    ray_dir_y = sin3d[ray_idx];

    dda_map_x = (unsigned char)(px >> 8);
    dda_map_y = (unsigned char)(py >> 8);

    ray_mag = (unsigned int)(ray_dir_x < 0 ? -ray_dir_x : ray_dir_x);
    dda_delta_x = recip3d[ray_mag];
    ray_mag = (unsigned int)(ray_dir_y < 0 ? -ray_dir_y : ray_dir_y);
    dda_delta_y = recip3d[ray_mag];

    if (ray_dir_x < 0) {
        dda_step_x = 0xFF;
        dda_side_x = scale_frac(px & 0x00FFu, dda_delta_x);
    } else {
        dda_step_x = 1;
        dda_side_x = scale_frac(256u - (px & 0x00FFu), dda_delta_x);
    }
    if (ray_dir_y < 0) {
        dda_step_y = 0xFF;
        dda_side_y = scale_frac(py & 0x00FFu, dda_delta_y);
    } else {
        dda_step_y = 1;
        dda_side_y = scale_frac(256u - (py & 0x00FFu), dda_delta_y);
    }

    dda_steps = MAX_STEPS;
    dda_cast();

    if (!dda_hit) {
        col_dist[col] = 0xFFFFu;
        draw_open_column(col);
        return;
    }

    if (dda_side == 0) {
        ray_dist = dda_side_x - dda_delta_x;
        ray_wall = PIX_WALL_X;
    } else {
        ray_dist = dda_side_y - dda_delta_y;
        ray_wall = PIX_WALL_Y;
    }

    /* Multiply by cos(relative angle) to turn the ray length into a
     * perpendicular distance, which removes the fisheye bow. */
    ray_mag = cos_rel[col];
    ray_dist = ((ray_dist >> 8) * ray_mag)
             + (((ray_dist & 0x00FFu) * ray_mag) >> 8);
    col_dist[col] = ray_dist;

    ray_dist >>= 4;
    if (ray_dist >= HEIGHT_STEPS) ray_dist = HEIGHT_STEPS - 1;
    ray_height = height_table[ray_dist];

    if (ray_height >= VIEW_ROWS) {
        ray_top = 0;
        ray_bottom = VIEW_ROWS - 1;
    } else {
        ray_top = (unsigned char)((VIEW_ROWS - ray_height) >> 1);
        ray_bottom = (unsigned char)(ray_top + ray_height - 1);
    }
    draw_wall_column(col, ray_top, ray_bottom, ray_wall);
}

void view3d_render(unsigned int px, unsigned int py, unsigned int angle)
{
    unsigned char col;

    for (col = 0; col < VIEW_COLS; ++col) {
        cast_column(col, angle + (unsigned int)ray_offset[col], px, py);
    }
}

/* The maze is static, so pack it into a bitmap once instead of every frame. */
void minimap_build(void)
{
    unsigned char row;
    unsigned char col;
    unsigned char byte;

    for (row = 0; row < MAZE_H; ++row) {
        for (byte = 0; byte < MINI_BYTES; ++byte) {
            minimap_bits[row][byte] = 0;
        }
        for (col = 0; col < MAZE_W; ++col) {
            if (maze_solid(col, row)) {
                minimap_bits[row][col >> 2] |=
                    (unsigned char)(0x02 << (6 - ((col & 3) << 1)));
            }
        }
    }
}

/* The whole map is static, so it is blitted once and never redrawn. */
void minimap_show(void)
{
    unsigned char *dest;
    unsigned char row;
    unsigned char byte;

    dest = view_buffer;
    for (row = 0; row < MAZE_H; ++row) {
        for (byte = 0; byte < MINI_BYTES; ++byte) {
            dest[byte] = minimap_bits[row][byte];
        }
        dest += VIEW_STRIDE;
    }
}

static void minimap_restore_row(unsigned char row)
{
    unsigned char *dest;
    unsigned char byte;

    if (row >= MAZE_H) return;
    dest = view_buffer + (unsigned int)row * VIEW_STRIDE;
    for (byte = 0; byte < MINI_BYTES; ++byte) {
        dest[byte] = minimap_bits[row][byte];
    }
}

static void minimap_plot(unsigned char col, unsigned char row, unsigned char value)
{
    unsigned char *dest;
    unsigned char shift;

    if (col >= MAZE_W || row >= MAZE_H) return;
    shift = (unsigned char)(6 - ((col & 3) << 1));
    dest = view_buffer + (unsigned int)row * VIEW_STRIDE + (col >> 2);
    *dest = (unsigned char)((*dest & ~(0x03 << shift)) | (value << shift));
}

void minimap_update(unsigned int px, unsigned int py, unsigned int angle)
{
    unsigned char col;
    unsigned char row;
    unsigned char facing;

    minimap_restore_row(marker_row);
    minimap_restore_row(nose_row);

    col = (unsigned char)(px >> 8);
    row = (unsigned char)(py >> 8);
    facing = (unsigned char)(((((unsigned int)(angle >> 8)) + 16) >> 5) & 7);

    nose_row = (unsigned char)(row + dir_dy[facing]);
    minimap_plot((unsigned char)(col + dir_dx[facing]), nose_row, 0x01);
    minimap_plot(col, row, 0x03);
    marker_row = row;
}

/* ANTIC mode 6 uses internal character codes, not ATASCII; bits 6-7 pick the
 * colour register, and COLPF2 is the only one bright enough over COLBK. */
#define HUD_COLOR 0x80

void hud_set_fps(unsigned char fps)
{
    if (fps > 99) fps = 99;
    hud_line[0] = (unsigned char)(38 | HUD_COLOR);   /* F */
    hud_line[1] = (unsigned char)(48 | HUD_COLOR);   /* P */
    hud_line[2] = (unsigned char)(51 | HUD_COLOR);   /* S */
    hud_line[4] = (unsigned char)((16 + (fps / 10)) | HUD_COLOR);
    hud_line[5] = (unsigned char)((16 + (fps % 10)) | HUD_COLOR);
}

unsigned char view3d_wall_height(unsigned int dist)
{
    dist >>= 4;
    if (dist >= HEIGHT_STEPS) dist = HEIGHT_STEPS - 1;
    return height_table[dist];
}

void view3d_init(void)
{
    unsigned int addr;
    unsigned char i;
    unsigned char n;

    for (addr = 0; addr < HEIGHT_STEPS; ++addr) {
        n = (addr <= (DIST_MIN >> 4))
            ? VIEW_ROWS
            : (unsigned char)(WALL_SCALE / (addr << 4) > VIEW_ROWS
                              ? VIEW_ROWS
                              : WALL_SCALE / (addr << 4));
        height_table[addr] = n;
    }

    addr = (unsigned int)view_buffer;
    n = 0;
    view_dlist[n++] = 0x70;
    view_dlist[n++] = 0x70;
    view_dlist[n++] = 0x70;
    view_dlist[n++] = 0x4D;                        /* mode D + load memory scan */
    view_dlist[n++] = (unsigned char)addr;
    view_dlist[n++] = (unsigned char)(addr >> 8);
    for (i = 1; i < VIEW_ROWS; ++i) {
        view_dlist[n++] = 0x0D;
    }
    addr = (unsigned int)hud_line;
    view_dlist[n++] = 0x46;                        /* mode 6 text + load memory scan */
    view_dlist[n++] = (unsigned char)addr;
    view_dlist[n++] = (unsigned char)(addr >> 8);
    addr = (unsigned int)view_dlist;
    view_dlist[n++] = 0x41;                        /* jump and wait for vblank */
    view_dlist[n++] = (unsigned char)addr;
    view_dlist[n++] = (unsigned char)(addr >> 8);

    *(volatile unsigned char *)708 = COLOR_FLOOR;
    *(volatile unsigned char *)709 = COLOR_WALL_Y;
    *(volatile unsigned char *)710 = COLOR_WALL_X;
    *(volatile unsigned char *)712 = COLOR_CEILING;

    OS.sdlst = view_dlist;
    OS.sdmctl = 0x22;
    ANTIC.dmactl = 0x22;
}

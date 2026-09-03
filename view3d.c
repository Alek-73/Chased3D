#include <atari.h>
#include "maze.h"
#include "trig3d.h"
#include "view3d.h"
#ifdef DEBUG_HUD
#include "build_number.h"
#endif

/* ANTIC mode D: 160x96, 4 colours, 2 bits per pixel, 40 bytes per row.
 * One ray column == one byte == 4 pixels, so no bit masking is ever needed. */
#define HORIZON (VIEW_ROWS / 2)
#define WALL_SCALE ((unsigned int)VIEW_ROWS * 256u)   /* a wall one cell away fills the view */
#define MAX_STEPS 24
#define DIST_MIN 96u

/* Wall height by distance, indexed by dist >> 4, so the per-ray divide becomes
 * a table read. */
#define HEIGHT_STEPS 400
#define FLOOR_BANDS 6
#define FLOOR_BAND_ROWS 7
#pragma bss-name (push, "HIGHBSS")
static unsigned char height_table[HEIGHT_STEPS];
#pragma bss-name (pop)

unsigned int col_dist[VIEW_COLS];

#define PIX_CEILING 0x00u   /* colour 0 -> COLBK  */
#define PIX_FLOOR   0x55u   /* colour 1 -> COLPF0 */
#define PIX_WALL_Y  0xAAu   /* colour 2 -> COLPF1 */
#define PIX_WALL_X  0xFFu   /* colour 3 -> COLPF2 */

/* Walls sit well below maximum luminance so the bright PMG targets stand out
 * against them. Luminance, not hue, is what separates colours here. */
#define COLOR_FLOOR   0x28
#define COLOR_WALL_Y  0x06
#define COLOR_WALL_X  0x0A
#define COLOR_CEILING 0x92

/* Minimap: one pixel per maze cell, so the 20 cell wide map packs into 5 bytes.
 * It owns the left 5 screen bytes; the 3D view starts after it so that the
 * raycaster never overwrites the map. */
#define MINI_BYTES 5
#define DECOY_BAR_INNER_WIDTH (MAZE_W - 2)

extern unsigned char col3d_x;
extern unsigned char col3d_row;
extern unsigned char col3d_end;
extern unsigned char col3d_color;
extern void col3d_fill_down(void);
extern void col3d_fill_up(void);
extern void col3d_fill_span(void);

extern unsigned int dda_side_x;
extern unsigned int dda_side_y;
extern unsigned int dda_delta_x;
extern unsigned int dda_delta_y;
extern unsigned char dda_side;
extern unsigned char dda_hit;
extern unsigned int ray_setup_angle;
extern unsigned int ray_setup_px;
extern unsigned int ray_setup_py;
extern void ray_setup_and_cast(void);
extern void floor_dli_install(void);
extern void floor_dli_set_rotation(unsigned char rotation);

/* cc65 puts function locals on a software stack reached through (sp),y, so the
 * per-ray values are file scope statics to get direct absolute addressing. */
static unsigned int ray_dist;
static unsigned int ray_height;
static unsigned int ray_mag;
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

#pragma bss-name (push, "HIGHBSS")
static unsigned char minimap_bits[MAZE_H][MINI_BYTES];
#pragma bss-name (pop)

static unsigned char marker_row = 0xFF;
static unsigned char nose_row = 0xFF;
static unsigned char floor_phase;
static unsigned char floor_rotation;

/* Facing quantised to 8 compass points; angle 0 is +X with +Y running down. */
static const signed char dir_dx[8] = {  1,  1,  0, -1, -1, -1,  0,  1 };
static const signed char dir_dy[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };

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
    ray_setup_angle = ray_angle;
    ray_setup_px = px;
    ray_setup_py = py;
    ray_setup_and_cast();

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

static void set_floor_dlis(void)
{
    unsigned char i;
    unsigned char row;

    for (row = HORIZON - 1; row < VIEW_ROWS; ++row)
        view_dlist[5 + row] &= 0x7F;
    for (i = 0; i < FLOOR_BANDS; ++i) {
        row = (unsigned char)(HORIZON + floor_phase + i * FLOOR_BAND_ROWS);
        view_dlist[5 + row - 1] |= 0x80;
    }
    view_dlist[5 + VIEW_ROWS - 1] |= 0x80;
}

void view3d_floor_motion(signed char direction)
{
    if (direction > 0) {
        if (++floor_phase >= FLOOR_BAND_ROWS) {
            floor_phase = 0;
            floor_rotation = floor_rotation == 0
                ? FLOOR_BANDS - 1 : (unsigned char)(floor_rotation - 1);
            floor_dli_set_rotation(floor_rotation);
        }
    } else {
        if (floor_phase == 0) {
            floor_phase = FLOOR_BAND_ROWS - 1;
            if (++floor_rotation >= FLOOR_BANDS) floor_rotation = 0;
            floor_dli_set_rotation(floor_rotation);
        } else {
            --floor_phase;
        }
    }
    set_floor_dlis();
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

/* ANTIC mode 6 uses internal character codes, not ATASCII; bits 6-7 select
 * the playfield colour register used by each character. */
#define HUD_LABEL_COLOR 0x40
#define HUD_NUMBER_COLOR 0x80
#define HUD_CHAR(character) ((unsigned char)(((character) - 32) | HUD_LABEL_COLOR))
#define HUD_DIGIT(digit) ((unsigned char)((16 + (digit)) | HUD_NUMBER_COLOR))

#ifdef DEBUG_HUD
void hud_set_fps(unsigned char fps)
{
    if (fps > 99) fps = 99;
    hud_line[0] = HUD_CHAR('F');
    hud_line[1] = HUD_CHAR('P');
    hud_line[2] = HUD_CHAR('S');
    hud_line[4] = HUD_DIGIT(fps / 10);
    hud_line[5] = HUD_DIGIT(fps % 10);
}

void hud_set_targets(unsigned char remaining)
{
    if (remaining > 99) remaining = 99;
    hud_line[10] = HUD_CHAR('T');
    hud_line[11] = HUD_CHAR('G');
    hud_line[12] = HUD_CHAR('T');
    hud_line[14] = HUD_DIGIT(remaining / 10);
    hud_line[15] = HUD_DIGIT(remaining % 10);
}
#endif

#ifndef DEBUG_HUD
static void hud_set_number5(unsigned char offset, unsigned int value)
{
    hud_line[offset++] = HUD_DIGIT(value / 10000u);
    value %= 10000u;
    hud_line[offset++] = HUD_DIGIT(value / 1000u);
    value %= 1000u;
    hud_line[offset++] = HUD_DIGIT(value / 100u);
    value %= 100u;
    hud_line[offset++] = HUD_DIGIT(value / 10u);
    hud_line[offset] = HUD_DIGIT(value % 10u);
}
#endif

void hud_set_game(unsigned char lives, unsigned char level,
                  unsigned int score, unsigned int high_score)
{
#ifdef DEBUG_HUD
    (void)lives;
    (void)level;
    (void)score;
    (void)high_score;
#else
    if (lives > 9) lives = 9;
    if (level > 9) level = 9;
    hud_line[0] = HUD_CHAR('L');
    hud_line[1] = HUD_DIGIT(lives);
    hud_line[2] = 0;
    hud_line[3] = HUD_CHAR('L');
    hud_line[4] = HUD_CHAR('V');
    hud_line[5] = HUD_DIGIT(level);
    hud_line[6] = 0;
    hud_line[7] = HUD_CHAR('S');
    hud_set_number5(8, score);
    hud_line[13] = 0;
    hud_line[14] = HUD_CHAR('H');
    hud_set_number5(15, high_score);
#endif
}

void hud_set_decoy(unsigned char progress, unsigned char maximum)
{
    unsigned char filled;
    unsigned char col;
    unsigned char row;
    unsigned char value;
    unsigned char shift;
    unsigned char *dest;

    filled = maximum == 0 ? DECOY_BAR_INNER_WIDTH
        : (unsigned char)(((unsigned int)progress * DECOY_BAR_INNER_WIDTH) / maximum);
    if (filled > DECOY_BAR_INNER_WIDTH) filled = DECOY_BAR_INNER_WIDTH;

    for (row = 0; row < DECOY_BAR_HEIGHT; ++row) {
        dest = view_buffer + (unsigned int)(DECOY_BAR_TOP + row) * VIEW_STRIDE;
        for (col = 0; col < MAZE_W; ++col) {
            if (row == 0 || row == DECOY_BAR_HEIGHT - 1 || col == 0 || col == MAZE_W - 1)
                value = 2;
            else
                value = col <= filled ? 3 : 0;
            shift = (unsigned char)(6 - ((col & 3) << 1));
            dest[col >> 2] = (unsigned char)(
                (dest[col >> 2] & ~(0x03 << shift)) | (value << shift));
        }
    }
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

    for (addr = 0; addr < VIEW_STRIDE * VIEW_ROWS; ++addr)
        view_buffer[addr] = 0;

    for (i = 0; i < sizeof(hud_line); ++i) hud_line[i] = 0;
#ifdef DEBUG_HUD
    hud_line[16] = HUD_CHAR('B');
    hud_line[17] = HUD_DIGIT(BUILD_DIGIT_100);
    hud_line[18] = HUD_DIGIT(BUILD_DIGIT_10);
    hud_line[19] = HUD_DIGIT(BUILD_DIGIT_1);
#endif

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
    for (i = 1; i < VIEW_ROWS; ++i) view_dlist[n++] = 0x0D;
    addr = (unsigned int)hud_line;
    view_dlist[n++] = 0x46;                        /* mode 6 text + load memory scan */
    view_dlist[n++] = (unsigned char)addr;
    view_dlist[n++] = (unsigned char)(addr >> 8);
    addr = (unsigned int)view_dlist;
    view_dlist[n++] = 0x41;                        /* jump and wait for vblank */
    view_dlist[n++] = (unsigned char)addr;
    view_dlist[n++] = (unsigned char)(addr >> 8);

    COLOR0 = COLOR_FLOOR;
    COLOR1 = COLOR_WALL_Y;
    COLOR2 = COLOR_WALL_X;
    COLOR4 = COLOR_CEILING;

    OS.sdlst = view_dlist;
    OS.sdmctl = 0x22;
    ANTIC.dmactl = 0x22;
    floor_phase = 0;
    floor_rotation = 0;
    set_floor_dlis();
    floor_dli_install();
}

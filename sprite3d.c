#include <atari.h>
#include "maze.h"
#include "trig3d.h"
#include "view3d.h"
#include "sprite3d.h"

/* Targets are drawn as Player/Missile billboards rather than into the frame
 * buffer: the playfield fill writes every pixel exactly once, and blending a
 * masked sprite into it would cost a read-modify-write per pixel. */

#define PMG_PLAYERS 4
#define PMG_PLAYER_SIZE 128
#define PMG_PLAYER0_OFFSET 0x200

/* Display row 0 lands on this player byte: ANTIC starts the display list at
 * scanline 8 (player byte 4) and the list opens with 24 blank scanlines, which
 * is 12 more two-line bytes. */
#define PMG_TOP_OFFSET 16

#define HPOSP0 0xD000
#define SIZEP0 0xD008
#define PCOLR0 0x02C0

#define VIEW_LEFT_PX 20                 /* 3D view starts at frame buffer byte 5 */
#define VIEW_RIGHT_PX 159
#define VIEW_CENTER_PX 90
#define PROJ_X 121                      /* (view width / 2) / tan(FOV / 2) */

#define MAX_TARGETS 24
#define RANGE_44 192                    /* 12 cells, in 1/16 cell units */
#define TARGET_COLOR 0x1E               /* bright yellow */

#pragma bss-name (push, "PMGRAM")
static unsigned char pmg_ram[1024];
#pragma bss-name (pop)

/* Charset characters 5 and 6 stacked: the same glyphs the 2D game drew for
 * tiles 3 and 4, here read as one upright 8 x 16 billboard. */
static const unsigned char target_sprite[16] = {
    225, 146,  76,  74,  49,  49,  77, 131,
    128, 128, 128, 128, 128, 128, 255, 255
};

static unsigned int target_px[MAX_TARGETS];
static unsigned int target_py[MAX_TARGETS];
static unsigned char target_count;

static unsigned int cand_dist[PMG_PLAYERS];
static unsigned char cand_x[PMG_PLAYERS];
static unsigned char cand_top[PMG_PLAYERS];
static unsigned char cand_height[PMG_PLAYERS];
static unsigned char cand_count;
static unsigned char last_used;

void sprite3d_init(void)
{
    unsigned int i;
    unsigned char player;

    for (i = 0; i < sizeof(pmg_ram); ++i) {
        pmg_ram[i] = 0;
    }

    ANTIC.pmbase = (unsigned char)((unsigned int)pmg_ram >> 8);
    OS.sdmctl = 0x2A;                   /* normal playfield, DL DMA, player DMA */
    ANTIC.dmactl = OS.sdmctl;
    GTIA_WRITE.gractl = 0x02;
    OS.gprior = 0x01;                   /* players in front of the playfield */
    GTIA_WRITE.prior = OS.gprior;

    for (player = 0; player < PMG_PLAYERS; ++player) {
        *(volatile unsigned char *)(SIZEP0 + player) = 0;
        *(volatile unsigned char *)(PCOLR0 + player) = TARGET_COLOR;
    }
    last_used = 0;
}

/* A target is a tile 3 with a tile 4 directly below it. The pair is one object,
 * placed on the boundary between the two cells. */
void sprite3d_build_targets(void)
{
    unsigned char row;
    unsigned char col;

    target_count = 0;
    for (row = 0; row + 1 < MAZE_H; ++row) {
        for (col = 0; col < MAZE_W; ++col) {
            if (maze_map[row][col] != 3 || maze_map[row + 1][col] != 4) continue;
            if (target_count >= MAX_TARGETS) return;
            target_px[target_count] = ((unsigned int)col << 8) | 0x80;
            target_py[target_count] = (unsigned int)(row + 1) << 8;
            ++target_count;
        }
    }
}

static void clear_player(unsigned char player)
{
    unsigned char *dest;
    unsigned char i;

    dest = pmg_ram + PMG_PLAYER0_OFFSET + ((unsigned int)player << 7);
    for (i = 0; i < PMG_PLAYER_SIZE; ++i) {
        dest[i] = 0;
    }
}

static void draw_billboard(unsigned char player, unsigned char sx,
                           unsigned char top, unsigned char height)
{
    unsigned char *dest;
    unsigned char i;
    unsigned char size;
    unsigned char width;
    unsigned int acc;
    unsigned int step;

    /* Player width only scales in hardware by 1x, 2x or 4x. */
    if (height >= 48) { size = 3; width = 32; }
    else if (height >= 24) { size = 1; width = 16; }
    else { size = 0; width = 8; }

    dest = pmg_ram + PMG_PLAYER0_OFFSET + ((unsigned int)player << 7);
    for (i = 0; i < PMG_PLAYER_SIZE; ++i) {
        dest[i] = 0;
    }

    step = (16u << 8) / height;
    acc = 0;
    dest += PMG_TOP_OFFSET + top;
    for (i = 0; i < height; ++i) {
        dest[i] = target_sprite[acc >> 8];
        acc += step;
    }

    *(volatile unsigned char *)(SIZEP0 + player) = size;
    *(volatile unsigned char *)(HPOSP0 + player) =
        (unsigned char)(48 + sx - (width >> 1));
}

static void add_candidate(unsigned int dist, unsigned char sx,
                          unsigned char top, unsigned char height)
{
    unsigned char slot;

    if (cand_count == PMG_PLAYERS && dist >= cand_dist[PMG_PLAYERS - 1]) return;

    slot = cand_count < PMG_PLAYERS ? cand_count : (unsigned char)(PMG_PLAYERS - 1);
    while (slot > 0 && cand_dist[slot - 1] > dist) {
        cand_dist[slot] = cand_dist[slot - 1];
        cand_x[slot] = cand_x[slot - 1];
        cand_top[slot] = cand_top[slot - 1];
        cand_height[slot] = cand_height[slot - 1];
        --slot;
    }
    cand_dist[slot] = dist;
    cand_x[slot] = sx;
    cand_top[slot] = top;
    cand_height[slot] = height;
    if (cand_count < PMG_PLAYERS) ++cand_count;
}

void sprite3d_draw_targets(unsigned int px, unsigned int py, unsigned int angle)
{
    unsigned char i;
    unsigned char idx;
    unsigned char height;
    unsigned char top;
    unsigned char column;
    int cos_h;
    int sin_h;
    int dx;
    int dy;
    int forward;
    int right;
    int limit;
    int screen_x;
    unsigned int dist;

    idx = (unsigned char)(angle >> 8);
    cos_h = sin3d[(unsigned char)(idx + 64)] >> 1;   /* halved to keep products in range */
    sin_h = sin3d[idx] >> 1;
    cand_count = 0;

    for (i = 0; i < target_count; ++i) {
        dx = ((int)target_px[i] - (int)px) >> 4;     /* 1/16 cell units */
        if (dx > RANGE_44 || dx < -RANGE_44) continue;
        dy = ((int)target_py[i] - (int)py) >> 4;
        if (dy > RANGE_44 || dy < -RANGE_44) continue;

        forward = ((dx * cos_h) >> 7) + ((dy * sin_h) >> 7);
        if (forward <= 8) continue;                  /* behind the camera or on top of it */

        right = ((-dx * sin_h) >> 7) + ((dy * cos_h) >> 7);
        limit = (forward * 5) >> 3;                  /* just outside the 60 degree view */
        if (right > limit || right < -limit) continue;

        screen_x = VIEW_CENTER_PX + ((right * PROJ_X) / forward);
        if (screen_x < VIEW_LEFT_PX || screen_x > VIEW_RIGHT_PX) continue;

        column = (unsigned char)((screen_x - VIEW_LEFT_PX) >> 3);
        if (column >= VIEW_COLS) continue;

        dist = (unsigned int)forward << 4;
        if (dist >= col_dist[column]) continue;      /* hidden behind a wall */

        height = view3d_wall_height(dist);
        top = height >= VIEW_ROWS
            ? 0
            : (unsigned char)((VIEW_ROWS - height) >> 1);
        if (height > VIEW_ROWS) height = VIEW_ROWS;

        add_candidate(dist, (unsigned char)screen_x, top, height);
    }

    for (i = 0; i < cand_count; ++i) {
        draw_billboard(i, cand_x[i], cand_top[i], cand_height[i]);
    }
    for (i = cand_count; i < last_used; ++i) {
        clear_player(i);
    }
    last_used = cand_count;
}

#include <atari.h>
#include "maze.h"
#include "trig3d.h"
#include "view3d.h"
#include "sprite3d.h"

/* Targets are drawn as Player/Missile billboards rather than into the frame
 * buffer: the playfield fill writes every pixel exactly once, and blending a
 * masked sprite into it would cost a read-modify-write per pixel. */

#define PMG_PLAYERS 4
#define PMG_TARGET_PLAYERS 3
#define PMG_PLAYER_SIZE 128
#define PMG_MISSILE_OFFSET 0x180
#define PMG_PLAYER0_OFFSET 0x200

/* Display row 0 lands on this player byte. Derived from the display list (24
 * blank scanlines after ANTIC starts) then corrected against measured output,
 * since PM DMA runs slightly ahead of the playfield fetch. */
#define PMG_TOP_OFFSET 18

#define HPOSP0 0xD000
#define SIZEP0 0xD008
#define PCOLR0 0x02C0

#define VIEW_LEFT_PX 20                 /* 3D view starts at frame buffer byte 5 */
#define VIEW_RIGHT_PX 159
#define VIEW_CENTER_PX 90
#define PROJ_X 121                      /* (view width / 2) / tan(FOV / 2) */

#define MAX_TARGETS 24
#define RANGE_44 192                    /* 12 cells, in 1/16 cell units */
#define COLLECT_RADIUS 160              /* 0.625 cell, in 8.8 world units */
#define TARGET_COLOR 0x1E               /* bright yellow */
#define PURSUER_COLOR 0x34              /* original pursuer PMG color */
#define DECOY_COLOR 154                  /* BASIC COLOR2, used by screen code 138 */

#pragma bss-name (push, "PMGRAM")
static unsigned char pmg_ram[1024];
#pragma bss-name (pop)

/* Charset characters 5 and 6 stacked: the same glyphs the 2D game drew for
 * tiles 3 and 4, here read as one upright 8 x 16 billboard. */
static const unsigned char target_sprite[16] = {
    225, 146,  76,  74,  49,  49,  77, 131,
    128, 128, 128, 128, 128, 128, 255, 255
};

static const unsigned char pursuer_sprite[22] = {
    0, 0, 0, 0, 0, 0, 0, 0, 66, 153, 189, 255,
    189, 153, 66, 0, 0, 0, 0, 0, 0, 0
};

/* Custom character 10 from chased1V3.BAS lines 16007 and 194. */
static const unsigned char decoy_sprite[8] = {
    18, 22, 24, 60, 60, 24, 104, 72
};

/* Open-door glyph: an outlined arch, drawn in the same color as targets since
 * the original revealed the exit as a target-colored character too. */
static const unsigned char exit_sprite[16] = {
    60, 66, 129, 129, 129, 129, 129, 129,
    129, 129, 129, 129, 129, 129, 129, 255
};

static unsigned int exit_px;
static unsigned int exit_py;

static unsigned int target_px[MAX_TARGETS];
static unsigned int target_py[MAX_TARGETS];
static unsigned char target_active[MAX_TARGETS];
static unsigned char target_count;
static unsigned char targets_left;

static unsigned int cand_dist[PMG_TARGET_PLAYERS];
static unsigned char cand_x[PMG_TARGET_PLAYERS];
static unsigned char cand_top[PMG_TARGET_PLAYERS];
static unsigned char cand_height[PMG_TARGET_PLAYERS];
static unsigned char cand_count;
static unsigned char last_used;
static unsigned char projected_x;
static unsigned char projected_top;
static unsigned char projected_height;
static unsigned int projected_dist;

static void draw_billboard(unsigned char player, unsigned char sx,
                           unsigned char top, unsigned char height,
                           const unsigned char *sprite, unsigned char sprite_rows);

void sprite3d_init(void)
{
    unsigned int i;
    unsigned char player;

    for (i = 0; i < sizeof(pmg_ram); ++i) {
        pmg_ram[i] = 0;
    }

    ANTIC.pmbase = (unsigned char)((unsigned int)pmg_ram >> 8);
    OS.sdmctl = 0x2E;                   /* normal playfield, DL, player+missile DMA */
    ANTIC.dmactl = OS.sdmctl;
    GTIA_WRITE.gractl = 0x03;
    OS.gprior = 0x11;                   /* players in front + missiles as Player 4 */
    GTIA_WRITE.prior = OS.gprior;
    OS.color3 = DECOY_COLOR;
    GTIA_WRITE.colpf3 = DECOY_COLOR;

    for (player = 0; player < PMG_PLAYERS; ++player) {
        *(volatile unsigned char *)(SIZEP0 + player) = 0;
        *(volatile unsigned char *)(PCOLR0 + player) =
            player == 0 ? PURSUER_COLOR : TARGET_COLOR;
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
    for (row = 0; row + 1 < MAZE_H && target_count < MAX_TARGETS; ++row) {
        for (col = 0; col < MAZE_W && target_count < MAX_TARGETS; ++col) {
            if (maze_map[row][col] != 3 || maze_map[row + 1][col] != 4) continue;
            target_px[target_count] = ((unsigned int)col << 8) | 0x80;
            target_py[target_count] = (unsigned int)(row + 1) << 8;
            target_active[target_count] = 1;
            ++target_count;
        }
    }
    targets_left = target_count;
}

unsigned char sprite3d_targets_left(void)
{
    return targets_left;
}

/* Cached once per level: the exit tile is fixed for the level's lifetime,
 * unlike targets which disappear as they are collected. */
void sprite3d_locate_exit(void)
{
    exit_px = ((unsigned int)maze_exit_col << 8) | 0x80;
    exit_py = ((unsigned int)maze_exit_row << 8) | 0x80;
}

unsigned char sprite3d_reached_exit(unsigned int px, unsigned int py)
{
    int dx;
    int dy;

    if (!maze_exit_found || targets_left != 0) return 0;
    dx = (int)exit_px - (int)px;
    if (dx < 0) dx = -dx;
    if (dx >= COLLECT_RADIUS) return 0;
    dy = (int)exit_py - (int)py;
    if (dy < 0) dy = -dy;
    return dy < COLLECT_RADIUS;
}

/* Box test rather than a true radius: no multiply, and the difference is not
 * noticeable when walking into a target. */
unsigned char sprite3d_collect(unsigned int px, unsigned int py)
{
    unsigned char i;
    unsigned char taken;
    int dx;
    int dy;

    taken = 0;
    for (i = 0; i < target_count; ++i) {
        if (!target_active[i]) continue;
        dx = (int)target_px[i] - (int)px;
        if (dx < 0) dx = -dx;
        if (dx >= COLLECT_RADIUS) continue;
        dy = (int)target_py[i] - (int)py;
        if (dy < 0) dy = -dy;
        if (dy >= COLLECT_RADIUS) continue;
        target_active[i] = 0;
        --targets_left;
        ++taken;
    }
    return taken;
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

static void clear_decoy(void)
{
    unsigned char *dest;
    unsigned char i;

    dest = pmg_ram + PMG_MISSILE_OFFSET;
    for (i = 0; i < PMG_PLAYER_SIZE; ++i) dest[i] = 0;
}

/* Called on a reposition so a sprite left undrawn by a failed projection on
 * the next frame does not keep showing at its old screen position. */
void sprite3d_clear_all(void)
{
    unsigned char player;

    for (player = 0; player < PMG_PLAYERS; ++player) {
        clear_player(player);
    }
    clear_decoy();
    last_used = 0;
}

static void draw_billboard(unsigned char player, unsigned char sx,
                           unsigned char top, unsigned char height,
                           const unsigned char *sprite, unsigned char sprite_rows)
{
    unsigned char *dest;
    unsigned char i;
    unsigned char size;
    unsigned char width;
    unsigned int acc;
    unsigned int step;
    int left;

    /* Player width only scales in hardware by 1x, 2x or 4x. */
    if (height >= 48) { size = 3; width = 32; }
    else if (height >= 24) { size = 1; width = 16; }
    else { size = 0; width = 8; }

    left = (int)sx - (width >> 1);
    if (left < VIEW_LEFT_PX) left = VIEW_LEFT_PX;
    if (left + width > VIEW_RIGHT_PX + 1)
        left = VIEW_RIGHT_PX + 1 - width;

    dest = pmg_ram + PMG_PLAYER0_OFFSET + ((unsigned int)player << 7);
    for (i = 0; i < PMG_PLAYER_SIZE; ++i) {
        dest[i] = 0;
    }

    step = ((unsigned int)sprite_rows << 8) / height;
    acc = 0;
    dest += PMG_TOP_OFFSET + top;
    for (i = 0; i < height; ++i) {
        dest[i] = sprite[acc >> 8];
        acc += step;
    }

    *(volatile unsigned char *)(SIZEP0 + player) = size;
    *(volatile unsigned char *)(HPOSP0 + player) =
        (unsigned char)(48 + left);
}

static void add_candidate(unsigned int dist, unsigned char sx,
                          unsigned char top, unsigned char height)
{
    unsigned char slot;

    if (cand_count == PMG_TARGET_PLAYERS && dist >= cand_dist[PMG_TARGET_PLAYERS - 1]) return;

    slot = cand_count < PMG_TARGET_PLAYERS ? cand_count : (unsigned char)(PMG_TARGET_PLAYERS - 1);
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
    if (cand_count < PMG_TARGET_PLAYERS) ++cand_count;
}

static unsigned char project_object(unsigned int px, unsigned int py,
                                    unsigned int angle, unsigned int object_x,
                                    unsigned int object_y)
{
    unsigned char idx;
    unsigned char column;
    unsigned char height;
    unsigned char top;
    int cos_h;
    int sin_h;
    int dx;
    int dy;
    int forward;
    int right;
    int limit;
    int screen_x;

    idx = (unsigned char)(angle >> 8);
    cos_h = sin3d[(unsigned char)(idx + 64)] >> 1;
    sin_h = sin3d[idx] >> 1;
    dx = ((int)object_x - (int)px) >> 4;
    dy = ((int)object_y - (int)py) >> 4;
    if (dx > RANGE_44 || dx < -RANGE_44 || dy > RANGE_44 || dy < -RANGE_44)
        return 0;

    forward = ((dx * cos_h) >> 7) + ((dy * sin_h) >> 7);
    /* Anything behind the camera must be rejected even if the object is only
     * slightly off the direct forward axis; the coarse projection test is not
     * precise enough to keep a stale billboard from appearing when the player
     * turns away. */
    if (forward <= 0) return 0;
    if (forward <= 8) return 0;
    right = ((-dx * sin_h) >> 7) + ((dy * cos_h) >> 7);
    limit = (forward * 5) >> 3;
    if (right > limit || right < -limit) return 0;

    screen_x = VIEW_CENTER_PX + ((right * PROJ_X) / forward);
    if (screen_x < VIEW_LEFT_PX || screen_x > VIEW_RIGHT_PX) return 0;
    column = (unsigned char)((screen_x - VIEW_LEFT_PX) >> 3);
    if (column >= VIEW_COLS) return 0;

    projected_dist = (unsigned int)forward << 4;
    if (projected_dist > col_dist[column]) return 0;
    height = view3d_wall_height(projected_dist);
    top = height >= VIEW_ROWS ? 0 : (unsigned char)((VIEW_ROWS - height) >> 1);
    if (height > VIEW_ROWS) height = VIEW_ROWS;
    projected_x = (unsigned char)screen_x;
    projected_top = top;
    projected_height = height;
    return 1;
}

void sprite3d_draw_decoy(unsigned char active, unsigned int px, unsigned int py,
                         unsigned int angle, unsigned int decoy_x,
                         unsigned int decoy_y)
{
    unsigned char *dest;
    unsigned char i;
    unsigned char height;
    unsigned char size;
    unsigned char missile_width;
    unsigned char width;
    unsigned int acc;
    unsigned int step;
    int left;

    clear_decoy();
    if (!active || !project_object(px, py, angle, decoy_x, decoy_y)) return;

    height = projected_height;
    if (height > 32) {
        height = 32;
        projected_top = (VIEW_ROWS - 32) >> 1;
    }
    if (height >= 24) {
        size = 0x55;
        missile_width = 4;
        width = 16;
    } else {
        size = 0;
        missile_width = 2;
        width = 8;
    }

    left = (int)projected_x - (width >> 1);
    if (left < VIEW_LEFT_PX) left = VIEW_LEFT_PX;
    if (left + width > VIEW_RIGHT_PX + 1)
        left = VIEW_RIGHT_PX + 1 - width;

    dest = pmg_ram + PMG_MISSILE_OFFSET + PMG_TOP_OFFSET + projected_top;
    step = ((unsigned int)8 << 8) / height;
    acc = 0;
    for (i = 0; i < height; ++i) {
        dest[i] = decoy_sprite[acc >> 8];
        acc += step;
    }

    *(volatile unsigned char *)0xD00C = size;
    for (i = 0; i < 4; ++i) {
        *(volatile unsigned char *)(0xD004 + i) =
            (unsigned char)(48 + left + i * missile_width);
    }
}

void sprite3d_draw_targets(unsigned int px, unsigned int py, unsigned int angle)
{
    unsigned char i;
    cand_count = 0;

    for (i = 0; i < target_count; ++i) {
        if (!target_active[i]) continue;
        if (project_object(px, py, angle, target_px[i], target_py[i]))
            add_candidate(projected_dist, projected_x, projected_top,
                          projected_height);
    }
    if (targets_left == 0 && maze_exit_found
        && project_object(px, py, angle, exit_px, exit_py))
        add_candidate(projected_dist, projected_x, projected_top, projected_height);

    /* When no targets remain, the only candidate this frame is the exit. */
    for (i = 0; i < cand_count; ++i) {
        draw_billboard((unsigned char)(i + 1), cand_x[i], cand_top[i],
                       cand_height[i],
                       targets_left == 0 ? exit_sprite : target_sprite, 16);
    }
    for (i = (unsigned char)(cand_count + 1); i <= last_used; ++i) {
        clear_player(i);
    }
    last_used = (unsigned char)(cand_count + 1);
}

void sprite3d_draw_pursuer(unsigned int px, unsigned int py, unsigned int angle,
                           unsigned int pursuer_x, unsigned int pursuer_y)
{
    int dx;
    int dy;
    int abs_dx;
    int abs_dy;
    int steps;
    int i;
    unsigned int sx;
    unsigned int sy;

    clear_player(0);
    *(volatile unsigned char *)SIZEP0 = 0;
    if (!project_object(px, py, angle, pursuer_x, pursuer_y)) return;

    /* The column-distance test above is only an approximation: it can miss a
     * wall that does not line up with the pursuer's own ray column, so a
     * real line-of-sight check confirms no wall actually lies in between. */
    dx = (int)pursuer_x - (int)px;
    dy = (int)pursuer_y - (int)py;
    abs_dx = dx < 0 ? -dx : dx;
    abs_dy = dy < 0 ? -dy : dy;
    steps = (abs_dx > abs_dy ? abs_dx : abs_dy) / 64;
    if (steps < 2) steps = 2;
    for (i = 1; i <= steps; ++i) {
        sx = (unsigned int)((int)px + dx * i / steps);
        sy = (unsigned int)((int)py + dy * i / steps);
        if (maze_solid((unsigned char)(sx >> 8), (unsigned char)(sy >> 8))) return;
    }

    if (projected_height > 32) {
        projected_height = 32;
        projected_top = (VIEW_ROWS - 32) >> 1;
    }
    draw_billboard(0, projected_x, projected_top, projected_height,
                   pursuer_sprite, 22);
}

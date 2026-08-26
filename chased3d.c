#include <atari.h>
#include "maze.h"
#include "trig3d.h"
#include "view3d.h"
#include "sprite3d.h"

#define KBD_SKSTAT 0xD20F
#define KBD_KBCODE 0xD209
#define RTCLOK_LOW 20   /* OS jiffy counter, bumped every vertical blank */

/* Movement is scaled by elapsed jiffies so speed does not change with the
 * render rate. The cap keeps a single step below one cell, which is what stops
 * the collision test from tunnelling through a wall. */
#define MOVE_PER_TICK 20
#define TURN_PER_TICK 400u
#define MAX_TICKS 6

static unsigned char frame_ticks = 1;
static unsigned int player_x;
static unsigned int player_y;
static unsigned int player_angle;

/* SKSTAT bit 2 clears while a key is held, which gives continuous movement. */
static unsigned char read_key(void)
{
    if ((*(volatile unsigned char *)KBD_SKSTAT & 0x04) != 0) return 0xFF;
    return *(volatile unsigned char *)KBD_KBCODE & 0x3F;
}

static void try_move(int delta_x, int delta_y)
{
    unsigned int next_x;
    unsigned int next_y;

    next_x = (unsigned int)((int)player_x + delta_x);
    next_y = (unsigned int)((int)player_y + delta_y);

    if (!maze_solid((unsigned char)(next_x >> 8), (unsigned char)(player_y >> 8)))
        player_x = next_x;
    if (!maze_solid((unsigned char)(player_x >> 8), (unsigned char)(next_y >> 8)))
        player_y = next_y;
}

static void step_forward(signed char direction)
{
    unsigned char idx;
    int dir_x;
    int dir_y;
    int step;

    step = (int)MOVE_PER_TICK * (int)frame_ticks;
    idx = (unsigned char)(player_angle >> 8);
    dir_x = sin3d[(unsigned char)(idx + 64)];
    dir_y = sin3d[idx];
    if (direction < 0) {
        dir_x = -dir_x;
        dir_y = -dir_y;
    }
    try_move((dir_x * step) >> 8, (dir_y * step) >> 8);
}

int main(void)
{
    unsigned char key;
    unsigned char ticks_per_second;
    unsigned char last_tick;
    unsigned char prev_tick;
    unsigned char now;
    unsigned char frames;

    maze_load_level(1);
    minimap_build();
    sprite3d_build_targets();

    /* Start in the open corridor at the top of the map, facing +X. */
    player_x = (1u << 8) | 0x80u;
    player_y = (1u << 8) | 0x80u;
    player_angle = 0;

    view3d_init();
    minimap_show();
    sprite3d_init();

    ticks_per_second = (get_tv() == AT_PAL) ? 50 : 60;
    last_tick = *(volatile unsigned char *)RTCLOK_LOW;
    prev_tick = last_tick;
    frames = 0;

    for (;;) {
        now = *(volatile unsigned char *)RTCLOK_LOW;
        frame_ticks = (unsigned char)(now - prev_tick);
        if (frame_ticks == 0) frame_ticks = 1;
        else if (frame_ticks > MAX_TICKS) frame_ticks = MAX_TICKS;
        prev_tick = now;

        key = read_key();
        if (key == KEY_ESC) break;
        if (key == KEY_W) step_forward(1);
        else if (key == KEY_S) step_forward(-1);
        else if (key == KEY_A) player_angle -= TURN_PER_TICK * frame_ticks;
        else if (key == KEY_D) player_angle += TURN_PER_TICK * frame_ticks;

        view3d_render(player_x, player_y, player_angle);
        sprite3d_draw_targets(player_x, player_y, player_angle);
        minimap_update(player_x, player_y, player_angle);

        ++frames;
        now = *(volatile unsigned char *)RTCLOK_LOW;
        if ((unsigned char)(now - last_tick) >= ticks_per_second) {
            hud_set_fps(frames);
            frames = 0;
            last_tick = now;
        }
        waitvsync();
    }
    return 0;
}

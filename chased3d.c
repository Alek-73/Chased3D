#include <atari.h>
#include <stdlib.h>
#include "maze.h"
#include "trig3d.h"
#include "view3d.h"
#include "sprite3d.h"
#include "melody.h"

#define KBD_SKSTAT 0xD20F
#define KBD_KBCODE 0xD209
#define POKEY_RANDOM 0xD20A
#define NOCLIK 0x02DB   /* non-zero silences the OS key click */
#define RTCLOK_LOW 20   /* OS jiffy counter, bumped every vertical blank */

/* Movement is scaled by elapsed jiffies so speed does not change with the
 * render rate. The cap keeps a single step below one cell, which is what stops
 * the collision test from tunnelling through a wall. */
#define MOVE_PER_TICK 20
#define TURN_PER_TICK 400u
#define MAX_TICKS 6
#define PURSUER_PER_TICK 8
#define THREAT_DISTANCE 4096
#define DECOY_CAPTURE_TICKS 40
#define DECOY_RECHARGE_TICKS 200
#define LASER_SECONDS 5

/* Same triplet string the original BASIC game plays when the pursuer catches
 * the player (line 305 of chased1V3.BAS). */
#define CAUGHT_MELODY "A14C28B18A14G14A11"
#define STARTING_LIVES 3

#define PLAYER_START_X ((1u << 8) | 0x80u)
#define PLAYER_START_Y ((1u << 8) | 0x80u)
#define PLAYER_START_ANGLE 0
#define PURSUER_START_X ((18u << 8) | 0x80u)
#define PURSUER_START_Y ((57u << 8) | 0x80u)
#define LEVEL_MAX 5

/* Same melodies as chased1V3.BAS lines 460 and 464: a level-clear jingle
 * followed by the loading tune while the next level is prepared. */
#define LEVEL_CLEAR_MELODY "C12C14E18B08C14A14G18F14E18D18E14D14C14P01"
#define LEVEL_LOAD_MELODY "C14C14E14C18F18C18E18C14D14P04"
#define DECOY_DEPLOY_MELODY "C21G21C22"
#define DECOY_TRAPPED_MELODY "G11D11G02"
#define DECOY_READY_MELODY "C21E21G21C22"

static unsigned char frame_ticks = 1;
static unsigned char lives = STARTING_LIVES;
static unsigned char level = 1;
static unsigned int player_x;
static unsigned int player_y;
static unsigned int player_angle;
static unsigned int pursuer_x;
static unsigned int pursuer_y;
static unsigned int decoy_x;
static unsigned int decoy_y;
static unsigned char decoy_active;
static unsigned char decoy_available = 1;
static unsigned char decoy_capture_ticks;
static unsigned char decoy_recharge_ticks = DECOY_RECHARGE_TICKS;
static unsigned int laser_elapsed_ticks;
static unsigned int laser_period_ticks;

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

/* Ticks before a stale target is abandoned, matching the original's
 * TARGETCNT threshold (chased1V3.BAS line 1004). */
#define RETARGET_STALE_TICKS 50

static signed char pursuer_dir_x = 0;
static signed char pursuer_dir_y = 0;
static unsigned char pursuer_target_col;
static unsigned char pursuer_target_row;
static unsigned char pursuer_retarget_timer = RETARGET_STALE_TICKS + 1;

/* Aims at a snapshot of the player's tile rather than their live position,
 * and keeps heading that way until it goes stale, is reached, or is blocked -
 * same navigation algorithm as chased1V3.BAS lines 900-1022. */
static void move_pursuer(void)
{
    static const signed char dir_x[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const signed char dir_y[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    unsigned char col;
    unsigned char row;
    unsigned char next_col;
    unsigned char next_row;
    unsigned char need_retarget;
    unsigned char i;
    unsigned char best;
    int best_distance;
    int distance;
    int candidate_x;
    int candidate_y;

    col = (unsigned char)(pursuer_x >> 8);
    row = (unsigned char)(pursuer_y >> 8);

    if (decoy_active
        && col == (unsigned char)(decoy_x >> 8)
        && row == (unsigned char)(decoy_y >> 8)) {
        if (decoy_capture_ticks == 0) melody_play(DECOY_TRAPPED_MELODY);
        decoy_capture_ticks += frame_ticks;
        if (decoy_capture_ticks < DECOY_CAPTURE_TICKS) return;
        decoy_active = 0;
        decoy_available = 0;
        decoy_capture_ticks = 0;
        decoy_recharge_ticks = 0;
        pursuer_retarget_timer = RETARGET_STALE_TICKS + 1;
    }

    pursuer_retarget_timer += frame_ticks;
    need_retarget = pursuer_retarget_timer > RETARGET_STALE_TICKS;
    if (col == pursuer_target_col && row == pursuer_target_row) need_retarget = 1;
    if (!need_retarget && (pursuer_dir_x != 0 || pursuer_dir_y != 0)) {
        next_col = (unsigned char)(col + pursuer_dir_x);
        next_row = (unsigned char)(row + pursuer_dir_y);
        if (maze_solid(next_col, row) || maze_solid(col, next_row)) need_retarget = 1;
    }

    if (need_retarget) {
        pursuer_target_col = (unsigned char)(
            (decoy_active ? decoy_x : player_x) >> 8);
        pursuer_target_row = (unsigned char)(
            (decoy_active ? decoy_y : player_y) >> 8);
        pursuer_retarget_timer = 0;

        best = 0;
        best_distance = 32767;
        for (i = 0; i < 8; ++i) {
            next_col = (unsigned char)(col + dir_x[i]);
            next_row = (unsigned char)(row + dir_y[i]);
            if (maze_solid(next_col, row) || maze_solid(col, next_row)) continue;
            distance = abs((int)next_col - (int)pursuer_target_col)
                     + abs((int)next_row - (int)pursuer_target_row);
            if (distance < best_distance) {
                best_distance = distance;
                best = i;
            }
        }
        pursuer_dir_x = dir_x[best];
        pursuer_dir_y = dir_y[best];
    }

    candidate_x = (int)pursuer_x + pursuer_dir_x * PURSUER_PER_TICK * frame_ticks;
    candidate_y = (int)pursuer_y + pursuer_dir_y * PURSUER_PER_TICK * frame_ticks;
    if (!maze_solid((unsigned char)(candidate_x >> 8), row))
        pursuer_x = (unsigned int)candidate_x;
    if (!maze_solid((unsigned char)(pursuer_x >> 8), (unsigned char)(candidate_y >> 8)))
        pursuer_y = (unsigned int)candidate_y;
}

/* Same maze cell as the original BASIC game's tile-equality catch test. */
static unsigned char pursuer_caught_player(void)
{
    return (player_x >> 8) == (pursuer_x >> 8)
        && (player_y >> 8) == (pursuer_y >> 8);
}

static void deploy_decoy(void)
{
    if (!decoy_available) return;
    decoy_x = player_x;
    decoy_y = player_y;
    decoy_active = 1;
    decoy_available = 0;
    decoy_capture_ticks = 0;
    decoy_recharge_ticks = 0;
    pursuer_retarget_timer = RETARGET_STALE_TICKS + 1;
    melody_play(DECOY_DEPLOY_MELODY);
}

static void update_decoy_recharge(void)
{
    unsigned int next;

    if (!decoy_active && decoy_recharge_ticks < DECOY_RECHARGE_TICKS) {
        next = (unsigned int)decoy_recharge_ticks + frame_ticks;
        decoy_recharge_ticks = next >= DECOY_RECHARGE_TICKS
            ? DECOY_RECHARGE_TICKS : (unsigned char)next;
        if (decoy_recharge_ticks >= DECOY_RECHARGE_TICKS)
            melody_play(DECOY_READY_MELODY);
    }
    if (!decoy_active && decoy_recharge_ticks >= DECOY_RECHARGE_TICKS)
        decoy_available = 1;
    hud_set_decoy(decoy_recharge_ticks, DECOY_RECHARGE_TICKS);
}

static void update_laser(void)
{
    laser_elapsed_ticks += frame_ticks;
    if (laser_elapsed_ticks < laser_period_ticks) return;
    laser_elapsed_ticks = 0;
    sprite3d_build_laser(player_x, player_y);
    melody_laser_buzz();
}

/* Repositions player and pursuer to their starting spots, used both after a
 * catch and after finishing a level. */
static void reset_positions(void)
{
    player_x = PLAYER_START_X;
    player_y = PLAYER_START_Y;
    player_angle = PLAYER_START_ANGLE;
    pursuer_x = PURSUER_START_X;
    pursuer_y = PURSUER_START_Y;
    pursuer_dir_x = 0;
    pursuer_dir_y = 0;
    pursuer_retarget_timer = RETARGET_STALE_TICKS + 1;
    decoy_active = 0;
    decoy_available = 1;
    decoy_capture_ticks = 0;
    decoy_recharge_ticks = DECOY_RECHARGE_TICKS;
    sprite3d_clear_all();
}

static void handle_catch(void)
{
    melody_set_threat_level(0);
    melody_play(CAUGHT_MELODY);
    while (melody_playing()) waitvsync();

    if (--lives == 0) lives = STARTING_LIVES;

    reset_positions();
}

static void update_threat_sound(void)
{
    int distance;
    unsigned char level_signal;

    distance = abs((int)player_x - (int)pursuer_x)
             + abs((int)player_y - (int)pursuer_y);
    if (distance >= THREAT_DISTANCE) {
        level_signal = 0;
    } else {
        level_signal = (unsigned char)((THREAT_DISTANCE - distance) >> 8);
        if (level_signal == 0) level_signal = 1;
        if (level_signal > 15) level_signal = 15;
    }
    melody_set_threat_level(level_signal);
}

/* Reached once all targets are collected and the exit becomes walkable;
 * mirrors chased1V3.BAS lines 460-469. */
static void handle_level_clear(void)
{
    melody_set_threat_level(0);
    melody_play(LEVEL_CLEAR_MELODY);
    while (melody_playing()) waitvsync();

    level = (level >= LEVEL_MAX) ? 1 : (unsigned char)(level + 1);
    maze_load_level(level);
    minimap_build();
    minimap_show();
    sprite3d_build_targets();
    sprite3d_build_laser(player_x, player_y);
    sprite3d_locate_exit();
    hud_set_targets(sprite3d_targets_left());

    melody_play(LEVEL_LOAD_MELODY);
    while (melody_playing()) waitvsync();

    reset_positions();
    laser_elapsed_ticks = 0;
    melody_laser_buzz();
}

int main(void)
{
    unsigned char key;
    unsigned char ticks_per_second;
    unsigned char last_tick;
    unsigned char prev_tick;
    unsigned char now;
    unsigned char frames;
    unsigned char previous_key;

    srand(((unsigned int)*(volatile unsigned char *)POKEY_RANDOM << 8)
          | *(volatile unsigned char *)RTCLOK_LOW);
    maze_load_level(1);
    minimap_build();
    sprite3d_build_targets();
    sprite3d_build_laser(PLAYER_START_X, PLAYER_START_Y);
    sprite3d_locate_exit();

    /* Start in the open corridor at the top of the map, facing +X. */
    player_x = PLAYER_START_X;
    player_y = PLAYER_START_Y;
    player_angle = PLAYER_START_ANGLE;
    pursuer_x = PURSUER_START_X;
    pursuer_y = PURSUER_START_Y;

    view3d_init();
    minimap_show();
    sprite3d_init();
    melody_install();
    melody_threat_play("A02A02E12A02A02E12");
    melody_laser_buzz();
    *(volatile unsigned char *)NOCLIK = 1;
    hud_set_targets(sprite3d_targets_left());
    hud_set_decoy(decoy_recharge_ticks, DECOY_RECHARGE_TICKS);

    ticks_per_second = (get_tv() == AT_PAL) ? 50 : 60;
    laser_period_ticks = (unsigned int)ticks_per_second * LASER_SECONDS;
    laser_elapsed_ticks = 0;
    last_tick = *(volatile unsigned char *)RTCLOK_LOW;
    prev_tick = last_tick;
    frames = 0;
    previous_key = 0xFF;

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
        else if (key == KEY_SPACE && previous_key != KEY_SPACE) deploy_decoy();
        previous_key = key;

        update_decoy_recharge();
        update_laser();
        move_pursuer();
        update_threat_sound();
        if (pursuer_caught_player() || sprite3d_hit_laser(player_x, player_y))
            handle_catch();
        if (sprite3d_collect(player_x, player_y)) {
            hud_set_targets(sprite3d_targets_left());
            melody_pickup();
        }
        if (sprite3d_targets_left() == 0) maze_set_exit_open(1);
        if (sprite3d_reached_exit(player_x, player_y)) handle_level_clear();

        view3d_render(player_x, player_y, player_angle);
        sprite3d_draw_targets(player_x, player_y, player_angle);
        sprite3d_draw_pursuer(player_x, player_y, player_angle,
                      pursuer_x, pursuer_y);
        sprite3d_draw_decoy(decoy_active, player_x, player_y, player_angle,
                    decoy_x, decoy_y);
        minimap_update(player_x, player_y, player_angle,
                   pursuer_x, pursuer_y);

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

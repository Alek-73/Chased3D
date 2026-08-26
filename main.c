#include <atari.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include "hardware.h"
#include "utils.h"
#include "melody.h"
#include "collision.h"
#include "pmg.h"
#include "display.h"
#include "charset.h"

#define MAP_W 20
#define MAP_H 59
#define TRACK_ROWS (MAP_H + 1)
#define SCREEN_W 20
#define SCREEN_H 20
#define PMG_DEBUG_ONLY 0

static unsigned char level_map[MAP_H][MAP_W];
static unsigned char screen_map[SCREEN_H][SCREEN_W];
static unsigned char *track_buffer;
volatile unsigned char game_dli_phase;
static const unsigned char level1_data[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
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
static unsigned char player_x;
static unsigned char player_y;
static unsigned char pursuer_x;
static unsigned char pursuer_y;
static signed char pursuer_dx;
static unsigned char fine_scroll;
static signed char pursuer_dy;
static unsigned char scroll_row;
static unsigned char decoy_x;
static unsigned char decoy_y;
static unsigned char decoy_active;
static unsigned char lay_decoy;
static unsigned char targets;
static unsigned char gate_open;
static unsigned char lives;
static unsigned char level;
static unsigned int score;
static unsigned int high_score;
static unsigned char captured;
static unsigned char level_transition_pending;
static unsigned char zapper;
static unsigned char zapper_counter;

static unsigned char p0_min_y;
static unsigned char p0_max_y;
static unsigned char p1_min_y;
static unsigned char p1_max_y;
static unsigned char p2_min_y;
static unsigned char p2_max_y;
static unsigned char p0_draw_y;
static unsigned char p1_draw_y;
static unsigned char p2_draw_y;

extern void game_dli_handler(void);
extern void Rainbow(void);

static void load_level(unsigned char requested_level);

static void allocate_track_buffer(void)
{
    unsigned char *raw_buffer;
    unsigned int address;

    raw_buffer = (unsigned char *)malloc(TRACK_ROWS * MAP_W + 0x0FFF);
    if (raw_buffer == 0) exit(EXIT_FAILURE);
    address = (unsigned int)raw_buffer;
    address = (unsigned int)((address + 0x0FFF) & 0xF000);
    track_buffer = (unsigned char *)address;
}

static void splash_screen(void)
{
    display_clear_screen();
    display_write_line(5, 7, "chased");
    display_write_line(7, 3, "BY ALEX VIROLI");
    display_write_line(9, 4, "FORLI' 2026");
    display_write_line(12, 4, "REVISION 1V3");
    display_write_line(14, 1, "LOADING.");
    load_level(1);
    display_write_line(14, 1, "LOADING...");
    display_write_line(14, 5, "Press FIRE");
    Rainbow();
    display_clear_screen();
}

static void clear_map(void)
{
    unsigned char row;
    unsigned char col;
    for (row = 0; row < MAP_H; ++row) {
        for (col = 0; col < MAP_W; ++col) {
            level_map[row][col] = 0;
            if (row == 0 || row == MAP_H - 1 || col == 0 || col == MAP_W - 1)
                level_map[row][col] = 1;
        }
    }
}

static void copy_level_data(const unsigned char source[MAP_H][MAP_W])
{
    unsigned char row;
    unsigned char col;
    for (row = 0; row < MAP_H; ++row)
        for (col = 0; col < MAP_W; ++col)
            level_map[row][col] = source[row][col];
}

static void load_level(unsigned char requested_level)
{
    FILE *file;
    const char *level_name;
    unsigned char row;
    unsigned char col;
    int value;
    int ch;

    clear_map();
    if (requested_level == 1) {
        copy_level_data(level1_data);
        return;
    }
    if (requested_level == 2) level_name = "D:L2.CSV";
    else if (requested_level == 3) level_name = "D:L3.CSV";
    else if (requested_level == 4) level_name = "D:L4.CSV";
    else level_name = "D:L5.CSV";
    file = fopen(level_name, "r");
    if (file == 0) return;
    do { ch = fgetc(file); } while (ch != EOF && ch != '\n' && ch != '\r');
    if (ch == '\r') ch = fgetc(file);
    for (row = 0; row < MAP_H; ++row) {
        do { ch = fgetc(file); } while (ch != EOF && ch != ';');
        if (ch == EOF) { fclose(file); return; }
        for (col = 0; col < MAP_W; ++col) {
            do { ch = fgetc(file); } while (ch != EOF && (ch < '0' || ch > '6'));
            if (ch == EOF) { fclose(file); return; }
            value = ch - '0';
            level_map[row][col] = (unsigned char)value;
        }
    }
    fclose(file);
}

static void copy_view(void)
{
    unsigned char row;
    unsigned char col;
    for (row = 0; row < SCREEN_H; ++row)
        for (col = 0; col < SCREEN_W; ++col)
            screen_map[row][col] = level_map[scroll_row + row][col];
}

static unsigned char tile_char(unsigned char tile)
{
    if (tile == 0) return 0;
    if (tile == 1) return 3;
    if (tile == 2) return 4;
    if (tile == 3) return 69;
    if (tile == 4) return 70;
    if (tile == 6) return 200;
    return 0;
}

static void draw_view(void)
{
    unsigned char row;
    unsigned char col;
    for (row = 0; row < MAP_H; ++row)
        for (col = 0; col < MAP_W; ++col)
            track_buffer[(unsigned int)row * MAP_W + col] = tile_char(level_map[row][col]);
    for (row = MAP_H; row < TRACK_ROWS; ++row)
        for (col = 0; col < MAP_W; ++col)
            track_buffer[(unsigned int)row * MAP_W + col] = 0;
}

static void draw_status(void)
{
    unsigned char *screen;
    unsigned char i;
    unsigned char p0p1;
    unsigned char p1pf;
    char status[21];
    screen = (unsigned char *)OS.savmsc;
    p0p1 = *(volatile unsigned char *)P1PL & 0x01;
    p1pf = game_p1pf_value;
    snprintf(status, sizeof(status), "P0-P1:%u P1-PF:%u", p0p1, p1pf);
    for (i = 0; i < 20; ++i) {
        if (status[i] == '\0') break;
        screen[i] = ascii_to_screen((unsigned char)status[i]);
    }
    while (i < 20) screen[i++] = 0;
}

static void draw_decoy_character(void)
{
    signed char column;
    signed char row;
    unsigned char *screen;
    if (!decoy_active || decoy_x < 48) return;
    column = (signed char)(decoy_x / 8 - 6);
    row = (signed char)(decoy_y / 4 - 5);
    if (column < 0 || column >= SCREEN_W || row < 0 || row >= SCREEN_H) return;
    screen = track_buffer != 0 ? track_buffer : (unsigned char *)OS.savmsc;
    screen[(unsigned int)row * SCREEN_W + (unsigned char)column] = 138;
}

static void reset_round(void)
{
    pmg_clear();
    player_x = 124; player_y = 30;
    pursuer_x = 56; pursuer_y = 30;
    pursuer_dx = 1; pursuer_dy = 0;
    scroll_row = 0; fine_scroll = 0;
    decoy_active = 0; lay_decoy = 0; captured = 0;
    level_transition_pending = 0;
    zapper = 1; zapper_counter = 40; targets = 10; gate_open = 0;
    p0_min_y = 0; p0_max_y = 127; p1_min_y = 0; p1_max_y = 127;
    p2_min_y = 0; p2_max_y = 127;
    p0_draw_y = 0; p1_draw_y = 0; p2_draw_y = 0;
    draw_view();
}

static unsigned char tile_at(unsigned char x, unsigned char y)
{
    signed int col;
    signed int row;
    col = (signed int)(x / 8) - 6;
    row = (signed int)(y / 4) - 5 + scroll_row;
    if (col < 0 || col >= MAP_W || row < 0 || row >= MAP_H) return 1;
    return level_map[row][col];
}

static void move_player(void)
{
    unsigned char joystick;
    joystick = *(volatile unsigned char *)JOY_PORT;
    if (joystick == 0 || joystick == 15) return;
    if (joystick == 5 || joystick == 6 || joystick == 7) ++player_x;
    if (joystick == 9 || joystick == 10 || joystick == 11) --player_x;
    if (joystick == 6 || joystick == 10 || joystick == 14) --player_y;
    if (joystick == 5 || joystick == 9 || joystick == 13) ++player_y;
    clamp_player_x(&player_x);
    if (player_y < 40 && (scroll_row > 0 || fine_scroll != 0)) {
        player_y = 40;
        if (fine_scroll == 0) {
            fine_scroll = 14; ++pursuer_y;
            if (decoy_active) ++decoy_y;
            display_set_track_scroll(scroll_row, fine_scroll);
        } else if (fine_scroll > 8) {
            fine_scroll = (unsigned char)(fine_scroll - 2); ++pursuer_y;
            if (decoy_active) ++decoy_y;
            if (fine_scroll == 8) { --scroll_row; fine_scroll = 0; }
            display_set_track_scroll(scroll_row, fine_scroll);
        }
    } else if (player_y > 70 && !(scroll_row == MAP_H - SCREEN_H && fine_scroll == 0)) {
        if (scroll_row < MAP_H - SCREEN_H) {
            player_y = 70;
            if (fine_scroll < 6) {
                fine_scroll = (unsigned char)(fine_scroll + 2); --pursuer_y;
                if (decoy_active) --decoy_y;
                display_set_track_scroll(scroll_row, fine_scroll);
            } else {
                ++scroll_row; fine_scroll = 0; --pursuer_y;
                if (decoy_active) --decoy_y;
                display_set_track_scroll(scroll_row, fine_scroll);
            }
        } else player_y = 70;
    }
    if (player_y < get_player_y_min(scroll_row, fine_scroll)) player_y = get_player_y_min(scroll_row, fine_scroll);
    if (player_y > get_player_y_max(scroll_row, fine_scroll, MAP_H - SCREEN_H)) player_y = get_player_y_max(scroll_row, fine_scroll, MAP_H - SCREEN_H);
    if (*(volatile unsigned char *)FIRE_PORT == 0 && !decoy_active) { decoy_x = player_x; decoy_y = player_y; lay_decoy = 1; }
    if (*(volatile unsigned char *)FIRE_PORT != 0 && lay_decoy) { lay_decoy = 0; decoy_active = 1; captured = 0; }
}

static void move_pursuer(void)
{
    static const signed char nav_x[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const signed char nav_y[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    unsigned char target_x, target_y, index, next_tile;
    signed int pursuer_col, pursuer_row, target_col, target_row;
    signed int candidate_x, candidate_y, distance, best_distance;
    target_x = decoy_active ? decoy_x : player_x;
    target_y = decoy_active ? decoy_y : player_y;
    pursuer_col = (signed int)(pursuer_x / 8) - 6;
    pursuer_row = (signed int)(pursuer_y / 4) - 5 + scroll_row;
    target_col = (signed int)(target_x / 8) - 6;
    target_row = (signed int)(target_y / 4) - 5 + scroll_row;
    pursuer_dx = 0; pursuer_dy = 0; best_distance = 32767;
    for (index = 0; index < 8; ++index) {
        candidate_x = pursuer_col + nav_x[index];
        candidate_y = pursuer_row + nav_y[index];
        next_tile = tile_at((unsigned char)((candidate_x + 6) * 8), (unsigned char)((candidate_y - scroll_row + 5) * 4));
        if (next_tile == 1 || next_tile == 3 || next_tile == 4) continue;
        distance = abs(target_col - candidate_x) + abs(target_row - candidate_y);
        if (distance < best_distance) { best_distance = distance; pursuer_dx = nav_x[index]; pursuer_dy = nav_y[index]; }
    }
    pursuer_x = (unsigned char)(pursuer_x + pursuer_dx);
    pursuer_y = (unsigned char)(pursuer_y + pursuer_dy);
    clamp_pursuer_x(&pursuer_x); clamp_pursuer_y(&pursuer_y);
}

static void check_targets(void)
{
    static const signed char target_offset[9] = {-21, -20, -19, -1, 0, 1, 19, 20, 21};
    signed int row, col, target_position, pair_position;
    unsigned char index, tile;
    row = (signed int)(player_y / 4) - 5 + scroll_row;
    col = (signed int)(player_x / 8) - 6;
    for (index = 0; index < 9; ++index) {
        target_position = row * MAP_W + col + target_offset[index];
        if (target_position < 0 || target_position >= MAP_H * MAP_W) continue;
        tile = track_buffer[target_position];
        if (tile != 69 && tile != 70) continue;
        level_map[target_position / MAP_W][target_position % MAP_W] = 0;
        track_buffer[target_position] = 0;
        pair_position = target_position + (tile == 69 ? 20 : -20);
        if (pair_position >= 0 && pair_position < MAP_H * MAP_W) {
            level_map[pair_position / MAP_W][pair_position % MAP_W] = 0;
            track_buffer[pair_position] = 0;
        }
        if (targets > 0) --targets;
        score += 50; play_event_sound(0);
        if (targets == 9 && !gate_open) gate_open = 1;
        break;
    }
}

static void update_screen(void)
{
    unsigned char new_p0_draw_y, new_player_draw_y;
    ANTIC.dmactl = OS.sdmctl; GTIA_WRITE.gractl = 3;
    new_p0_draw_y = pursuer_y; new_player_draw_y = player_y;
    pmg_draw(0, 2, pursuer_x, new_p0_draw_y);
    pmg_draw(1, 4, player_x, new_player_draw_y);
    pmg_draw(2, 1, player_x, new_player_draw_y);
    p0_draw_y = new_p0_draw_y; p1_draw_y = new_player_draw_y; p2_draw_y = new_player_draw_y;
    p0_min_y = pursuer_y; p0_max_y = (unsigned char)(pursuer_y + PLAYER_HEIGHT - 1);
    p1_min_y = player_y; p1_max_y = (unsigned char)(player_y + PLAYER_HEIGHT - 1);
    p2_min_y = player_y; p2_max_y = (unsigned char)(player_y + PLAYER_HEIGHT - 1);
    draw_decoy_character(); draw_status();
}

static void lose_life(void)
{
    if (lives > 0) --lives;
    play_event_sound(3); decoy_active = 0; reset_round();
}

static void pmg_debug_loop(void)
{
    *(volatile unsigned char *)HPOSP0 = 56; *(volatile unsigned char *)HPOSP1 = 124; *(volatile unsigned char *)HPOSP2 = 124;
    pmg_draw(0, 2, 56, 10); pmg_draw(1, 4, 124, 10); pmg_draw(2, 1, 124, 10);
    while (1) waitvsync();
}

int main(void)
{
    _graphics(17); allocate_track_buffer(); charset_install_custom(); display_install_game_list(); splash_screen();
    display_restore_default_list(); display_set_gameplay_colors(); display_select_game_screen(track_buffer); pmg_init(); display_install_dli();
#if PMG_DEBUG_ONLY
    pmg_debug_loop();
#endif
    lives = 3; level = 1; score = 0; high_score = 0; reset_round(); update_screen(); clear_gtia_collisions();
    start_melody("C12E12G12B12C22B12G12E12" "D12F12A12C22D12A12F12" "E12G12B12D22E12B12G12" "F12A12C22E12D12C12G14");
    while (lives != 0) {
        if (level_transition_pending) {
            if (!game_melody_active) {
                level_transition_pending = 0; ++level;
                if (level > 6) break;
                load_level(level); reset_round();
            }
        } else {
            move_player(); move_pursuer(); check_targets();
            if (decoy_active) {
                if (pursuer_x > decoy_x - 3 && pursuer_x < decoy_x + 3 && pursuer_y > decoy_y - 3 && pursuer_y < decoy_y + 3) {
                    if (captured == 0) score += 70;
                    ++captured;
                    if (captured > 40) { decoy_active = 0; captured = 0; }
                }
            }
            if (gate_open && player_y < 42) { start_melody("C14C14E14C18F18C18E18C14D14P04"); level_transition_pending = 1; }
            if (score > high_score) high_score = score;
        }
        update_screen(); waitvsync();
        if (!decoy_active && pursuer_hit_ugug()) { lose_life(); update_screen(); }
    }
    *(volatile unsigned char *)GRACTL = 0; display_restore_dli(); play_sound(0, 0, 0, 0); play_sound(1, 0, 0, 0); *(volatile unsigned char *)GRACTL = 0;
    clrscr(); cprintf("GAME OVER"); while (!kbhit()) { }
    return 0;
}

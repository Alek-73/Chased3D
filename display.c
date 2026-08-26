#include <atari.h>
#include "hardware.h"
#include "utils.h"
#include "display.h"

extern void game_dli_handler(void);
extern void game_vbi_handler(void);
extern volatile unsigned char game_dli_phase;
void (*game_saved_vvblkd)(void);

static unsigned int track_screen_address;
static unsigned char saved_dl[32];
static unsigned char saved_dl_valid;
static unsigned char saved_nmien;
static void (*saved_vdslst)(void);
static unsigned char *game_screen;

void display_install_dli(void)
{
    unsigned char *display_list;
    unsigned char index;

    display_list = OS.sdlst;
    saved_vdslst = OS.vdslst;
    game_saved_vvblkd = OS.vvblkd;
    saved_nmien = *(volatile unsigned char *)NMIEN;
    for (index = 10; index <= 28; ++index) {
        display_list[index] |= 0x80;
    }
    game_dli_phase = 0;
    OS.vdslst = game_dli_handler;
    OS.vvblkd = game_vbi_handler;
    *(volatile unsigned char *)NMIEN = NMIEN_DLI_VBI;
}

void display_restore_dli(void)
{
    *(volatile unsigned char *)NMIEN = 0;
    OS.vdslst = saved_vdslst;
    OS.vvblkd = game_saved_vvblkd;
    *(volatile unsigned char *)NMIEN = saved_nmien;
}

void display_clear_screen(void)
{
    unsigned char *screen;
    unsigned int index;

    screen = (unsigned char *)OS.savmsc;
    for (index = 0; index < 20 * 24; ++index) {
        screen[index] = 0;
    }
}

void display_write_line(unsigned char row, unsigned char column, const char *text)
{
    unsigned char *screen;
    unsigned char index;

    screen = (unsigned char *)OS.savmsc;
    for (index = 0; index < 20; ++index) {
        screen[(unsigned int)row * SCREEN_W + index] = 0;
    }
    index = 0;
    while (text[index] != '\0' && column + index < SCREEN_W) {
        screen[(unsigned int)row * SCREEN_W + column + index] =
            ascii_to_screen((unsigned char)text[index]);
        ++index;
    }
}

void display_install_game_list(void)
{
    unsigned char *display_list;
    unsigned char index;

    display_list = OS.sdlst;
    if (!saved_dl_valid) {
        for (index = 0; index < sizeof(saved_dl); ++index) {
            saved_dl[index] = display_list[index];
        }
        saved_dl_valid = 1;
    }
    *(volatile unsigned char *)710 = 0;
    GTIA_WRITE.colpf2 = 0;
    display_list[10] = 7;
    display_list[17] = 2;
    display_list[18] = 112;
    display_list[19] = 2;
}

void display_restore_default_list(void)
{
    unsigned char *display_list;
    unsigned char index;

    display_list = OS.sdlst;
    if (!saved_dl_valid) return;
    for (index = 0; index < sizeof(saved_dl); ++index) {
        display_list[index] = saved_dl[index];
    }
}

void display_set_gameplay_colors(void)
{
    *(volatile unsigned char *)708 = GAME_COLOR_PLAYFIELD_0;
    *(volatile unsigned char *)709 = GAME_COLOR_PLAYFIELD_1;
    *(volatile unsigned char *)710 = GAME_COLOR_PLAYFIELD_2;
    *(volatile unsigned char *)711 = GAME_COLOR_PLAYFIELD_3;
    *(volatile unsigned char *)712 = GAME_COLOR_BG;
    GTIA_WRITE.colpf0 = GAME_COLOR_PLAYFIELD_0;
    GTIA_WRITE.colpf1 = GAME_COLOR_PLAYFIELD_1;
    GTIA_WRITE.colpf2 = GAME_COLOR_PLAYFIELD_2;
    GTIA_WRITE.colpf3 = GAME_COLOR_PLAYFIELD_3;
    GTIA_WRITE.colbk = GAME_COLOR_BG;
}

void display_select_game_screen(unsigned char *track_buffer)
{
    unsigned char *display_list;
    unsigned int address;
    unsigned char index;
    unsigned char saved_dma;

    game_screen = track_buffer;
    address = (unsigned int)game_screen;
    track_screen_address = address;
    display_list = OS.sdlst;
    saved_dma = OS.sdmctl;
    OS.sdmctl = 0;
    ANTIC.dmactl = 0;
    ANTIC.vscrol = 0;
    display_list[3] = (unsigned char)(display_list[3] + 1);
    display_list[6] = 112;
    display_list[7] = 70;
    display_list[8] = (unsigned char)address;
    display_list[9] = (unsigned char)(address >> 8);
    display_list[7] |= 0x20;
    for (index = 10; index <= 28; ++index) {
        display_list[index] = (unsigned char)(display_list[index] + 32);
    }
    OS.sdmctl = saved_dma;
    ANTIC.dmactl = saved_dma;
}

void display_set_track_scroll(unsigned char row, unsigned char fine)
{
    unsigned char *display_list;
    unsigned int address;

    address = track_screen_address + (unsigned int)row * SCREEN_W;
    display_list = OS.sdlst;
    display_list[8] = (unsigned char)address;
    display_list[9] = (unsigned char)(address >> 8);
    ANTIC.vscrol = fine;
}

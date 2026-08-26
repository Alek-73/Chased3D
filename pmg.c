#include <atari.h>
#include "hardware.h"
#include "pmg.h"

static volatile unsigned char *pmg;
static unsigned char pmg_page;
static const unsigned char sprite1[PLAYER_HEIGHT] = {
    0, 0, 0, 0, 0, 0, 0, 24, 60, 60, 102, 102, 195, 195, 126, 0,
    0, 0, 0, 0, 0, 0
};
static const unsigned char sprite2[PLAYER_HEIGHT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 66, 153, 189, 255, 189, 153, 66, 0,
    0, 0, 0, 0, 0, 0
};
static const unsigned char sprite4[PLAYER_HEIGHT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 24, 40, 40, 0, 36,
    0, 0, 0, 0, 0, 0
};

static unsigned int player_base(unsigned char player)
{
    return (unsigned int)0x200 + (unsigned int)player * 0x80;
}

void pmg_clear(void)
{
    unsigned int index;

    for (index = 0; index < 0x400; ++index) {
        pmg[index] = 0;
    }
}

void pmg_draw(unsigned char player, unsigned char sprite_id,
              unsigned char x, unsigned char y)
{
    const unsigned char *sprite;
    unsigned int base;
    unsigned char row;

    if (y > 127) return;
    if (player == 0) *(volatile unsigned char *)HPOSP0 = x;
    else if (player == 1) *(volatile unsigned char *)HPOSP1 = x;
    else if (player == 2) *(volatile unsigned char *)HPOSP2 = x;
    if (sprite_id == 1) sprite = sprite1;
    else if (sprite_id == 2) sprite = sprite2;
    else sprite = sprite4;
    base = player_base(player);
    for (row = 0; row < PLAYER_HEIGHT && (unsigned int)y + row < 128; ++row) {
        pmg[base + y + row] = sprite[row];
    }
}

void pmg_init(void)
{
    unsigned char screen_page;

    screen_page = (unsigned char)((unsigned int)OS.savmsc >> 8);
    pmg_page = (unsigned char)((screen_page - 16) & 0xFC);
    ANTIC.pmbase = pmg_page;
    pmg = (volatile unsigned char *)((unsigned int)pmg_page << 8);
    pmg_clear();
    *(volatile unsigned char *)PMBASE = pmg_page;
    OS.sdmctl = 46;
    ANTIC.dmactl = OS.sdmctl;
    GTIA_WRITE.gractl = 3;
    /* Players should be in front of the playfield so P1PF reflects actual
     * overlap with the background graphics. */
    OS.gprior = PRIOR_P03_PF03;
    GTIA_WRITE.prior = OS.gprior;
    GTIA_WRITE.sizep0 = PMG_SIZE_NORMAL;
    GTIA_WRITE.sizep1 = PMG_SIZE_NORMAL;
    GTIA_WRITE.sizep2 = PMG_SIZE_NORMAL;
    GTIA_WRITE.sizep3 = PMG_SIZE_NORMAL;
    GTIA_WRITE.sizem = 0;
    OS.pcolr0 = GAME_COLOR_PLAYER_0;
    OS.pcolr1 = GAME_COLOR_PLAYER_1;
    OS.pcolr2 = GAME_COLOR_PLAYER_2;
    GTIA_WRITE.colpm0 = OS.pcolr0;
    GTIA_WRITE.colpm1 = OS.pcolr1;
    GTIA_WRITE.colpm2 = OS.pcolr2;
    *(volatile unsigned char *)COLBK = GAME_COLOR_BG;
    *(volatile unsigned char *)COLPM0 = GAME_COLOR_PLAYER_0;
    *(volatile unsigned char *)COLPM1 = GAME_COLOR_PLAYER_1;
    *(volatile unsigned char *)COLPM2 = GAME_COLOR_PLAYER_2;
}

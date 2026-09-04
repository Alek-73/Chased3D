#include <atari.h>
#include <fcntl.h>
#include <unistd.h>
#include "splash_screen.h"
#include "textplot.h"
#include "view3d.h"
#include "build_number.h"

#define STRINGIFY_VALUE(value) #value
#define STRINGIFY(value) STRINGIFY_VALUE(value)
#define SPLASH_REVISION "Rev1." STRINGIFY(BUILD_DIGIT_100) \
    STRINGIFY(BUILD_DIGIT_10) STRINGIFY(BUILD_DIGIT_1) " press Fire"

#define SPLASH_BITMAP_ROWS 46
#define SPLASH_BITMAP_TOP (VIEW_ROWS - SPLASH_BITMAP_ROWS)
#define SPLASH_BMP_DATA_OFFSET 70

#pragma bss-name (push, "DLIST")
static unsigned char splash_dlist[VIEW_ROWS + 8];
#pragma bss-name (pop)

static void splash_build_dlist(void)
{
    unsigned int screen_addr;
    unsigned int dlist_addr;
    unsigned char index;
    unsigned char row;

    screen_addr = (unsigned int)view_buffer;
    index = 0;
    splash_dlist[index++] = 0x70;
    splash_dlist[index++] = 0x70;
    splash_dlist[index++] = 0x70;
    splash_dlist[index++] = 0x4D;
    splash_dlist[index++] = (unsigned char)screen_addr;
    splash_dlist[index++] = (unsigned char)(screen_addr >> 8);
    for (row = 1; row < VIEW_ROWS; ++row) splash_dlist[index++] = 0x0D;
    dlist_addr = (unsigned int)splash_dlist;
    splash_dlist[index++] = 0x41;
    splash_dlist[index++] = (unsigned char)dlist_addr;
    splash_dlist[index] = (unsigned char)(dlist_addr >> 8);
}

static void splash_load_bitmap(void)
{
    int file;
    unsigned char *source;
    unsigned char *dest;
    unsigned char source_row;
    unsigned char byte;

    source = view_buffer;
    file = open("D:SPLASH.BMP", O_RDONLY);
    if (file < 0) return;
    if (read(file, source, SPLASH_BMP_DATA_OFFSET) != SPLASH_BMP_DATA_OFFSET
        || source[0] != 'B' || source[1] != 'M') goto done;

    for (source_row = 0; source_row < SPLASH_BITMAP_ROWS; ++source_row) {
        if (read(file, source, VIEW_STRIDE * 2) != VIEW_STRIDE * 2) goto done;
        dest = view_buffer
             + (unsigned int)(SPLASH_BITMAP_TOP + SPLASH_BITMAP_ROWS - 1
                              - source_row) * VIEW_STRIDE;
        for (byte = 0; byte < VIEW_STRIDE; ++byte) {
            dest[byte] = (unsigned char)(
                ((source[byte * 2] & 0x30) << 2)
                | ((source[byte * 2] & 0x03) << 4)
                | ((source[byte * 2 + 1] & 0x30) >> 2)
                | (source[byte * 2 + 1] & 0x03));
        }
    }

done:
    close(file);
    for (byte = 0; byte < VIEW_STRIDE * 2; ++byte) source[byte] = 0;
}

void splash_screen_show(void)
{
    unsigned int addr;

    OS.sdmctl = 0;
    ANTIC.dmactl = 0;
    for (addr = 0; addr < VIEW_STRIDE * VIEW_ROWS; ++addr)
        view_buffer[addr] = 0;

    splash_load_bitmap();
    textplot_print_fullscreen(TEXTPLOT_ALIGN_CENTER, "Chased3D", 7, 3,
                              TEXTPLOT_SIZE_DOUBLE);
    textplot_print_fullscreen(TEXTPLOT_ALIGN_CENTER,
                              "by Alex Viroli, 2026", 18, 2,
                              TEXTPLOT_SIZE_NORMAL);
    textplot_print_fullscreen(TEXTPLOT_ALIGN_CENTER,
                              SPLASH_REVISION, 35, 3, TEXTPLOT_SIZE_HALF);
    splash_build_dlist();

    COLOR0 = 0x8A;
    COLOR1 = 0xCA;
    COLOR2 = 0x1E;
    COLOR3 = 0x00;
    COLOR4 = 0x02;
    OS.sdlst = splash_dlist;
    OS.sdmctl = 0x22;
    ANTIC.dmactl = 0x22;
}


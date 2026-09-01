#include <atari.h>
#include <stdio.h>
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
    FILE *file;
    unsigned char *dest;
    unsigned char source_row;
    unsigned char byte;
    int first;
    int second;

    file = fopen("D:SPLASH.BMP", "rb");
    if (file == 0) return;
    if (fgetc(file) != 'B' || fgetc(file) != 'M') {
        fclose(file);
        return;
    }
    for (byte = 2; byte < SPLASH_BMP_DATA_OFFSET; ++byte) {
        if (fgetc(file) == EOF) {
            fclose(file);
            return;
        }
    }

    for (source_row = 0; source_row < SPLASH_BITMAP_ROWS; ++source_row) {
        dest = view_buffer
             + (unsigned int)(SPLASH_BITMAP_TOP + SPLASH_BITMAP_ROWS - 1
                              - source_row) * VIEW_STRIDE;
        for (byte = 0; byte < VIEW_STRIDE; ++byte) {
            first = fgetc(file);
            second = fgetc(file);
            if (first == EOF || second == EOF) {
                fclose(file);
                return;
            }
            dest[byte] = (unsigned char)(
                ((first & 0x30) << 2) | ((first & 0x03) << 4)
                | ((second & 0x30) >> 2) | (second & 0x03));
        }
    }
    fclose(file);
}

void splash_screen_show(void)
{
    unsigned int addr;

    OS.sdmctl = 0;
    ANTIC.dmactl = 0;
    for (addr = 0; addr < VIEW_STRIDE * VIEW_ROWS; ++addr)
        view_buffer[addr] = 0;

    splash_load_bitmap();
    textplot_print_fullscreen(TEXTPLOT_ALIGN_CENTER, "Chased3D", 7, 3, 2);
    textplot_print_fullscreen(TEXTPLOT_ALIGN_CENTER,
                              "by Alex Viroli, 2026", 18, 2, 1);
    textplot_print_fullscreen(TEXTPLOT_ALIGN_CENTER,
                              SPLASH_REVISION, 35, 3, 1);
    splash_build_dlist();

    COLOR0 = 0x3A;
    COLOR1 = 0xCA;
    COLOR2 = 0x1E;
    COLOR3 = 0x00;
    COLOR4 = 0x02;
    OS.sdlst = splash_dlist;
    OS.sdmctl = 0x22;
    ANTIC.dmactl = 0x22;
}


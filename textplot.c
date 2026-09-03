#include "textplot.h"
#include "view3d.h"

#define OS_CHARSET_BASE ((const unsigned char *)0xE000)
#define GLYPH_WIDTH 8
#define GLYPH_HEIGHT 8
#define COMPACT_GLYPH_WIDTH 4
#define COMPACT_GLYPH_HEIGHT 6

static const unsigned char compact_font[64][3] = {
    {0x00, 0x00, 0x00}, {0x44, 0x40, 0x40}, {0xAA, 0x00, 0x00}, {0xAF, 0xAA, 0xFA},
    {0x6F, 0xC7, 0xF6}, {0x92, 0x48, 0x90}, {0x4A, 0x4B, 0xA5}, {0x44, 0x00, 0x00},
    {0x24, 0x88, 0x42}, {0x84, 0x22, 0x48}, {0x0A, 0x4A, 0x00}, {0x04, 0xE4, 0x00},
    {0x00, 0x00, 0x48}, {0x00, 0xE0, 0x00}, {0x00, 0x00, 0x04}, {0x12, 0x24, 0x88},
    {0x69, 0xBD, 0x96}, {0x4C, 0x44, 0x4E}, {0x69, 0x16, 0x8F}, {0xE1, 0x61, 0x96},
    {0x26, 0xAF, 0x22}, {0xF8, 0xE1, 0x96}, {0x68, 0xE9, 0x96}, {0xF1, 0x24, 0x44},
    {0x69, 0x69, 0x96}, {0x69, 0x97, 0x16}, {0x04, 0x00, 0x40}, {0x04, 0x00, 0x48},
    {0x24, 0x84, 0x20}, {0x0E, 0x0E, 0x00}, {0x84, 0x24, 0x80}, {0x69, 0x24, 0x04},
    {0x69, 0xBB, 0x87}, {0x69, 0x9F, 0x99}, {0xE9, 0xE9, 0x9E}, {0x78, 0x88, 0x87},
    {0xE9, 0x99, 0x9E}, {0xF8, 0xE8, 0x8F}, {0xF8, 0xE8, 0x88}, {0x78, 0xB9, 0x97},
    {0x99, 0xF9, 0x99}, {0xE4, 0x44, 0x4E}, {0x11, 0x19, 0x96}, {0x9A, 0xCA, 0xA9},
    {0x88, 0x88, 0x8F}, {0x9F, 0xF9, 0x99}, {0x9D, 0xB9, 0x99}, {0x69, 0x99, 0x96},
    {0xE9, 0x9E, 0x88}, {0x69, 0x9B, 0xA5}, {0xE9, 0x9E, 0xA9}, {0x78, 0x61, 0x1E},
    {0xF4, 0x44, 0x44}, {0x99, 0x99, 0x96}, {0x99, 0x99, 0x66}, {0x99, 0x9F, 0xF9},
    {0x99, 0x66, 0x99}, {0x99, 0x64, 0x44}, {0xF1, 0x24, 0x8F}, {0xE8, 0x88, 0x8E},
    {0x84, 0x42, 0x11}, {0xE2, 0x22, 0x2E}, {0x4A, 0x00, 0x00}, {0x00, 0x00, 0x0F}
};

static unsigned char plot_color;
static unsigned char plot_y;
static unsigned char plot_size;
static unsigned char plot_compact;

static unsigned char atascii_to_screen_code(unsigned char character)
{
    if (character >= 32 && character <= 95)
        return character - 32;
    if (character >= 96 && character <= 127)
        return character;
    return 0;
}

static unsigned char atascii_to_compact_code(unsigned char character)
{
    if (character >= 'a' && character <= 'z') character -= 32;
    if (character >= 32 && character <= 95) return character - 32;
    return 0;
}

static void plot_text_pixel(unsigned char x, unsigned char y)
{
    unsigned char shift;
    unsigned char *dest;

    shift = (unsigned char)(6 - ((x & 3) << 1));
    dest = view_buffer + (unsigned int)y * VIEW_STRIDE + (x >> 2);
    *dest = (unsigned char)((*dest & ~(3 << shift)) | (plot_color << shift));
}

static void draw_character(unsigned char x, unsigned char character)
{
    const unsigned char *glyph;
    unsigned char row;
    unsigned char column;
    unsigned char bits;
    unsigned char scale_x;
    unsigned char scale_y;

    if (plot_compact) {
        glyph = &compact_font[atascii_to_compact_code(character)][0];
        for (row = 0; row < COMPACT_GLYPH_HEIGHT; ++row) {
            bits = glyph[row >> 1];
            bits = row & 1 ? bits & 0x0F : bits >> 4;
            for (column = 0; column < COMPACT_GLYPH_WIDTH; ++column) {
                if (bits & (0x08 >> column))
                    plot_text_pixel((unsigned char)(x + column),
                                    (unsigned char)(plot_y + row));
            }
        }
        return;
    }

    glyph = OS_CHARSET_BASE
          + (unsigned int)atascii_to_screen_code(character) * GLYPH_HEIGHT;
    for (row = 0; row < GLYPH_HEIGHT; ++row) {
        bits = glyph[row];
        for (column = 0; column < GLYPH_WIDTH; ++column) {
            if (bits & (0x80 >> column)) {
                for (scale_y = 0; scale_y < plot_size; ++scale_y) {
                    for (scale_x = 0; scale_x < plot_size; ++scale_x) {
                        plot_text_pixel((unsigned char)(x + column * plot_size + scale_x),
                            (unsigned char)(plot_y + row * plot_size + scale_y));
                    }
                }
            }
        }
    }
}

static void textplot_print_area(unsigned char alignment, const char *text,
                               unsigned char y, unsigned char color,
                               unsigned char size, unsigned char area_x,
                               unsigned char area_width)
{
    unsigned char length;
    unsigned char x;
    unsigned char index;
    unsigned char width;
    unsigned char char_width;
    unsigned char max_chars;
    unsigned char height;

    if (text == 0) return;
    plot_compact = size == TEXTPLOT_SIZE_HALF;
    if (plot_compact) size = 1;
    if (size == 0) size = 1;
    if (size > VIEW_ROWS / GLYPH_HEIGHT) size = VIEW_ROWS / GLYPH_HEIGHT;
    plot_size = size;
    char_width = plot_compact ? COMPACT_GLYPH_WIDTH
                              : GLYPH_WIDTH * plot_size;
    height = plot_compact ? COMPACT_GLYPH_HEIGHT
                          : GLYPH_HEIGHT * plot_size;
    max_chars = area_width / char_width;
    plot_y = y > VIEW_ROWS - height ? VIEW_ROWS - height : y;
    plot_color = color & 3;

    length = 0;
    while (text[length] != '\0' && length < max_chars) ++length;
    width = (unsigned char)(length * char_width);

    if (alignment == TEXTPLOT_ALIGN_RIGHT) {
        x = area_x + area_width - width;
    } else if (alignment == TEXTPLOT_ALIGN_CENTER) {
        x = area_x + (area_width - width) / 2;
    } else {
        x = area_x;
    }

    for (index = 0; index < length; ++index) {
        draw_character(x, (unsigned char)text[index]);
        x += char_width;
    }
}

void textplot_print_fullscreen(unsigned char alignment, const char *text,
                               unsigned char y, unsigned char color,
                               unsigned char size)
{
    textplot_print_area(alignment, text, y, color, size,
                        0, VIEW_STRIDE * 4);
}
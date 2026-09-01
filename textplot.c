#include "textplot.h"
#include "view3d.h"

#define OS_CHARSET_BASE ((const unsigned char *)0xE000)
#define GLYPH_WIDTH 8
#define GLYPH_HEIGHT 8

static unsigned char plot_color;
static unsigned char plot_y;
static unsigned char plot_size;

static unsigned char atascii_to_screen_code(unsigned char character)
{
    if (character >= 32 && character <= 95)
        return character - 32;
    if (character >= 96 && character <= 127)
        return character;
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
    if (size == 0) size = 1;
    if (size > VIEW_ROWS / GLYPH_HEIGHT) size = VIEW_ROWS / GLYPH_HEIGHT;
    plot_size = size;
    char_width = GLYPH_WIDTH * plot_size;
    height = GLYPH_HEIGHT * plot_size;
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

void textplot_print(unsigned char alignment, const char *text,
                    unsigned char y, unsigned char color, unsigned char size)
{
    textplot_print_area(alignment, text, y, color, size,
                        VIEW3D_X_PIXELS, VIEW3D_WIDTH_PIXELS);
}

void textplot_print_fullscreen(unsigned char alignment, const char *text,
                               unsigned char y, unsigned char color,
                               unsigned char size)
{
    textplot_print_area(alignment, text, y, color, size,
                        0, VIEW_STRIDE * 4);
}
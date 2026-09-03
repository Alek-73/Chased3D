#ifndef TEXTPLOT_H
#define TEXTPLOT_H

#define TEXTPLOT_ALIGN_LEFT   0
#define TEXTPLOT_ALIGN_CENTER 1
#define TEXTPLOT_ALIGN_RIGHT  2

#define TEXTPLOT_SIZE_HALF      0x80
#define TEXTPLOT_SIZE_NORMAL    1
#define TEXTPLOT_SIZE_DOUBLE    2
#define TEXTPLOT_SIZE_QUADRUPLE 4

void textplot_print_fullscreen(unsigned char alignment, const char *text,
						   unsigned char y, unsigned char color, unsigned char size);

#endif
#ifndef TEXTPLOT_H
#define TEXTPLOT_H

#define TEXTPLOT_ALIGN_LEFT   0
#define TEXTPLOT_ALIGN_CENTER 1
#define TEXTPLOT_ALIGN_RIGHT  2

void textplot_print(unsigned char alignment, const char *text,
					unsigned char y, unsigned char color, unsigned char size);

#endif
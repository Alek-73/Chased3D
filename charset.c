#include <atari.h>
#include "charset.h"

void charset_install_custom(void)
{
    static const unsigned char charset_data[] = {
        3, 0, 60, 60, 60, 60, 60, 60, 60,
        4, 170, 170, 85, 85, 170, 170, 85, 85,
        5, 225, 146, 76, 74, 49, 49, 77, 131,
        6, 128, 128, 128, 128, 128, 128, 255, 255,
        8, 0, 32, 80, 136, 5, 2, 0, 0,
        9, 24, 24, 60, 60, 126, 126, 24, 24,
        10, 18, 22, 24, 60, 60, 24, 104, 72,
        11, 24, 60, 60, 126, 126, 235, 235, 126
    };
    unsigned char *charset;
    unsigned int index;
    unsigned char row;
    unsigned char character;

    charset = (unsigned char *)(((unsigned int)OS.ramtop - 8) << 8);
    for (index = 0; index < 1024; ++index) {
        charset[index] = ((unsigned char *)0xE000)[index];
    }
    for (index = 0; index < sizeof(charset_data); index += 9) {
        character = charset_data[index];
        for (row = 0; row < 8; ++row) {
            charset[(unsigned int)character * 8 + row] = charset_data[index + row + 1];
        }
    }
    *(volatile unsigned char *)0x02F4 = (unsigned char)((unsigned int)charset >> 8);
}

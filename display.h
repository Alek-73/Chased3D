#ifndef GAME_DISPLAY_H
#define GAME_DISPLAY_H

void display_install_dli(void);
void display_restore_dli(void);
void display_clear_screen(void);
void display_write_line(unsigned char row, unsigned char column, const char *text);
void display_install_game_list(void);
void display_restore_default_list(void);
void display_set_gameplay_colors(void);
void display_select_game_screen(unsigned char *track_buffer);
void display_set_track_scroll(unsigned char row, unsigned char fine);

#endif

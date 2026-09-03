#ifndef VIEW3D_H
#define VIEW3D_H

#include "maze.h"

#define VIEW_COLS 18
#define VIEW_ROWS 92
#define VIEW_STRIDE 40
#define VIEW3D_X_PIXELS 20
#define VIEW3D_WIDTH_PIXELS (VIEW_STRIDE * 4 - VIEW3D_X_PIXELS)
#define DECOY_BAR_TOP (MAZE_H + 2)
#define DECOY_BAR_HEIGHT 6

#define COLOR0 (*(volatile unsigned char *)708)
#define COLOR1 (*(volatile unsigned char *)709)
#define COLOR2 (*(volatile unsigned char *)710)
#define COLOR3 (*(volatile unsigned char *)711)
#define COLOR4 (*(volatile unsigned char *)712)

void view3d_init(void);
void view3d_render(unsigned int px, unsigned int py, unsigned int angle);
void view3d_floor_motion(signed char direction);
unsigned char view3d_wall_height(unsigned int dist);
void minimap_build(void);
void minimap_show(void);
void minimap_update(unsigned int px, unsigned int py, unsigned int angle);
#ifdef DEBUG_HUD
void hud_set_fps(unsigned char fps);
void hud_set_targets(unsigned char remaining);
#endif
void hud_set_game(unsigned char lives, unsigned char level,
				  unsigned int score, unsigned int high_score);
void hud_set_decoy(unsigned char progress, unsigned char maximum);

/* Perpendicular wall distance per ray column; sprites depth test against it. */
extern unsigned int col_dist[VIEW_COLS];
extern unsigned char view_buffer[VIEW_STRIDE * VIEW_ROWS];

#endif

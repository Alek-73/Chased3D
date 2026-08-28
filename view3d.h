#ifndef VIEW3D_H
#define VIEW3D_H

#define VIEW_COLS 18
#define VIEW_ROWS 92

void view3d_init(void);
void view3d_render(unsigned int px, unsigned int py, unsigned int angle);
unsigned char view3d_wall_height(unsigned int dist);
void minimap_build(void);
void minimap_show(void);
void minimap_update(unsigned int px, unsigned int py, unsigned int angle,
					unsigned int pursuer_x, unsigned int pursuer_y);
void hud_set_fps(unsigned char fps);
void hud_set_targets(unsigned char remaining);
void hud_set_decoy(unsigned char progress, unsigned char maximum);

/* Perpendicular wall distance per ray column; sprites depth test against it. */
extern unsigned int col_dist[VIEW_COLS];

#endif

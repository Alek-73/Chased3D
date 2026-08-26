#ifndef VIEW3D_H
#define VIEW3D_H

#define VIEW_COLS 18
#define VIEW_ROWS 92

void view3d_init(void);
void view3d_render(unsigned int px, unsigned int py, unsigned int angle);
void minimap_build(void);
void minimap_show(void);
void minimap_update(unsigned int px, unsigned int py, unsigned int angle);
void hud_set_fps(unsigned char fps);

#endif

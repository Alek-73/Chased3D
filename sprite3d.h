#ifndef SPRITE3D_H_INCLUDED
#define SPRITE3D_H_INCLUDED

void sprite3d_init(void);
void sprite3d_build_targets(void);
void sprite3d_locate_exit(void);
void sprite3d_draw_targets(unsigned int px, unsigned int py, unsigned int angle);
void sprite3d_draw_pursuer(unsigned int px, unsigned int py, unsigned int angle,
						   unsigned int pursuer_x, unsigned int pursuer_y);
unsigned char sprite3d_collect(unsigned int px, unsigned int py);
unsigned char sprite3d_targets_left(void);
void sprite3d_clear_all(void);
unsigned char sprite3d_reached_exit(unsigned int px, unsigned int py);

#endif

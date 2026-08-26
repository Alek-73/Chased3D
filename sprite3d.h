#ifndef SPRITE3D_H_INCLUDED
#define SPRITE3D_H_INCLUDED

void sprite3d_init(void);
void sprite3d_build_targets(void);
void sprite3d_draw_targets(unsigned int px, unsigned int py, unsigned int angle);
unsigned char sprite3d_collect(unsigned int px, unsigned int py);
unsigned char sprite3d_targets_left(void);

#endif

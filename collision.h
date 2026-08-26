#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

extern volatile unsigned char game_collision_pending;
extern volatile unsigned char game_p1pf_value;

void clear_gtia_collisions(void);
unsigned char pursuer_hit_ugug(void);

#endif

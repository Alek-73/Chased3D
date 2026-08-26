#include "hardware.h"
#include "collision.h"

volatile unsigned char game_collision_pending;
volatile unsigned char game_p1pf_value;

void clear_gtia_collisions(void)
{
    *(volatile unsigned char *)HITCLR = 1;
}

unsigned char pursuer_hit_ugug(void)
{
    unsigned char collision;

    collision = game_collision_pending;
    game_collision_pending = 0;
    return collision;
}

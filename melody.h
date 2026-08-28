#ifndef MELODY_H_INCLUDED
#define MELODY_H_INCLUDED

void melody_install(void);
void melody_play(const char *melody);
void melody_pickup(void);
unsigned char melody_playing(void);
void melody_threat_play(const char *melody);
void melody_set_threat_level(unsigned char level);
void melody_laser_buzz(void);

#endif

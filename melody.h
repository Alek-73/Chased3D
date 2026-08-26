#ifndef GAME_MELODY_H
#define GAME_MELODY_H

extern volatile unsigned char game_melody_active;
extern volatile unsigned char game_melody_timer;
extern volatile unsigned char game_melody_index;
extern volatile unsigned char game_melody_length;
extern volatile unsigned char game_melody_frequency0[32];
extern volatile unsigned char game_melody_frequency1[32];
extern volatile unsigned char game_melody_duration[32];

void play_sound(unsigned char voice, unsigned char frequency,
                unsigned char distortion, unsigned char volume);
void play_event_sound(unsigned char event);
void start_melody(const char *melody);

#endif

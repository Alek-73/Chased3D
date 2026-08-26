#include <atari.h>
#include "melody.h"

static const unsigned char note_frequency[17] = {
    144, 128, 121, 108, 96, 91, 81, 72, 64,
    60, 53, 47, 45, 40, 35, 31, 0
};

volatile unsigned char game_melody_active;
volatile unsigned char game_melody_timer;
volatile unsigned char game_melody_index;
volatile unsigned char game_melody_length;
volatile unsigned char game_melody_frequency0[32];
volatile unsigned char game_melody_frequency1[32];
volatile unsigned char game_melody_duration[32];

void play_sound(unsigned char voice, unsigned char frequency,
                unsigned char distortion, unsigned char volume)
{
    _sound(voice, frequency, distortion, volume);
}

void play_event_sound(unsigned char event)
{
    if (event == 0) {
        play_sound(0, 121, 10, 8);
        play_sound(1, 121, 12, 8);
    } else if (event == 1) {
        play_sound(0, 100, 8, 8);
    } else if (event == 2) {
        play_sound(1, 100, 8, 8);
    } else {
        play_sound(0, 0, 0, 0);
        play_sound(1, 0, 0, 0);
    }
}

static unsigned char melody_note_index(unsigned char note, unsigned char octave)
{
    if (note == 'P') return 16;
    if (octave == '0') {
        if (note == 'A') return 0;
        if (note == 'B') return 1;
    } else if (octave == '1') {
        if (note == 'C') return 2;
        if (note == 'D') return 3;
        if (note == 'E') return 4;
        if (note == 'F') return 5;
        if (note == 'G') return 6;
        if (note == 'A') return 7;
        if (note == 'B') return 8;
    } else if (octave == '2') {
        if (note == 'C') return 9;
        if (note == 'D') return 10;
        if (note == 'E') return 11;
        if (note == 'F') return 12;
        if (note == 'G') return 13;
        if (note == 'A') return 14;
        if (note == 'B') return 15;
    }
    return 16;
}

void start_melody(const char *melody)
{
    unsigned int index;
    unsigned char note;
    unsigned char duration;
    unsigned char length;

    game_melody_active = 0;
    game_melody_timer = 0;
    game_melody_index = 0;
    length = 0;
    for (index = 0; melody[index + 2] != '\0' && length < 32; index += 3) {
        note = melody_note_index((unsigned char)melody[index],
                                 (unsigned char)melody[index + 1]);
        duration = (unsigned char)(melody[index + 2] - '0');
        if (duration != 1 && duration != 2 && duration != 4) duration = 1;
        game_melody_frequency0[length] = note_frequency[note];
        game_melody_frequency1[length] =
            note <= 14 ? note_frequency[note + 2] : 0;
        game_melody_duration[length] = (unsigned char)(duration * 5);
        ++length;
    }
    game_melody_length = length;
    game_melody_active = length != 0;
}

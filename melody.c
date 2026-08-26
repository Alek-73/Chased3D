#include "melody.h"

#define MELODY_MAX 32
#define REST 16

extern unsigned char melody_active;
extern unsigned char melody_timer;
extern unsigned char melody_index;
extern unsigned char melody_length;
extern unsigned char melody_freq0[];
extern unsigned char melody_freq1[];
extern unsigned char melody_dur[];

/* A0 B0 C1 D1 E1 F1 G1 A1 B1 C2 D2 E2 F2 G2 A2 B2, then a rest. */
static const unsigned char note_frequency[17] = {
    144, 128, 121, 108, 96, 91, 81, 72, 64,
    60, 53, 47, 45, 40, 35, 31, 0
};

static unsigned char melody_note_index(unsigned char note, unsigned char octave)
{
    if (note == 'P') return REST;
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
    return REST;
}

/* Note, octave and duration triplets, for example "C11E11G11C22". */
void melody_play(const char *melody)
{
    unsigned int index;
    unsigned char note;
    unsigned char duration;
    unsigned char length;

    /* Cleared first so the vertical blank tick cannot read a half written note
     * list while it is being replaced. */
    melody_active = 0;
    melody_timer = 0;
    melody_index = 0;
    length = 0;
    for (index = 0; length < MELODY_MAX; index += 3) {
        /* Every character of the triplet is checked: testing only [index + 2]
         * reads past the terminator when the string ends on a triplet boundary,
         * and parses whatever follows it in memory as extra notes. */
        if (melody[index] == '\0'
            || melody[index + 1] == '\0'
            || melody[index + 2] == '\0') break;
        note = melody_note_index((unsigned char)melody[index],
                                 (unsigned char)melody[index + 1]);
        duration = (unsigned char)(melody[index + 2] - '0');
        if (duration != 1 && duration != 2 && duration != 4) duration = 1;
        melody_freq0[length] = note_frequency[note];
        /* Second voice a third above: this harmony is what gives the original
         * its character. */
        melody_freq1[length] = note <= 14 ? note_frequency[note + 2] : 0;
        melody_dur[length] = (unsigned char)(duration * 5);
        ++length;
    }
    melody_length = length;
    melody_active = length != 0;
}

void melody_pickup(void)
{
    melody_play("C11E11G11C22");
}

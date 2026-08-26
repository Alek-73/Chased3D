#include "melody.h"

#define PICKUP_NOTES 4

extern unsigned char melody_active;
extern unsigned char melody_index;
extern unsigned char melody_length;
extern unsigned char melody_timer;
extern unsigned char melody_freq[];
extern unsigned char melody_dur[];

/* POKEY dividers: lower is higher pitched. An ascending C-E-G-C arpeggio. */
static const unsigned char pickup_freq[PICKUP_NOTES] = { 121, 96, 81, 60 };
static const unsigned char pickup_dur[PICKUP_NOTES] = { 6, 6, 6, 14 };

void melody_pickup(void)
{
    unsigned char i;

    /* Cleared first so the vertical blank tick cannot read a half written note
     * list while it is being replaced. */
    melody_active = 0;
    for (i = 0; i < PICKUP_NOTES; ++i) {
        melody_freq[i] = pickup_freq[i];
        melody_dur[i] = pickup_dur[i];
    }
    melody_length = PICKUP_NOTES;
    melody_index = 0;
    melody_timer = 0;
    melody_active = 1;
}

#ifndef GAME_UTILS_H
#define GAME_UTILS_H

/* ============================================================================
 * Utility Functions for Chased Game
 * Centralized functions to reduce code duplication and improve maintainability.
 * ============================================================================
 */

/**
 * Clamp player X coordinate within valid playfield bounds.
 * Player stays between x=56 (left edge) and x=192 (right edge)
 */
static inline void clamp_player_x(unsigned char *x)
{
    if (*x < 56) *x = 56;
    if (*x > 192) *x = 192;
}

/**
 * Clamp pursuer X coordinate within slightly tighter bounds.
 * Pursuer stays between x=56 and x=190 for game difficulty
 */
static inline void clamp_pursuer_x(unsigned char *x)
{
    if (*x < 56) *x = 56;
    if (*x > 190) *x = 190;
}

/**
 * Clamp pursuer Y coordinate for vertical movement.
 * Keeps pursuer visible on screen (y=10 to y=100)
 */
static inline void clamp_pursuer_y(unsigned char *y)
{
    if (*y < 10) *y = 10;
    if (*y > 100) *y = 100;
}

/**
 * Calculate minimum Y bound for player based on scroll position.
 * When at top of map, player can go to y=20; otherwise min y=30
 * 
 * @param scroll_row Current row in scrollable map
 * @param fine_scroll Fine scroll offset (0-14)
 * @return Minimum valid Y coordinate for player
 */
static inline unsigned char get_player_y_min(unsigned char scroll_row, 
                                             unsigned char fine_scroll)
{
    return (scroll_row == 0 && fine_scroll == 0) ? 20 : 30;
}

/**
 * Calculate maximum Y bound for player based on scroll position.
 * When at bottom of map, player can go to y=95; otherwise max y=70
 * 
 * @param scroll_row Current row in scrollable map
 * @param fine_scroll Fine scroll offset (0-14)
 * @param max_scroll_row Maximum scroll row (MAP_H - SCREEN_H)
 * @return Maximum valid Y coordinate for player
 */
static inline unsigned char get_player_y_max(unsigned char scroll_row,
                                             unsigned char fine_scroll,
                                             unsigned char max_scroll_row)
{
    return (scroll_row == max_scroll_row && fine_scroll == 0) ? 95 : 70;
}

/**
 * Convert printable ASCII to Atari display screen codes.
 * Atari uses different internal character encoding than ASCII.
 * For visible ASCII range (0x20-0x5F), subtract 0x20 to get screen code.
 * 
 * @param ch ASCII character
 * @return Atari screen code
 */
static inline unsigned char ascii_to_screen(unsigned char ch)
{
    if (ch >= 0x20 && ch <= 0x5F) {
        return (unsigned char)(ch - 0x20);
    }
    return ch;
}

/**
 * Macro to calculate 2D array index from row/col into 1D array.
 * Prevents arithmetic errors when indexing track_buffer.
 * Usage: index = ARRAY_INDEX(row, col, MAP_W)
 */
#define ARRAY_INDEX(row, col, width) ((unsigned int)(row) * (width) + (col))

#endif

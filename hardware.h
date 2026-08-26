#ifndef GAME_HARDWARE_H
#define GAME_HARDWARE_H

/* ============================================================================
 * Atari 8-bit Hardware Register Definitions
 * Centralizes all hardware addresses to reduce magic numbers in code.
 * ============================================================================
 */

/* --- POKEY Input Ports --- */
#define JOY_PORT        0x0278  /* Joystick input port */
#define FIRE_PORT       0x0284  /* Fire button input */

/* --- GTIA Registers (Graphics) --- */
#define HPOSP0          0xD000  /* Horizontal position Player 0 */
#define HPOSP1          0xD001  /* Horizontal position Player 1 */
#define HPOSP2          0xD002  /* Horizontal position Player 2 */
#define HPOSM0          0xD004  /* Horizontal position Missile 0 */
#define HPOSM1          0xD005  /* Horizontal position Missile 1 */
#define HPOSM2          0xD006  /* Horizontal position Missile 2 */
#define HPOSM3          0xD007  /* Horizontal position Missile 3 */

#define P0PF            0xD004  /* Player 0 / Playfield collision */
#define P1PF            0xD005  /* Player 1 / Playfield collision */
#define P2PF            0xD006  /* Player 2 / Playfield collision */
#define P3PF            0xD007  /* Player 3 / Playfield collision */
#define M0PL            0xD008  /* Missile 0 / Player collision */
#define M1PL            0xD009  /* Missile 1 / Player collision */
#define M2PL            0xD00A  /* Missile 2 / Player collision */
#define M3PL            0xD00B  /* Missile 3 / Player collision */
#define P0PL            0xD00C  /* Player 0 / Player collision */
#define P1PL            0xD00D  /* Player 1 / Player collision */
#define P2PL            0xD00E  /* Player 2 / Player collision */
#define P3PL            0xD00F  /* Player 3 / Player collision */
#define TRIG0           0xD010  /* Joystick trigger 0 (read-only) */
#define TRIG1           0xD011  /* Joystick trigger 1 (read-only) */
#define TRIG2           0xD012  /* Joystick trigger 2 (read-only) */
#define TRIG3           0xD013  /* Joystick trigger 3 (read-only) */
#define HITCLR          0xD01E  /* Collision clear register */

#define GRACTL          0xD01D  /* Graphics control (enable PMG) */
#define PRIOR           0xD01B  /* Priority control */
#define COLPM0          0xD012  /* Color Player 0 */
#define COLPM1          0xD013  /* Color Player 1 */
#define COLPM2          0xD014  /* Color Player 2 */
#define COLBK           0xD01A  /* Color Background */

/* --- ANTIC Registers (Display List) --- */
#define PMBASE          0xD407  /* PMG memory base address */
#define DMACTL          0xD400  /* DMA control */
#define NMIEN           0xD40E  /* NMI enable */
#define VSCROL          0xD405  /* Vertical scroll */

/* --- OS Zero Page RAM Addresses --- */
#define PCOLR0          0x02C0  /* Player color 0 (RAM mirror) */
#define PCOLR1          0x02C1  /* Player color 1 (RAM mirror) */
#define PCOLR2          0x02C2  /* Player color 2 (RAM mirror) */
#define SDLSTL          0x0230  /* Screen display list low byte */
#define SDLSTH          0x0231  /* Screen display list high byte */

/* --- Interrupt Control --- */
#define NMIEN_DLI_VBI   0xC0    /* Enable DLI and VBI interrupts */

/* --- Game-Specific Color Scheme --- */
#define GAME_COLOR_BG           0x02    /* Dark background */
#define GAME_COLOR_PLAYFIELD_0  0x94    /* Light purple */
#define GAME_COLOR_PLAYFIELD_1  0xFC    /* Cyan */
#define GAME_COLOR_PLAYFIELD_2  0x9A    /* Green */
#define GAME_COLOR_PLAYFIELD_3  0xB4    /* Magenta */
#define GAME_COLOR_PLAYER_0     0x34    /* Purple pursuer */
#define GAME_COLOR_PLAYER_1     0x9A    /* Green player */
#define GAME_COLOR_PLAYER_2     0x5A    /* Orange player */

/* Screen menu colors */
#define MENU_COLOR_BG           0x00
#define MENU_COLOR_TEXT         0xFC

/* --- Sprite/PMG Parameters --- */
#define PLAYER_HEIGHT           22      /* Sprite height in scanlines */
#define SCREEN_W                20      /* Screen width in characters */

/* --- Display Dimensions --- */
#define SCREEN_WIDTH            20      /* Character mode width (alias) */
#define SCREEN_HEIGHT           24      /* Standard screen height */

#endif

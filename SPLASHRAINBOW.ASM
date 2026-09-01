        .setcpu "6502"

        .export _splash_screen_rainbow

VCOUNT = $D40B
WSYNC  = $D40A
COLPF2 = $D018
STRIG0 = $D010
CONSOL = $D01F
CH     = $02FC
SDMCTL = $022F
DMACTL = $D400

STATIC_TITLE = $1E

        .segment "ZEROPAGE"
rainbow_phase:
        .res 1
rainbow_delay:
        .res 1

        .segment "CODE"

; Playfield color 3 is reserved exclusively for the CHASED 3D title, so it can
; be changed on every scanline without affecting the bitmap or other text.
; Joystick fire, START, or any keyboard key dismisses the splash screen.
_splash_screen_rainbow:
        lda #$00
        sta rainbow_phase
        sta rainbow_delay
        lda #$FF
        sta CH

rainbow_loop:
        lda VCOUNT
        clc
        adc rainbow_phase
        sta WSYNC
        sta COLPF2

        inc rainbow_delay
        bne rainbow_input
        inc rainbow_phase

rainbow_input:
        lda STRIG0
        beq rainbow_done
        lda CONSOL
        and #$01
        beq rainbow_done
        lda CH
        cmp #$FF
        beq rainbow_loop

rainbow_done:
        lda #$FF
        sta CH
        lda #STATIC_TITLE
        sta COLPF2
        lda #$00
        sta SDMCTL
        sta DMACTL
        rts
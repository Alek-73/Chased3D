        .setcpu "6502"

        .export _Rainbow

VCOUNT  = $D40B
COLBK   = $D01A
STRIG0  = $D010
WSYNC   = $D40A

        .segment "ZEROPAGE"
temp:   .res 1

        .segment "CODE"

; Wait for joystick 0 fire while coloring the background from VCOUNT.
_Rainbow:
        ldx #0
        ldy #0
wait_fire:
        inx
        txa
        cmp #250
        bne delay
        ldx #0
        iny
delay:
        lda VCOUNT
        sta temp
        tya
        clc
        adc temp
        sta WSYNC
        sta COLBK
        lda STRIG0
        bne wait_fire
        rts

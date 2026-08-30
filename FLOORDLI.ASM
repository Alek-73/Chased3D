        .setcpu "6502"

; Six horizontal floor luminance bands plus a final HUD boundary. Each DLI is
; attached to the mode line before a boundary; WSYNC makes the new color start
; cleanly on the following scanline.

        .export _floor_dli_install
        .export _floor_dli_set_rotation

VDSLST = $0200
WSYNC  = $D40A
VCOUNT = $D40B
NMIEN  = $D40E
COLPF0 = $D016
COLBK  = $D01A

HUD_VCOUNT = 104

        .segment "PAGE6CODE"

_floor_dli_install:
        lda #$40                   ; keep VBI on, disable DLI while vector changes
        sta NMIEN
        lda #<floor_dli
        sta VDSLST
        lda #>floor_dli
        sta VDSLST+1
        lda #$00
        sta floor_dli_stage
        sta floor_dli_rotation
        lda #$C0                   ; enable VBI and DLI NMIs
        sta NMIEN
        rts

_floor_dli_set_rotation:
        sta floor_dli_rotation
        rts

floor_dli:
        pha
        txa
        pha
        tya
        pha

        lda VCOUNT
        cmp #HUD_VCOUNT
        bcs floor_dli_hud
        ldx floor_dli_stage
        cpx #$06
        bcc floor_dli_band
        ldx #$00
        stx floor_dli_stage
floor_dli_band:
        txa
        clc
        adc floor_dli_rotation
        cmp #$06
        bcc floor_dli_color
        sbc #$06
floor_dli_color:
        tax
        lda floor_colors,x
        sta WSYNC
        sta COLPF0

        inc floor_dli_stage
        jmp floor_dli_restore

floor_dli_hud:
        lda #$00
        sta WSYNC
        sta COLBK
        sta floor_dli_stage
floor_dli_restore:
        pla
        tay
        pla
        tax
        pla
        rti

        .segment "PAGE6BSS"
floor_dli_stage:
        .res 1
floor_dli_rotation:
        .res 1

        .segment "PAGE6DATA"
floor_colors:
        .byte $22, $24, $26, $28, $26, $24
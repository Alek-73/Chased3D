        .setcpu "6502"

; Four horizontal floor luminance bands. Each DLI is attached to the mode line
; before a band boundary; WSYNC makes the new COLPF0 value start cleanly on
; the following scanline.

        .export _floor_dli_install
        .export _floor_dli_set_rotation

VDSLST = $0200
WSYNC  = $D40A
NMIEN  = $D40E
COLPF0 = $D016

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

        ldx floor_dli_stage
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
        lda floor_dli_stage
        cmp #$06
        bcc floor_dli_restore
        lda #$00
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
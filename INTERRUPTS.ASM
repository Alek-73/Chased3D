        .setcpu "6502"

        .export _game_dli_handler
        .export _game_vbi_handler
        .export _melody_tick
        .import _game_collision_pending
        .import _game_p1pf_value
        .import _game_saved_vvblkd
        .import _game_melody_active
        .import _game_melody_timer
        .import _game_melody_index
        .import _game_melody_length
        .import _game_melody_frequency0
        .import _game_melody_frequency1
        .import _game_melody_duration
        .import _game_dli_phase

WSYNC   = $D40A
COLBK   = $D01A
P1PF    = $D005
P0PL    = $D00C
HITCLR  = $D01E
AUDF1   = $D200
AUDC1   = $D201
AUDF2   = $D202
AUDC2   = $D203

        .segment "BSS"
melody_volume:
        .res 1

        .segment "CODE"

; Every gameplay mode line provides a background color change.
_game_dli_handler:
        pha
        txa
        pha
        tya
        pha

        sta WSYNC

        ldx _game_dli_phase
        lda background_colors,x
        sta COLBK

        inx
        cpx #$13
        bcc save_phase
        ldx #$00
save_phase:
        stx _game_dli_phase
done:
        pla
        tay
        pla
        tax
        pla
        rti

; Sample completed-frame GTIA collisions, clear the latches, and advance
; the frame-based melody before chaining to the OS deferred VBI handler.
_game_vbi_handler:
        pha
        txa
        pha
        tya
        pha

        lda P0PL
        beq no_pursuer_collision
        lda #$01
        sta _game_collision_pending
no_pursuer_collision:
        lda P1PF
        sta _game_p1pf_value
        lda #$01
        sta HITCLR
        jsr _melody_tick

        pla
        tay
        pla
        tax
        pla
        jmp (_game_saved_vvblkd)

_melody_tick:
        lda _game_melody_active
        beq melody_done
        lda _game_melody_timer
        beq load_note
        dec _game_melody_timer
        lda melody_volume
        cmp #$0A
        bcs melody_done
        adc #$02
        sta melody_volume
        ora #$A0
        sta AUDC1
        sta AUDC2
        rts

load_note:
        ldx _game_melody_index
        cpx _game_melody_length
        bcc have_note
        lda #$00
        sta _game_melody_active
        sta AUDC1
        sta AUDC2
        rts

have_note:
        lda _game_melody_frequency0,x
        sta AUDF1
        lda _game_melody_frequency1,x
        sta AUDF2
        ora _game_melody_frequency0,x
        beq silence_note
        lda #$00
        sta melody_volume
        lda #$A0
        sta AUDC1
        sta AUDC2
        jmp save_note

silence_note:
        lda #$00
        sta melody_volume
        sta AUDC1
        sta AUDC2

save_note:
        lda _game_melody_duration,x
        sta _game_melody_timer
        inx
        stx _game_melody_index
melody_done:
        rts

background_colors:
        .byte $1C, $2A, $3C, $4E, $5A, $6C, $7E, $8E
        .byte $9E, $AE, $BE, $CE, $DE, $EE, $FE, $1C
        .byte $2A, $4E, $7E
        .byte $DE, $EE, $FE, $1C

        .setcpu "6502"

; Melody playback driven from the deferred vertical blank vector.
;
; The tick has to run on an interrupt: the render loop takes several frames per
; pass, so advancing notes from the main loop would tie tempo to the frame rate.
; It also has to be assembly rather than C, because cc65 uses a software stack
; and shared temporaries that an interrupt would corrupt mid-call.

        .export _melody_install
        .export _melody_laser_buzz
        .export _melody_active
        .export _melody_timer
        .export _melody_index
        .export _melody_length
        .export _melody_freq0
        .export _melody_freq1
        .export _melody_dur
        .export _melody_threat_level
        .export _melody_threat_freq
        .export _melody_threat_dur
        .export _melody_threat_length

SETVBV  = $E45C
VVBLKD  = $0224
AUDF1   = $D200
AUDC1   = $D201
AUDF2   = $D202
AUDC2   = $D203
AUDF3   = $D204
AUDC3   = $D205
AUDF4   = $D206
AUDC4   = $D207
AUDCTL  = $D208
SKCTL   = $D20F

MELODY_MAX = 32
PEAK_VOLUME = $0A

        .segment "BSS"
_melody_active:
        .res 1
_melody_timer:
        .res 1
_melody_index:
        .res 1
_melody_length:
        .res 1
_melody_freq0:
        .res MELODY_MAX
_melody_freq1:
        .res MELODY_MAX
_melody_dur:
        .res MELODY_MAX
_melody_threat_level:
        .res 1
_melody_threat_freq:
        .res 16
_melody_threat_dur:
        .res 16
_melody_threat_length:
        .res 1
threat_index:
        .res 1
threat_timer:
        .res 1
threat_frequency:
        .res 1
melody_volume:
        .res 1
laser_buzz_timer:
        .res 1
saved_vbi:
        .res 2

        .segment "CODE"

_melody_install:
        lda #$00
        sta AUDCTL
        sta AUDC1
        sta AUDC2
        sta AUDC3
        sta AUDC4
        sta _melody_threat_level
        sta threat_index
        sta threat_timer
        sta laser_buzz_timer
        sta _melody_active
        lda #$03
        sta SKCTL

        lda VVBLKD
        sta saved_vbi
        lda VVBLKD+1
        sta saved_vbi+1

        ; SETVBV avoids the race of writing the vector bytes non-atomically.
        ; Vector 7 is the deferred VBI; 6 is the immediate one, which would
        ; bypass the OS stage 1 work that updates RTCLOK.
        ldy #<melody_vbi
        ldx #>melody_vbi
        lda #$07
        jsr SETVBV
        rts

melody_vbi:
        pha
        txa
        pha
        tya
        pha
        jsr melody_tick
        jsr threat_tick
        jsr laser_buzz_tick
        pla
        tay
        pla
        tax
        pla
        jmp (saved_vbi)

; Two voices with the volume swelling into each note rather than decaying.
melody_tick:
        lda _melody_active
        beq melody_done
        lda _melody_timer
        beq load_note
        dec _melody_timer
        lda melody_volume
        cmp #PEAK_VOLUME
        bcs melody_done
        adc #$02                   ; carry is clear here, from the cmp above
        sta melody_volume
        ora #$A0                   ; pure tone, distortion off
        sta AUDC1
        sta AUDC2
        rts

load_note:
        ldx _melody_index
        cpx _melody_length
        bcc have_note
        lda #$00
        sta _melody_active
        sta AUDC1
        sta AUDC2
        rts

have_note:
        lda _melody_freq0,x
        sta AUDF1
        lda _melody_freq1,x
        sta AUDF2
        ora _melody_freq0,x
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
        lda _melody_dur,x
        sta _melody_timer
        inx
        stx _melody_index
melody_done:
        rts

_melody_laser_buzz:
        lda #$20
        sta AUDF4
        lda #$88                   ; poly noise, volume 8
        sta AUDC4
        lda #$18                   ; short burst, independent of frame rate
        sta laser_buzz_timer
        rts

laser_buzz_tick:
        lda laser_buzz_timer
        beq laser_buzz_silence
        dec laser_buzz_timer
        bne laser_buzz_done
laser_buzz_silence:
        lda #$00
        sta AUDC4
laser_buzz_done:
        rts

threat_tick:
        lda _melody_threat_level
        beq threat_silence
        lda threat_timer
        beq threat_load_note
        dec threat_timer
        lda threat_frequency
        beq threat_silence
        sta AUDF3
        lda _melody_threat_level
        ora #$A0
        sta AUDC3
        rts

threat_load_note:
        ldx threat_index
        cpx _melody_threat_length
        bcc threat_have_note
        ldx #$00
        stx threat_index
threat_have_note:
        lda _melody_threat_freq,x
        sta threat_frequency
        lda _melody_threat_dur,x
        sta threat_timer
        inc threat_index
        lda threat_frequency
        beq threat_silence
        sta AUDF3
        lda _melody_threat_level
        ora #$A0
        sta AUDC3
        rts

threat_silence:
        lda #$00
        sta AUDC3
        rts

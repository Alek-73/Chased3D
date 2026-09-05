        .setcpu "6502"

; Runs before the generic cc65 Atari startup. With BASIC enabled, OS MEMTOP is
; low enough that crt0 would place the C software stack inside the program.

        .export basic_off_start
        .import start

PORTB  = $D301
PBCTL  = $D303
MEMTOP = $02E5

        .segment "PAGE6CODE"

basic_off_start:
        lda PBCTL
        ora #$04
        sta PBCTL
        lda PORTB
        ora #$02
        sta PORTB

        lda #<$BC20
        sta MEMTOP
        lda #>$BC20
        sta MEMTOP+1
        jmp start

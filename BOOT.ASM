        .setcpu "6502"

; Runs before the generic cc65 Atari startup. With BASIC enabled, OS MEMTOP is
; low enough that crt0 would place the C software stack inside the program.

        .export basic_off_start
        .export _startup_portb, _startup_memtop
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
        sta _startup_portb
        ora #$02
        sta PORTB

        lda MEMTOP
        sta _startup_memtop
        lda MEMTOP+1
        sta _startup_memtop+1
        lda #<$BC20
        sta MEMTOP
        lda #>$BC20
        sta MEMTOP+1
        jmp start

        .segment "PAGE6BSS"
_startup_portb:
        .res 1
_startup_memtop:
        .res 2
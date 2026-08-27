        .setcpu "6502"

; Grid traversal (DDA) for the Chased3D raycaster.
;
; This is the hot loop of the renderer: up to MAX_STEPS iterations per ray,
; times one ray per screen column. In C each step cost roughly 300 cycles
; because of 16-bit arithmetic and the map pointer indirection.

        .export _dda_map_x, _dda_map_y
        .export _dda_step_x, _dda_step_y
        .export _dda_side_x, _dda_side_y
        .export _dda_delta_x, _dda_delta_y
        .export _dda_side, _dda_hit, _dda_steps
        .export _dda_cast

        .import _maze_row_lo, _maze_row_hi
        .import _maze_exit_open

MAZE_W = 20
MAZE_H = 59

        .segment "ZEROPAGE"
dda_ptr:
        .res 2

        .segment "BSS"
_dda_map_x:
        .res 1
_dda_map_y:
        .res 1
_dda_step_x:
        .res 1
_dda_step_y:
        .res 1
_dda_side_x:
        .res 2
_dda_side_y:
        .res 2
_dda_delta_x:
        .res 2
_dda_delta_y:
        .res 2
_dda_side:
        .res 1
_dda_hit:
        .res 1
_dda_steps:
        .res 1

        .segment "CODE"

; Walks the grid until a wall tile (1 or 2) is hit, the ray leaves the map, or
; _dda_steps iterations elapse. On a hit _dda_side_x / _dda_side_y hold the
; accumulated ray length, which the caller turns into a wall distance.
_dda_cast:
        lda #$00
        sta _dda_hit
        ldx _dda_steps
        bne dda_start
        jmp dda_done

dda_start:
dda_loop:
        lda _dda_side_x
        cmp _dda_side_y
        lda _dda_side_x+1
        sbc _dda_side_y+1
        bcs dda_step_y

        lda _dda_map_x
        clc
        adc _dda_step_x
        sta _dda_map_x
        cmp #MAZE_W                ; unsigned test also catches the $FF underflow
        bcc dda_x_ok
        jmp dda_done
dda_x_ok:
        clc
        lda _dda_side_x
        adc _dda_delta_x
        sta _dda_side_x
        lda _dda_side_x+1
        adc _dda_delta_x+1
        sta _dda_side_x+1
        lda #$00
        sta _dda_side
        jmp dda_tile

dda_step_y:
        lda _dda_map_y
        clc
        adc _dda_step_y
        sta _dda_map_y
        cmp #MAZE_H
        bcc dda_y_ok
        jmp dda_done
dda_y_ok:
        clc
        lda _dda_side_y
        adc _dda_delta_y
        sta _dda_side_y
        lda _dda_side_y+1
        adc _dda_delta_y+1
        sta _dda_side_y+1
        lda #$01
        sta _dda_side

dda_tile:
        ldy _dda_map_y
        lda _maze_row_lo,y
        sta dda_ptr
        lda _maze_row_hi,y
        sta dda_ptr+1
        ldy _dda_map_x
        lda (dda_ptr),y
        cmp #$01
        beq dda_wall_near
        cmp #$02
        beq dda_wall_near
        cmp #$05                   ; exit gate: solid only while closed
        bne dda_continue
        lda _maze_exit_open
        beq dda_wall_near
dda_continue:
        dex
        beq dda_done
        jmp dda_loop

dda_done:
        rts

dda_wall_near:
        jmp dda_wall

dda_wall:
        lda #$01
        sta _dda_hit
        rts

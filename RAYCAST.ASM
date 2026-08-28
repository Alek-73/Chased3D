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
        .export _ray_setup_angle, _ray_setup_px, _ray_setup_py
        .export _ray_setup_and_cast

        .import _maze_row_lo, _maze_row_hi
        .import _maze_exit_open
        .import _sin3d, _recip3d

MAZE_W = 20
MAZE_H = 59

        .segment "ZEROPAGE"
dda_ptr:
        .res 2
setup_ptr:
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
_ray_setup_angle:
        .res 2
_ray_setup_px:
        .res 2
_ray_setup_py:
        .res 2
setup_value:
        .res 2
setup_recip:
        .res 2
setup_scaled:
        .res 2
setup_offset:
        .res 2
setup_negative:
        .res 1
mul_left:
        .res 2
mul_right:
        .res 1
mul_product:
        .res 2

        .segment "CODE"

.macro SETUP_AXIS adjust, position, delta, side, step
        .local positive, full_delta, save_side
        lda _ray_setup_angle+1
        clc
        adc #adjust
        jsr load_recip
        lda setup_recip
        sta delta
        lda setup_recip+1
        sta delta+1

        lda setup_negative
        beq positive
        lda #$FF
        sta step
        lda position
        jsr scale_recip
        jmp save_side

positive:
        lda #$01
        sta step
        lda position
        beq full_delta
        eor #$FF
        clc
        adc #$01
        jsr scale_recip
        jmp save_side

full_delta:
        lda setup_recip
        sta setup_scaled
        lda setup_recip+1
        sta setup_scaled+1
save_side:
        lda setup_scaled
        sta side
        lda setup_scaled+1
        sta side+1
.endmacro

; Builds both axis deltas and initial side distances, then enters the existing
; grid traversal. The high angle byte is the 256-step trigonometric index.
_ray_setup_and_cast:
        lda _ray_setup_px+1
        sta _dda_map_x
        lda _ray_setup_py+1
        sta _dda_map_y

        SETUP_AXIS 64, _ray_setup_px, _dda_delta_x, _dda_side_x, _dda_step_x
        SETUP_AXIS 0,  _ray_setup_py, _dda_delta_y, _dda_side_y, _dda_step_y

        lda #24
        sta _dda_steps
        jmp _dda_cast

; A is a sine-table index. Returns abs(sin[index])'s reciprocal in
; setup_recip and records the original sign in setup_negative.
load_recip:
        asl
        sta setup_offset
        lda #$00
        rol
        sta setup_offset+1
        clc
        lda #<_sin3d
        adc setup_offset
        sta setup_ptr
        lda #>_sin3d
        adc setup_offset+1
        sta setup_ptr+1
        ldy #$00
        lda (setup_ptr),y
        sta setup_value
        iny
        lda (setup_ptr),y
        sta setup_value+1
        bmi load_recip_negative
        lda #$00
        sta setup_negative
        jmp load_recip_abs

load_recip_negative:
        lda #$01
        sta setup_negative
        sec
        lda #$00
        sbc setup_value
        sta setup_value
        lda #$00
        sbc setup_value+1
        sta setup_value+1

load_recip_abs:
        asl setup_value
        rol setup_value+1
        clc
        lda #<_recip3d
        adc setup_value
        sta setup_ptr
        lda #>_recip3d
        adc setup_value+1
        sta setup_ptr+1
        ldy #$00
        lda (setup_ptr),y
        sta setup_recip
        iny
        lda (setup_ptr),y
        sta setup_recip+1
        rts

; A is an 8-bit fraction. Computes (fraction * setup_recip) >> 8 using
; two 8x8 products, matching view3d.c's former scale_frac implementation.
scale_recip:
        sta setup_value
        ldy setup_recip+1
        jsr multiply_8x8
        lda mul_product
        sta setup_scaled
        lda mul_product+1
        sta setup_scaled+1
        lda setup_value
        ldy setup_recip
        jsr multiply_8x8
        clc
        lda setup_scaled
        adc mul_product+1
        sta setup_scaled
        bcc scale_recip_done
        inc setup_scaled+1
scale_recip_done:
        rts

; A * Y -> mul_product, unsigned.
multiply_8x8:
        sta mul_left
        lda #$00
        sta mul_left+1
        sty mul_right
        sta mul_product
        sta mul_product+1
        .repeat 8
                lsr mul_right
                bcc :+
                clc
                lda mul_product
                adc mul_left
                sta mul_product
                lda mul_product+1
                adc mul_left+1
                sta mul_product+1
:               asl mul_left
                rol mul_left+1
        .endrepeat
        rts

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

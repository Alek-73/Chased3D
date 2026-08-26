        .setcpu "6502"

; Unrolled vertical column blitter for the Chased3D raycaster.
;
; The view is ANTIC mode D (160x96, 4 colours, 40 bytes per row). Each of the
; 40 ray columns is exactly one byte wide, so a column fill is one "sta abs,x"
; per row with X holding the column index. Every store is a 3-byte absolute
; instruction, which lets the entry tables be computed as base + 3 * row.

        .export _col3d_x
        .export _col3d_row
        .export _col3d_end
        .export _col3d_color
        .export _col3d_fill_down
        .export _col3d_fill_up
        .export _col3d_fill_span

        .import _view_buffer

VIEW_ROWS  = 92
VIEW_STRIDE = 40

        .segment "BSS"
        .align 2
col3d_vec:
        .res 2
_col3d_x:
        .res 1
_col3d_row:
        .res 1
_col3d_end:
        .res 1
_col3d_color:
        .res 1
saved_op:
        .res 1

        .segment "CODE"

; Fill rows _col3d_row .. 95 of column _col3d_x with _col3d_color.
_col3d_fill_down:
        ldx _col3d_row
        lda asc_lo,x
        sta col3d_vec
        lda asc_hi,x
        sta col3d_vec+1
        ldx _col3d_x
        lda _col3d_color
        jmp (col3d_vec)

; Fill rows _col3d_row .. 0 of column _col3d_x with _col3d_color.
_col3d_fill_up:
        ldx _col3d_row
        lda desc_lo,x
        sta col3d_vec
        lda desc_hi,x
        sta col3d_vec+1
        ldx _col3d_x
        lda _col3d_color
        jmp (col3d_vec)

; Fill rows _col3d_row .. _col3d_end-1 only, by temporarily planting an RTS at
; the row after the span. Writing each pixel once avoids the visible overdraw
; of filling to the screen bottom and painting the floor back over it.
_col3d_fill_span:
        ldx _col3d_end
        lda asc_lo,x
        sta span_load+1
        sta span_store+1
        sta span_restore+1
        lda asc_hi,x
        sta span_load+2
        sta span_store+2
        sta span_restore+2
span_load:
        lda $FFFF
        sta saved_op
        lda #$60
span_store:
        sta $FFFF

        ldx _col3d_row
        lda asc_lo,x
        sta col3d_vec
        lda asc_hi,x
        sta col3d_vec+1
        ldx _col3d_x
        lda _col3d_color
        jsr span_call

        lda saved_op
span_restore:
        sta $FFFF
        rts

span_call:
        jmp (col3d_vec)

asc_block:
        .repeat VIEW_ROWS, I
        sta _view_buffer + I * VIEW_STRIDE,x
        .endrepeat
        rts

desc_block:
        .repeat VIEW_ROWS, I
        sta _view_buffer + (VIEW_ROWS - 1 - I) * VIEW_STRIDE,x
        .endrepeat
        rts

        .segment "RODATA"
; One extra entry so a span ending at the last row lands on the trailing rts.
asc_lo:
        .repeat VIEW_ROWS + 1, I
        .byte <(asc_block + 3 * I)
        .endrepeat
asc_hi:
        .repeat VIEW_ROWS + 1, I
        .byte >(asc_block + 3 * I)
        .endrepeat
desc_lo:
        .repeat VIEW_ROWS, I
        .byte <(desc_block + 3 * (VIEW_ROWS - 1 - I))
        .endrepeat
desc_hi:
        .repeat VIEW_ROWS, I
        .byte >(desc_block + 3 * (VIEW_ROWS - 1 - I))
        .endrepeat

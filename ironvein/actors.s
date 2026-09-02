; actors.s - the twelve drones and the multiplexer table, in assembly
;
; The same work in cc65 C cost 13 800 + 23 300 cycles on a quiet
; machine: every byte expression promoted to int, every step a runtime
; call. This is what the game's object code will have to look like.
;
; C side: dr_xlo/xhi/ylo/yhi (world pixels), dr_col; cam_px/cam_py
; (16-bit, set by C each frame). The objects' physics is in physics.s.

.export _build_mux
.export _cam_px, _cam_py, _mux_bank
.import _dr_xlo, _dr_xhi, _dr_ylo, _dr_yhi, _dr_vx, _dr_vy, _dr_col
.import _dr_st, _dr_ptr
.import _frame
.import _mux_y, _mux_xlo, _mux_slot, _mux_slot2
.import _mux_ptr, _mux_col, _mux_xand, _mux_xor, _mux_next
.importzp tmp1, tmp2, tmp3, tmp4
.macpack longbranch

NUM_DRONES = 12                 ; walkers and shots, see physics.s
MUX_BANK   = 32
SCREEN_LEFT_X = 31
SCREEN_TOP_Y  = $56

.data
_cam_px:   .word 0
_cam_py:   .word 0
_mux_bank: .byte 0

.bss
dlo:    .res 1
dhi:    .res 1
n:      .res 1
s_y:    .res NUM_DRONES
s_xlo:  .res NUM_DRONES
s_xhi:  .res NUM_DRONES
s_col:  .res NUM_DRONES
s_ptr:  .res NUM_DRONES
s_ord:  .res NUM_DRONES
cxl:    .res 1
cxh:    .res 1
cyl:    .res 1
cyh:    .res 1
k:      .res 1
e:      .res 1
slot:   .res 1
xand:   .res 1
ptrval: .res 1

.rodata
bit_of:  .byte 1, 2, 4, 8, 16, 32, 64, 128
slot_of: .byte 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5
inv_of:  .byte $FB, $F7, $EF, $DF, $BF, $7F, $FB, $F7, $EF, $DF, $BF, $7F, $FB, $F7, $EF, $DF

.code

; ----------------------------------------------------------------- mux
_build_mux:
        ; camera origin in sprite coordinates
        lda _cam_px
        sec
        sbc #SCREEN_LEFT_X
        sta cxl
        lda _cam_px+1
        sbc #0
        sta cxh
        lda _cam_py
        sec
        sbc #SCREEN_TOP_Y
        sta cyl
        lda _cam_py+1
        sbc #0
        sta cyh
        lda #0
        sta n
        ldx #0
bm_loop:
        lda _dr_st,x
        jeq bm_next
        ; screen y = world y - cam y, must have high byte 0 and be $40..$F0
        lda _dr_ylo,x
        sec
        sbc cyl
        sta dlo
        lda _dr_yhi,x
        sbc cyh
        jne bm_next
        lda dlo
        cmp #$40
        jcc bm_next
        cmp #$F1
        jcs bm_next
        ; screen x: 8..340 -> high byte 0 with lo >= 8, or high 1 with lo <= 84
        lda _dr_xlo,x
        sec
        sbc cxl
        sta dhi                 ; (reusing dhi as x low)
        lda _dr_xhi,x
        sbc cxh
        cmp #2
        jcs bm_next
        cmp #1
        bne bm_x0
        ldy dhi
        cpy #85
        jcs bm_next
        bcc bm_keep
bm_x0:  ldy dhi
        cpy #8
        jcc bm_next
bm_keep:
        ldy n
        sta s_xhi,y
        lda dhi
        sta s_xlo,y
        lda dlo
        sta s_y,y
        lda _dr_col,x
        sta s_col,y
        lda _dr_ptr,x
        sta s_ptr,y
        tya
        sta s_ord,y
        inc n
bm_next:
        inx
        cpx #NUM_DRONES
        jne bm_loop

        ; insertion sort of s_ord by s_y
        ldx #1
srt_outer:
        cpx n
        jcs srt_done
        lda s_ord,x
        sta k                   ; the element being placed
        tay
        lda s_y,y
        sta tmp1                ; its y
        stx tmp2                ; j = i
srt_inner:
        ldy tmp2
        jeq srt_place
        dey
        lda s_ord,y
        tay
        lda s_y,y
        cmp tmp1
        jeq srt_place
        jcc srt_place           ; s_y[ord[j-1]] <= y: stop
        ldy tmp2
        lda s_ord-1,y
        sta s_ord,y             ; shift right
        dec tmp2
        jmp srt_inner
srt_place:
        ldy tmp2
        lda k
        sta s_ord,y
        inx
        jmp srt_outer
srt_done:

        ; emit into the bank the IRQ is not reading. Per entry the
        ; source fields are copied to zero page first, so the loop never
        ; juggles two indices: X is the source order, Y the destination.
        lda _mux_bank
        sta e
        lda #0
        sta k
        ldx #0
em_loop:
        cpx n
        jcs em_done
        ldy s_ord,x
        lda s_y,y
        sta tmp1
        lda s_xlo,y
        sta tmp2
        lda s_xhi,y
        sta tmp3
        lda s_col,y
        sta tmp4
        lda s_ptr,y
        sta ptrval
        lda k
        cmp #6
        bcc em_ok
        ; the slot comes round every six entries; the entry six back
        ; must be at least 25 lines above this one
        ldy e
        lda _mux_y-6,y
        clc
        adc #25
        cmp tmp1
        bcc em_ok
        beq em_ok
        jmp em_skip
em_ok:
        ldy k
        lda slot_of,y
        sta slot
        lda inv_of,y
        sta xand
        ldy e
        lda tmp1
        sta _mux_y,y
        lda tmp2
        sta _mux_xlo,y
        lda tmp4
        sta _mux_col,y
        lda xand
        sta _mux_xand,y
        lda ptrval
        sta _mux_ptr,y
        lda slot
        sta _mux_slot,y
        asl a
        sta _mux_slot2,y
        lda tmp3
        beq em_xz
        ldy slot
        lda bit_of,y
        ldy e
        sta _mux_xor,y
        jmp em_adv
em_xz:  lda #0
        sta _mux_xor,y
em_adv: inc k
        inc e
em_skip:
        inx
        jmp em_loop
em_done:
        ldy e
        lda #$FF
        sta _mux_y,y
        lda _mux_bank
        sta _mux_next           ; publish
        eor #MUX_BANK
        sta _mux_bank
        rts

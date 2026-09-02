; actors.s - the twelve drones and the multiplexer table, in assembly
;
; The same work in cc65 C cost 13 800 + 23 300 cycles on a quiet
; machine: every byte expression promoted to int, every step a runtime
; call. This is what the game's object code will have to look like.
;
; C side: dr_xlo/xhi/ylo/yhi (world pixels), dr_vx/vy (signed bytes),
; dr_col; cam_px/cam_py (16-bit, set by C each frame); spawn_drone(i).

.export _update_drones, _build_mux
.export _cam_px, _cam_py, _mux_bank
.import _dr_xlo, _dr_xhi, _dr_ylo, _dr_yhi, _dr_vx, _dr_vy, _dr_col
.import _spawn_drone, _rand, _frame
.import _mux_y, _mux_xlo, _mux_slot, _mux_slot2
.import _mux_ptr, _mux_col, _mux_xand, _mux_xor, _mux_next
.importzp tmp1, tmp2, tmp3, tmp4
.macpack longbranch

NUM_DRONES = 8
MUX_BANK   = 32
SPR_PTR    = 96                 ; sprite pointer base ($5800 / 64)
SF_DRONE1  = 1
SF_DRONE2  = 2
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
s_ord:  .res NUM_DRONES
cxl:    .res 1
cxh:    .res 1
cyl:    .res 1
cyh:    .res 1
k:      .res 1
e:      .res 1
slot:   .res 1

.rodata
bit_of:  .byte 1, 2, 4, 8, 16, 32, 64, 128
slot_of: .byte 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5
inv_of:  .byte $FB, $F7, $EF, $DF, $BF, $7F, $FB, $F7, $EF, $DF, $BF, $7F, $FB, $F7, $EF, $DF

.code

; ---------------------------------------------------------------- drones
; add a signed byte (A) to the 16-bit pair lo,x / hi,x
.macro ADD_SIGNED lo, hi
        .local pos, done
        bpl pos
        clc
        adc lo,x
        sta lo,x
        lda hi,x
        adc #$FF
        sta hi,x
        jmp done
pos:    clc
        adc lo,x
        sta lo,x
        lda hi,x
        adc #0
        sta hi,x
done:
.endmacro

_update_drones:
        ldx #0
ud_loop:
        lda _dr_vx,x
        ADD_SIGNED _dr_xlo, _dr_xhi
        lda _dr_vy,x
        ADD_SIGNED _dr_ylo, _dr_yhi
        ; d = x - cam_px + 80 must be in 0..480 ($1E0)
        lda _dr_xlo,x
        sec
        sbc _cam_px
        sta dlo
        lda _dr_xhi,x
        sbc _cam_px+1
        sta dhi
        lda dlo
        clc
        adc #80
        sta dlo
        lda dhi
        adc #0
        sta dhi
        cmp #2
        jcs respawn             ; >= 2 (also negative): out
        cmp #1
        jne ud_y
        lda dlo
        cmp #$E1
        jcs respawn
ud_y:   ; d = y - cam_py + 60 must be in 0..300 ($12C)
        lda _dr_ylo,x
        sec
        sbc _cam_py
        sta dlo
        lda _dr_yhi,x
        sbc _cam_py+1
        sta dhi
        lda dlo
        clc
        adc #60
        sta dlo
        lda dhi
        adc #0
        sta dhi
        cmp #2
        jcs respawn
        cmp #1
        jne ud_jit
        lda dlo
        cmp #$2D
        jcs respawn
ud_jit: ; every 16 frames each drone picks a new vertical drift
        lda _frame
        and #15
        sta tmp1
        txa
        and #15
        cmp tmp1
        jne ud_next
        stx tmp2
        jsr _rand
        ldx tmp2
        lsr a
        lsr a
        lsr a
        lsr a
        and #3
        sec
        sbc #1
        sta _dr_vy,x
        jmp ud_next
respawn:
        stx tmp2
        txa
        jsr _spawn_drone        ; fastcall: A = i
        ldx tmp2
ud_next:
        inx
        cpx #NUM_DRONES
        jne ud_loop
        rts

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

        ; emit into the bank the IRQ is not reading
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
        sta tmp1                ; y of this entry
        lda k
        cmp #6
        jcc em_ok
        ; slot reuse: the entry six back must be at least 25 lines above
        ldy e
        lda _mux_y-6,y
        clc
        adc #25
        cmp tmp1
        jcc em_ok
        jeq em_ok
        jmp em_skip
em_ok:
        ldy k
        lda slot_of,y
        sta slot
        ldy e
        lda tmp1
        sta _mux_y,y
        lda slot
        sta _mux_slot,y
        asl a
        sta _mux_slot2,y
        sty tmp2
        ldy s_ord,x
        lda s_xlo,y
        sta tmp3
        lda s_col,y
        sta tmp4
        lda s_xhi,y
        pha
        ldy tmp2
        lda tmp3
        sta _mux_xlo,y
        lda tmp4
        sta _mux_col,y
        lda _frame
        and #8
        beq :+
        lda #SPR_PTR+SF_DRONE1
        bne :++
:       lda #SPR_PTR+SF_DRONE2
:       sta _mux_ptr,y
        sty tmp2
        ldy k
        lda inv_of,y
        ldy tmp2
        sta _mux_xand,y
        pla                     ; x high byte
        beq :+
        ldy slot
        lda bit_of,y
        ldy tmp2
        sta _mux_xor,y
        jmp :++
:       ldy tmp2
        lda #0
        sta _mux_xor,y
:       inc k
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

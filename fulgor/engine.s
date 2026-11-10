; engine.s - FULGOR: raster chain, 8-way scroll, multiplexer, collision
;
; What IRON VEIN proved and this keeps: screen buffers flipped in the
; bottom border (three of them here, see back_base), colour per world row, the HUD split written at line $48
; (or $49 when YSCROLL is 0), a double-banked multiplexer table.
;
; What is new here:
;   * the main loop COMMITS a frame (commit_ready); the frame interrupt
;     applies buffer, fine scroll, row colours and sprite table together,
;     or nothing. A late frame repeats the last picture whole instead of
;     tearing it.
;   * the screen copy is absolute,X code unrolled over the 22 rows, one
;     routine per direction and buffer: 9.2 cycles a byte against 16.
;     That is what pays for two pixels a frame.
;   * the preparation is a list of four units (half copy, column, row,
;     half copy) dealt out by how many frames remain before the crossing.
;   * all eight hardware sprites go through the multiplexer, hero included.
;   * no KERNAL: $01 = $35, vectors at $FFFA-$FFFF.

.include "assets.inc"
.include "defs.inc"
.macpack longbranch

.import _mt_tab, _mt_solid, _mt_oneway, _mt_flags, _level_map
.import _maprow_lo, _maprow_hi, _row_col, _row_bg, _row_var
.importzp sp

.export _sys_init, _video_on

.segment "ZEROPAGE"
irq_a:   .res 1
irq_x:   .res 1
irq_y:   .res 1
nmi_a:   .res 1
nmi_a1:  .res 1
er_mp:   .res 2                 ; map row pointer (edge writers)
er_dst:  .res 2                 ; screen destination (edge writers)
cp:      .res 2                 ; map row pointer (collision)
px:      .res 2                 ; collision probe, world pixels
py:      .res 2
zt:      .res 4                 ; scratch, main thread only
bm_cxl:  .res 1                 ; the multiplexer build's working set
bm_cxh:  .res 1
bm_cyl:  .res 1
bm_cyh:  .res 1
bm_i:    .res 1
bm_w:    .res 1
bm_pk:   .res 1
bm_ky:   .res 1
bm_obj:  .res 1
bm_d:    .res 1
bm_base: .res 1
bm_x:    .res 1

.data
front:        .byte 0           ; the buffer to show: 0 A, 1 B, 2 C
commit_ready: .byte 0
c_d018:       .byte D018_A
c_fx:         .byte 7
c_fy:         .byte 6
c_bank:       .byte 0
c_cnt:        .byte 0
c_cy:         .byte 0
c_cam:        .byte 0, 0, 0, 0
c_sky:        .byte 0
d_cy:         .byte 0
d_sky:        .byte 0
nm_d011:      .byte $17
mux_cnt:      .byte 0
m_n:          .byte 0
d_fx:         .byte 7           ; latched by the frame interrupt
d_fy:         .byte 6
mux_cur:      .byte 0           ; bank the interrupts are walking
mux_idx:      .byte 8
mux_bank:     .byte MUX_BANK    ; bank the main loop builds next
fill_n:       .byte 0
p_active:     .byte 0
p_units:      .byte 0
copy_stop:    .byte 0
_frame:       .byte 0

.bss
cam_x:   .res 2
cam_y:   .res 2
cur_cy:  .res 1
fill_r:  .res PF_ROWS
fill_c:  .res PF_ROWS
bf_r:    .res 1
s_f:     .res 2
s_v:     .res 2
s_ov:    .res 2
s_req:   .res 2
s_stl:   .res 2
s_t:     .res 2
s_want:  .res 2
s_tmin:  .res 1
s_fr:    .res 1
s_flip:  .res 1
p_d:     .res 2
pn_x:    .res 2
pn_cy:   .res 1
dr_x:    .res 2
dr_wy:   .res 1
dr_mc:   .res 1
dr_ys:   .res 1
dc_x:    .res 2
dc_wy:   .res 1
dc_mc:   .res 1
dc_mr:   .res 1
dc_n:    .res 1
rd_n:    .res 1
fl_ow:   .res 1
mc_row:  .res 1
mc_col:  .res 1
tc_row:  .res 1
tc_col:  .res 1
tc_n:    .res 1

; objects: structure of arrays
o_st:    .res NUM_OBJ
o_xl:    .res NUM_OBJ
o_xh:    .res NUM_OBJ
o_yl:    .res NUM_OBJ
o_yh:    .res NUM_OBJ
o_ptr:   .res NUM_OBJ
o_col:   .res NUM_OBJ

; multiplexer tables, two banks
m_y:     .res 2*MUX_BANK
m_xl:    .res 2*MUX_BANK
m_ptr:   .res 2*MUX_BANK
m_col:   .res 2*MUX_BANK
m_d010:  .res 2*MUX_BANK
s_y:     .res NUM_OBJ
s_x2:    .res NUM_OBJ
m_ord:   .res NUM_OBJ
o_in:    .res NUM_OBJ

.rodata
shr5:   .repeat 256, i
        .byte i >> 5
        .endrepeat
sub6:   .repeat 256, i
        .byte ((i >> 3) & 3) << 6
        .endrepeat
xbit:   .repeat 256, i
        .byte 8 >> ((i >> 3) & 3)
        .endrepeat
shl3:   .repeat 32, i
        .byte (i << 3) & $ff
        .endrepeat
mttab_lo:
        .repeat 16, i
        .byte <(_mt_tab + i * 64)
        .endrepeat
mttab_hi:
        .repeat 16, i
        .byte >(_mt_tab + i * 64)
        .endrepeat
row_lo: .repeat PF_ROWS, r
        .byte <(SCREEN_A + PF_OFF + r * 40)
        .endrepeat
rowa_hi: .repeat PF_ROWS, r
        .byte >(SCREEN_A + PF_OFF + r * 40)
        .endrepeat
back_hi: .byte >(SCREEN_B + PF_OFF), >(SCREEN_C + PF_OFF), >(SCREEN_A + PF_OFF)
d018_tab: .byte D018_A, D018_B, D018_C
bit_of: .byte 1, 2, 4, 8, 16, 32, 64, 128
en_tab: .byte 0, 1, 3, 7, 15, 31, 63, 127, 255
rung_lo: .byte <rung0, <rung1, <rung2, <rung3, <rung4, <rung5, <rung6, <rung7, <rung8
rung_hi: .byte >rung0, >rung1, >rung2, >rung3, >rung4, >rung5, >rung6, >rung7, >rung8
; frames until a crossing: ceil(distance / speed), speed 1..4, distance 1..8
tdiv:   .byte 1, 2, 3, 4, 5, 6, 7, 8
        .byte 1, 1, 2, 2, 3, 3, 4, 4
        .byte 1, 1, 1, 2, 2, 2, 3, 3
        .byte 1, 1, 1, 1, 2, 2, 2, 2
popc:   .repeat 64, i
        .byte (i & 1) + ((i >> 1) & 1) + ((i >> 2) & 1) + ((i >> 3) & 1) + ((i >> 4) & 1) + ((i >> 5) & 1)
        .endrepeat
nbit_of: .byte $fe, $fd, $fb, $f7, $ef, $df, $bf, $7f

; ====================================================================
; unrolled code: the screen copies and the colour row fills
; ====================================================================
.segment "HICODE"

; Copy the playfield from SRC to DST displaced by (DX, DY) characters,
; columns X down to copy_stop+1. The entering row and column are left
; alone: the edge writers own them, in any order.
.macro MKCOPY SRC, DST, DX, DY
        .local lp, dn
lp:
        .repeat PF_ROWS, r
          .if (r + (DY) >= 0) .and (r + (DY) < PF_ROWS)
            lda SRC + PF_OFF + (r + (DY)) * 40 + (DX), x
            sta DST + PF_OFF + r * 40, x
          .endif
        .endrepeat
        dex
        cpx copy_stop
        beq dn
        jmp lp
dn:     rts
.endmacro

cp0a:   MKCOPY SCREEN_A, SCREEN_B, -1, -1
cp0b:   MKCOPY SCREEN_B, SCREEN_C, -1, -1
cp0c:   MKCOPY SCREEN_C, SCREEN_A, -1, -1
cp1a:   MKCOPY SCREEN_A, SCREEN_B,  0, -1
cp1b:   MKCOPY SCREEN_B, SCREEN_C,  0, -1
cp1c:   MKCOPY SCREEN_C, SCREEN_A,  0, -1
cp2a:   MKCOPY SCREEN_A, SCREEN_B,  1, -1
cp2b:   MKCOPY SCREEN_B, SCREEN_C,  1, -1
cp2c:   MKCOPY SCREEN_C, SCREEN_A,  1, -1
cp3a:   MKCOPY SCREEN_A, SCREEN_B, -1,  0
cp3b:   MKCOPY SCREEN_B, SCREEN_C, -1,  0
cp3c:   MKCOPY SCREEN_C, SCREEN_A, -1,  0
cp5a:   MKCOPY SCREEN_A, SCREEN_B,  1,  0
cp5b:   MKCOPY SCREEN_B, SCREEN_C,  1,  0
cp5c:   MKCOPY SCREEN_C, SCREEN_A,  1,  0
cp6a:   MKCOPY SCREEN_A, SCREEN_B, -1,  1
cp6b:   MKCOPY SCREEN_B, SCREEN_C, -1,  1
cp6c:   MKCOPY SCREEN_C, SCREEN_A, -1,  1
cp7a:   MKCOPY SCREEN_A, SCREEN_B,  0,  1
cp7b:   MKCOPY SCREEN_B, SCREEN_C,  0,  1
cp7c:   MKCOPY SCREEN_C, SCREEN_A,  0,  1
cp8a:   MKCOPY SCREEN_A, SCREEN_B,  1,  1
cp8b:   MKCOPY SCREEN_B, SCREEN_C,  1,  1
cp8c:   MKCOPY SCREEN_C, SCREEN_A,  1,  1

; index: direction * 3 + the buffer on screen
copy_lo:
        .byte <cp0a, <cp0b, <cp0c
        .byte <cp1a, <cp1b, <cp1c
        .byte <cp2a, <cp2b, <cp2c
        .byte <cp3a, <cp3b, <cp3c
        .byte 0, 0, 0
        .byte <cp5a, <cp5b, <cp5c
        .byte <cp6a, <cp6b, <cp6c
        .byte <cp7a, <cp7b, <cp7c
        .byte <cp8a, <cp8b, <cp8c
copy_hi:
        .byte >cp0a, >cp0b, >cp0c
        .byte >cp1a, >cp1b, >cp1c
        .byte >cp2a, >cp2b, >cp2c
        .byte >cp3a, >cp3b, >cp3c
        .byte 0, 0, 0
        .byte >cp5a, >cp5b, >cp5c
        .byte >cp6a, >cp6b, >cp6c
        .byte >cp7a, >cp7b, >cp7c
        .byte >cp8a, >cp8b, >cp8c

FILL_LEN = 40 * 3 + 1
fill_rt:
        .repeat PF_ROWS, r
          .repeat 40, c
            sta COLRAM + PF_OFF + r * 40 + c
          .endrepeat
          rts
        .endrepeat
fill_lo:
        .repeat PF_ROWS, r
        .byte <(fill_rt + r * FILL_LEN)
        .endrepeat
fill_hi:
        .repeat PF_ROWS, r
        .byte >(fill_rt + r * FILL_LEN)
        .endrepeat

.code

; ====================================================================
; start-up
; ====================================================================
_sys_init:
        sei
        lda #$00                ; the C stack goes under the I/O area
        sta sp
        lda #$d0
        sta sp+1
        lda #$7f
        sta $dc0d
        sta $dd0d
        lda $dc0d
        lda $dd0d
        lda #0
        sta $d01a
        sta $d015
        sta $d020
        sta $d021
        sta $d418               ; silence
        lda #$0b                ; display off until the first frame
        sta $d011
        ; the ROM font: chars 0..63 of the upper-case set
        lda #$33
        sta $01
        ldx #0
@font:  lda $d000,x
        sta $5000,x
        lda $d100,x
        sta $5100,x
        inx
        bne @font
        lda #$35                ; RAM everywhere, I/O at $D000
        sta $01
        lda #<nmi_first
        sta $fffa
        lda #>nmi_first
        sta $fffb
        lda #<(312 * 63 - 1)    ; CIA 2 timer B: one PAL frame, to the cycle
        sta $dd06
        lda #>(312 * 63 - 1)
        sta $dd07
        jsr tb_sync
        lda #<irq_top
        sta $fffe
        lda #>irq_top
        sta $ffff
        lda $dd02               ; VIC bank 1: $4000-$7FFF
        ora #3
        sta $dd02
        lda $dd00
        and #$fc
        ora #2
        sta $dd00
        lda #0                  ; joystick lines are inputs
        sta $dc02
        lda #COL_MC1
        sta $d022
        lda #COL_MC2
        sta $d023
        lda #0
        sta $d025
        sta $d01b
        sta $d017
        sta $d01d
        lda #1
        sta $d026
        lda #$ff
        sta $d01c
        ; empty multiplexer tables: an unterminated one is walked into
        ; whatever follows it
        ldx #2*MUX_BANK-1
        lda #$ff
@mx:    sta m_y,x
        dex
        bpl @mx
        ldx #NUM_OBJ-1
        lda #0
@ob:    sta o_st,x
        sta o_in,x
        dex
        bpl @ob
        sta m_n
        ldx #$1f
@tl:    sta T_FRAMES,x
        dex
        bpl @tl
        rts

_video_on:
        sei
        lda #LINE_TOP
        sta $d012
        lda #$17
        sta $d011
        lda #$10
        sta $d016
        lda #D018_A
        sta $d018
        lda #$ff
        sta $d015
        sta $d019
        lda #1
        sta $d01a
        lda $dd0d
        lda #$83                ; timer A and B of CIA 2 raise the NMI
        sta $dd0d
        cli
        rts

; ====================================================================
; interrupts
; ====================================================================

; ---- line $F8: the frame. Apply a committed frame whole, or nothing.
irq_top:
        sta irq_a
        stx irq_x
        sty irq_y
        lda #1
        sta $d019
        lda #0                  ; no more sky bands this frame
        sta $dd0e
        lda #$17                ; HUD: 24 rows, YSCROLL 7
        sta $d011
        lda #$10                ; multicolour, 38 columns, XSCROLL 0
        sta $d016
        lda #0
        sta $d021
        lda commit_ready
        beq @late
        lda c_d018
        sta $d018
        lda c_fx
        sta d_fx
        lda c_fy
        sta d_fy
        lda c_bank
        sta mux_cur
        lda c_cnt
        sta mux_cnt
        lda c_cy
        sta d_cy
        lda c_sky
        sta d_sky
        lda c_cam               ; for check.py: the camera of the picture
        sta T_CAM               ; now on screen
        lda c_cam+1
        sta T_CAM+1
        lda c_cam+2
        sta T_CAM+2
        lda c_cam+3
        sta T_CAM+3
        ldx fill_n              ; row colours of a vertical step
        beq @nofill
@fl:    dex
        ldy fill_r,x
        lda fill_lo,y
        sta @fj+1
        lda fill_hi,y
        sta @fj+2
        lda fill_c,x
@fj:    jsr $ffff
        cpx #0
        bne @fl
        stx fill_n
@nofill:
        lda #0
        sta commit_ready
        inc T_FRAMES
        bne @spr
        inc T_FRAMES+1
        jmp @spr
@late:  inc T_LATE
        bne @spr
        inc T_LATE+1
@spr:   ldy mux_cur             ; the first sprites of the frame, up to
        ldx mux_cnt             ; eight: enter the ladder at the right rung
        lda en_tab,x
        sta $d015
        lda rung_lo,x
        sta @j+1
        lda rung_hi,x
        sta @j+2
@j:     jmp $ffff
        .repeat 8, i
          .ident(.sprintf("rung%d", 8 - i)):
          lda m_y + 7 - i,y
          sta $d001 + (7 - i) * 2
          lda m_xl + 7 - i,y
          sta $d000 + (7 - i) * 2
          lda m_ptr + 7 - i,y
          sta SCREEN_A + $3f8 + 7 - i
          sta SCREEN_B + $3f8 + 7 - i
          sta SCREEN_C + $3f8 + 7 - i
          lda m_col + 7 - i,y
          sta $d027 + 7 - i
        .endrepeat
rung0:  tya
        clc
        adc mux_cnt
        tay
        sty mux_idx
        lda m_d010-1,y
        sta $d010
        lda #LINE_SPLIT
        sta $d012
        lda #<irq_split
        sta $fffe
        lda #>irq_split
        sta $ffff
        jsr snd_play
irq_exit:
        lda irq_a
        ldx irq_x
        ldy irq_y
        rti

; ---- line $46: the split between HUD and playfield
irq_split:
        sta irq_a
        stx irq_x
        sty irq_y
        lda #1
        sta $d019
        ldx #$48
        lda d_fy
        bne :+
        inx                     ; YSCROLL 0 would match line $48: use $49
:       stx @cmp+1
@wait:  lda $d012
@cmp:   cmp #$48
        bcc @wait
        lda d_fy                ; ECM on top of multicolour is an invalid
        ora #$50                ; mode: black. It hides the lines where
        sta $d011               ; the first row comes and goes, until the
        lda d_fx                ; frame NMI opens the playfield at $56.
        ora #$10
        sta $d016
        ldy d_cy                ; the first row's sky, under the black
        lda _row_bg,y
        sta $d021
        iny
        sty nb_c+1              ; the bands go on from the second row
        ldx d_fy
        lda nmi_vl,x
        sta $fffa
        txa
        ora #$10
        sta nm_d011
        lda d_sky               ; timer A is stopped: its latch can be
        beq @flat               ; set now, and the NMI only starts it
        lda d1_lo,x
        sta $dd04
        lda d1_hi,x
        sta $dd05
        jmp @armed
@flat:  lda #$ff                ; one colour all the way down: no bands
        sta $dd04
        sta $dd05
@armed:
        ldy mux_idx
        lda m_y,y
        cmp #$ff
        beq to_top
        sec
        sbc #MUX_LEAD
        sta $d012
        lda #<irq_mux
        sta $fffe
        lda #>irq_mux
        sta $ffff
        jmp irq_exit

to_top: lda #LINE_TOP
        sta $d012
        lda #<irq_top
        sta $fffe
        lda #>irq_top
        sta $ffff
        jmp irq_exit

; ---- one multiplexed sprite per interrupt (more if they are close)
irq_mux:
        sta irq_a
        stx irq_x
        sty irq_y
        lda #1
        sta $d019
mux_body:
        ldy mux_idx
        tya
        and #7
        tax                     ; hardware sprite
        lda m_ptr,y
        sta SCREEN_A + $3f8,x
        sta SCREEN_B + $3f8,x
        sta SCREEN_C + $3f8,x
        lda m_col,y
        sta $d027,x
        txa
        asl a
        tax
        lda m_xl,y
        sta $d000,x
        lda m_y,y
        sta $d001,x
        lda m_d010,y
        sta $d010
        iny
        sty mux_idx
        lda m_y,y
        cmp #$ff
        beq to_top
        sec
        sbc #MUX_LEAD
        sta $d012
        cmp $d012
        bcc mux_body            ; that line has passed: do it now
        beq mux_body
        jmp irq_exit

; ====================================================================
; the sky
; ====================================================================
; Background colour belongs to the world row, like colour RAM does, and
; is changed on the last raster line of every character row by an NMI
; from CIA 2 timer A, eight lines (504 cycles) apart. The chain is
; started each frame by timer B, which runs one frame long and so fires
; at the same raster position for ever: line $55, the line before the
; playfield opens. Its handler removes its own jitter by reading the
; timer and sliding, then starts timer A to the cycle.
;
; The vertical fine scroll moves the rows, so the first delay depends on
; YSCROLL (d1 table). When YSCROLL is 5, line $55 is itself a bad line
; and the handler arrives 40 cycles late: that case has its own entry.
;
; Everything here is counted in cycles; the segment is page-aligned so
; that no branch crosses a page.

TB_LINE  = $54
TB_DELAY = 8
K1       = 172                  ; timer B low byte at the slide, on time
K2       = 121                  ; ... and after a bad line
D1N_ADJ  = 15                   ; cycles from timer A's start to cycle 40
D1B      = 376

.segment "TIMING"

tb_sync:
        lda #$20                ; well away from the raster wrap
:       cmp $d012
        bne :-
        lda $d012
@sy:    sta zt                  ; 62 cycles a turn: each read is a cycle
        ldx #9                  ; earlier in its line than the last, until
:       dex                     ; two reads fall in the same line - the
        bne :-                  ; second one on its last cycle
        bit zt
        lda $d012
        cmp zt
        bne @sy
@w:     lda $d012               ; 63 cycles a turn, to the line we want
        cmp #TB_LINE
        beq @go
        ldx #9
:       dex
        bne :-
        nop
        nop
        nop
        jmp @w
@go:    ldx #TB_DELAY
:       dex
        bne :-
        lda #$11
        sta $dd0f               ; load and run, continuous
        rts

; All three handlers live in this one page, so only the low byte of the
; NMI vector ever changes. Timer A's first count is put in its latch by
; the split interrupt while the timer is stopped, so starting it here is
; one write. Timer B is acknowledged before that: with YSCROLL 7 the
; first band is due 52 cycles later, and an underflow that found the NMI
; line still held down would be lost - the whole sky would slip a row.
; For the same reason the two kinds of handler save A in different
; places.
nmi_first:
        sta nmi_a1
        lda #<nmi_band
        sta $fffa
        lda $dd0d
        lda #K1
        sec
        sbc $dd06
        sta nf_b+1
nf_b:   bpl nf_s
nf_s:   .byte $c9, $c9, $c9, $c9, $c9, $c9, $c9, $c9, $c5, $ea
        lda #$11                ; on the cycle from here
        sta $dd0e               ; timer A: load and run
        lda nm_d011
nf_w:   sta $d011               ; open the playfield (ECM off), in the
                                ; blanking at the end of line $55
    .ifdef PROFILE
        lda nf_b+1              ; (the slide's offset, for calibration)
        sta T_DBG
    .endif
nf_end: lda #<(504 - 1)
        sta $dd04
        lda #>(504 - 1)
        sta $dd05               ; every later band: eight lines
        lda nmi_a1
        rti

; YSCROLL 5: line $55 is a bad line and the CPU is held from cycle 12 to
; 54 whatever it was doing. That is a synchronisation in itself - the
; first two instructions always end just after it - so the playfield is
; opened at once, and the slide then squares the timer start.
nmi_first_bad:
        sta nmi_a1
        lda nm_d011
nfb_w:  sta $d011
        lda #<nmi_band
        sta $fffa
        lda $dd0d
        lda #K2
        sec
        sbc $dd06
        sta nfb_b+1
nfb_b:  bpl nfb_s
nfb_s:  .byte $c9, $c9, $c9, $c9, $c9, $c9, $c9, $c9, $c5, $ea
        lda #$11
        sta $dd0e
    .ifdef PROFILE
        lda nfb_b+1
        sta T_DBG+1
    .endif
        jmp nf_end

nmi_band:
        sta nmi_a
nb_c:   lda _row_bg
nb_w:   sta $d021
        inc nb_c+1
        lda $dd0d
        lda nmi_a
        rti

.assert >tb_sync = >nmi_band, error, "timing code crosses a page"

.rodata
nmi_vl: .byte <nmi_first, <nmi_first, <nmi_first, <nmi_first
        .byte <nmi_first, <nmi_first_bad, <nmi_first, <nmi_first
; lines from $55 to the last line of the first row, by YSCROLL
.define D1N(n) ((n) * 63 - D1N_ADJ)
d1_lo:  .byte <D1N(2), <D1N(3), <D1N(4), <D1N(5), <D1N(6), <D1B, <D1N(8), <D1N(1)
d1_hi:  .byte >D1N(2), >D1N(3), >D1N(4), >D1N(5), >D1N(6), >D1B, >D1N(8), >D1N(1)

.code

; ====================================================================
; frame hand-over
; ====================================================================
_wait_commit:
@w:     lda commit_ready
        bne @w
        inc _frame
        rts

_commit:
        lda cam_x
        and #7
        eor #7
        sta c_fx                ; XSCROLL = 7 - fx
        lda cam_y
        and #7
        sta zt
        lda #6
        sec
        sbc zt
        and #7
        sta c_fy                ; YSCROLL = (6 - fy) & 7
        ldx front
        lda d018_tab,x
        sta c_d018
        jsr calc_cy
        sta c_cy
        tay
        lda _row_var,y
        sta c_sky
        lda cam_x
        sta c_cam
        lda cam_x+1
        sta c_cam+1
        lda cam_y
        sta c_cam+2
        lda cam_y+1
        sta c_cam+3
        jsr elapsed
        sta T_LOAD
        cmp T_LOADMAX
        bcc :+
        sta T_LOADMAX
:       lsr a                   ; histogram, eight bins of 32 lines
        lsr a
        lsr a
        lsr a
        lsr a
        tax
        inc T_HIST,x
        lda #1
        sta commit_ready
        rts

; A = raster lines since the frame interrupt (255 if more).
elapsed:
        lda $d012
        ldx $d011
        cmp $d012
        beq :+
        lda $d012
        ldx $d011
:       cpx #$80
        bcs @hi                 ; lines 256..311
        cmp #LINE_TOP
        bcs @same               ; 248..255: still the frame's first lines
        clc
        adc #(312 - LINE_TOP)   ; lines 0..247
        bcc @r
        lda #$ff
        rts
@hi:    clc
        adc #(256 - LINE_TOP)
        rts
@same:  sec
        sbc #LINE_TOP
@r:     rts

; Profiler checkpoint Y (0..7): elapsed lines, last and worst.
_prof:
        jsr elapsed
        sta T_PROF,y
        cmp T_PROF+8,y
        bcc :+
        sta T_PROF+8,y
:       rts

; ====================================================================
; drawing the map
; ====================================================================

; cur_cy = cam_y >> 3 (also returned in A)
calc_cy:
        lda cam_y
        lsr a
        lsr a
        lsr a
        sta cur_cy
        lda cam_y+1
        asl a
        asl a
        asl a
        asl a
        asl a
        ora cur_cy
        sta cur_cy
        rts

; draw_row: 40 cells of world char row dr_wy, from world pixel dr_x (low
; three bits ignored), into (er_dst),0..39. Four cells per metatile, the
; four tables of this character row patched into the loop.
draw_row:
        lda dr_wy
        lsr a
        lsr a
        tax
        lda _maprow_lo,x
        sta er_mp
        lda _maprow_hi,x
        sta er_mp+1
        lda dr_wy
        and #3
        asl a
        asl a
        tax
        lda mttab_lo,x
        sta dr_c0+1
        lda mttab_hi,x
        sta dr_c0+2
        lda mttab_lo+1,x
        sta dr_c1+1
        lda mttab_hi+1,x
        sta dr_c1+2
        lda mttab_lo+2,x
        sta dr_c2+1
        lda mttab_hi+2,x
        sta dr_c2+2
        lda mttab_lo+3,x
        sta dr_c3+1
        lda mttab_hi+3,x
        sta dr_c3+2
        ldx dr_x+1
        lda shl3,x
        ldx dr_x
        ora shr5,x
        sta dr_mc               ; metatile column
        lda dr_x
        lsr a
        lsr a
        lsr a
        and #3
        sta zt                  ; sub column of the first cell
        ldy dr_mc
        lda (er_mp),y
        tax
        inc dr_mc
        ldy #0
        lda zt
        beq dr_c0
        cmp #2
        bcc dr_c1
        beq dr_c2
        bne dr_c3
dr_next:
        sty dr_ys
        ldy dr_mc
        lda (er_mp),y
        tax
        inc dr_mc
        ldy dr_ys
dr_c0:  lda $ffff,x
        sta (er_dst),y
        iny
        cpy #40
        beq dr_done
dr_c1:  lda $ffff,x
        sta (er_dst),y
        iny
        cpy #40
        beq dr_done
dr_c2:  lda $ffff,x
        sta (er_dst),y
        iny
        cpy #40
        beq dr_done
dr_c3:  lda $ffff,x
        sta (er_dst),y
        iny
        cpy #40
        bne dr_next
dr_done:
        rts

; draw_col: 22 cells of the world column at pixel dc_x, from world char
; row dc_wy downwards, into (er_dst) stepping a screen row each time.
draw_col:
        ldx dc_x+1
        lda shl3,x
        ldx dc_x
        ora shr5,x
        sta dc_mc
        lda dc_x
        lsr a
        lsr a
        lsr a
        and #3
        tax                     ; sub column
        lda mttab_lo,x
        sta dc_s0+1
        lda mttab_hi,x
        sta dc_s0+2
        lda mttab_lo+4,x
        sta dc_s1+1
        lda mttab_hi+4,x
        sta dc_s1+2
        lda mttab_lo+8,x
        sta dc_s2+1
        lda mttab_hi+8,x
        sta dc_s2+2
        lda mttab_lo+12,x
        sta dc_s3+1
        lda mttab_hi+12,x
        sta dc_s3+2
        lda dc_wy
        lsr a
        lsr a
        sta dc_mr
        lda #PF_ROWS
        sta dc_n
        jsr dc_fetch
        ldy #0
        lda dc_wy
        and #3
        beq dc_s0
        cmp #2
        bcc dc_s1
        beq dc_s2
        bne dc_s3
dc_s0:  lda $ffff,x
        sta (er_dst),y
        jsr dc_adv
        beq dc_done
dc_s1:  lda $ffff,x
        sta (er_dst),y
        jsr dc_adv
        beq dc_done
dc_s2:  lda $ffff,x
        sta (er_dst),y
        jsr dc_adv
        beq dc_done
dc_s3:  lda $ffff,x
        sta (er_dst),y
        jsr dc_adv
        beq dc_done
        inc dc_mr
        jsr dc_fetch
        ldy #0
        jmp dc_s0
dc_done:
        rts

dc_fetch:
        ldx dc_mr
        lda _maprow_lo,x
        sta er_mp
        lda _maprow_hi,x
        sta er_mp+1
        ldy dc_mc
        lda (er_mp),y
        tax
        rts

dc_adv: lda er_dst
        clc
        adc #40
        sta er_dst
        bcc :+
        inc er_dst+1
:       dec dc_n                ; Z: that was the last cell
        rts

; Paint both screens and the colour rows for the camera as it stands.
_scroll_init:
        lda #0
        sta front
        sta p_active
        sta p_units
        sta fill_n
        jsr calc_cy
        lda #<(SCREEN_A + PF_OFF)   ; the front buffer only: the others
        ldx #>(SCREEN_A + PF_OFF)   ; are rebuilt before they are shown
        jsr redraw
        ldx #0
@c:     stx zt+1
        txa
        clc
        adc cur_cy
        tay
        lda fill_lo,x
        sta @j+1
        lda fill_hi,x
        sta @j+2
        lda _row_col,y
@j:     jsr $ffff
        ldx zt+1
        inx
        cpx #PF_ROWS
        bne @c
        rts

redraw: sta er_dst
        stx er_dst+1
        lda cam_x
        and #$f8
        sta dr_x
        lda cam_x+1
        sta dr_x+1
        lda cur_cy
        sta dr_wy
        lda #PF_ROWS
        sta rd_n
@r:     jsr draw_row
        inc dr_wy
        lda er_dst
        clc
        adc #40
        sta er_dst
        bcc :+
        inc er_dst+1
:       dec rd_n
        bne @r
        rts

; ====================================================================
; the scroll step
; ====================================================================
; In: s_v = the camera's wish for this frame, signed pixels per axis
; (x up to 3, y up to 4), already clamped to the map. A coarse boundary
; is crossed only into a buffer prepared for exactly that step; otherwise
; the axis waits at the boundary. Then the next crossing is predicted and
; the preparation is dealt out over the frames that remain.

_scroll_step:
        lda cam_x
        and #7
        sta s_f
        lda cam_y
        and #7
        sta s_f+1
        lda #0
        sta s_flip
        ldx #1
@rq:    lda s_v,x
        sta s_ov,x
        lda #0
        sta s_stl,x
        lda s_f,x
        clc
        adc s_v,x
        bmi @neg
        cmp #8
        bcc @none
        lda #1
        bne @set
@neg:   lda #$ff
        bne @set
@none:  lda #0
@set:   sta s_req,x
        dex
        bpl @rq

        lda s_req
        ora s_req+1
        beq @move
        lda p_active
        beq @stall
        lda p_units
        bne @stall
        ; a finished buffer: is it the one this crossing needs?
        ldx #1
@ck:    lda p_d,x
        cmp s_req,x
        beq @ckn
        lda p_d,x
        bne @stall              ; shifted on an axis that is not crossing
        jsr stall_axis          ; not shifted here: this axis waits
@ckn:   dex
        bpl @ck
        inc s_flip
        bne @move
@stall: ldx #1
@sa:    jsr stall_axis
        dex
        bpl @sa
        inc T_STALLS
        bne @move
        inc T_STALLS+1

@move:  lda s_v
        bmi @xn
        clc
        adc cam_x
        sta cam_x
        bcc @my
        inc cam_x+1
        bne @my
@xn:    clc
        adc cam_x
        sta cam_x
        bcs @my
        dec cam_x+1
@my:    lda s_v+1
        bmi @yn
        clc
        adc cam_y
        sta cam_y
        bcc @mv
        inc cam_y+1
        bne @mv
@yn:    clc
        adc cam_y
        sta cam_y
        bcs @mv
        dec cam_y+1
@mv:    lda s_flip
        beq @predict
        ldx front               ; the next of the three buffers
        inx
        cpx #3
        bcc :+
        ldx #0
:       stx front
        lda #0
        sta p_active
        inc T_FLIPS
        bne :+
        inc T_FLIPS+1
:       lda p_d+1
        beq @predict
        jsr build_fills

        ; ---- when is the next crossing, and of which axes?
@predict:
        lda cam_x
        and #7
        sta s_f
        lda cam_y
        and #7
        sta s_f+1
        ldx #1
@pt:    lda s_ov,x
        bne :+
        lda #255
        bne @pst
:       ldy s_stl,x
        beq :+
        lda #1
        bne @pst
:       cmp #0
        bmi @pn
        sec                     ; moving up the axis: distance 8 - f
        sbc #1
        asl a
        asl a
        asl a
        sta zt
        lda #7
        sec
        sbc s_f,x
        ora zt
        tay
        lda tdiv,y
        bne @pst
@pn:    eor #$ff                ; moving down it: distance f + 1
        asl a
        asl a
        asl a
        ora s_f,x
        tay
        lda tdiv,y
@pst:   sta s_t,x
        dex
        bpl @pt

        lda s_t
        cmp s_t+1
        bcc :+
        lda s_t+1
:       sta s_tmin
        cmp #255
        jeq @run                ; standing still: finish what is under way
        clc
        adc #2
        sta zt                  ; an axis within one frame of the first
        lda #0                  ; one crosses together with it
        sta s_fr
        ldx #1
@w:     lda #0
        sta s_want,x
        lda s_t,x
        cmp zt
        bcs @wn
        cmp s_fr
        bcc :+
        sta s_fr
:       lda s_ov,x
        bmi :+
        lda #1
        bne @ws
:       lda #$ff
@ws:    sta s_want,x
@wn:    dex
        bpl @w
        lda p_active
        beq @start
        lda s_want
        cmp p_d
        bne @start
        lda s_want+1
        cmp p_d+1
        beq @run
@start: lda s_want
        sta p_d
        lda s_want+1
        sta p_d+1
        lda #U_CA|U_CB|U_CC|U_CD
        ldx p_d
        beq :+
        ora #U_EC
:       ldx p_d+1
        beq :+
        ora #U_ER
:       sta p_units
        lda #1
        sta p_active

        ; ---- deal out the work over the frames that remain: one unit
        ; a frame if there is time, two if not, never more. Two bands of
        ; the copy are 5 300 cycles; a third unit on top of them was the
        ; 130-line frame the profiler caught. A wait at the boundary is
        ; kinder than a lost frame.
@run:   lda p_active
        beq @done
        ldx p_units
        beq @done
        lda s_tmin
        cmp #255
        beq @one
        lda s_fr
        cmp popc,x
        bcs @one
        jsr run_unit
        lda p_units
        beq @done
@one:   jsr run_unit
@done:  rts

; Hold axis X at its boundary this frame.
stall_axis:
        lda s_req,x
        beq @r
        bmi @n
        lda #7
        bne @m
@n:     lda #0
@m:     sec
        sbc s_f,x
        sta s_v,x
        lda #1
        sta s_stl,x
        lda #0
        sta s_req,x
@r:     rts

; Row colours after a vertical step: only the rows whose colour differs
; from the row that stood there before. The frame interrupt paints them.
build_fills:
        jsr calc_cy
        tay
        lda #0
        sta bf_r
        lda p_d+1
        bmi @up
@dn:    lda _row_col,y
        cmp _row_col-1,y
        beq :+
        jsr @add
:       iny
        inc bf_r
        lda bf_r
        cmp #PF_ROWS
        bne @dn
        rts
@up:    lda _row_col,y
        cmp _row_col+1,y
        beq :+
        jsr @add
:       iny
        inc bf_r
        lda bf_r
        cmp #PF_ROWS
        bne @up
        rts
@add:   ldx fill_n
        sta fill_c,x
        lda bf_r
        sta fill_r,x
        inc fill_n
        rts

; Run the first pending unit.
run_unit:
        lda p_units
        lsr a
        bcs @ec
        lsr a
        bcs @er
        lsr a
        bcs @ca
        lsr a
        bcs @cb
        lsr a
        bcs @cc
        lsr a
        bcs @cd
        rts
@ec:    lda p_units
        and #<~U_EC
        sta p_units
        jmp unit_col
@er:    lda p_units
        and #<~U_ER
        sta p_units
        jmp unit_row
@ca:    lda p_units
        and #<~U_CA
        sta p_units
        lda #29
        sta copy_stop
        ldx #39
        lda p_d
        cmp #1
        bne do_copy
        dex                     ; moving right: column 39 is the edge's
        bne do_copy
@cb:    lda p_units
        and #<~U_CB
        sta p_units
        lda #19
        sta copy_stop
        ldx #29
        bne do_copy
@cc:    lda p_units
        and #<~U_CC
        sta p_units
        lda #9
        sta copy_stop
        ldx #19
        bne do_copy
@cd:    lda p_units
        and #<~U_CD
        sta p_units
        ldy #$ff
        lda p_d
        bpl :+
        ldy #0                  ; moving left: column 0 is the edge's
:       sty copy_stop
        ldx #9

do_copy:
        stx zt
        lda p_d+1
        clc
        adc #1
        sta zt+1
        asl a
        clc
        adc zt+1                ; (dy + 1) * 3
        clc
        adc p_d
        clc
        adc #1                  ; + dx + 1
        sta zt+1
        asl a
        clc
        adc zt+1
        clc
        adc front               ; direction * 3 + front
        tay
        lda copy_lo,y
        sta @j+1
        lda copy_hi,y
        sta @j+2
        ldx zt
@j:     jmp $ffff

; The coarse origin of the buffer under preparation.
prep_origin:
        lda cam_x
        and #$f8
        sta pn_x
        lda cam_x+1
        sta pn_x+1
        lda p_d
        beq @y
        bmi @m
        clc
        lda pn_x
        adc #8
        sta pn_x
        bcc @y
        inc pn_x+1
        bne @y
@m:     sec
        lda pn_x
        sbc #8
        sta pn_x
        bcs @y
        dec pn_x+1
@y:     jsr calc_cy
        clc
        adc p_d+1
        sta pn_cy
        rts

; A/X = the playfield of the buffer being prepared: the one after the
; front one, of three. There are three because the step from one page to
; the next is decided a frame before it is seen: the buffer on screen
; must be left alone until then, and the new front one is what the next
; preparation reads. With two buffers the first band of that
; preparation - the ten columns on the right - went into the picture
; that was still being shown. The human saw it; the counters did not.
back_base:
        ldx front
        lda back_hi,x
        tax
        lda #<(SCREEN_A + PF_OFF)
        rts

unit_col:
        jsr prep_origin
        lda p_d
        bmi @left
        clc
        lda pn_x
        adc #<(39 * 8)
        sta dc_x
        lda pn_x+1
        adc #>(39 * 8)
        sta dc_x+1
        ldy #39
        bne @go
@left:  lda pn_x
        sta dc_x
        lda pn_x+1
        sta dc_x+1
        ldy #0
@go:    sty zt
        jsr back_base
        clc
        adc zt
        sta er_dst
        txa
        adc #0
        sta er_dst+1
        lda pn_cy
        sta dc_wy
        jmp draw_col

unit_row:
        jsr prep_origin
        lda pn_x
        sta dr_x
        lda pn_x+1
        sta dr_x+1
        lda p_d+1
        bmi @top
        clc
        lda pn_cy
        adc #PF_ROWS-1
        sta dr_wy
        jsr back_base
        clc
        adc #<((PF_ROWS-1) * 40)
        sta er_dst
        txa
        adc #>((PF_ROWS-1) * 40)
        sta er_dst+1
        jmp draw_row
@top:   lda pn_cy
        sta dr_wy
        jsr back_base
        sta er_dst
        stx er_dst+1
        jmp draw_row

; ====================================================================
; the multiplexer table
; ====================================================================
; m_ord is the list of live objects, kept in Y order from frame to frame.
; Spawning code adds an object with obj_add (at the end) or obj_add_at
; (next to another one, when it is known to appear beside it); one
; whose o_st has gone to zero is dropped here.
;
; One pass: screen-space the object, write it straight into the bank
; the interrupts are not reading, and put it back in the list in its
; place. The table is therefore written in LAST frame's order: when two
; objects cross it is one frame behind, which the first eight sprites
; do not notice at all (they are all set at the top of the frame) and
; the multiplexed ones forgive (a line that has passed is done at once).
; Entry i goes to hardware sprite i & 7; an entry that would reuse a
; sprite still on screen is dropped for the frame.
;
; The first version was three passes over absolute variables and cost
; 340 cycles a sprite - 1 950 to show the hero alone. This is about 225,
; and nothing for objects that are not there.

; X = object: put it at the end of the list (once).
obj_add:
        lda o_in,x
        bne @r
        lda #1
        sta o_in,x
        ldy m_n
        txa
        sta m_ord,y
        inc m_n
@r:     rts

; X = object, A = an object already listed: put X right after it.
obj_add_at:
        sta zt
        lda o_in,x
        bne @r
        lda #1
        sta o_in,x
        stx zt+1
        ldy m_n
        inc m_n
@sh:    dey                     ; open a gap behind the neighbour
        bmi @put
        lda m_ord,y
        cmp zt
        beq @put
        sta m_ord+1,y
        jmp @sh
@put:   lda zt+1
        sta m_ord+1,y
        tax
@r:     rts

_build_mux:
        sec
        lda cam_x
        sbc #SCR_LEFT_X
        sta bm_cxl
        lda cam_x+1
        sbc #0
        sta bm_cxh
        sec
        lda cam_y
        sbc #SCR_TOP_Y
        sta bm_cyl
        lda cam_y+1
        sbc #0
        sta bm_cyh
        ldx mux_bank
        stx bm_base
        lda #0
        sta bm_d
        sta bm_i
        sta bm_w
        sta bm_pk
@lp:    ldy bm_i
        cpy m_n
        jcs @end
        lda m_ord,y
        inc bm_i
        tay
        lda o_st,y
        bne @live
        sta o_in,y              ; gone: off the list
        beq @lp
@unseen:
        lda #$ff
        ldy bm_obj
        sta s_y,y
        jmp @list
@live:  sty bm_obj
        sec
        lda o_yl,y
        sbc bm_cyl
        sta zt
        lda o_yh,y
        sbc bm_cyh
        bne @unseen
        lda zt
        cmp #CLIP_TOP
        bcc @unseen
        cmp #CLIP_BOT
        bcs @unseen
        sec
        lda o_xl,y
        sbc bm_cxl
        sta zt+1
        lda o_xh,y
        sbc bm_cxh
        beq @x0
        cmp #1
        bne @unseen
        lda zt+1
        cmp #88                 ; past the right border
        bcs @unseen
        lda #$ff
        bne @xk
@x0:    lda zt+1
        cmp #8                  ; wholly under the left border
        bcc @unseen
        lda #0
@xk:    sta zt+2
        lsr a                   ; half the 9-bit X in a byte, for the
        lda zt+1                ; collision tests
        ror a
        sta s_x2,y
        lda zt
        sta s_y,y
        txa                     ; its hardware sprite, eight entries
        sec                     ; back, must be clear of the beam
        sbc bm_base
        cmp #8
        bcc @ok
        lda m_y-8,x
        clc
        adc #MUX_GAP
        bcs @keep
        cmp zt
        beq @ok
        bcs @keep
@ok:    lda zt
        sta m_y,x
        lda zt+1
        sta m_xl,x
        lda o_ptr,y
        sta m_ptr,x
        lda o_col,y
        sta m_col,x
        txa
        and #7
        tay
        lda bm_d
        and nbit_of,y
        bit zt+2
        bpl :+
        ora bit_of,y
:       sta bm_d
        sta m_d010,x
        inx
@keep:  lda zt
@list:  ldy bm_w                ; back into the list, in its place: nearly
        cmp bm_pk               ; always where it was
        bcs @last
        sta bm_ky
        stx bm_x
@ins:   lda m_ord-1,y
        sta m_ord,y
        dey
        beq @put
        ldx m_ord-1,y
        lda s_y,x
        cmp bm_ky
        beq @put
        bcs @ins
@put:   lda bm_obj
        sta m_ord,y
        ldx bm_x
        inc bm_w
        jmp @lp
@last:  sta bm_pk
        lda bm_obj
        sta m_ord,y
        inc bm_w
        jmp @lp
@end:   lda bm_w
        sta m_n
        lda #$ff
        sta m_y,x
        txa
        sec
        sbc bm_base
        cmp #8
        bcc :+
        lda #8
:       sta c_cnt
        lda mux_bank
        sta c_bank
        eor #MUX_BANK
        sta mux_bank
        rts

; ====================================================================
; collision
; ====================================================================
; solid_at: is the character cell at world pixel (px, py) solid?
;           Z clear (A != 0) if so. Above the map is air, below is rock.
; floor_at: the same, plus fl_ow != 0 if the cell is a one-way ledge.

map_cell:                       ; -> X = metatile | char row << 6; C set if off the map
        ldx py+1
        cpx #>(MAP_H * 32 + 255)
        bcs @off
        lda shl3,x
        ldx py
        ora shr5,x
        tax
        cpx #MAP_H
        bcs @off
        stx mc_row
        lda _maprow_lo,x
        sta cp
        lda _maprow_hi,x
        sta cp+1
        ldx px+1
        lda shl3,x
        ldx px
        ora shr5,x
        tay
        sty mc_col
        lda (cp),y
        ldx py
        ora sub6,x
        tax
        clc
        rts
@off:   sec
        rts

; flags_at: the flags of the metatile at (px, py); 0 off the map.
flags_at:
        jsr map_cell
        bcs @none
        txa
        and #$3f
        tay
        lda _mt_flags,y
        rts
@none:  lda #0
        rts

; take_cell: turn the metatile last looked at by map_cell into air, in
; the map and on the screen. Only the front buffer is touched; the
; buffer under preparation is thrown away and starts again from it.
take_cell:
        ldy mc_col
        lda #MT_AIR
        sta (cp),y
        lda #0
        sta p_active
        jsr calc_cy
        lda mc_row              ; first screen row of the cell, may be
        asl a                   ; off either end
        asl a
        sec
        sbc cur_cy
        sta tc_row
        lda cam_x               ; first screen column: col*4 - cam_x/8
        sta zt
        lda cam_x+1
        lsr a
        ror zt
        lsr a
        ror zt
        lsr a
        ror zt
        sta zt+1
        lda mc_col
        asl a
        asl a
        sec
        sbc zt
        sta tc_col
        lda #4
        sta tc_n
@row:   ldx tc_row
        cpx #PF_ROWS
        bcs @next               ; (negative rows are >= PF_ROWS too)
        lda front               ; the buffers are $400 apart
        asl a
        asl a
        clc
        adc rowa_hi,x
        sta er_dst+1
        lda row_lo,x
        sta er_dst
        ldx #4
        ldy tc_col
@cell:  cpy #40
        bcs :+
        lda #32
        sta (er_dst),y
:       iny
        dex
        bne @cell
@next:  inc tc_row
        dec tc_n
        bne @row
        rts

solid_at:
        jsr map_cell
        bcs off_map
        lda _mt_solid,x
        ldx px
        and xbit,x
        rts
off_map:
        lda py+1                ; negative: sky. Otherwise: bedrock.
        bmi :+
        lda #1
        rts
:       lda #0
        rts

floor_at:
        lda #0
        sta fl_ow
        jsr map_cell
        bcs off_map
        ldy px
        lda _mt_oneway,x
        and xbit,y
        sta fl_ow
        lda _mt_solid,x
        and xbit,y
        rts

; side_solid: A probes down the column at px, from py, eight pixels
; apart. The column and its bit are worked out once, and the map is read
; again only when a probe enters another metatile. Z clear if any cell
; is solid. (The hero's side: inside the map by construction.)
side_solid:
        sta sp_n
        ldx px+1
        lda shl3,x
        ldx px
        ora shr5,x
        sta sp_col
        lda xbit,x
        sta sp_bit
        lda #$ff
        sta sp_row
@l:     ldx py+1
        lda shl3,x
        ldx py
        ora shr5,x
        cmp sp_row
        beq @same
        sta sp_row
        tax
        lda _maprow_lo,x
        sta cp
        lda _maprow_hi,x
        sta cp+1
        ldy sp_col
        lda (cp),y
        sta sp_mt
        ldx py
@same:  lda sub6,x
        ora sp_mt
        tax
        lda _mt_solid,x
        and sp_bit
        bne @hit
        clc
        lda py
        adc #8
        sta py
        bcc :+
        inc py+1
:       dec sp_n
        bne @l
        lda #0
@hit:   rts

.bss
sp_n:    .res 1
sp_col:  .res 1
sp_bit:  .res 1
sp_row:  .res 1
sp_mt:   .res 1

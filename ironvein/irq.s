; irq.s - raster engine for IRON VEIN: 8-way scroll split + multiplexer
;
; Per frame:
;   irq_top   ($F8, bottom border)  flip $D018 latched with both fine
;             scroll values, put the HUD's $D011/$D016 back, park the
;             multiplexed sprites, bump the frame flag. Firing in the
;             bottom border gives the colour blast that follows the flip
;             a 140-line head start on the beam. Chains into the KERNAL
;             IRQ so jiffy/keyboard stay alive.
;   irq_split ($46, the last line of HUD row 1)  wait for line $48 -
;             or $49 when YSCROLL is 0, because a YSCROLL written on a
;             line whose low bits equal it fires a late bad line and
;             garbles the row - write it, then XSCROLL, then latch the
;             multiplexer bank and start it. The wait is ">=", never
;             "==": an interrupt that lands one line late (sprite DMA
;             near the HUD, a long instruction) used to spin for a whole
;             frame, and that frame showed the playfield with the HUD's
;             YSCROLL - a picture that jumped, and 19 600 cycles gone.
;   irq_mux   (variable) one hardware sprite per interrupt, sprites 2-7.
;
; Multiplexer table is double-banked: C fills the bank the IRQ is not
; reading and publishes its offset in _mux_next; irq_split latches it.
; Entry i of bank b lives at index b*MUX_BANK + i, $FF terminates.

.export _irq_init
.export _finex_next, _finey_next, _d018_next, _vsync_flag
.export _mux_y, _mux_xlo, _mux_slot, _mux_slot2
.export _mux_ptr, _mux_col, _mux_xand, _mux_xor, _mux_next
.export _vsync_count, _irq_count

LINE_TOP   = $F8
LINE_SPLIT = $46                ; last line of HUD row 1: not a bad line
MUX_BANK   = 32
SCR_A_PTR  = $47F8
SCR_B_PTR  = $4BF8

.data
_finex_next: .byte 7            ; XSCROLL for the playfield
_finey_next: .byte 6            ; YSCROLL for the playfield
disp_finex:  .byte 7
disp_finey:  .byte 6
_d018_next:  .byte $14          ; screen $4400, charset $5000
_vsync_flag: .byte 0
_vsync_count: .word 0        ; every irq_top, ever
_irq_count:   .byte 0        ; every interrupt entry, reset by C
_mux_next:   .byte 0            ; bank offset published by C
mux_idx:     .byte 0
target:      .byte 0

.bss
_mux_y:     .res 2*MUX_BANK
_mux_xlo:   .res 2*MUX_BANK
_mux_slot:  .res 2*MUX_BANK
_mux_slot2: .res 2*MUX_BANK
_mux_ptr:   .res 2*MUX_BANK
_mux_col:   .res 2*MUX_BANK
_mux_xand:  .res 2*MUX_BANK
_mux_xor:   .res 2*MUX_BANK

.code

_irq_init:
        sei
        lda #$7f
        sta $dc0d
        lda $dc0d
        lda #<irq_top
        sta $0314
        lda #>irq_top
        sta $0315
        lda #LINE_TOP
        sta $d012
        lda #$17                ; DEN, 24 rows, YSCROLL 7, raster bit 8 = 0
        sta $d011
        lda #$10                ; MCM, 38 columns, XSCROLL 0
        sta $d016
        lda #$01
        sta $d01a
        lda #$ff
        sta $d019
        cli
        rts

irq_top:
        lda #$01
        sta $d019
        lda _d018_next
        sta $d018
        lda _finex_next
        sta disp_finex
        lda _finey_next
        sta disp_finey
        lda #$17                ; HUD rows: 24-row mode, YSCROLL 7
        sta $d011
        lda #$10                ; HUD: MCM, 38 col, XSCROLL 0
        sta $d016
        lda #$ff                ; park sprites 2-7 out of sight
        sta $d005
        sta $d007
        sta $d009
        sta $d00b
        sta $d00d
        sta $d00f
        inc _vsync_flag
        inc _vsync_count
        bne :+
        inc _vsync_count+1
:       inc _irq_count
        lda #LINE_SPLIT
        sta $d012
        lda #<irq_split
        sta $0314
        lda #>irq_split
        sta $0315
        jmp $ea81               ; no KERNAL keyboard scan: 1200 cycles a frame

irq_split:
        lda #$01
        sta $d019
        inc _irq_count
        lda $d012               ; probe: latest line at which we got in
        cmp $0367
        bcc :+
        sta $0367
:
        ldx #$48
        lda disp_finey
        bne :+
        inx                     ; YSCROLL 0 would match line $48: use $49
:       ora #$10                ; DEN, 24 rows
        ; sync to the start of the chosen line; if we are somehow past
        ; it already, store now rather than wait a frame
        stx target
wait_line:
        lda $d012
        cmp target
        bcc wait_line
        lda disp_finey
        ora #$10
        sta $d011
        lda disp_finex
        ora #$10                ; MCM, 38 col
        sta $d016
        ; --- multiplexer ---
        ldy _mux_next
        sty mux_idx
        lda _mux_y,y
        cmp #$ff
        beq mux_done
        sec
        sbc #4
        cmp #LINE_SPLIT+4
        bcs :+
        lda #LINE_SPLIT+4
:       sta $d012
        lda #<irq_mux
        sta $0314
        lda #>irq_mux
        sta $0315
        jmp $ea81

irq_mux:
        lda #$01
        sta $d019
        inc _irq_count
mux_body:
        ldy mux_idx
        lda _mux_y,y
        cmp #$ff
        beq mux_done
        ldx _mux_slot2,y
        lda _mux_xlo,y
        sta $d000,x
        lda _mux_y,y
        sta $d001,x
        lda $d010
        and _mux_xand,y
        ora _mux_xor,y
        sta $d010
        ldx _mux_slot,y
        lda _mux_col,y
        sta $d027,x
        lda _mux_ptr,y
        sta SCR_A_PTR,x
        sta SCR_B_PTR,x
        iny
        sty mux_idx
        lda _mux_y,y
        cmp #$ff
        beq mux_done
        sec
        sbc #4
        sta $d012
        cmp $d012
        bcc mux_body
        beq mux_body
        lda #<irq_mux
        sta $0314
        lda #>irq_mux
        sta $0315
        jmp $ea81

mux_done:
        lda #LINE_TOP
        sta $d012
        lda #<irq_top
        sta $0314
        lda #>irq_top
        sta $0315
        jmp $ea81

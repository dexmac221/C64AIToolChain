; edges.s - the cells that enter the screen on a coarse step
;
; One row (40 cells) or one column (22 cells) fetched from the metatile
; map into the back screen and the colour shadow. In C this was 270
; cycles a cell - cc65 promotes every byte expression to int and calls
; its runtime; here it is ~55. Inputs are globals set by C.
;
;   edge_bs   -> destination in the back screen
;   edge_cs   -> destination in the colour shadow
;   edge_mp   -> level_map row containing the first cell
;   edge_cx   world char column of the first cell
;   edge_cy   world char row of the first cell
;
; Char id = mt_chars[mt*16 + (cy&3)*4 + (cx&3)]; mt < 16 so the three
; fields never overlap and it is an OR, not an add.

.export _edge_row, _edge_col
.export _edge_bs, _edge_mp, _edge_cx, _edge_cy
.import _mt_chars
.importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3, tmp4
.macpack longbranch

.data
_edge_bs: .word 0
_edge_mp: .word 0
_edge_cx: .byte 0
_edge_cy: .byte 0

.code

setup:
        lda _edge_bs
        sta ptr1
        lda _edge_bs+1
        sta ptr1+1
        lda _edge_mp
        sta ptr3
        lda _edge_mp+1
        sta ptr3+1
        rts

; ---- a row of 40 cells, cx advancing ----
_edge_row:
        jsr setup
        lda _edge_cy
        and #3
        asl a
        asl a
        sta tmp2                ; (cy&3)*4, constant for the row
        lda _edge_cx
        sta tmp1                ; cx
        lda #0
        sta tmp4                ; i
row_loop:
        lda tmp1
        lsr a
        lsr a
        tay
        lda (ptr3),y            ; metatile
        asl a
        asl a
        asl a
        asl a
        ora tmp2
        sta tmp3
        lda tmp1
        and #3
        ora tmp3
        tay
        lda _mt_chars,y         ; char
        ldy tmp4
        sta (ptr1),y
        inc tmp1
        iny
        sty tmp4
        cpy #40
        jne row_loop
        rts

; ---- a column of 22 cells, cy advancing, map row every 4 ----
_edge_col:
        jsr setup
        lda _edge_cx
        lsr a
        lsr a
        sta tmp2                ; cx>>2: index into the map row
        lda _edge_cx
        and #3
        sta tmp3                ; cx&3
        lda _edge_cy
        sta tmp1                ; cy
        lda #22
        sta tmp4                ; rows left
col_loop:
        ldy tmp2
        lda (ptr3),y            ; metatile
        asl a
        asl a
        asl a
        asl a
        sta col_t
        lda tmp1
        and #3
        asl a
        asl a
        ora col_t
        ora tmp3
        tay
        lda _mt_chars,y
        ldy #0
        sta (ptr1),y
        ; next screen row
        lda ptr1
        clc
        adc #40
        sta ptr1
        bcc :+
        inc ptr1+1
:       inc tmp1
        lda tmp1
        and #3
        bne :+
        lda ptr3                ; next metatile row: +64
        clc
        adc #64
        sta ptr3
        bcc :+
        inc ptr3+1
:       dec tmp4
        jne col_loop
        rts

.bss
col_t:  .res 1

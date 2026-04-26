; fastscroll.s - Dreadline deck row scroller for cc65/ca65
;
; C entry point:
;   void scroll_deck_rows(void);
;
; Shifts C64 screen RAM and color RAM rows 4..22 one character left.
; Dreadline's C code then fills the new rightmost column.

.export _scroll_deck_rows

.segment "CODE"

.macro SCROLL_ROW sbase, cbase, loop_label
    ldx #0
loop_label:
    lda sbase + 1,x
    sta sbase,x
    lda cbase + 1,x
    sta cbase,x
    inx
    cpx #39
    bne loop_label
.endmacro

_scroll_deck_rows:
    SCROLL_ROW $44A0, $D8A0, row04_loop ; row 4
    SCROLL_ROW $44C8, $D8C8, row05_loop ; row 5
    SCROLL_ROW $44F0, $D8F0, row06_loop ; row 6
    SCROLL_ROW $4518, $D918, row07_loop ; row 7
    SCROLL_ROW $4540, $D940, row08_loop ; row 8
    SCROLL_ROW $4568, $D968, row09_loop ; row 9
    SCROLL_ROW $4590, $D990, row10_loop ; row 10
    SCROLL_ROW $45B8, $D9B8, row11_loop ; row 11
    SCROLL_ROW $45E0, $D9E0, row12_loop ; row 12
    SCROLL_ROW $4608, $DA08, row13_loop ; row 13
    SCROLL_ROW $4630, $DA30, row14_loop ; row 14
    SCROLL_ROW $4658, $DA58, row15_loop ; row 15
    SCROLL_ROW $4680, $DA80, row16_loop ; row 16
    SCROLL_ROW $46A8, $DAA8, row17_loop ; row 17
    SCROLL_ROW $46D0, $DAD0, row18_loop ; row 18
    SCROLL_ROW $46F8, $DAF8, row19_loop ; row 19
    SCROLL_ROW $4720, $DB20, row20_loop ; row 20
    SCROLL_ROW $4748, $DB48, row21_loop ; row 21
    SCROLL_ROW $4770, $DB70, row22_loop ; row 22
    rts

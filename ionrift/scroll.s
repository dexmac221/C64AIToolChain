; scroll.s - coarse scroll row copier for ION RIFT
;
; Copies 22 playfield rows (rows 3..24) one character to the left:
;   dst[row][0..38] = src[row][1..39]
; Also used to shift colour RAM in place (dst < src, ascending copy).
;
; C interface:
;   extern unsigned char *scr_src;   set to source base + row3 offset + 1
;   extern unsigned char *scr_dst;   set to dest   base + row3 offset
;   void scroll_rows(void);

.export _scroll_rows
.export _scr_src, _scr_dst, _scr_rows
.importzp ptr1, ptr2

COLS = 39

.data
_scr_src:  .word 0
_scr_dst:  .word 0
_scr_rows: .byte 22

.code

_scroll_rows:
        lda _scr_src
        sta ptr1
        lda _scr_src+1
        sta ptr1+1
        lda _scr_dst
        sta ptr2
        lda _scr_dst+1
        sta ptr2+1
        ldx _scr_rows
row_loop:
        ldy #0
col_loop:
        lda (ptr1),y
        sta (ptr2),y
        iny
        cpy #COLS
        bne col_loop
        ; advance both pointers by 40
        lda ptr1
        clc
        adc #40
        sta ptr1
        bcc :+
        inc ptr1+1
:       lda ptr2
        clc
        adc #40
        sta ptr2
        bcc :+
        inc ptr2+1
:       dex
        bne row_loop
        rts

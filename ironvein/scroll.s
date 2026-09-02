; scroll.s - generic shifted row copier for IRON VEIN
;
; Copies scr_rows rows of 40 bytes (ten cells unrolled, four passes) from scr_src to scr_dst. The caller
; folds the scroll direction into scr_src: front + dy*40 + dx. Source
; and destination are different buffers (or colour RAM -> shadow), so
; the copy order never matters. The stray byte at the edge of each row
; (a neighbour row's cell) is overwritten by the edge writer afterwards.
;
; C interface:
;   extern unsigned char *scr_src, *scr_dst; extern unsigned char scr_rows;
;   void scroll_rows(void);

.export _scroll_rows
.export _scr_src, _scr_dst, _scr_rows
.importzp ptr1, ptr2

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
        .repeat 40
        lda (ptr1),y
        sta (ptr2),y
        iny
        .endrepeat
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
        beq :+
        jmp row_loop            ; the unrolled row is past branch range
:       rts

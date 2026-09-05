; DEEP SIGNAL / Codex — original fixed-destination viewport renderer.
; No engine code imported from another game. 25*40 bytes per coarse step;
; paired source rows share a base pointer and use Y=0/Y=128.
.export _engine_init, _read_input, _clock_read, _render_a, _render_b
.export _ticks, _missed, _ready, _stage_d018, _stage_dx, _stage_dy
.import _camera_x, _camera_y, _state
.export _sx, _sy, _sptr, _scol, _xmask, _enable, _view_ptr
.importzp ptr1
src = ptr1
.segment "BSS"
_view_ptr: .res 2
_ticks: .res 2
_missed: .res 2
_ready: .res 1
_stage_d018: .res 1
_stage_dx: .res 1
_stage_dy: .res 1
_sx: .res 8
_sy: .res 8
_sptr: .res 8
_scol: .res 8
_xmask: .res 1
_enable: .res 1
input: .res 1
.segment "CODE"
_engine_init:
    sei
    lda #$7f
    sta $dc0d
    sta $dd0d
    lda $dc0d
    lda $dd0d
    lda #0
    sta $dc0e
    sta $dc0f
    sta $dd0e
    sta $dd0f
    lda #$35
    sta $01
    lda #<irq
    sta $fffe
    lda #>irq
    sta $ffff
    lda #<nmi
    sta $fffa
    lda #>nmi
    sta $fffb
    lda $dd02
    ora #3
    sta $dd02
    lda $dd00
    and #$fc
    ora #2
    sta $dd00
    lda #$ff
    sta $dc02
    lda #0
    sta $dc03
    lda #$ff
    sta $dc00
    ; Free-running CIA2 timer A: profiler, never generates NMI.
    sta $dd04
    sta $dd05
    lda #$11
    sta $dd0e
    lda #250
    sta $d012
    lda #$17
    sta $d011
    lda #$ff
    sta $d019
    sta $d01c
    lda #0
    sta $d017
    sta $d01d
    sta $d01b
    sta $d025
    lda #1
    sta $d026
    sta $d01a
    cli
    rts
_clock_read:
    lda $dd04
    ldx $dd05
    rts
_read_input:
    lda #$ff
    sta $dc00
    lda $dc00
    eor #$ff
    and #$1f
    ora $033c
    ora $033e
    sta input
    lda #0
    sta $033c
    lda #$fd
    sta $dc00
    lda $dc01
    and #2
    bne :+
    lda input
    ora #1
    sta input
:   lda $dc01
    and #4
    bne :+
    lda input
    ora #4
    sta input
:   lda $dc01
    and #32
    bne :+
    lda input
    ora #2
    sta input
:   lda #$fb
    sta $dc00
    lda $dc01
    and #4
    bne :+
    lda input
    ora #8
    sta input
:   lda $dc01
    and #2
    bne :+
    lda input
    ora #64
    sta input
:   lda #$7f
    sta $dc00
    lda $dc01
    and #16
    bne :+
    lda input
    ora #16
    sta input
:   lda #$fe
    sta $dc00
    lda $dc01
    and #16
    bne :+
    lda input
    ora #32
    sta input
:   lda #$ff
    sta $dc00
    lda input
    ldx #0
    rts
irq:
    pha
    txa
    pha
    tya
    pha
    lda #1
    sta $d019
    inc _ticks
    bne :+
    inc _ticks+1
:   lda _ready
    bne commit
    lda _state
    cmp #1
    beq :+
    jmp done
:   inc _missed
    beq :+
    jmp done
:   inc _missed+1
    jmp done
commit:
    lda _camera_x
    sta $0360
    lda _camera_x+1
    sta $0361
    lda _camera_y
    sta $0362
    lda _camera_y+1
    sta $0363
    lda _stage_d018
    sta $d018
    lda _stage_dx
    sta $d016
    lda _stage_dy
    sta $d011
    lda _xmask
    sta $d010
    lda _enable
    sta $d015
.repeat 8, I
    lda _sx+I
    sta $d000+I*2
    lda _sy+I
    sta $d001+I*2
    lda _sptr+I
    sta $43f8+I
    sta $47f8+I
    lda _scol+I
    sta $d027+I
.endrepeat
    lda #0
    sta _ready
done:
    pla
    tay
    pla
    tax
    pla
nmi:rti
.segment "BLITTER"
_render_a:
    lda _view_ptr
    sta src
    lda _view_ptr+1
    sta src+1
.repeat 25, R
    ldy #((R .mod 2)*128)
    .repeat 40, C
        lda (src),y
        sta $4000+R*40+C
        .if C < 39
            iny
        .endif
    .endrepeat
    .if (R .mod 2) = 1
        inc src+1
    .endif
.endrepeat
    rts
_render_b:
    lda _view_ptr
    sta src
    lda _view_ptr+1
    sta src+1
.repeat 25, R
    ldy #((R .mod 2)*128)
    .repeat 40, C
        lda (src),y
        sta $4400+R*40+C
        .if C < 39
            iny
        .endif
    .endrepeat
    .if (R .mod 2) = 1
        inc src+1
    .endif
.endrepeat
    rts

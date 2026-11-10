; sound.s - FULGOR: a three-voice SID driver with wavetables and effects
;
; Called once a frame from the frame interrupt. The song and the tables
; come from music.py (music.s). Voices are addressed by their SID
; register offset (0, 7, 14), so the same index reaches both the voice's
; state and its registers.
;
;   voice 0 (0)   bass and drums       voice 1 (7)   arpeggios, and the
;   voice 2 (14)  lead, with vibrato                 sound effects
;
; A note is gated off for its last two frames so that the next one gets
; its attack: the SID's envelope restarts only on a rising gate.

.include "assets.inc"
.include "defs.inc"
.macpack longbranch

.export _snd_init, _snd_tune
.import ftab_lo, ftab_hi, ins_wt, ins_ad, ins_sr, wt_ctl, wt_note
.import pat_lo, pat_hi, orders, ord_start, ord_loop
.import sfx_start, sfx_ctl, sfx_note

SPEED = 6                       ; frames a row
SID   = $d400

.segment "ZEROPAGE"
snd_p:  .res 2
snd_t:  .res 2

.data
snd_on:  .byte 0
tune_on: .byte 0
tick:    .byte 0
sfx_pos: .byte 0
vib_d:   .byte 0                ; frames before the lead's vibrato starts
vib_p:   .byte 0
endpat:  .byte $ff

.bss
v_cnt:   .res 15                ; rows left of the note
v_dur:   .res 15                ; rows a note
v_note:  .res 15
v_ins:   .res 15
v_wt:    .res 15                ; wavetable position
v_gate:  .res 15                ; $FF, or $FE while resting
v_seq:   .res 15                ; position in the order list
v_loop:  .res 15
v_pl:    .res 15                ; pattern pointer
v_ph:    .res 15
v_pw:    .res 15                ; pulse width wobble, phase

.rodata
pwtab:  .byte 4, 5, 6, 7, 8, 9, 10, 11, 12, 11, 10, 9, 8, 7, 6, 5
vib_lo: .byte $00, $38, $70, $38, $00, <-$38, <-$70, <-$38
vib_hi: .byte $00, $00, $00, $00, $00, $ff, $ff, $ff
vidx:   .byte 0, 7, 14

.code

_snd_init:
        ldx #$18
        lda #0
:       sta SID,x
        dex
        bpl :-
        lda #$0f
        sta SID+$18
        lda #0
        sta sfx_pos
        sta tune_on
        lda #1
        sta snd_on
        rts

; A = 1: start the tune from the top. A = 0: silence it.
_snd_tune:
        php
        sei
        sta tune_on
        ldy #2
@v:     ldx vidx,y
        lda #0
        sta SID+4,x
        sta v_pw,x
        lda #$ff
        sta v_gate,x
        lda #1
        sta v_cnt,x
        sta v_dur,x
        lda #<endpat            ; an empty pattern: the first row fetches
        sta v_pl,x              ; the first real one
        lda #>endpat
        sta v_ph,x
        lda ord_start,y
        sta v_seq,x
        lda ord_loop,y
        sta v_loop,x
        dey
        bpl @v
        lda #0
        sta tick
        plp
        rts

; ---- once a frame, from the frame interrupt
snd_play:
        lda snd_on
        bne :+
        rts
:       ldx _sfx_req            ; a new effect takes voice 1
        beq @tune
        lda #0
        sta _sfx_req
        lda sfx_start-1,x
        sta sfx_pos
        lda #$00
        sta SID+7+5
        lda #$f6
        sta SID+7+6
        lda #$08
        sta SID+7+3
@tune:  lda tune_on
        beq @sfx
        dec tick
        bpl @frames
        lda #SPEED-1
        sta tick
        ldx #14
@row:   dec v_cnt,x
        bne :+
        jsr next_note
:       txa
        sec
        sbc #7
        tax
        bpl @row
@frames:
        ldx #14
@fl:    jsr voice_frame
        txa
        sec
        sbc #7
        tax
        bpl @fl
@sfx:   ldy sfx_pos
        beq @r
        lda sfx_ctl,y
        beq @end
        sta SID+7+4
        ldx sfx_note,y
        lda ftab_lo,x
        sta SID+7
        lda ftab_hi,x
        sta SID+7+1
        inc sfx_pos
@r:     rts
@end:   sta sfx_pos
        sta SID+7+4
        rts

; ---- one frame of voice X: the wavetable's next step
voice_frame:
        cpx #7
        bne :+
        lda sfx_pos             ; the effect has this voice
        beq :+
        rts
:       ldy v_wt,x
        lda wt_ctl,y
        bne :+
        lda wt_note,y           ; a jump
        tay
        lda wt_ctl,y
:       and v_gate,x
        sta snd_t
        lda v_cnt,x             ; the last two frames of a note: gate off
        cmp #1
        bne :+
        lda tick
        cmp #2
        bcs :+
        lda snd_t
        and #$fe
        sta snd_t
:       lda snd_t
        sta SID+4,x
        iny
        sty snd_t               ; (the next step)
        dey
        lda wt_note,y
        bmi @abs
        clc
        adc v_note,x
        bpl @got
@abs:   and #$7f
@got:   tay
        lda snd_t
        sta v_wt,x
        lda ftab_lo,y
        sta snd_t
        lda ftab_hi,y
        sta snd_t+1
        cpx #14                 ; the lead: vibrato, after a moment
        bne @w
        lda vib_d
        beq @vib
        dec vib_d
        bne @w
@vib:   inc vib_p
        lda vib_p
        and #7
        tay
        clc
        lda snd_t
        adc vib_lo,y
        sta snd_t
        lda snd_t+1
        adc vib_hi,y
        sta snd_t+1
@w:     lda snd_t
        sta SID,x
        lda snd_t+1
        sta SID+1,x
        inc v_pw,x              ; a slow wobble of the pulse width
        lda v_pw,x
        lsr a
        lsr a
        and #15
        tay
        lda pwtab,y
        sta SID+3,x
@r:     rts

; ---- voice X has finished a note: read the pattern on
next_note:
@rd:    lda v_pl,x
        sta snd_p
        lda v_ph,x
        sta snd_p+1
        ldy #0
        lda (snd_p),y
        inc v_pl,x
        bne :+
        inc v_ph,x
:       cmp #$ff
        beq @next
        cmp #$a0
        bcs @ins
        cmp #$80
        bcs @dur
        cmp #$60
        beq @rest
        sta v_note,x
        ldy v_ins,x
        lda ins_wt,y
        sta v_wt,x
        lda ins_ad,y
        sta SID+5,x
        lda ins_sr,y
        sta SID+6,x
        lda #$ff
        sta v_gate,x
        cpx #14
        bne @cnt
        lda #12
        sta vib_d
@cnt:   lda v_dur,x
        sta v_cnt,x
        rts
@rest:  lda #$fe
        sta v_gate,x
        bne @cnt
@dur:   and #$1f
        clc
        adc #1
        sta v_dur,x
        bne @rd
@ins:   and #$1f
        sta v_ins,x
        jmp @rd
@next:  ldy v_seq,x             ; the next pattern of this voice's list
        lda orders,y
        cmp #$ff
        bne :+
        ldy v_loop,x
        lda orders,y
:       iny
        pha
        tya
        sta v_seq,x
        pla
        tay
        lda pat_lo,y
        sta v_pl,x
        lda pat_hi,y
        sta v_ph,x
        jmp @rd

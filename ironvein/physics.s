; physics.s - solid-tile test and the walkers' physics, in assembly
;
; solid_at:  col_x, col_y (world pixels, 16-bit) -> A = 1 if the
;            metatile there is solid (or off the map), else 0.
;            Metatiles are 32x32 pixels; the map is 64x32 of them.
;
; update_walkers: eight objects with gravity. Each frame: gravity every
;            other frame up to a terminal speed, vertical move with
;            landing (snapped to the tile top) and head bump, horizontal
;            move with wall reversal; even-numbered walkers also turn at
;            a ledge instead of walking off it. One that drifts far from
;            the camera is respawned by C.
;
; Object arrays are the dr_* SoA from ironvein.c (x/y as lo/hi bytes,
; vx/vy signed bytes, on = standing on ground).

.export _solid_at, _col_x, _col_y, _update_walkers
.export _update_shots, _hero_touch
.import _level_map, _mt_solid
.import _dr_xlo, _dr_xhi, _dr_ylo, _dr_yhi, _dr_vx, _dr_vy, _dr_on
.import _dr_st, _dr_ptr, _dr_life, _dr_col, _kills, _boss_hits, _hero_x, _hero_y
.import _spawn_walker, _frame, _cam_px, _cam_py
.importzp ptr1, tmp1, tmp2, tmp3, tmp4
.macpack longbranch
.include "sprites.inc"

NUM_WK   = 6                    ; objects 0..5 walk
BOSS0    = 6                    ; 6..9 the boss's four parts (C moves them)
SHOT0    = 10                   ; 10..13 the hero's shots
NUM_OBJ  = 14
SPR_PTR  = 96
DYING    = 17                   ; dr_st counts down from here to 1
BOX_L    = 6                    ; collision box inside the 24x21 sprite
BOX_R    = 17
BOX_T    = 2
BOX_B    = 20                   ; feet line = y + 20
FALL_MAX = 4

.data
_col_x: .word 0
_col_y: .word 0

.bss
dlo:    .res 1
dhi:    .res 1

.code

; ------------------------------------------------------------ solid_at
_solid_at:
        lda _col_y+1
        cmp #4                  ; y >= 1024: below the map
        bcs solid_yes
        lda _col_x+1
        cmp #8                  ; x >= 2048: past the map
        bcs solid_yes
        ; row = y >> 5
        lda _col_y
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        sta tmp1
        lda _col_y+1
        asl a
        asl a
        asl a
        ora tmp1                ; row 0..31
        ; index = row * 64 + (x >> 5)
        sta tmp1
        lsr a
        lsr a
        sta ptr1+1              ; row >> 2 = high byte of row*64
        lda tmp1
        asl a
        asl a
        asl a
        asl a
        asl a
        asl a                   ; (row << 6) & 255
        sta ptr1
        lda _col_x
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        sta tmp2
        lda _col_x+1
        asl a
        asl a
        asl a
        ora tmp2                ; column 0..63
        clc
        adc ptr1
        sta ptr1
        bcc :+
        inc ptr1+1
:       lda ptr1
        clc
        adc #<_level_map
        sta ptr1
        lda ptr1+1
        adc #>_level_map
        sta ptr1+1
        ldy #0
        lda (ptr1),y
        tay
        lda _mt_solid,y
        rts
solid_yes:
        lda #1
        rts

; ------------------------------------------------- helpers for walkers
; set col_x = x + A, col_y = y + tmp3  (X = object index)
probe_at:
        clc
        adc _dr_xlo,x
        sta _col_x
        lda _dr_xhi,x
        adc #0
        sta _col_x+1
        lda tmp3
        clc
        adc _dr_ylo,x
        sta _col_y
        lda _dr_yhi,x
        adc #0
        sta _col_y+1
        jmp _solid_at

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

; ------------------------------------------------------- update_walkers
_update_walkers:
        ldx #0
wk_loop:
        stx tmp4
        ; --- state: 0 inactive, 1 alive, 2..DYING an explosion
        lda _dr_st,x
        cmp #1
        jeq wk_alive
        jcc wk_next
        dec _dr_st,x
        lda _dr_st,x
        cmp #1
        bne wk_boom
        txa
        jsr _spawn_walker       ; the explosion is over: a new one drops in
        jmp wk_next
wk_boom:
        cmp #12
        bcs wk_boom1
        cmp #7
        bcs wk_boom2
        lda #SPR_PTR+SF_BOOM3
        jmp wk_boomset
wk_boom2:
        lda #SPR_PTR+SF_BOOM2
        jmp wk_boomset
wk_boom1:
        lda #SPR_PTR+SF_BOOM1
wk_boomset:
        sta _dr_ptr,x
        jmp wk_next
wk_alive:
        ; --- two-frame walk
        lda _frame
        and #8
        beq wk_frm2
        lda #SPR_PTR+SF_DRONE1
        jmp wk_frmset
wk_frm2:
        lda #SPR_PTR+SF_DRONE2
wk_frmset:
        sta _dr_ptr,x
        ; --- gravity, every other frame, up to FALL_MAX
        lda _frame
        and #1
        jne no_grav
        lda _dr_vy,x
        cmp #FALL_MAX
        jpl no_grav             ; signed: >= FALL_MAX
        inc _dr_vy,x
no_grav:
        ; --- vertical move
        lda _dr_vy,x
        ADD_SIGNED _dr_ylo, _dr_yhi
        lda #0
        sta _dr_on,x
        lda _dr_vy,x
        jeq wk_horiz
        jmi wk_head
        ; falling: feet on solid? one probe under the middle of the box
        ; (12 pixels wide on 32-pixel tiles: the corners rarely differ,
        ; and a probe is 120 cycles)
        lda #BOX_B+1
        sta tmp3
        lda #(BOX_L+BOX_R)/2
        jsr probe_at
        jeq wk_horiz_x
wk_land:
        ldx tmp4
        ; snap: feet line to the top of the tile row: y = (y+21 & ~31) - 21
        lda _dr_ylo,x
        clc
        adc #BOX_B+1
        and #$E0
        sec
        sbc #BOX_B+1
        sta _dr_ylo,x
        bcs :+
        dec _dr_yhi,x
:       lda #0
        sta _dr_vy,x
        lda #1
        sta _dr_on,x
        jmp wk_horiz
wk_head:
        ; rising: head into solid? then stop rising
        lda #BOX_T
        sta tmp3
        lda #(BOX_L+BOX_R)/2
        jsr probe_at
        jeq wk_horiz_x
wk_bump:
        ldx tmp4
        lda _dr_vy,x            ; undo the move
        eor #$FF
        clc
        adc #1
        ADD_SIGNED _dr_ylo, _dr_yhi
        lda #0
        sta _dr_vy,x
wk_horiz_x:
        ldx tmp4
wk_horiz:
        ; --- horizontal move, then the wall ahead at knee and chest
        ldx tmp4
        lda _dr_vx,x
        ADD_SIGNED _dr_xlo, _dr_xhi
        lda _dr_vx,x
        jmi wk_left
        lda #BOX_R+1
        jne wk_wall
wk_left:
        lda #BOX_L-1
wk_wall:
        sta tmp2                ; probe column ahead, at waist height
        lda #(BOX_T+BOX_B)/2
        sta tmp3
        lda tmp2
        jsr probe_at
        jne wk_reverse
        ; even walkers: turn at a ledge instead of walking off it
        ldx tmp4
        txa
        and #1
        jne wk_range
        lda _dr_on,x
        jeq wk_range
        lda #BOX_B+1
        sta tmp3
        lda tmp2
        jsr probe_at
        jne wk_range            ; ground ahead: fine
wk_reverse:
        ldx tmp4
        lda _dr_vx,x
        eor #$FF
        clc
        adc #1
        sta _dr_vx,x
        ADD_SIGNED _dr_xlo, _dr_xhi   ; step back out of the wall
wk_range:
        ldx tmp4
        ; --- far from the camera? checked one frame in eight per object
        lda _frame
        and #7
        sta tmp1
        txa
        and #7
        cmp tmp1
        jne wk_next
        ; x - cam_px + 80 in 0..480, y - cam_py + 60 in 0..300
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
        cmp #2
        jcs wk_respawn
        cmp #1
        bne :+
        lda dlo
        cmp #$E1
        jcs wk_respawn
:       lda _dr_ylo,x
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
        cmp #2
        jcs wk_respawn
        cmp #1
        jne wk_next
        lda dlo
        cmp #$2D
        jcc wk_next
wk_respawn:
        txa
        jsr _spawn_walker       ; fastcall: A = index
wk_next:
        ldx tmp4
        inx
        cpx #NUM_WK
        jne wk_loop
        rts

; --------------------------------------------------------- update_shots
; Objects SHOT0..NUM_OBJ-1: fly at dr_vx, die on a wall, when dr_life
; runs out, or on a walker - which then explodes. The bolt is drawn on
; rows 9-10, columns 6..13 of the sprite; a walker's box is 6..17 x
; 2..20, so the two overlap when shot-walker is in -6..14 horizontally
; and -8..11 vertically (shifted into 0.. for an unsigned compare).
_update_shots:
        ldx #SHOT0
sh_loop:
        stx tmp4
        lda _dr_st,x
        jeq sh_next
        dec _dr_life,x
        jeq sh_kill
        lda _dr_vx,x
        ADD_SIGNED _dr_xlo, _dr_xhi
        lda #10
        sta tmp3
        lda #10
        jsr probe_at
        jne sh_kill_x
        ldx tmp4
        ldy #0
sh_wk:
        lda _dr_st,y
        cmp #1
        bne sh_wnext
        lda _dr_xlo,x
        sec
        sbc _dr_xlo,y
        sta dlo
        lda _dr_xhi,x
        sbc _dr_xhi,y
        sta dhi
        lda dlo
        clc
        adc #6
        sta dlo
        lda dhi
        adc #0
        bne sh_wnext
        lda dlo
        cmp #21
        bcs sh_wnext
        lda _dr_ylo,x
        sec
        sbc _dr_ylo,y
        sta dlo
        lda _dr_yhi,x
        sbc _dr_yhi,y
        sta dhi
        lda dlo
        clc
        adc #8
        sta dlo
        lda dhi
        adc #0
        bne sh_wnext
        lda dlo
        cmp #20
        bcs sh_wnext
        cpy #BOSS0              ; hit: a walker explodes, the boss counts
        bcs sh_boss
        lda #DYING
        sta _dr_st,y
        lda #7                  ; the burst is yellow whatever it was
        sta _dr_col,y
        inc _kills
        jmp sh_kill
sh_boss:
        inc _boss_hits
        jmp sh_kill
sh_wnext:
        iny
        cpy #SHOT0              ; every target: walkers and boss parts
        bne sh_wk
        jmp sh_next
sh_kill_x:
        ldx tmp4
sh_kill:
        lda #0
        sta _dr_st,x
sh_next:
        ldx tmp4
        inx
        cpx #NUM_OBJ
        jne sh_loop
        rts

; ----------------------------------------------------------- hero_touch
; A = 1 if a live walker's or boss part's box overlaps the hero's (same box):
; |dx| < 12 and |dy| < 19.
_hero_touch:
        ldx #0
ht_loop:
        lda _dr_st,x
        cmp #1
        bne ht_next
        lda _hero_x
        sec
        sbc _dr_xlo,x
        sta dlo
        lda _hero_x+1
        sbc _dr_xhi,x
        sta dhi
        lda dlo
        clc
        adc #11
        sta dlo
        lda dhi
        adc #0
        bne ht_next
        lda dlo
        cmp #23
        bcs ht_next
        lda _hero_y
        sec
        sbc _dr_ylo,x
        sta dlo
        lda _hero_y+1
        sbc _dr_yhi,x
        sta dhi
        lda dlo
        clc
        adc #18
        sta dlo
        lda dhi
        adc #0
        bne ht_next
        lda dlo
        cmp #37
        bcs ht_next
        lda #1
        rts
ht_next:
        inx
        cpx #SHOT0              ; walkers and boss parts alike
        bne ht_loop
        lda #0
        rts

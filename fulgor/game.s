; game.s - FULGOR: the hero, his shots, the enemies, the camera, the frame
;
; Everything that runs every frame is here, in assembly, with bytes: the
; lesson of IRON VEIN's profiler was that cc65 turns per-object C into
; four times the cycles. C (main.c) sees the game through a few bytes:
; g_event, the HUD values and their dirty flags.
;
; Objects (engine.s holds the arrays):
;    0      the hero's upper half        1      his legs
;    2..6   his shots                    7..11  enemies
;   12..15  enemy bullets

.include "assets.inc"
.include "defs.inc"
.macpack longbranch

.export _game_reset, _game_frame, _respawn, _title_init, _title_frame
.export _idle_frame
.export _g_event, _g_lives, _g_energy, _g_gems, _g_power, _g_score
.export _g_dirty, _g_spot
.import _spot_xl, _spot_xh, _spot_yl, _spot_yh
.import _sp_type, _sp_xl, _sp_xh, _sp_yl, _sp_yh, _sp_len

HSPD     = 2                    ; run speed, pixels a frame
GRAV     = $48                  ; 0.28 pixels a frame, every frame
JUMP_V   = $fac0                ; -5.25: 49 pixels up in 19 frames
JUMP_CUT = $fe                  ; rising speed kept when UP is let go
VY_MAX   = 3                    ; a fall the camera can follow (see DEVLOG)
BOX_L    = 6                    ; the hero's box inside his 24 x 42 picture
BOX_R    = 17
BOX_T    = 3
FEET     = 40                   ; first row below his soles
HERO_COL = 15
ENERGY_MAX = 8
INV_TIME = 60                   ; frames of blinking after a hit
DEAD_TIME = 70

SHOT0    = 2
SHOT_END = 7
EN0      = 7
EN_END   = 12
FX0      = 12
SHOT_V   = 6
SHOT_LIFE = 30
E_BOOM   = 4                    ; an enemy that is exploding
E_BULLET = 5

CAM_XLO  = 148                  ; the camera leaves the hero alone while
CAM_XHI  = 172                  ; his centre is inside this box
CAM_VX   = 3                    ; and catches up no faster than this
; Vertically there are two boxes. In the air it is tall, so a jump on
; level ground does not move the picture at all; on the ground it is
; narrow and the camera settles gently, bringing a hero who has climbed
; or dropped back to his usual height on the screen.
CAM_YLO_G = 86
CAM_YHI_G = 102
CAM_VY_G  = 2
CAM_YLO_A = 44
CAM_YHI_A = 122
CAM_VY_A  = 3

; HUD dirty bits
D_SCORE  = 1
D_ENERGY = 2
D_LIVES  = 4
D_GEMS   = 8
D_POWER  = 16

.data
cam_box:  .byte CAM_YLO_G, CAM_YHI_G, CAM_VY_G
          .byte CAM_YLO_A, CAM_YHI_A, CAM_VY_A
_joy:     .byte 0
hero_dir: .byte 0               ; 0 right, 1 left
hero_on:  .byte 0
_g_event: .byte 0               ; 1 game over, 2 level clear, 3 respawn
_g_lives: .byte 3
_g_energy: .byte ENERGY_MAX
_g_gems:  .byte 0
_g_power: .byte 0
_g_dirty: .byte $ff
_g_spot:  .byte 0               ; the last checkpoint touched
_g_score: .byte 0, 0, 0, 0, 0, 0
_sfx_req: .byte 0

.rodata
; a sine's steps, for things that bob: 32 frames, +-12 pixels
dsin:   .byte 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, <-1, <-1, <-1, <-1, <-2, <-2
        .byte <-2, <-2, <-2, <-2, <-1, <-1, <-1, <-1, 0, 0, 1, 1, 1, 1, 2, 2
en_hp:  .byte 0, 1, 2, 3
en_col: .byte 0, 4, 7, 12
en_pts: .byte 0, 1, 2, 3        ; hundreds
shot_vy: .byte 0, <-2, 2
shot_dy: .byte 6, 2, 10
fire_cd_tab: .byte 7, 8, 10
wave:   .byte 0, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7, 8, 8, 8, 8, 8
        .byte 8, 8, 8, 8, 7, 7, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0
logo_col: .byte 7, 7, 15, 3, 14, 3, 15, 7

.bss
hero_x:    .res 2
hero_y:    .res 2
hero_yf:   .res 1               ; fraction of y
hero_vyl:  .res 1               ; vertical speed, 8.8 signed
hero_vyh:  .res 1
hero_anim: .res 1
hero_inv:  .res 1
hero_dead: .res 1
prev_y:    .res 2
fire_cd:   .res 1
sp_i:      .res 1
cp_i:      .res 1
gt:        .res 2
eh_x:      .res 1
eh_y:      .res 1
ob_vx:     .res NUM_OBJ
ob_vy:     .res NUM_OBJ
ob_life:   .res NUM_OBJ          ; shots: frames left; enemies: patrol count
ob_type:   .res NUM_OBJ
ob_hp:     .res NUM_OBJ
ob_len:    .res NUM_OBJ
ob_ph:     .res NUM_OBJ
ob_sp:     .res NUM_OBJ
ob_fl:     .res NUM_OBJ
sp_state:  .res NUM_SPAWN        ; 0 waiting, 1 on stage, 2 destroyed

.code

; ====================================================================
; A new game: counters, the level's enemies, a clean stage.
_game_reset:
        lda #3
        sta _g_lives
        lda #0
        sta _g_gems
        sta _g_power
        sta _g_spot
        sta $d017               ; (the title shows its sprites doubled)
        sta $d01d
        ldx #5
:       sta _g_score,x
        dex
        bpl :-
        ldx #NUM_SPAWN-1
:       sta sp_state,x
        dex
        bpl :-
        ; fall through: a fresh life

; A new life: full energy, nothing on stage but the hero.
_respawn:
        lda #ENERGY_MAX
        sta _g_energy
        lda #INV_TIME
        sta hero_inv
        lda #0
        sta hero_dead
        sta fire_cd
        sta _g_event
        ldx #NUM_OBJ-1
@o:     cpx #2
        bcc @keep
        sta o_st,x
@keep:  dex
        bpl @o
        ldx #NUM_SPAWN-1        ; whoever was on stage goes back to waiting
@s:     lda sp_state,x
        cmp #1
        bne :+
        lda #0
        sta sp_state,x
:       dex
        bpl @s
        lda #$ff
        sta _g_dirty
        rts

; Drop the hero at spot A and redraw around him.
_teleport:
        tax
        lda _spot_xl,x
        sta hero_x
        lda _spot_xh,x
        sta hero_x+1
        lda _spot_yl,x
        sta hero_y
        lda _spot_yh,x
        sta hero_y+1
        lda #0
        sta hero_vyl
        sta hero_vyh
        sta hero_yf
        sta hero_anim
        sta hero_dir
        sta _joy
        sta $033c
        sta $033e
        lda #1
        sta hero_on
        sta o_st
        sta o_st+1
        ldx #0
        jsr obj_add
        ldx #1
        jsr obj_add
        jsr _wait_commit
        jsr camera_place
        jsr _scroll_init
        jsr hero_sprites
        jsr _build_mux
        jmp _commit

; Put the camera where the follow box would have it, inside the map.
camera_place:
        sec
        lda hero_x
        sbc #<(160 - 12)
        sta cam_x
        lda hero_x+1
        sbc #>(160 - 12)
        sta cam_x+1
        bpl :+
        lda #0
        sta cam_x
        sta cam_x+1
:       lda cam_x+1
        cmp #>CAM_X_MAX
        bcc @y
        bne @xm
        lda cam_x
        cmp #<CAM_X_MAX
        bcc @y
@xm:    lda #<CAM_X_MAX
        sta cam_x
        lda #>CAM_X_MAX
        sta cam_x+1
@y:     sec
        lda hero_y
        sbc #<(94 - 20)
        sta cam_y
        lda hero_y+1
        sbc #>(94 - 20)
        sta cam_y+1
        bpl :+
        lda #0
        sta cam_y
        sta cam_y+1
:       lda cam_y+1
        cmp #>CAM_Y_MAX
        bcc @d
        bne @ym
        lda cam_y
        cmp #<CAM_Y_MAX
        bcc @d
@ym:    lda #<CAM_Y_MAX
        sta cam_y
        lda #>CAM_Y_MAX
        sta cam_y+1
@d:     rts

; ====================================================================
; the title: the level's first view, the name in doubled sprites
; ====================================================================
_title_init:
        lda #0
        sta o_st
        sta o_st+1
        lda #$ff
        sta $d017
        sta $d01d
        ldx #5
@l:     lda #1
        sta o_st+2,x
        txa
        clc
        adc #SPR_BASE+SF_LOGO0
        sta o_ptr+2,x
        stx zt
        inx
        inx
        jsr obj_add
        ldx zt
        dex
        bpl @l
        rts

; One frame of the title. Returns the joystick's fire bit.
_title_frame:
        jsr _wait_commit
        jsr read_joy
        ldx #5
@l:     txa                     ; x = camera + 12 + 48 * letter
        asl a
        asl a
        asl a
        asl a
        sta zt
        asl a
        clc
        adc zt
        clc
        adc #12
        clc
        adc cam_x
        sta o_xl+2,x
        lda cam_x+1
        adc #0
        sta o_xh+2,x
        txa                     ; a slow wave along the name
        asl a
        asl a
        clc
        adc _frame
        lsr a
        and #31
        tay
        lda wave,y
        clc
        adc #30
        clc
        adc cam_y
        sta o_yl+2,x
        lda cam_y+1
        adc #0
        sta o_yh+2,x
        txa                     ; and colours that travel with it
        clc
        adc _frame
        lsr a
        lsr a
        lsr a
        and #7
        tay
        lda logo_col,y
        sta o_col+2,x
        dex
        bpl @l
        jsr _build_mux
        jsr _commit
        lda _joy
        and #J_FIRE
        ldx #0
        rts

; A frame with nothing moving: for the pauses between states.
_idle_frame:
        jsr _wait_commit
        jsr _build_mux
        jmp _commit

read_joy:
        lda $dc00               ; joystick 2, plus the agent's bytes:
        eor #$ff                ; $033C is one press, $033E is held
        and #$1f
        ora $033c
        ora $033e
        sta _joy
        lda #0
        sta $033c
        rts

; ====================================================================
_game_frame:
        jsr _wait_commit
        jsr read_joy
        PROF 0
        lda hero_dead
        beq @alive
        jsr hero_dying
        jmp @rest
@alive: jsr hero_update
        jsr hero_world
@rest:  PROF 1
        jsr shots_update
        jsr spawn_scan
        jsr enemies_update
        jsr fx_update
        jsr collide
        PROF 2
        jsr camera_update
        PROF 3
        jsr hero_sprites
        jsr _build_mux
        PROF 4
        jmp _commit

; ====================================================================
; the hero
; ====================================================================
hero_update:
        lda hero_inv
        beq :+
        dec hero_inv
:
        ; ---- sideways: two pixels, or up to the wall
        lda _joy
        and #J_RIGHT
        beq @nr
        lda #0
        sta hero_dir
        clc
        lda hero_x
        adc #BOX_R+HSPD
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        jsr side_probe
        bne @rb
        clc
        lda hero_x
        adc #HSPD
        sta hero_x
        bcc @hd
        inc hero_x+1
        jmp @hd
@rb:    lda px                  ; against the cell's left face
        and #$f8
        sec
        sbc #BOX_R+1
        sta hero_x
        lda px+1
        sbc #0
        sta hero_x+1
        jmp @hd
@nr:    lda _joy
        and #J_LEFT
        beq @hd
        lda #1
        sta hero_dir
        clc
        lda hero_x
        adc #BOX_L-HSPD
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        jsr side_probe
        bne @lb
        sec
        lda hero_x
        sbc #HSPD
        sta hero_x
        bcs @hd
        dec hero_x+1
        jmp @hd
@lb:    lda px                  ; against the cell's right face
        and #$f8
        clc
        adc #8-BOX_L
        sta hero_x
        lda px+1
        adc #0
        sta hero_x+1
@hd:
        ; ---- gravity, capped
        clc
        lda hero_vyl
        adc #GRAV
        sta hero_vyl
        lda hero_vyh
        adc #0
        sta hero_vyh
        bmi @nocap
        cmp #VY_MAX
        bcc @nocap
        lda #VY_MAX
        sta hero_vyh
        lda #0
        sta hero_vyl
@nocap: lda hero_y
        sta prev_y
        lda hero_y+1
        sta prev_y+1
        clc
        lda hero_yf
        adc hero_vyl
        sta hero_yf
        lda hero_vyh
        tax
        adc hero_y
        sta hero_y
        txa
        bmi @sneg
        lda hero_y+1
        adc #0
        sta hero_y+1
        jmp @vcol
@sneg:  lda hero_y+1
        adc #$ff
        sta hero_y+1

@vcol:  lda hero_vyh
        jmi @rise
        ; ---- falling or standing: what is under the soles?
        clc
        lda hero_y
        adc #FEET
        sta py
        lda hero_y+1
        adc #0
        sta py+1
        clc
        lda hero_x
        adc #BOX_L
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        jsr foot
        bne @land
        clc
        lda hero_x
        adc #BOX_R
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        jsr foot
        bne @land
        lda #0
        sta hero_on
        jmp @jump
@land:  lda py                  ; stand on the cell's top
        and #$f8
        sec
        sbc #FEET
        sta hero_y
        lda py+1
        sbc #0
        sta hero_y+1
        lda #0
        sta hero_vyl
        sta hero_vyh
        sta hero_yf
        lda #1
        sta hero_on
        jmp @jump

        ; ---- rising: the head
@rise:  lda #0
        sta hero_on
        clc
        lda hero_y
        adc #BOX_T
        sta py
        lda hero_y+1
        adc #0
        sta py+1
        clc
        lda hero_x
        adc #BOX_L
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        jsr solid_at
        bne @bump
        clc
        lda hero_x
        adc #BOX_R
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        jsr solid_at
        beq @jump
@bump:  lda py                  ; under the cell's bottom face
        and #$f8
        clc
        adc #8-BOX_T
        sta hero_y
        lda py+1
        adc #0
        sta hero_y+1
        lda #0
        sta hero_vyl
        sta hero_vyh

        ; ---- jump: from the ground; letting go of UP cuts it short
@jump:  lda _joy
        and #J_UP
        beq @cut
        lda hero_on
        beq @done
        lda #<JUMP_V
        sta hero_vyl
        lda #>JUMP_V
        sta hero_vyh
        lda #0
        sta hero_on
        rts
@cut:   lda hero_vyh
        bpl @done
        cmp #JUMP_CUT
        bcs @done
        lda #JUMP_CUT
        sta hero_vyh
        lda #0
        sta hero_vyl
@done:  rts

; Is there ground at (px, py)? Rock always; a ledge only if the soles
; were above its top before this frame's move. Z clear if so.
foot:   jsr floor_at
        bne @yes
        lda fl_ow
        beq @no
        clc                     ; prev_y + FEET <= cell top ?
        lda prev_y
        adc #FEET
        sta zt
        lda prev_y+1
        adc #0
        sta zt+1
        lda py
        and #$f8
        cmp zt
        lda py+1
        sbc zt+1
        bcc @no                 ; top < old soles: we are inside it
@yes:   lda #1
        rts
@no:    lda #0
        rts

; Five probes down the hero's side at column px, eight pixels apart.
side_probe:
        clc
        lda hero_y
        adc #7
        sta py
        lda hero_y+1
        adc #0
        sta py+1
        lda #5
        jmp side_solid

; What the hero touches in the world: things to take at his middle,
; things that kill at his feet, checkpoints, the way out.
hero_world:
        clc
        lda hero_x
        adc #12
        sta px
        lda hero_x+1
        adc #0
        sta px+1
        clc
        lda hero_y
        adc #20
        sta py
        lda hero_y+1
        adc #0
        sta py+1
        jsr flags_at
        and #F_GEM|F_POWER
        beq @feet
        and #F_POWER
        bne @power
        jsr take_cell           ; a gem: fifty points
        ldx #4
        lda #5
        jsr score_add
        inc _g_gems
        lda _g_dirty
        ora #D_GEMS
        sta _g_dirty
        lda #SFX_GEM
        sta _sfx_req
        bne @feet
@power: jsr take_cell           ; a power unit: the next weapon, a full
        lda _g_power            ; charge, five hundred points
        cmp #2
        bcs :+
        inc _g_power
:       lda #ENERGY_MAX
        sta _g_energy
        lda _g_dirty
        ora #D_POWER|D_ENERGY
        sta _g_dirty
        ldx #3
        lda #5
        jsr score_add
        lda #SFX_POWER
        sta _sfx_req
@feet:  clc
        lda hero_y
        adc #37
        sta py
        lda hero_y+1
        adc #0
        sta py+1
        jsr flags_at
        and #F_HAZARD
        beq @spot
        jmp hero_die            ; lava
@spot:  ldx cp_i                ; checkpoints: one looked at each frame
        dex
        bpl :+
        ldx #NUM_SPOTS-1
:       stx cp_i
        sec
        lda hero_x
        sbc _spot_xl,x
        sta gt
        lda hero_x+1
        sbc _spot_xh,x
        sta gt+1
        clc                     ; within 48 pixels either way
        lda gt
        adc #48
        sta gt
        lda gt+1
        adc #0
        bne @exit
        lda gt
        cmp #96
        bcs @exit
        sec
        lda hero_y
        sbc _spot_yl,x
        sta gt
        lda hero_y+1
        sbc _spot_yh,x
        sta gt+1
        clc
        lda gt
        adc #48
        sta gt
        lda gt+1
        adc #0
        bne @exit
        lda gt
        cmp #96
        bcs @exit
        stx _g_spot
@exit:  lda hero_x+1
        cmp #>EXIT_X
        bcc @r
        bne @out
        lda hero_x
        cmp #<EXIT_X
        bcc @r
@out:   lda #2
        sta _g_event
@r:     rts

; One hit: a bar of energy and a second of grace, or the end of a life.
hero_hurt:
        lda hero_inv
        ora hero_dead
        bne @r
        lda _g_dirty
        ora #D_ENERGY
        sta _g_dirty
        dec _g_energy
        beq hero_die
        lda #INV_TIME
        sta hero_inv
        lda #SFX_HURT
        sta _sfx_req
@r:     rts

hero_die:
        lda hero_dead
        bne @r
        sta _g_energy
        lda _g_dirty
        ora #D_ENERGY
        sta _g_dirty
        lda #DEAD_TIME
        sta hero_dead
        lda #SFX_BOOM
        sta _sfx_req
@r:     rts

; The seconds after death: a burst where he stood, then C decides.
hero_dying:
        dec hero_dead
        bne @r
        inc hero_dead           ; stay dead until C has acted
        lda _g_event
        bne @r
        lda _g_dirty
        ora #D_LIVES
        sta _g_dirty
        dec _g_lives
        bne @again
        lda #1                  ; game over
        sta _g_event
        rts
@again: lda #3
        sta _g_event
@r:     rts

; The hero's two sprites: upper half and legs, frame by state.
hero_sprites:
        lda #HERO_COL
        sta o_col
        sta o_col+1
        lda hero_x
        sta o_xl
        sta o_xl+1
        lda hero_x+1
        sta o_xh
        sta o_xh+1
        lda hero_dead
        beq @live
        cmp #DEAD_TIME-11       ; dying: a burst, then nothing
        bcc @gone
        sec
        lda #DEAD_TIME
        sbc hero_dead
        lsr a
        lsr a
        clc
        adc #SPR_BASE+SF_BOOM1
        sta o_ptr
        sta o_ptr+1
        lda #7
        sta o_col
        sta o_col+1
        lda #0
        sta zt+1
        beq @place
@gone:  lda #$7f
        sta o_yh
        sta o_yh+1
        rts
@live:  lda hero_inv            ; blinking after a hit
        and #2
        bne @gone
        lda hero_on
        bne @gr
        lda hero_vyh
        bmi @up
        lda #SF_HERO_FALL
        bne @lf
@up:    lda #SF_HERO_JUMP
        bne @lf
@gr:    lda _joy
        and #J_LEFT|J_RIGHT
        bne @run
        sta hero_anim
        lda #SF_HERO_STAND
        bne @lf
@run:   inc hero_anim
        lda hero_anim
        lsr a
        lsr a
        and #3
        clc
        adc #SF_HERO_RUN1
@lf:    sta zt
        ldy #0                  ; on the contact frames the body sinks
        cmp #SF_HERO_RUN1       ; a pixel onto the legs
        beq @b1
        cmp #SF_HERO_RUN3
        bne @b0
@b1:    iny
@b0:    sty zt+1
        ldx #0
        lda hero_dir
        beq :+
        ldx #HERO_NFRM
:       stx zt+2
        clc
        lda zt
        adc zt+2
        adc #SPR_BASE
        sta o_ptr+1
        clc
        lda #SF_HERO_TOP+SPR_BASE
        adc zt+2
        sta o_ptr
@place: clc
        lda hero_y
        adc zt+1
        sta o_yl
        lda hero_y+1
        adc #0
        sta o_yh
        clc
        lda hero_y
        adc #21
        sta o_yl+1
        lda hero_y+1
        adc #0
        sta o_yh+1
        rts

; Score: X = which digit (0 the hundred thousands), A = how many to add
; there. The carry ripples left; no division anywhere.
score_add:
        sta gt
@a:     stx gt+1
@c:     inc _g_score,x
        lda _g_score,x
        cmp #10
        bcc @n
        lda #0
        sta _g_score,x
        dex
        bpl @c
@n:     ldx gt+1
        dec gt
        bne @a
        lda _g_dirty
        ora #D_SCORE
        sta _g_dirty
        rts

; ====================================================================
; shots
; ====================================================================
; One bolt, twin bolts, or three spread, by the weapon's power.
shots_update:
        lda fire_cd
        beq :+
        dec fire_cd
:       lda hero_dead
        bne @move
        lda _joy
        and #J_FIRE
        beq @move
        lda fire_cd
        bne @move
        ldy _g_power
        lda fire_cd_tab,y
        sta fire_cd
        lda #SFX_SHOT
        sta _sfx_req
        lda _g_power
        cmp #1
        beq @twin
        ldy #0
        jsr shot_new
        lda _g_power
        beq @move
@twin:  ldy #1
        jsr shot_new
        ldy #2
        jsr shot_new
@move:  ldx #SHOT0
@ml:    lda o_st,x
        beq @mn
        dec ob_life,x
        beq @kill
        lda ob_vx,x
        jsr obj_movex
        lda ob_vy,x
        beq :+
        jsr obj_movey
:       txa                     ; rock ends a bolt; looked at every other
        eor _frame              ; frame, the walls being 32 pixels thick
        and #1
        bne @mn
        clc
        lda o_xl,x
        adc #12
        sta px
        lda o_xh,x
        adc #0
        sta px+1
        clc
        lda o_yl,x
        adc #10
        sta py
        lda o_yh,x
        adc #0
        sta py+1
        stx zt+2
        jsr solid_at
        ldx zt+2
        cmp #0
        beq @mn
@kill:  lda #0
        sta o_st,x
@mn:    inx
        cpx #SHOT_END
        bne @ml
        rts

; Y = 0 the middle bolt, 1 the upper, 2 the lower (they climb and dive
; only at full power).
shot_new:
        ldx #SHOT0
@find:  lda o_st,x
        beq @got
        inx
        cpx #SHOT_END
        bne @find
        rts
@got:   lda #1
        sta o_st,x
        lda #SHOT_LIFE
        sta ob_life,x
        lda #SPR_BASE+SF_SHOT
        sta o_ptr,x
        lda #7
        sta o_col,x
        lda #0
        sta ob_vy,x
        lda _g_power
        cmp #2
        bcc :+
        lda shot_vy,y
        sta ob_vy,x
:       clc
        lda hero_y
        adc shot_dy,y
        sta o_yl,x
        lda hero_y+1
        adc #0
        sta o_yh,x
        lda hero_dir
        bne @sl
        lda #SHOT_V
        sta ob_vx,x
        clc
        lda hero_x
        adc #14
        sta o_xl,x
        lda hero_x+1
        adc #0
        sta o_xh,x
        lda #0
        jmp obj_add_at
@sl:    lda #<(-SHOT_V)
        sta ob_vx,x
        sec
        lda hero_x
        sbc #14
        sta o_xl,x
        lda hero_x+1
        sbc #0
        sta o_xh,x
        lda #0
        jmp obj_add_at

; Move object X by the signed byte in A.
obj_movex:
        cmp #0
        bmi @n
        clc
        adc o_xl,x
        sta o_xl,x
        bcc @r
        inc o_xh,x
        rts
@n:     clc
        adc o_xl,x
        sta o_xl,x
        bcs @r
        dec o_xh,x
@r:     rts

obj_movey:
        cmp #0
        bmi @n
        clc
        adc o_yl,x
        sta o_yl,x
        bcc @r
        inc o_yh,x
        rts
@n:     clc
        adc o_yl,x
        sta o_yl,x
        bcs @r
        dec o_yh,x
@r:     rts

; ====================================================================
; enemies
; ====================================================================
; One entry of the level's list is looked at each frame: an enemy that
; is waiting comes on stage when its place is near the view.
spawn_scan:
        ldy sp_i
        iny
        cpy #NUM_SPAWN
        bcc :+
        ldy #0
:       sty sp_i
        lda sp_state,y
        bne @r
        sec                     ; -64 .. 447 pixels from the view's left
        lda _sp_xl,y
        sbc cam_x
        sta gt
        lda _sp_xh,y
        sbc cam_x+1
        sta gt+1
        clc
        lda gt
        adc #64
        lda gt+1
        adc #0
        cmp #2
        bcs @r
        sec                     ; -32 .. 223 from its top
        lda _sp_yl,y
        sbc cam_y
        sta gt
        lda _sp_yh,y
        sbc cam_y+1
        sta gt+1
        clc
        lda gt
        adc #32
        lda gt+1
        adc #0
        bne @r
        ldx #EN0
@f:     lda o_st,x
        beq @got
        inx
        cpx #EN_END
        bne @f
@r:     rts
@got:   lda #1
        sta o_st,x
        sta sp_state,y
        sta ob_vx,x
        tya
        sta ob_sp,x
        lda _sp_xl,y
        sta o_xl,x
        lda _sp_xh,y
        sta o_xh,x
        lda _sp_yl,y
        sta o_yl,x
        lda _sp_yh,y
        sta o_yh,x
        lda _sp_len,y
        sta ob_len,x
        sta ob_life,x
        lda _sp_type,y
        sta ob_type,x
        tay
        lda en_hp,y
        sta ob_hp,x
        lda en_col,y
        sta o_col,x
        lda #0
        sta ob_fl,x
        sta ob_ph,x
        lda #SPR_BASE+SF_CRAWL1
        cpy #E_TURRET
        bne :+
        lda #60
        sta ob_ph,x
        lda #SPR_BASE+SF_TURRET
:       sta o_ptr,x
        jmp obj_add

enemies_update:
        ldx #EN0
@l:     lda o_st,x
        jeq @n
        txa                     ; far from the view? back to waiting
        eor _frame
        and #7
        bne @act
        sec
        lda o_xl,x
        sbc cam_x
        sta gt
        lda o_xh,x
        sbc cam_x+1
        sta gt+1
        clc
        lda gt
        adc #128
        lda gt+1
        adc #0
        cmp #3
        bcs @away
        sec
        lda o_yl,x
        sbc cam_y
        sta gt
        lda o_yh,x
        sbc cam_y+1
        sta gt+1
        clc
        lda gt
        adc #96
        lda gt+1
        adc #0
        cmp #2
        bcc @act
@away:  lda #0
        sta o_st,x
        ldy ob_sp,x
        lda sp_state,y
        cmp #1
        bne :+
        lda #0
        sta sp_state,y
:       jmp @n
@act:   lda ob_fl,x             ; the white of a hit fades
        beq :+
        dec ob_fl,x
        bne :+
        ldy ob_type,x
        lda en_col,y
        sta o_col,x
:       lda ob_type,x
        cmp #E_WASP
        jeq @wasp
        cmp #E_TURRET
        jeq @turret
        cmp #E_BOOM
        jeq @boom
        ; ---- the crawler: back and forth along its ground
        lda ob_vx,x
        jsr obj_movex
        dec ob_life,x
        bne :+
        jsr @turn
:       lda _frame
        lsr a
        lsr a
        lsr a
        and #1
        clc
        adc #SPR_BASE+SF_CRAWL1
        sta o_ptr,x
        jmp @n
        ; ---- the wasp: twice as fast, on a wave
@wasp:  lda ob_vx,x
        asl a
        jsr obj_movex
        ldy ob_ph,x
        lda dsin,y
        jsr obj_movey
        iny
        tya
        and #31
        sta ob_ph,x
        dec ob_life,x
        dec ob_life,x
        lda ob_life,x
        cmp #2
        bcs :+
        jsr @turn
:       lda _frame
        lsr a
        and #1
        clc
        adc #SPR_BASE+SF_WASP1
        sta o_ptr,x
        jmp @n
        ; ---- the turret: a shot towards the hero now and then
@turret:
        dec ob_ph,x
        bne @n
        lda #90
        sta ob_ph,x
        lda s_y,x               ; only when it is in sight
        cmp #$ff
        beq @n
        jsr bullet_new
        jmp @n
        ; ---- a burst where an enemy was
@boom:  dec ob_ph,x
        beq @gone
        lda #12
        sec
        sbc ob_ph,x
        lsr a
        lsr a
        clc
        adc #SPR_BASE+SF_BOOM1
        sta o_ptr,x
        bne @n
@gone:  lda #0
        sta o_st,x
@n:     inx
        cpx #EN_END
        jne @l
        rts
@turn:  lda ob_vx,x
        eor #$ff
        clc
        adc #1
        sta ob_vx,x
        lda ob_len,x
        sta ob_life,x
        rts

; A bullet from enemy X towards the hero's side.
bullet_new:
        ldy #FX0
@f:     lda o_st,y
        beq @got
        iny
        cpy #NUM_OBJ
        bne @f
        rts
@got:   lda #1
        sta o_st,y
        lda #E_BULLET
        sta ob_type,y
        lda #110
        sta ob_life,y
        lda #SPR_BASE+SF_BULLET
        sta o_ptr,y
        lda #10
        sta o_col,y
        lda o_xl,x
        sta o_xl,y
        lda o_xh,x
        sta o_xh,y
        lda o_yl,x
        sta o_yl,y
        lda o_yh,x
        sta o_yh,y
        lda hero_x              ; which side is he on?
        cmp o_xl,x
        lda hero_x+1
        sbc o_xh,x
        lda #2
        bcs :+
        lda #<-2
:       sta ob_vx,y
        stx gt
        tya
        tax
        jsr obj_add
        ldx gt
        lda #SFX_ESHOT
        sta _sfx_req
        rts

fx_update:
        ldx #FX0
@l:     lda o_st,x
        beq @n
        dec ob_life,x
        beq @kill
        lda ob_vx,x
        jsr obj_movex
        jmp @n
@kill:  lda #0
        sta o_st,x
@n:     inx
        cpx #NUM_OBJ
        bne @l
        rts

; ====================================================================
; who touches whom
; ====================================================================
; All of it in screen space, with the bytes the multiplexer left behind
; last frame: s_y (255 = not on screen) and s_x2, half the 9-bit X.
collide:
        ldx #SHOT0
@s:     lda o_st,x
        beq @sn
        lda s_y,x
        cmp #$ff
        beq @sn
        ldy #EN0
@e:     lda o_st,y
        beq @en
        lda ob_type,y
        cmp #E_BOOM
        bcs @en
        lda s_y,y
        cmp #$ff
        beq @en
        lda s_y,x               ; the bolt's middle inside the enemy
        sec
        sbc s_y,y
        clc
        adc #8
        cmp #19
        bcs @en
        lda s_x2,x
        sec
        sbc s_x2,y
        clc
        adc #5
        cmp #10
        bcs @en
        lda #0
        sta o_st,x              ; the bolt is spent
        jsr enemy_hit
        jmp @sn
@en:    iny
        cpy #EN_END
        bne @e
@sn:    inx
        cpx #SHOT_END
        bne @s

        lda hero_inv
        ora hero_dead
        jne @r
        lda s_y+1
        cmp #$ff
        jeq @r
        ldy #EN0
@h:     lda o_st,y
        beq @hn
        lda s_y,y
        cmp #$ff
        beq @hn
        lda ob_type,y
        cmp #E_BOOM
        beq @hn
        cmp #E_BULLET
        beq @bul
        lda s_y,y               ; a body: any overlap of the two boxes
        sec
        sbc s_y+1
        clc
        adc #38
        cmp #56
        bcs @hn
        lda s_x2,y
        sec
        sbc s_x2
        clc
        adc #7
        cmp #15
        bcs @hn
        jmp hero_hurt
@bul:   lda s_y,y               ; a bullet: its middle inside the hero
        sec
        sbc s_y+1
        clc
        adc #28
        cmp #38
        bcs @hn
        lda s_x2,y
        sec
        sbc s_x2
        clc
        adc #3
        cmp #6
        bcs @hn
        lda #0
        sta o_st,y
        jmp hero_hurt
@hn:    iny
        cpy #NUM_OBJ
        bne @h
@r:     rts

; Enemy Y takes a hit: white for a moment, or a burst and its points.
; X is kept.
enemy_hit:
        lda ob_hp,y
        sec
        sbc #1
        sta ob_hp,y
        beq @dead
        lda #4
        sta ob_fl,y
        lda #1
        sta o_col,y
        lda #SFX_HIT
        sta _sfx_req
        rts
@dead:  stx eh_x
        sty eh_y
        ldx ob_type,y
        lda en_pts,x
        ldx #3
        jsr score_add
        ldy eh_y
        ldx ob_sp,y
        lda #2
        sta sp_state,x
        lda #E_BOOM
        sta ob_type,y
        lda #12
        sta ob_ph,y
        lda #7
        sta o_col,y
        lda #0
        sta ob_fl,y
        lda #SFX_BOOM
        sta _sfx_req
        ldx eh_x
        rts

; ====================================================================
; the camera
; ====================================================================
; It moves only when the hero's centre leaves a box in the middle of the
; view, by as much as he is outside it, up to its top speed. That top
; speed is above his own, so a wait at a boundary is made up afterwards.
camera_update:
        sec
        lda hero_x
        sbc cam_x
        sta zt
        lda hero_x+1
        sbc cam_x+1
        sta zt+1
        clc
        lda zt
        adc #12
        sta zt
        bcc :+
        inc zt+1
:       ldx #0
        lda zt+1
        bmi @xl3
        bne @xr3
        lda zt
        cmp #CAM_XHI+1
        bcs @xr
        cmp #CAM_XLO
        bcs @xs
        lda #CAM_XLO
        sec
        sbc zt
        cmp #CAM_VX+1
        bcc :+
        lda #CAM_VX
:       eor #$ff
        clc
        adc #1
        tax
        jmp @xs
@xr:    sec
        sbc #CAM_XHI
        cmp #CAM_VX+1
        bcc :+
        lda #CAM_VX
:       tax
        jmp @xs
@xl3:   ldx #<(-CAM_VX)
        bne @xs
@xr3:   ldx #CAM_VX
@xs:    txa                     ; keep the view inside the map
        beq @xset
        bmi @xneg
        sec
        lda #<CAM_X_MAX
        sbc cam_x
        sta zt
        lda #>CAM_X_MAX
        sbc cam_x+1
        bne @xset
        cpx zt
        bcc @xset
        ldx zt
        jmp @xset
@xneg:  lda cam_x+1
        bne @xset
        txa
        eor #$ff
        clc
        adc #1
        cmp cam_x
        bcc @xset
        lda cam_x
        eor #$ff
        clc
        adc #1
        tax
@xset:  stx s_v

        ldx #2                  ; pick the box: ground or air
        lda hero_on
        bne :+
        ldx #5
:       lda cam_box,x
        sta @vcap1+1
        sta @vcap2+1
        sta @vcap3+1
        sta @vcap4+1
        clc
        adc #1
        sta @vlim1+1
        sta @vlim2+1
        lda cam_box-1,x
        sta @yhi1+1
        sta @yhi2+1
        inc @yhi1+1
        lda cam_box-2,x
        sta @ylo1+1
        sta @ylo2+1
        sec
        lda hero_y
        sbc cam_y
        sta zt
        lda hero_y+1
        sbc cam_y+1
        sta zt+1
        clc
        lda zt
        adc #20
        sta zt
        bcc :+
        inc zt+1
:       ldx #0
        lda zt+1
        bmi @yu4
        bne @yd4
        lda zt
@yhi1:  cmp #CAM_YHI_A+1
        bcs @yd
@ylo1:  cmp #CAM_YLO_A
        bcs @ys
@ylo2:  lda #CAM_YLO_A
        sec
        sbc zt
@vlim1: cmp #CAM_VY_A+1
        bcc :+
@vcap1: lda #CAM_VY_A
:       eor #$ff
        clc
        adc #1
        tax
        jmp @ys
@yd:    sec
@yhi2:  sbc #CAM_YHI_A
@vlim2: cmp #CAM_VY_A+1
        bcc :+
@vcap2: lda #CAM_VY_A
:       tax
        jmp @ys
@yu4:
@vcap3: lda #CAM_VY_A
        eor #$ff
        clc
        adc #1
        tax
        jmp @ys
@yd4:
@vcap4: ldx #CAM_VY_A
@ys:    txa
        beq @yset
        bmi @yneg
        sec
        lda #<CAM_Y_MAX
        sbc cam_y
        sta zt
        lda #>CAM_Y_MAX
        sbc cam_y+1
        bne @yset
        cpx zt
        bcc @yset
        ldx zt
        jmp @yset
@yneg:  lda cam_y+1
        bne @yset
        txa
        eor #$ff
        clc
        adc #1
        cmp cam_y
        bcc @yset
        lda cam_y
        eor #$ff
        clc
        adc #1
        tax
@yset:  stx s_v+1
        jmp _scroll_step

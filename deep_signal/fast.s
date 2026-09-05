; DEEP SIGNAL fast actor routines, written for this game's data layout.
.import _px,_py,_pyq,_vy,_grounded,_facing,_keys,_oldkeys,_demo,_phase
.import _ex,_ey,_alive,_enemy_x,_enemy_y,_shot_x,_shot_y,_shot_life,_shot_left
.import _fire_delay,_score,_invulnerable,_health,_signals,_camera_x,_camera_y
.import _route,_route_x,_route_y,_relay_x,_relay_y
.export _demo_input
.import _sx,_sy,_sptr,_scol,_xmask,_enable
.importzp ptr1
.export _fast_physics,_fast_actors,_project_sprites,_effect,_needs_hurt
.segment "BSS"
qx: .res 2
qy: .res 2
nx: .res 2
ny: .res 2
oldfoot: .res 2
foot: .res 2
_effect: .res 1
_needs_hurt: .res 1
ii: .res 1
slot: .res 1
frame: .res 1
ink: .res 1
vx: .res 2
wobble: .res 1
adx: .res 2
ady: .res 2
choice: .res 1
.segment "RODATA"
bits: .byte 1,2,4,8,16,32,64,128
offsets: .byte 0,29,58,87
yoffsets: .byte 0,17,34,51
.segment "CODE"
; qx/qy pixels -> map tile. The caller owns ptr1; IRQ never touches it.
get_tile:
    lda qx+1
    cmp #4
    bcs outside
    lda qy+1
    cmp #2
    bcs outside
    lda qx
    lsr
    lsr
    lsr
    sta ptr1
    lda qx+1
    asl
    asl
    asl
    asl
    asl
    ora ptr1
    sta ptr1
    lda qy
    and #8
    asl
    asl
    asl
    asl
    ora ptr1
    sta ptr1
    lda qy
    lsr
    lsr
    lsr
    lsr
    sta ptr1+1
    lda qy+1
    asl
    asl
    asl
    asl
    ora ptr1+1
    ora #$80
    sta ptr1+1
    ldy #0
    lda (ptr1),y
    rts
outside:
    lda #128
    rts
is_wall:
    jsr get_tile
    cmp #128
    bcc no_wall
    cmp #160
    bcs no_wall
    lda #1
    rts
no_wall:
    lda #0
    rts
; Two wall probes for horizontal movement. qx prepared by caller.
side_wall:
    clc
    lda _py
    adc #3
    sta qy
    lda _py+1
    adc #0
    sta qy+1
    jsr is_wall
    bne side_done
    clc
    lda qy
    adc #14
    sta qy
    bcc :+
    inc qy+1
:   jsr is_wall
side_done:rts
set_qx_left:
    clc
    lda _px
    adc #2
    sta qx
    lda _px+1
    adc #0
    sta qx+1
    rts
qx_right:
    clc
    lda qx
    adc #9
    sta qx
    bcc :+
    inc qx+1
:   rts
store_yq:
    lda ny
    sta _py
    asl
    sta _pyq
    lda ny+1
    sta _py+1
    rol
    sta _pyq+1
    asl _pyq
    rol _pyq+1
    rts
_fast_physics:
    lda #0
    sta _needs_hurt
    sta _effect
    lda _keys
    and #4
    beq right
    lda #1
    sta _facing
    lda _px+1
    bne left_ok
    lda _px
    cmp #10
    bcc right
left_ok:
    sec
    lda _px
    sbc #2
    sta nx
    sta qx
    lda _px+1
    sbc #0
    sta nx+1
    sta qx+1
    jsr side_wall
    bne right
    lda nx
    sta _px
    lda nx+1
    sta _px+1
right:
    lda _keys
    and #8
    beq drop
    lda #0
    sta _facing
    lda _px+1
    cmp #3
    bcc right_ok
    lda _px
    cmp #232
    bcs drop
right_ok:
    clc
    lda _px
    adc #2
    sta nx
    lda _px+1
    adc #0
    sta nx+1
    clc
    lda nx
    adc #12
    sta qx
    lda nx+1
    adc #0
    sta qx+1
    jsr side_wall
    bne drop
    lda nx
    sta _px
    lda nx+1
    sta _px+1
drop:
    lda _keys
    and #2
    bne :+
    jmp jump
:   lda _oldkeys
    and #2
    beq :+
    jmp jump
:   lda _grounded
    bne :+
    jmp jump
:
    ; DOWN links a nearby relay without dropping its supporting platform.
    ldx #0
drop_relay:
    sec
    lda _px
    sbc _relay_x,x
    sta vx
    lda _px+1
    sbc _relay_x+1,x
    sta vx+1
    clc
    lda vx
    adc #19
    tay
    lda vx+1
    adc #0
    bne drop_next
    cpy #43
    bcs drop_next
    sec
    lda _py
    sbc _relay_y,x
    sta vx
    lda _py+1
    sbc _relay_y+1,x
    sta vx+1
    clc
    lda vx
    adc #21
    tay
    lda vx+1
    adc #0
    bne drop_next
    cpy #47
    bcs drop_next
    jmp jump
drop_next:
    inx
    inx
    cpx #6
    bne drop_relay
    ; The exit hatch also handles DOWN before any drop.
    lda _px+1
    cmp #3
    bcc do_drop
    lda _px
    cmp #184
    bcc do_drop
    lda _py+1
    bne do_drop
    lda _py
    cmp #150
    bcs do_drop
    jmp jump
do_drop:
    inc _py
    bne :+
    inc _py+1
:   lda _py
    sta ny
    lda _py+1
    sta ny+1
    jsr store_yq
    lda #4
    sta _vy
    lda #0
    sta _grounded
jump:
    lda _grounded
    beq gravity
    lda _keys
    and #1
    beq gravity
    lda _demo
    bne launch
    lda _oldkeys
    and #1
    bne gravity
launch:
    lda #232
    sta _vy
    lda #0
    sta _grounded
    lda #2
    sta _effect
gravity:
    lda _vy
    bmi accelerate
    cmp #24
    bcs integrate
accelerate:
    inc _vy
integrate:
    clc
    lda _py
    adc #20
    sta oldfoot
    lda _py+1
    adc #0
    sta oldfoot+1
    ldx #0
    lda _vy
    bpl :+
    dex
:   clc
    adc _pyq
    sta _pyq
    txa
    adc _pyq+1
    sta _pyq+1
    lda _pyq+1
    lsr
    sta ny+1
    lda _pyq
    ror
    sta ny
    lsr ny+1
    ror ny
    lda #0
    sta _grounded
    lda _vy
    bpl falling
    lda ny
    sta qy
    lda ny+1
    sta qy+1
    jsr set_qx_left
    jsr is_wall
    bne hit_head
    jsr qx_right
    jsr is_wall
    bne hit_head
    jmp store_position
hit_head:
    clc
    lda ny
    and #$f8
    adc #8
    sta ny
    bcc :+
    inc ny+1
:   lda #0
    sta _vy
    jsr store_yq
    jmp store_position
falling:
    clc
    lda ny
    adc #20
    sta qy
    sta foot
    lda ny+1
    adc #0
    sta qy+1
    sta foot+1
    jsr set_qx_left
    jsr get_tile
    cmp #128
    bcc second_foot
    cmp #176
    bcc floor_hit
second_foot:
    jsr qx_right
    jsr get_tile
    cmp #128
    bcc store_position
    cmp #176
    bcs store_position
floor_hit:
    lda foot
    and #$f8
    sta foot
    lda oldfoot+1
    cmp foot+1
    bcc land
    bne store_position
    lda oldfoot
    cmp foot
    beq land
    bcs store_position
land:
    sec
    lda foot
    sbc #20
    sta ny
    lda foot+1
    sbc #0
    sta ny+1
    lda #0
    sta _vy
    lda #1
    sta _grounded
    jsr store_yq
store_position:
    lda ny
    sta _py
    lda ny+1
    sta _py+1
    cmp #1
    bcc hazard_probe
    bne hurt
    lda ny
    cmp #225
    bcs hurt
hazard_probe:
    clc
    lda _px
    adc #6
    sta qx
    lda _px+1
    adc #0
    sta qx+1
    clc
    lda _py
    adc #18
    sta qy
    lda _py+1
    adc #0
    sta qy+1
    jsr get_tile
    cmp #192
    bcc physics_done
hurt:
    lda #1
    sta _needs_hurt
physics_done:rts

_fast_actors:
    lda _fire_delay
    beq :+
    dec _fire_delay
:   lda _keys
    and #16
    beq shot_move
    lda _shot_life
    ora _fire_delay
    bne shot_move
    lda _facing
    sta _shot_left
    beq muzzle_right
    lda _px
    sta _shot_x
    lda _px+1
    sta _shot_x+1
    jmp muzzle_y
muzzle_right:
    clc
    lda _px
    adc #12
    sta _shot_x
    lda _px+1
    adc #0
    sta _shot_x+1
muzzle_y:
    clc
    lda _py
    adc #3
    sta _shot_y
    lda _py+1
    adc #0
    sta _shot_y+1
    lda #38
    sta _shot_life
    lda #9
    sta _fire_delay
    lda #1
    sta _effect
shot_move:
    lda _shot_life
    beq enemies_start
    dec _shot_life
    lda _shot_left
    beq shot_right
    sec
    lda _shot_x
    sbc #6
    sta _shot_x
    lda _shot_x+1
    sbc #0
    sta _shot_x+1
    bcc shot_end
    jmp shot_probe
shot_right:
    clc
    lda _shot_x
    adc #6
    sta _shot_x
    lda _shot_x+1
    adc #0
    sta _shot_x+1
shot_probe:
    clc
    lda _shot_x
    adc #6
    sta qx
    lda _shot_x+1
    adc #0
    sta qx+1
    clc
    lda _shot_y
    adc #8
    sta qy
    lda _shot_y+1
    adc #0
    sta qy+1
    jsr is_wall
    beq enemies_start
shot_end:
    lda #0
    sta _shot_life
enemies_start:
    lda #0
    sta ii
enemy_loop:
    ldy ii
    lda _alive,y
    bne :+
    jmp next_enemy
:   lda _phase
    clc
    adc offsets,y
    and #63
    cmp #32
    bcc :+
    eor #63
:   sta wobble
    tya
    asl
    tax
    clc
    lda _enemy_x,x
    adc wobble
    sta _ex,x
    lda _enemy_x+1,x
    adc #0
    sta _ex+1,x
    sec
    lda _ex,x
    sbc #16
    sta _ex,x
    lda _ex+1,x
    sbc #0
    sta _ex+1,x
    lda _phase
    clc
    adc yoffsets,y
    and #16
    beq :+
    lda #3
:   clc
    adc _enemy_y,x
    sta _ey,x
    lda _enemy_y+1,x
    adc #0
    sta _ey+1,x
    lda _shot_life
    beq hero_collision
    ; abs(shot_x - enemy_x + 4) < 18
    sec
    lda _shot_x
    sbc _ex,x
    sta vx
    lda _shot_x+1
    sbc _ex+1,x
    sta vx+1
    clc
    lda vx
    adc #21
    tay
    lda vx+1
    adc #0
    bne hero_collision
    cpy #35
    bcs hero_collision
    sec
    lda _shot_y
    sbc _ey,x
    sta vx
    lda _shot_y+1
    sbc _ey+1,x
    sta vx+1
    clc
    lda vx
    adc #13
    tay
    lda vx+1
    adc #0
    bne hero_collision
    cpy #27
    bcs hero_collision
    ldy ii
    lda #0
    sta _alive,y
    sta _shot_life
    clc
    lda _score
    adc #50
    sta _score
    bcc :+
    inc _score+1
:   lda #3
    sta _effect
    jmp next_enemy
hero_collision:
    lda _invulnerable
    bne next_enemy
    sec
    lda _px
    sbc _ex,x
    sta vx
    lda _px+1
    sbc _ex+1,x
    sta vx+1
    clc
    lda vx
    adc #13
    tay
    lda vx+1
    adc #0
    bne next_enemy
    cpy #27
    bcs next_enemy
    sec
    lda _py
    sbc _ey,x
    sta vx
    lda _py+1
    sbc _ey+1,x
    sta vx+1
    clc
    lda vx
    adc #15
    tay
    lda vx+1
    adc #0
    bne next_enemy
    cpy #27
    bcs next_enemy
    lda #1
    sta _needs_hurt
next_enemy:
    inc ii
    lda ii
    cmp #4
    beq actors_done
    jmp enemy_loop
actors_done:rts

; qx/qy world position, frame, ink, slot -> staged VIC registers.
project:
    sec
    lda qx
    sbc _camera_x
    sta vx
    lda qx+1
    sbc _camera_x+1
    sta vx+1
    clc
    lda vx
    adc #25
    sta vx
    lda vx+1
    adc #0
    sta vx+1
    cmp #2
    bcs project_done
    cmp #1
    beq high_x
    lda vx
    cmp #8
    bcc project_done
    jmp project_y
high_x:
    lda vx
    cmp #80
    bcs project_done
project_y:
    sec
    lda qy
    sbc _camera_y
    sta ny
    lda qy+1
    sbc _camera_y+1
    sta ny+1
    clc
    lda ny
    adc #57
    tay
    lda ny+1
    adc #0
    bne project_done
    cpy #50
    bcc project_done
    cpy #244
    bcs project_done
    ldx slot
    tya
    sta _sy,x
    lda vx
    sta _sx,x
    lda frame
    clc
    adc #64
    sta _sptr,x
    lda ink
    sta _scol,x
    lda bits,x
    ora _enable
    sta _enable
    lda vx+1
    beq project_done
    lda bits,x
    ora _xmask
    sta _xmask
project_done:rts
_project_sprites:
    lda #0
    sta _xmask
    sta _enable
    sta slot
    lda _invulnerable
    beq player_visible
    lda _phase
    and #4
    beq enemies_visible
player_visible:
    lda _px
    sta qx
    lda _px+1
    sta qx+1
    lda _py
    sta qy
    lda _py+1
    sta qy+1
    lda #0
    sta frame
    lda _grounded
    bne walking
    lda #3
    bne hero_frame
walking:
    lda _keys
    and #12
    beq hero_frame
    lda _phase
    lsr
    lsr
    lsr
    and #1
hero_frame:
    sta frame
    lda _facing
    beq :+
    lda frame
    ora #4
    sta frame
:   lda #8
    sta ink
    jsr project
enemies_visible:
    lda #0
    sta ii
sprite_loop:
    ldy ii
    lda _alive,y
    beq next_sprite
    tya
    asl
    tax
    lda _ex,x
    sta qx
    lda _ex+1,x
    sta qx+1
    lda _ey,x
    sta qy
    lda _ey+1,x
    sta qy+1
    iny
    sty slot
    lda _phase
    lsr
    lsr
    lsr
    and #1
    ora #8
    sta frame
    lda ii
    and #1
    asl
    clc
    adc #2
    sta ink
    jsr project
next_sprite:
    inc ii
    lda ii
    cmp #4
    bne sprite_loop
    lda _shot_life
    beq hud
    lda _shot_x
    sta qx
    lda _shot_x+1
    sta qx+1
    lda _shot_y
    sta qy
    lda _shot_y+1
    sta qy+1
    lda _shot_left
    clc
    adc #12
    sta frame
    lda #5
    sta slot
    lda #7
    sta ink
    jsr project
hud:
    lda #35
    sta _sx+6
    lda #49
    sta _sx+7
    lda #58
    sta _sy+6
    sta _sy+7
    lda _health
    clc
    adc #78
    sta _sptr+6
    lda _signals
    clc
    adc #84
    sta _sptr+7
    lda #3
    sta _scol+6
    lda #5
    sta _scol+7
    lda _enable
    ora #192
    sta _enable
    lda _xmask
    ora #128
    sta _xmask
    rts

; Waypoint demo uses exactly the same input bits and collision rules as a player.
_demo_input:
    lda _route
    asl
    tax
    sec
    lda _px
    sbc _route_x,x
    sta qx
    lda _px+1
    sbc _route_x+1,x
    sta qx+1
    bpl dx_positive
    sec
    lda #0
    sbc qx
    sta adx
    lda #0
    sbc qx+1
    sta adx+1
    jmp demo_dy
dx_positive:
    lda qx
    sta adx
    lda qx+1
    sta adx+1
demo_dy:
    sec
    lda _py
    sbc _route_y,x
    sta qy
    lda _py+1
    sbc _route_y+1,x
    sta qy+1
    bpl dy_positive
    sec
    lda #0
    sbc qy
    sta ady
    lda #0
    sbc qy+1
    sta ady+1
    jmp route_arrival
dy_positive:
    lda qy
    sta ady
    lda qy+1
    sta ady+1
route_arrival:
    lda _grounded
    beq demo_choose
    lda adx+1
    ora ady+1
    bne demo_choose
    lda adx
    cmp #12
    bcs demo_choose
    lda ady
    cmp #10
    bcs demo_choose
    lda _route
    cmp #6
    bcs demo_choose
    cmp #1
    beq needs_one
    cmp #3
    beq needs_two
    cmp #5
    beq needs_three
    jmp advance
needs_one:
    lda _signals
    and #1
    beq demo_choose
    bne advance
needs_two:
    lda _signals
    and #2
    beq demo_choose
    bne advance
needs_three:
    lda _signals
    and #4
    beq demo_choose
advance:
    inc _route
    jmp _demo_input
demo_choose:
    lda #16
    sta choice
    lda adx+1
    bne choose_horizontal
    lda adx
    cmp #5
    bcc choose_vertical
choose_horizontal:
    lda qx+1
    bmi go_right
    lda #4
    bne append_horizontal
go_right:
    lda #8
append_horizontal:
    ora choice
    sta choice
choose_vertical:
    lda _grounded
    beq choose_down
    lda qy+1
    bmi choose_down
    bne jump_choice
    lda qy
    cmp #5
    bcc choose_down
jump_choice:
    lda choice
    ora #1
    sta choice
choose_down:
    lda adx+1
    bne choice_done
    lda adx
    cmp #16
    bcs choice_done
    lda qy+1
    bmi down_choice
    lda _route
    and #1
    bne :+
    lda _route
    cmp #6
    bne choice_done
:   lda ady+1
    bne choice_done
    lda ady
    cmp #16
    bcs choice_done
down_choice:
    lda choice
    ora #2
    sta choice
choice_done:
    lda choice
    ldx #0
    rts

.import _updates,_keys,_route
.export _trace_frame
_trace_frame:
    lda _updates
    asl
    asl
    asl
    sta ptr1
    lda _updates
    lsr
    lsr
    lsr
    lsr
    lsr
    ora #$68
    sta ptr1+1
    ldy #0
    lda _px
    sta (ptr1),y
    iny
    lda _px+1
    sta (ptr1),y
    iny
    lda _py
    sta (ptr1),y
    iny
    lda _py+1
    sta (ptr1),y
    iny
    lda _keys
    sta (ptr1),y
    iny
    lda _grounded
    sta (ptr1),y
    iny
    lda _route
    sta (ptr1),y
    iny
    lda _vy
    sta (ptr1),y
    rts

.import _coarse_x,_coarse_y,_front,_dirty,_renders,_last_diag
.import _view_ptr,_stage_dx,_stage_dy,_stage_d018,_dir_x,_dir_y
.import _render_a,_render_b
.export _fast_camera
.segment "BSS"
render_start: .res 2
.segment "CODE"
_fast_camera:
    lda $033f
    bne diagnostic_camera
    sta _last_diag
    sec
    lda _px
    sbc #144
    sta _camera_x
    lda _px+1
    sbc #0
    sta _camera_x+1
    sec
    lda _py
    sbc #88
    sta _camera_y
    lda _py+1
    sbc #0
    sta _camera_y+1
    jmp clamp_camera
diagnostic_camera:
    lda _last_diag
    bne diag_step
    lda #64
    sta _camera_x
    lda #1
    sta _camera_x+1
    lda #96
    sta _camera_y
    lda #0
    sta _camera_y+1
    lda #1
    sta _last_diag
diag_step:
    lda _updates
    rol
    rol
    rol
    and #3
    sta ii
    lda _updates+1
    and #1
    asl
    asl
    ora ii
    tax
    lda _dir_x,x
    beq diag_y
    bmi diag_left
    inc _camera_x
    bne diag_y
    inc _camera_x+1
    jmp diag_y
diag_left:
    lda _camera_x
    bne :+
    dec _camera_x+1
:   dec _camera_x
diag_y:
    lda _dir_y,x
    beq clamp_camera
    bmi diag_up
    inc _camera_y
    bne clamp_camera
    inc _camera_y+1
    jmp clamp_camera
diag_up:
    lda _camera_y
    bne :+
    dec _camera_y+1
:   dec _camera_y
clamp_camera:
    lda _camera_x+1
    bmi clamp_x_zero
    cmp #2
    bcc clamp_y
    bne clamp_x_max
    lda _camera_x
    cmp #192
    bcc clamp_y
clamp_x_max:
    lda #192
    sta _camera_x
    lda #2
    sta _camera_x+1
    jmp clamp_y
clamp_x_zero:
    lda #0
    sta _camera_x
    sta _camera_x+1
clamp_y:
    lda _camera_y+1
    bmi clamp_y_zero
    cmp #1
    bcc coarse
    bne clamp_y_max
    lda _camera_y
    cmp #56
    bcc coarse
clamp_y_max:
    lda #56
    sta _camera_y
    lda #1
    sta _camera_y+1
    jmp coarse
clamp_y_zero:
    lda #0
    sta _camera_y
    sta _camera_y+1
coarse:
    lda _camera_x
    lsr
    lsr
    lsr
    sta ii
    lda _camera_x+1
    asl
    asl
    asl
    asl
    asl
    ora ii
    sta ii
    lda _camera_y
    lsr
    lsr
    lsr
    sta wobble
    lda _camera_y+1
    asl
    asl
    asl
    asl
    asl
    ora wobble
    sta wobble
    lda _dirty
    bne redraw
    lda ii
    cmp _coarse_x
    bne redraw
    lda wobble
    cmp _coarse_y
    bne redraw
    jmp fine
redraw:
    lda ii
    sta _coarse_x
    lda wobble
    sta _coarse_y
    lsr
    ora #$80
    sta _view_ptr+1
    lda wobble
    and #1
    beq :+
    lda #128
:   ora ii
    sta _view_ptr
    lda $dd04
    sta render_start
    lda $dd05
    sta render_start+1
    lda _front
    beq draw_b
    jsr _render_a
    lda #0
    sta _front
    lda #2
    bne rendered
draw_b:
    jsr _render_b
    lda #1
    sta _front
    lda #18
rendered:
    sta _stage_d018
    lda $dd04
    sta vx
    lda $dd05
    sta vx+1
    sec
    lda render_start
    sbc vx
    sta $0370
    lda render_start+1
    sbc vx+1
    sta $0371
    inc _renders
    bne :+
    inc _renders+1
:   lda #0
    sta _dirty
fine:
    lda _camera_x
    eor #7
    and #7
    ora #16
    sta _stage_dx
    lda _camera_y
    eor #7
    and #7
    ora #16
    sta _stage_dy
    rts

.import _checkpoint_x,_checkpoint_y
.export _fast_interact
_fast_interact:
    lda _keys
    and #2
    bne :+
    rts
:   lda #0
    sta ii
link_loop:
    ldx ii
    lda bits,x
    and _signals
    beq :+
    jmp next_link
:   txa
    asl
    tax
    sec
    lda _px
    sbc _relay_x,x
    sta vx
    lda _px+1
    sbc _relay_x+1,x
    sta vx+1
    clc
    lda vx
    adc #19
    tay
    lda vx+1
    adc #0
    beq :+
    jmp next_link
:   cpy #43
    bcc :+
    jmp next_link
:   sec
    lda _py
    sbc _relay_y,x
    sta vx
    lda _py+1
    sbc _relay_y+1,x
    sta vx+1
    clc
    lda vx
    adc #21
    tay
    lda vx+1
    adc #0
    beq :+
    jmp next_link
:   cpy #47
    bcc :+
    jmp next_link
:   lda _relay_x,x
    sta _checkpoint_x
    sta qx
    lda _relay_x+1,x
    sta _checkpoint_x+1
    sta qx+1
    lda _relay_y,x
    sta qy
    clc
    adc #4
    sta _checkpoint_y
    lda _relay_y+1,x
    sta qy+1
    adc #0
    sta _checkpoint_y+1
    ldx ii
    lda bits,x
    ora _signals
    sta _signals
    clc
    lda _score
    adc #250
    sta _score
    bcc :+
    inc _score+1
:   lda #5
    sta _health
    lda #2
    sta _effect
    jsr get_tile
    ldy #0
    lda #86
    sta (ptr1),y
    iny
    lda #87
    sta (ptr1),y
    ldy #128
    lda #88
    sta (ptr1),y
    iny
    lda #89
    sta (ptr1),y
    inc ptr1+1
    ldy #0
    lda #90
    sta (ptr1),y
    iny
    lda #91
    sta (ptr1),y
    lda #1
    sta _dirty
    rts
next_link:
    inc ii
    lda ii
    cmp #3
    beq links_done
    jmp link_loop
links_done:rts

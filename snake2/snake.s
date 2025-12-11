; -----------------------------------------------------------------------------
; SNAKE 2 - Sprite-Based Version for C64
; Uses hardware sprites for smooth movement
; -----------------------------------------------------------------------------

; --- VIC-II Sprite Registers ---
VIC_BASE    = $D000
SPR0_X      = $D000     ; Sprite 0 X position
SPR0_Y      = $D001     ; Sprite 0 Y position
SPR1_X      = $D002     ; Sprite 1 X position (apple)
SPR1_Y      = $D003     ; Sprite 1 Y position
SPR_ENABLE  = $D015     ; Sprite enable register
SPR_X_MSB   = $D010     ; MSB of X coordinates (bit 8)
SPR_EXPAND_X = $D01D    ; Sprite X expand
SPR_EXPAND_Y = $D017    ; Sprite Y expand
SPR_PRIORITY = $D01B    ; Sprite priority (0=front)
SPR_MULTICLR = $D01C    ; Multicolor mode
SPR_SPR_COL = $D01E     ; Sprite-sprite collision
SPR_BG_COL  = $D01F     ; Sprite-background collision
SPR0_COLOR  = $D027     ; Sprite 0 color
SPR1_COLOR  = $D028     ; Sprite 1 color
SPR2_COLOR  = $D029     ; Sprite 2 color
SPR3_COLOR  = $D02A     ; Sprite 3 color
SPR4_COLOR  = $D02B     ; Sprite 4 color
SPR5_COLOR  = $D02C     ; Sprite 5 color
SPR6_COLOR  = $D02D     ; Sprite 6 color

; --- VIC-II Sprite Offset Constants ---
; Sprites are offset from visible screen area
SPRITE_X_OFFSET = 24    ; X offset for visible area
SPRITE_Y_OFFSET = 50    ; Y offset for visible area
SPR7_COLOR  = $D02E     ; Sprite 7 color
SPR_PTRS    = $07F8     ; Sprite pointers (screen + $3F8)

; --- Other Hardware ---
SCREEN_RAM  = $0400
COLOR_RAM   = $D800
JOYSTICK    = $DC00
BORDER      = $D020
BG_COLOR    = $D021

; --- SID Sound Chip ---
SID_BASE    = $D400
SID_V1_FREQ_LO = $D400      ; Voice 1 frequency low
SID_V1_FREQ_HI = $D401      ; Voice 1 frequency high
SID_V1_PW_LO = $D402        ; Pulse width low
SID_V1_PW_HI = $D403        ; Pulse width high
SID_V1_CTRL = $D404         ; Control: gate, waveform
SID_V1_AD   = $D405         ; Attack/Decay
SID_V1_SR   = $D406         ; Sustain/Release
SID_VOLUME  = $D418         ; Master volume + filter mode

; --- Game Constants ---
SPRITE_BASE = $2000     ; Sprite data starts here (block 128 = $2000/64)
SPRITE_BLOCK = 128      ; First sprite block number ($2000/64)

; Screen boundaries for sprites (in pixels)
MIN_X       = 24        ; Left edge
MAX_X       = 255       ; Right edge (without MSB)
MIN_Y       = 50        ; Top edge
MAX_Y       = 229       ; Bottom edge

; Movement step (in pixels)
MOVE_STEP   = 4         ; 4 pixels for tighter snake movement

; Character codes for background
CHAR_SPACE  = 32
CHAR_WALL   = 160
CHAR_GRASS  = 46        ; Period for grass texture

; --- Zero Page Segment ---
.segment "ZEROPAGE"
head_x:     .res 2      ; 16-bit X position for MSB handling
head_y:     .res 1      ; Y position
dir:        .res 1      ; 0=N, 1=S, 2=W, 3=E
apple_x:    .res 2      ; 16-bit Apple X
apple_y:    .res 1      ; Apple Y
score_lo:   .res 1      ; Score low byte (0-255)
score_hi:   .res 1      ; Score high byte (for 256-999)
hiscore_lo: .res 1      ; High score low byte
hiscore_hi: .res 1      ; High score high byte
frame_cnt:  .res 1
tick:       .res 1      ; RNG counter
ptr_lo:     .res 1
ptr_hi:     .res 1
tmp_lo:     .res 1
tmp_hi:     .res 1
tail_idx:   .res 1      ; Tail buffer index
head_idx:   .res 1      ; Head buffer index
grow:       .res 1      ; Grow flag
input_delay:.res 1
game_over:  .res 1
speed:      .res 1      ; Game speed (frames between moves)
temp_count: .res 1      ; Temporary counter for loops
demo_mode:  .res 1      ; 0 = player mode, 1 = demo mode

; --- BSS Segment ---
.segment "BSS"
; Trail positions for body segments (using sprites 2-7)
trail_x_lo: .res 64     ; X low byte history
trail_x_hi: .res 64     ; X high byte history  
trail_y:    .res 64     ; Y history

; --- BASIC Header ---
.segment "HEADER"
    .word $0801
    .word next_line
    .word 10
    .byte $9E
    .byte "2061", 0
next_line:
    .word 0

; --- Main Code ---
.segment "CODE"
start:
    sei
    cld
    
    ; Initialize SID for sound effects
    jsr init_sound
    
    ; Initialize high score (once at start)
    lda #0
    sta hiscore_lo
    sta hiscore_hi
    
    ; Start in demo mode
    lda #1
    sta demo_mode

restart_game:
    ; Set up sprite data area
    jsr init_sprites
    jsr init_screen
    jsr set_sprite_pointers     ; MUST be after init_screen (which clears $07F8)
    jsr init_vars
    jsr spawn_apple
    
    ; Show "PUSH FIRE TO START" on row 1 (during demo)
    lda demo_mode
    beq game_loop               ; Skip if player mode
    jsr draw_push_fire
    
game_loop:
    jsr wait_frame
    
    lda game_over
    bne game_over_handler
    
    ; Check for fire button to switch from demo to player mode
    lda demo_mode
    beq player_mode
    
    ; Demo mode - AI controls
    jsr check_fire_button
    bcs switch_to_player        ; Fire pressed, switch to player
    jsr ai_move
    jmp continue_game
    
player_mode:
    jsr read_joy
    
continue_game:
    jsr move_snake
    jsr check_collision
    jsr update_sprites
    jsr draw_score
    jsr draw_hiscore
    
    jmp game_loop

switch_to_player:
    lda #0
    sta demo_mode
    jsr clear_push_fire         ; Clear the "PUSH FIRE" message
    ; Wait for fire release (debounce)
wait_fire_release2:
    jsr wait_frame
    jsr check_fire_button
    bcs wait_fire_release2
    jmp restart_game            ; Restart game in player mode

game_over_handler:
    ; Check if we beat high score
    jsr check_high_score
    
    lda demo_mode
    beq game_over_loop          ; Player mode - just flash
    ; Demo mode - restart demo after brief delay
    ldx #10                     ; ~0.15 second delay (very quick restart)
game_over_delay:
    jsr wait_frame
    inc BORDER                  ; Flash border quickly
    ; Check if fire pressed during delay - switch to player mode
    jsr check_fire_button
    bcs switch_to_player
    dex
    bne game_over_delay
    jmp restart_game            ; Restart demo

game_over_loop:
    ; Flash border on game over (player mode) - slower flash
    inc BORDER
    jsr wait_frame
    jsr wait_frame
    jsr wait_frame              ; 3 frames between flashes
    ; Check fire to restart
    jsr check_fire_button
    bcc game_over_loop
    ; Wait for fire release (debounce)
wait_fire_release:
    jsr wait_frame
    jsr check_fire_button
    bcs wait_fire_release
    jmp restart_game

; -----------------------------------------------------------------------------
; CHECK FIRE BUTTON - Returns carry set if pressed
; -----------------------------------------------------------------------------
check_fire_button:
    lda JOYSTICK
    and #$10                    ; Fire button (active low)
    bne fire_not_pressed
    sec                         ; Carry set = pressed
    rts
fire_not_pressed:
    ; Also check space bar
    lda $C6                     ; Keyboard buffer count
    beq no_key_pressed
    lda #0
    sta $C6                     ; Clear buffer
    sec
    rts
no_key_pressed:
    clc                         ; Carry clear = not pressed
    rts

; -----------------------------------------------------------------------------
; DRAW "PUSH FIRE TO START" MESSAGE
; -----------------------------------------------------------------------------
draw_push_fire:
    ldx #0
push_fire_loop:
    lda press_text, x
    beq push_fire_done
    sta SCREEN_RAM + 40 + 10, x ; Row 1, centered
    lda #7                      ; Yellow
    sta COLOR_RAM + 40 + 10, x
    inx
    bne push_fire_loop
push_fire_done:
    rts

; -----------------------------------------------------------------------------
; CLEAR "PUSH FIRE TO START" MESSAGE
; -----------------------------------------------------------------------------
clear_push_fire:
    ldx #20
clear_fire_loop:
    lda #32                     ; Space
    sta SCREEN_RAM + 40 + 10, x
    dex
    bpl clear_fire_loop
    rts

; -----------------------------------------------------------------------------
; SPRITE INITIALIZATION
; -----------------------------------------------------------------------------
init_sprites:
    ; Copy sprite data to VIC-visible sprite area at $2000
    ; Each sprite is 64 bytes
    
    ; Copy head sprite (64 bytes)
    ldx #0
copy_head:
    lda sprite_head, x
    sta $2000, x                ; Block 128
    inx
    cpx #64
    bne copy_head
    
    ; Copy apple sprite (64 bytes)
    ldx #0
copy_apple:
    lda sprite_apple, x
    sta $2040, x                ; Block 129
    inx
    cpx #64
    bne copy_apple
    
    ; Copy body sprite (64 bytes)
    ldx #0
copy_body:
    lda sprite_body, x
    sta $2080, x                ; Block 130
    inx
    cpx #64
    bne copy_body
    
    ; Set sprite pointers
    lda #SPRITE_BLOCK           ; Head = block 128 ($2000)
    sta SPR_PTRS
    lda #SPRITE_BLOCK + 1       ; Apple = block 129 ($2040)
    sta SPR_PTRS + 1
    
    ; Body segments (sprites 2-7) use block 130 ($2080)
    lda #SPRITE_BLOCK + 2
    sta SPR_PTRS + 2
    sta SPR_PTRS + 3
    sta SPR_PTRS + 4
    sta SPR_PTRS + 5
    sta SPR_PTRS + 6
    sta SPR_PTRS + 7
    
    ; Enable sprites (head + apple + 6 body)
    lda #%11111111              ; All 8 sprites enabled
    sta SPR_ENABLE
    
    ; Set sprite colors
    lda #5                      ; Green for head
    sta SPR0_COLOR
    lda #2                      ; Red for apple
    sta SPR1_COLOR
    lda #13                     ; Light green for body
    sta SPR2_COLOR
    sta SPR3_COLOR
    sta SPR4_COLOR
    sta SPR5_COLOR
    sta SPR6_COLOR
    sta SPR7_COLOR
    
    ; Don't expand sprites (keep normal 24x21 size)
    lda #0
    sta SPR_EXPAND_X
    sta SPR_EXPAND_Y
    
    ; IMPORTANT: Disable multicolor mode for all sprites (hires mode)
    lda #0
    sta SPR_MULTICLR
    
    ; Clear X MSB register
    lda #0
    sta SPR_X_MSB
    
    rts

; -----------------------------------------------------------------------------
; SET SPRITE POINTERS (must be called AFTER init_screen)
; Screen clear at $0400+768 overwrites $07F8, so set pointers after that
; -----------------------------------------------------------------------------
set_sprite_pointers:
    ; Set sprite pointers
    lda #SPRITE_BLOCK           ; Head = block 128 ($2000)
    sta SPR_PTRS
    lda #SPRITE_BLOCK + 1       ; Apple = block 129 ($2040)
    sta SPR_PTRS + 1
    
    ; Body segments (sprites 2-7) use block 130 ($2080)
    lda #SPRITE_BLOCK + 2
    sta SPR_PTRS + 2
    sta SPR_PTRS + 3
    sta SPR_PTRS + 4
    sta SPR_PTRS + 5
    sta SPR_PTRS + 6
    sta SPR_PTRS + 7
    
    rts

; -----------------------------------------------------------------------------
; SCREEN INITIALIZATION
; -----------------------------------------------------------------------------
init_screen:
    ; Set colors
    lda #0                      ; Black background
    sta BG_COLOR
    lda #5                      ; Green border
    sta BORDER
    
    ; Fill screen with grass first
    ldx #0
fill_grass:
    lda #CHAR_GRASS
    sta SCREEN_RAM, x
    sta SCREEN_RAM + 256, x
    sta SCREEN_RAM + 512, x
    sta SCREEN_RAM + 768, x
    lda #11                     ; Dark grey for grass
    sta COLOR_RAM, x
    sta COLOR_RAM + 256, x
    sta COLOR_RAM + 512, x
    sta COLOR_RAM + 768, x
    inx
    bne fill_grass
    
    ; Add sparse random bushes
    jsr draw_sparse_bushes
    
    ; Draw walls (top and bottom)
    ldx #0
wall_top_bottom:
    lda #CHAR_WALL
    sta SCREEN_RAM, x           ; Top row
    sta SCREEN_RAM + 24*40, x   ; Bottom row (row 24)
    lda #12                     ; Grey color
    sta COLOR_RAM, x
    sta COLOR_RAM + 24*40, x
    inx
    cpx #40
    bne wall_top_bottom
    
    ; Draw side walls
    lda #<SCREEN_RAM
    sta ptr_lo
    lda #>SCREEN_RAM
    sta ptr_hi
    
    ldx #25                     ; 25 rows
side_wall_loop:
    ldy #0
    lda #CHAR_WALL
    sta (ptr_lo), y             ; Left wall
    ldy #39
    sta (ptr_lo), y             ; Right wall
    
    ; Set wall colors
    lda #<COLOR_RAM
    sta tmp_lo
    lda #>COLOR_RAM
    sta tmp_hi
    lda ptr_lo
    sec
    sbc #<SCREEN_RAM
    clc
    adc tmp_lo
    sta tmp_lo
    lda ptr_hi
    sbc #>SCREEN_RAM
    adc tmp_hi
    sta tmp_hi
    ldy #0
    lda #12
    sta (tmp_lo), y
    ldy #39
    sta (tmp_lo), y
    
    ; Add 40 to pointer
    lda ptr_lo
    clc
    adc #40
    sta ptr_lo
    lda ptr_hi
    adc #0
    sta ptr_hi
    
    dex
    bne side_wall_loop
    
    ; Draw "SCORE:" text
    ldx #0
draw_score_label:
    lda score_text, x
    beq score_label_done
    sta SCREEN_RAM + 40, x      ; Row 1
    lda #1                      ; White
    sta COLOR_RAM + 40, x
    inx
    jmp draw_score_label
score_label_done:
    
    rts

; -----------------------------------------------------------------------------
; DRAW SPARSE BUSHES - fixed grid placement for guaranteed even distribution
; Places 16 bushes in a 4x4 grid pattern across the play area
; -----------------------------------------------------------------------------
draw_sparse_bushes:
    ; Row 4 (top area)
    lda #4
    ldx #5
    jsr place_bush_at
    lda #4
    ldx #15
    jsr place_bush_at
    lda #4
    ldx #25
    jsr place_bush_at
    lda #4
    ldx #35
    jsr place_bush_at
    
    ; Row 9
    lda #9
    ldx #8
    jsr place_bush_at
    lda #9
    ldx #18
    jsr place_bush_at
    lda #9
    ldx #28
    jsr place_bush_at
    lda #9
    ldx #37
    jsr place_bush_at
    
    ; Row 15
    lda #15
    ldx #3
    jsr place_bush_at
    lda #15
    ldx #12
    jsr place_bush_at
    lda #15
    ldx #22
    jsr place_bush_at
    lda #15
    ldx #32
    jsr place_bush_at
    
    ; Row 21 (bottom area)
    lda #21
    ldx #6
    jsr place_bush_at
    lda #21
    ldx #16
    jsr place_bush_at
    lda #21
    ldx #26
    jsr place_bush_at
    lda #21
    ldx #36
    jsr place_bush_at
    
    rts

; Place a bush at row A, column X
place_bush_at:
    sta tmp_hi                  ; Row
    stx tmp_lo                  ; Column
    
    ; Calculate screen offset: row * 40 + column
    lda tmp_hi                  ; Row
    asl                         ; *2
    asl                         ; *4
    asl                         ; *8
    sta ptr_lo
    lda tmp_hi
    asl                         ; *2
    asl                         ; *4
    asl                         ; *8
    asl                         ; *16
    asl                         ; *32
    clc
    adc ptr_lo                  ; *40
    sta ptr_lo
    lda #0
    adc #0
    sta ptr_hi
    
    ; Add column
    lda ptr_lo
    clc
    adc tmp_lo
    sta ptr_lo
    lda ptr_hi
    adc #0
    sta ptr_hi
    
    ; Add SCREEN_RAM base
    lda ptr_lo
    clc
    adc #<SCREEN_RAM
    sta ptr_lo
    lda ptr_hi
    adc #>SCREEN_RAM
    sta ptr_hi
    
    ; Draw bush character
    ldy #0
    lda #102                    ; Bush/checkerboard char
    sta (ptr_lo), y
    
    ; Set bush color (green)
    lda ptr_lo
    clc
    adc #<(COLOR_RAM - SCREEN_RAM)
    sta ptr_lo
    lda ptr_hi
    adc #>(COLOR_RAM - SCREEN_RAM)
    sta ptr_hi
    lda #5                      ; Green
    sta (ptr_lo), y
    
    rts

; -----------------------------------------------------------------------------
; VARIABLE INITIALIZATION
; -----------------------------------------------------------------------------
init_vars:
    ; Initialize snake position (safe center position)
    lda #160                    ; X position (low byte) - center-right for safe eastward movement
    sta head_x
    lda #0                      ; X position (high byte)
    sta head_x + 1
    lda #140                    ; Y position - center
    sta head_y
    
    lda #3                      ; Start moving East
    sta dir
    
    lda #0
    sta score_lo
    sta score_hi
    sta frame_cnt
    sta tail_idx
    sta game_over
    sta grow
    
    lda #5                      ; Initial length - very short snake (5 segments)
    sta head_idx
    
    lda #8                      ; Speed (frames between moves)
    sta speed
    
    ; Initialize RNG
    lda $D012
    sta tick
    
    ; Initialize trail - snake as a horizontal line going West from head
    ; Only initialize the segments we actually use (0 to head_idx-1)
    ; All segments at same Y, X decreasing by 8 pixels each
    ldx #0
    lda head_x
    sta tmp_lo
init_trail:
    cpx head_idx
    beq init_trail_done
    
    ; Store position for this segment
    lda tmp_lo
    sta trail_x_lo, x
    lda #0
    sta trail_x_hi, x
    lda head_y
    sta trail_y, x
    
    ; Move 4 pixels West for next (older) segment
    lda tmp_lo
    sec
    sbc #4
    sta tmp_lo
    
    inx
    jmp init_trail
    
init_trail_done:
    ; Fill rest of buffer with safe position (off-screen but valid)
init_trail_fill:
    cpx #64
    beq init_trail_complete
    lda #80                     ; Safe X
    sta trail_x_lo, x
    lda #0
    sta trail_x_hi, x
    lda #140                    ; Safe Y
    sta trail_y, x
    inx
    jmp init_trail_fill
    
init_trail_complete:
    rts

; -----------------------------------------------------------------------------
; FRAME WAIT (VBlank sync)
; -----------------------------------------------------------------------------
wait_frame:
    inc tick                    ; RNG tick
    
wait_raster:
    lda $D012
    cmp #250
    bne wait_raster
wait_raster2:
    lda $D012
    cmp #250
    beq wait_raster2
    
    ; Speed control
    inc frame_cnt
    lda frame_cnt
    cmp speed
    bcc wait_frame              ; Wait more frames
    lda #0
    sta frame_cnt
    rts

; -----------------------------------------------------------------------------
; AI MOVEMENT (Demo Mode) - Smarter obstacle-avoiding AI
; Boundaries: X = 40-223 (safe zone), Y = 66-213 (safe zone)
; Direction: 0=N, 1=S, 2=W, 3=E
; -----------------------------------------------------------------------------
ai_move:
    ; Check if there's an obstacle ahead (bush or wall)
    jsr ai_check_ahead
    lda tmp_lo                  ; 1 = obstacle ahead
    beq ai_no_obstacle
    jmp ai_avoid_obstacle

ai_no_obstacle:
    ; First check if we're near a wall and need to turn

    ; Check if too close to left wall (X < 40)
    lda head_x
    cmp #40
    bcs ai_not_left_wall
    ; Near left wall - must not go West
    lda dir
    cmp #2                      ; Going West?
    bne ai_not_left_wall
    ; Turn based on Y position
    lda head_y
    cmp #140
    bcc @turn_s1
    jmp ai_turn_north
@turn_s1:
    jmp ai_turn_south
ai_not_left_wall:

    ; Check if too close to right wall (X > 220)
    lda head_x
    cmp #220
    bcc ai_not_right_wall
    ; Near right wall - must not go East
    lda dir
    cmp #3                      ; Going East?
    bne ai_not_right_wall
    lda head_y
    cmp #140
    bcc @turn_s2
    jmp ai_turn_north
@turn_s2:
    jmp ai_turn_south
ai_not_right_wall:

    ; Check if too close to top wall (Y < 60)
    lda head_y
    cmp #60
    bcs ai_not_top_wall
    ; Near top wall - must not go North
    lda dir
    cmp #0                      ; Going North?
    bne ai_not_top_wall
    lda head_x
    cmp #140
    bcc @turn_e1
    jmp ai_turn_west
@turn_e1:
    jmp ai_turn_east
ai_not_top_wall:

    ; Check if too close to bottom wall (Y > 220)
    lda head_y
    cmp #220
    bcc ai_not_bottom_wall
    ; Near bottom wall - must not go South
    lda dir
    cmp #1                      ; Going South?
    bne ai_not_bottom_wall
    lda head_x
    cmp #140
    bcc @turn_e2
    jmp ai_turn_west
@turn_e2:
    jmp ai_turn_east
ai_not_bottom_wall:

    ; Safe from walls - now seek the apple
    ; Compare X positions
    lda head_x
    cmp apple_x
    beq ai_check_y
    bcs @go_left
    jmp ai_want_right
@go_left:
    ; Want to go left (West) - but check if safe
    lda head_x
    cmp #48                     ; Not too close to left wall
    bcc ai_check_y              ; Too close, try Y instead
    lda dir
    cmp #3                      ; Can't reverse if going East
    beq ai_check_y
    lda #2
    sta dir
    rts

ai_want_right:
    ; Check if safe to go right
    lda head_x
    cmp #216                    ; Not too close to right wall
    bcs ai_check_y              ; Too close, try Y instead
    lda dir
    cmp #2                      ; Can't reverse if going West
    beq ai_check_y
    lda #3
    sta dir
    rts

ai_check_y:
    lda head_y
    cmp apple_y
    beq ai_done
    bcc ai_want_down
    
    ; Want to go up (North) - check if safe
    lda head_y
    cmp #80                     ; Not too close to top
    bcc ai_done
    lda dir
    cmp #1                      ; Can't reverse if going South
    beq ai_done
    lda #0
    sta dir
    rts

ai_want_down:
    ; Check if safe to go down
    lda head_y
    cmp #192                    ; Not too close to bottom
    bcs ai_done
    lda dir
    cmp #0                      ; Can't reverse if going North
    beq ai_done
    lda #1
    sta dir
    rts
    
ai_done:
    rts

; AI helper - turn directions
ai_turn_north:
    lda dir
    cmp #1                      ; Can't reverse if going South
    beq ai_turn_east            ; Try East instead
    lda #0
    sta dir
    rts

ai_turn_south:
    lda dir
    cmp #0                      ; Can't reverse if going North
    beq ai_turn_west            ; Try West instead
    lda #1
    sta dir
    rts

ai_turn_east:
    lda dir
    cmp #2                      ; Can't reverse if going West
    beq ai_turn_south_force
    lda #3
    sta dir
    rts

ai_turn_west:
    lda dir
    cmp #3                      ; Can't reverse if going East
    beq ai_turn_north_force
    lda #2
    sta dir
    rts

ai_turn_north_force:
    lda #0
    sta dir
    rts

ai_turn_south_force:
    lda #1
    sta dir
    rts

; -----------------------------------------------------------------------------
; AI CHECK AHEAD - Check if there's a bush or wall 16 pixels ahead
; Returns: tmp_lo = 1 if obstacle, 0 if clear
; -----------------------------------------------------------------------------
ai_check_ahead:
    lda #0
    sta tmp_lo                  ; Default: no obstacle

    ; Calculate position 16 pixels ahead based on direction
    lda dir
    cmp #0                      ; North
    beq ai_check_north
    cmp #1                      ; South
    beq ai_check_south
    cmp #2                      ; West
    beq ai_check_west
    ; East
ai_check_east:
    lda head_x
    clc
    adc #16
    jmp ai_do_check

ai_check_west:
    lda head_x
    sec
    sbc #16
    jmp ai_do_check

ai_check_north:
    lda head_x
    sta tmp_hi                  ; Save X for later
    lda head_y
    sec
    sbc #16
    pha
    lda tmp_hi
    jmp ai_check_y_pos

ai_check_south:
    lda head_x
    sta tmp_hi
    lda head_y
    clc
    adc #16
    pha
    lda tmp_hi
    jmp ai_check_y_pos

ai_do_check:
    ; A has X position to check
    sta tmp_hi                  ; Save X
    lda head_y
    pha                         ; Save Y on stack
    lda tmp_hi                  ; Restore X

ai_check_y_pos:
    ; Stack has Y, A has X
    ; Convert to screen coordinates
    sec
    sbc #24                     ; Remove sprite offset
    lsr
    lsr
    lsr
    sta tmp_hi                  ; Screen X

    pla                         ; Get Y from stack
    sec
    sbc #50
    lsr
    lsr
    lsr                         ; Screen Y

    ; Calculate screen position
    asl                         ; *2
    asl                         ; *4
    asl                         ; *8
    sta ptr_lo
    asl
    asl                         ; *32
    clc
    adc ptr_lo                  ; *40
    clc
    adc tmp_hi                  ; + X
    sta ptr_lo
    lda #0
    adc #0
    sta ptr_hi

    ; Add screen base
    lda ptr_lo
    clc
    adc #<SCREEN_RAM
    sta ptr_lo
    lda ptr_hi
    adc #>SCREEN_RAM
    sta ptr_hi

    ; Check character
    ldy #0
    lda (ptr_lo), y
    cmp #102                    ; Bush
    beq ai_found_obstacle
    cmp #160                    ; Wall
    beq ai_found_obstacle
    rts

ai_found_obstacle:
    lda #1
    sta tmp_lo
    rts

; -----------------------------------------------------------------------------
; AI AVOID OBSTACLE - Turn to avoid bush/wall ahead
; -----------------------------------------------------------------------------
ai_avoid_obstacle:
    ; Try turning perpendicular to current direction
    lda dir
    cmp #0                      ; Going North?
    beq ai_avoid_n
    cmp #1                      ; Going South?
    beq ai_avoid_s
    cmp #2                      ; Going West?
    beq ai_avoid_w
    ; Going East
ai_avoid_e:
    ; Try North first, then South
    lda head_y
    cmp #100
    bcs ai_avoid_e_north
    jmp ai_turn_south           ; Near top, go South
ai_avoid_e_north:
    jmp ai_turn_north           ; Else go North

ai_avoid_w:
    lda head_y
    cmp #100
    bcs ai_avoid_w_north
    jmp ai_turn_south
ai_avoid_w_north:
    jmp ai_turn_north

ai_avoid_n:
    lda head_x
    cmp #120
    bcs ai_avoid_n_west
    jmp ai_turn_east            ; Near left, go East
ai_avoid_n_west:
    jmp ai_turn_west            ; Else go West

ai_avoid_s:
    lda head_x
    cmp #120
    bcs ai_avoid_s_west
    jmp ai_turn_east
ai_avoid_s_west:
    jmp ai_turn_west

; -----------------------------------------------------------------------------
; READ JOYSTICK (Player Mode)
; Joystick port 2: $DC00
; Bits: 0=Up, 1=Down, 2=Left, 3=Right (active low)
; Direction: 0=N, 1=S, 2=W, 3=E
; -----------------------------------------------------------------------------
read_joy:
    lda JOYSTICK
    
    ; Check Up (bit 0)
    lsr                         ; Bit 0 -> carry
    bcs joy_not_up
    ; Want to go North
    lda dir
    cmp #1                      ; Can't reverse if going South
    beq joy_done
    cmp #0                      ; Already going North?
    beq joy_done
    lda #0
    sta dir
    jsr sound_turn              ; Play turn sound
    rts
joy_not_up:

    ; Check Down (bit 1) 
    lsr                         ; Bit 1 -> carry
    bcs joy_not_down
    ; Want to go South
    lda dir
    cmp #0                      ; Can't reverse if going North
    beq joy_done
    cmp #1                      ; Already going South?
    beq joy_done
    lda #1
    sta dir
    jsr sound_turn              ; Play turn sound
    rts
joy_not_down:

    ; Check Left (bit 2)
    lsr                         ; Bit 2 -> carry
    bcs joy_not_left
    ; Want to go West
    lda dir
    cmp #3                      ; Can't reverse if going East
    beq joy_done
    cmp #2                      ; Already going West?
    beq joy_done
    lda #2
    sta dir
    jsr sound_turn              ; Play turn sound
    rts
joy_not_left:

    ; Check Right (bit 3)
    lsr                         ; Bit 3 -> carry
    bcs joy_done
    ; Want to go East
    lda dir
    cmp #2                      ; Can't reverse if going West
    beq joy_done
    cmp #3                      ; Already going East?
    beq joy_done
    lda #3
    sta dir
    jsr sound_turn              ; Play turn sound
    
joy_done:
    rts

; -----------------------------------------------------------------------------
; MOVE SNAKE
; -----------------------------------------------------------------------------
move_snake:
    ; Save current position to trail
    ldx head_idx
    lda head_x
    sta trail_x_lo, x
    lda head_x + 1
    sta trail_x_hi, x
    lda head_y
    sta trail_y, x
    
    ; Increment head index (circular buffer)
    inx
    txa
    and #$3F                    ; Wrap at 64
    sta head_idx
    
    ; Move based on direction
    lda dir
    cmp #0
    beq move_north
    cmp #1
    beq move_south
    cmp #2
    beq move_west
    ; else East
    
move_east:
    lda head_x
    clc
    adc #MOVE_STEP
    sta head_x
    lda head_x + 1
    adc #0
    sta head_x + 1
    jmp move_done

move_west:
    lda head_x
    sec
    sbc #MOVE_STEP
    sta head_x
    lda head_x + 1
    sbc #0
    sta head_x + 1
    jmp move_done

move_north:
    lda head_y
    sec
    sbc #MOVE_STEP
    sta head_y
    jmp move_done

move_south:
    lda head_y
    clc
    adc #MOVE_STEP
    sta head_y

move_done:
    ; Update tail (unless growing)
    lda grow
    bne skip_tail_move
    inc tail_idx
    lda tail_idx
    and #$3F
    sta tail_idx
skip_tail_move:
    lda #0
    sta grow
    rts

; -----------------------------------------------------------------------------
; CHECK COLLISION
; -----------------------------------------------------------------------------
check_collision:
    ; Check wall collision (use pixel boundaries)
    lda head_x
    cmp #MIN_X + 16             ; Left wall
    bcc hit_wall
    cmp #MAX_X - 32             ; Right wall (approximate)
    bcs check_right_msb
    jmp check_top

check_right_msb:
    lda head_x + 1
    bne hit_wall                ; X > 255, definitely hit wall

check_top:
    lda head_y
    cmp #MIN_Y + 16             ; Top wall
    bcc hit_wall
    cmp #MAX_Y - 16             ; Bottom wall
    bcs hit_wall
    jmp check_bush_collision

hit_wall:
    jsr sound_game_over         ; Play death sound
    lda #1
    sta game_over
    rts

; Check if snake head hit a bush (character 102)
; DISABLED - bushes are decorative only
check_bush_collision:
    jmp check_apple             ; Skip bush collision check
    ; Screen X = (head_x - 24 + 4) / 8  (offset to center of sprite)
    ; Screen Y = (head_y - 50 + 4) / 8  (offset to center of sprite)

    lda head_x
    sec
    sbc #24                     ; Remove sprite offset
    clc
    adc #4                      ; Add half sprite width for center check
    ; Divide by 8 (shift right 3 times)
    lsr
    lsr
    lsr
    sta tmp_lo                  ; Screen X (0-39)

    lda head_y
    sec
    sbc #50                     ; Remove sprite offset
    clc
    adc #4                      ; Add half sprite height for center check
    lsr
    lsr
    lsr
    sta tmp_hi                  ; Screen Y (0-24)

    ; Calculate screen offset: Y * 40 + X
    lda tmp_hi
    asl                         ; *2
    asl                         ; *4
    asl                         ; *8
    sta ptr_lo
    lda tmp_hi
    asl                         ; *2
    asl                         ; *4
    asl                         ; *8
    asl                         ; *16
    asl                         ; *32
    clc
    adc ptr_lo                  ; *40
    sta ptr_lo
    lda #0
    adc #0
    sta ptr_hi

    ; Add X coordinate
    lda ptr_lo
    clc
    adc tmp_lo
    sta ptr_lo
    lda ptr_hi
    adc #0
    sta ptr_hi

    ; Add screen base
    lda ptr_lo
    clc
    adc #<SCREEN_RAM
    sta ptr_lo
    lda ptr_hi
    adc #>SCREEN_RAM
    sta ptr_hi

    ; Check character at this position
    ldy #0
    lda (ptr_lo), y
    cmp #102                    ; Bush character
    beq hit_bush
    jmp check_apple

hit_bush:
    jsr sound_game_over         ; Play death sound
    lda #1
    sta game_over
    rts

check_apple:
    ; Self-collision disabled - snake only collides with walls
    
    ; Check if head overlaps apple (within 16 pixels)
    lda head_x
    sec
    sbc apple_x
    bcs apple_diff_x_pos
    eor #$FF
    clc
    adc #1
apple_diff_x_pos:
    cmp #16                     ; Within 16 pixels?
    bcs no_apple
    
    lda head_y
    sec
    sbc apple_y
    bcs apple_diff_y_pos
    eor #$FF
    clc
    adc #1
apple_diff_y_pos:
    cmp #16
    bcs no_apple
    
    ; Ate the apple!
    jsr sound_eat               ; Play eat sound!
    inc score_lo
    bne :+
    inc score_hi                ; Increment high byte on overflow
:   lda #1
    sta grow
    jsr spawn_apple

    ; Increase speed every 10 apples (more gradual progression)
    lda score_lo
    cmp #10
    bcc no_apple
    cmp #20
    bcc check_speed_10
    cmp #30
    bcc check_speed_20
    cmp #40
    bcc check_speed_30
    jmp no_apple

check_speed_10:
    lda speed
    cmp #6
    bcc no_apple
    lda #6
    sta speed
    jmp no_apple

check_speed_20:
    lda speed
    cmp #4
    bcc no_apple
    lda #4
    sta speed
    jmp no_apple

check_speed_30:
    lda speed
    cmp #3
    bcc no_apple
    lda #3
    sta speed
    
no_apple:
collision_done:
    rts

; -----------------------------------------------------------------------------
; CHECK SELF COLLISION - DISABLED
; Snake can only collide with walls now
; -----------------------------------------------------------------------------
check_self_collision:
    rts

; -----------------------------------------------------------------------------
; SPAWN APPLE
; -----------------------------------------------------------------------------
spawn_apple:
    ; Random X position (40-200)
    lda tick
    eor $D012
    and #$7F                    ; 0-127
    clc
    adc #50                     ; 50-177
    sta apple_x
    lda #0
    sta apple_x + 1
    
    ; Random Y position (60-200)
    lda tick
    eor $D011
    and #$7F
    clc
    adc #60
    sta apple_y
    
    rts

; -----------------------------------------------------------------------------
; UPDATE SPRITES
; -----------------------------------------------------------------------------
update_sprites:
    ; Update head sprite position (add VIC-II offset)
    lda head_x
    clc
    adc #SPRITE_X_OFFSET
    sta SPR0_X
    lda head_y
    clc
    adc #SPRITE_Y_OFFSET
    sta SPR0_Y
    
    ; Update apple sprite position (add VIC-II offset)
    lda apple_x
    clc
    adc #SPRITE_X_OFFSET
    sta SPR1_X
    lda apple_y
    clc
    adc #SPRITE_Y_OFFSET
    sta SPR1_Y
    
    ; Update X MSB for head and apple
    ; Check if head_x + offset > 255
    lda #0
    ldx head_x + 1
    bne set_head_msb            ; Already > 255
    lda head_x
    clc
    adc #SPRITE_X_OFFSET
    bcc no_head_msb_check       ; No carry = no overflow
set_head_msb:
    lda #$01                    ; Set bit 0 for sprite 0
    jmp check_apple_msb
no_head_msb_check:
    lda #0
check_apple_msb:
    pha
    lda apple_x + 1
    bne set_apple_msb           ; Already > 255
    lda apple_x
    clc
    adc #SPRITE_X_OFFSET
    bcc no_apple_msb_check
set_apple_msb:
    pla
    ora #$02                    ; Set bit 1 for sprite 1
    jmp store_msb
no_apple_msb_check:
    pla
store_msb:
    sta SPR_X_MSB
    
    ; Update body segments (sprites 2-7)
    ; Body follows head through the trail buffer
    ; head_idx points to next empty slot, so head_idx-1 is where head just was
    lda head_idx
    sec
    sbc #1                      ; Start at previous head position
    and #$3F
    tax
    
    ldy #0                      ; Sprite register offset (0, 2, 4, 6, 8, 10)
    sty temp_count              ; Use temp for sprite count
body_loop:
    ; Get trail position and add offset
    lda trail_x_lo, x
    clc
    adc #SPRITE_X_OFFSET
    sta SPR0_X + 4, y           ; Sprite 2+ X (offset by 4 bytes)
    lda trail_y, x
    clc
    adc #SPRITE_Y_OFFSET
    sta SPR0_Y + 4, y           ; Sprite 2+ Y
    
    ; Move back 2 positions in trail for next body segment (8 pixels = tight spacing)
    txa
    sec
    sbc #2                      ; Space segments by 2 moves (8 pixels)
    and #$3F
    tax
    
    ; Next sprite
    iny
    iny                         ; Add 2 (X and Y per sprite)
    inc temp_count
    lda temp_count
    cmp #6                      ; 6 body sprites
    bne body_loop
    
    rts

; -----------------------------------------------------------------------------
; DRAW SCORE (decimal display, up to 999)
; -----------------------------------------------------------------------------
draw_score:
    ; Convert 16-bit score to 3 decimal digits
    ; We'll divide by 100 for hundreds, then by 10 for tens
    
    lda score_lo
    sta tmp_lo
    lda score_hi
    sta tmp_hi
    
    ; Divide by 100 for hundreds digit
    ldx #0
hundreds_loop:
    lda tmp_lo
    sec
    sbc #100
    tay
    lda tmp_hi
    sbc #0
    bcc hundreds_done
    sta tmp_hi
    sty tmp_lo
    inx
    jmp hundreds_loop
hundreds_done:
    txa
    clc
    adc #48                     ; Convert to ASCII '0'
    sta SCREEN_RAM + 47         ; Hundreds position
    
    ; Divide remainder by 10 for tens digit
    lda tmp_lo                  ; Remainder is < 100
    ldx #0
    sec
tens_loop:
    sbc #10
    bcc tens_done
    inx
    bne tens_loop
tens_done:
    adc #10                     ; Restore ones digit
    pha                         ; Save ones
    txa                         ; Tens digit
    clc
    adc #48                     ; Convert to ASCII '0'
    sta SCREEN_RAM + 48         ; Tens position
    pla                         ; Ones digit
    clc
    adc #48                     ; Convert to ASCII '0'
    sta SCREEN_RAM + 49         ; Ones position
    rts

; -----------------------------------------------------------------------------
; CHECK HIGH SCORE - Update if current score is higher
; -----------------------------------------------------------------------------
check_high_score:
    ; Compare score_hi with hiscore_hi first
    lda score_hi
    cmp hiscore_hi
    bcc not_new_high            ; score_hi < hiscore_hi, not new high
    bne new_high_score          ; score_hi > hiscore_hi, new high
    
    ; High bytes equal, compare low bytes
    lda score_lo
    cmp hiscore_lo
    bcc not_new_high            ; score_lo < hiscore_lo
    beq not_new_high            ; equal, not new (need > not >=)
    
new_high_score:
    ; Copy current score to high score
    lda score_lo
    sta hiscore_lo
    lda score_hi
    sta hiscore_hi
    
not_new_high:
    rts

; -----------------------------------------------------------------------------
; DRAW HIGH SCORE (next to regular score)
; -----------------------------------------------------------------------------
draw_hiscore:
    ; Draw "HI:" label at position 30
    lda #8                      ; 'H'
    sta SCREEN_RAM + 70
    lda #9                      ; 'I'
    sta SCREEN_RAM + 71
    lda #58                     ; ':'
    sta SCREEN_RAM + 72
    
    ; Set color to yellow
    lda #7
    sta COLOR_RAM + 70
    sta COLOR_RAM + 71
    sta COLOR_RAM + 72
    sta COLOR_RAM + 73
    sta COLOR_RAM + 74
    sta COLOR_RAM + 75
    
    ; Convert high score to decimal
    lda hiscore_lo
    sta tmp_lo
    lda hiscore_hi
    sta tmp_hi
    
    ; Hundreds
    ldx #0
hi_hundreds_loop:
    lda tmp_lo
    sec
    sbc #100
    tay
    lda tmp_hi
    sbc #0
    bcc hi_hundreds_done
    sta tmp_hi
    sty tmp_lo
    inx
    jmp hi_hundreds_loop
hi_hundreds_done:
    txa
    clc
    adc #48
    sta SCREEN_RAM + 73
    
    ; Tens
    lda tmp_lo
    ldx #0
    sec
hi_tens_loop:
    sbc #10
    bcc hi_tens_done
    inx
    bne hi_tens_loop
hi_tens_done:
    adc #10
    pha
    txa
    clc
    adc #48
    sta SCREEN_RAM + 74
    pla
    clc
    adc #48
    sta SCREEN_RAM + 75
    rts

; -----------------------------------------------------------------------------
; SOUND INITIALIZATION
; -----------------------------------------------------------------------------
init_sound:
    ; Clear all SID registers
    ldx #24
    lda #0
clear_sid:
    sta SID_BASE, x
    dex
    bpl clear_sid
    
    ; Set master volume to max
    lda #$0F
    sta SID_VOLUME
    
    ; Set up voice 1 for sound effects
    ; Attack=0, Decay=8 (quick)
    lda #$08
    sta SID_V1_AD
    ; Sustain=0, Release=4
    lda #$04
    sta SID_V1_SR
    
    rts

; -----------------------------------------------------------------------------
; SOUND: EAT APPLE (quick rising blip)
; -----------------------------------------------------------------------------
sound_eat:
    ; High-pitched blip
    lda #$30                    ; Frequency low
    sta SID_V1_FREQ_LO
    lda #$20                    ; Frequency high (~2kHz)
    sta SID_V1_FREQ_HI
    
    ; Triangle wave + gate on
    lda #$11
    sta SID_V1_CTRL
    
    ; Let it play briefly then gate off
    ldx #10
eat_delay:
    dex
    bne eat_delay
    
    ; Gate off
    lda #$10
    sta SID_V1_CTRL
    rts

; -----------------------------------------------------------------------------
; SOUND: GAME OVER (descending tone)
; -----------------------------------------------------------------------------
sound_game_over:
    ; Start with high frequency
    lda #$00
    sta SID_V1_FREQ_LO
    lda #$30                    ; High note
    sta SID_V1_FREQ_HI
    
    ; Sawtooth wave + gate on
    lda #$21
    sta SID_V1_CTRL
    
    ; Descend through frequencies
    ldx #48                     ; Number of steps
descend_loop:
    ; Wait a bit
    ldy #60
descend_wait:
    dey
    bne descend_wait
    
    ; Lower frequency
    lda SID_V1_FREQ_HI
    sec
    sbc #1
    sta SID_V1_FREQ_HI
    
    dex
    bne descend_loop
    
    ; Gate off
    lda #$20
    sta SID_V1_CTRL
    rts

; -----------------------------------------------------------------------------
; SOUND: DIRECTION CHANGE (subtle click)
; -----------------------------------------------------------------------------
sound_turn:
    ; Very short noise burst
    lda #$50
    sta SID_V1_FREQ_LO
    lda #$08
    sta SID_V1_FREQ_HI
    
    ; Noise wave + gate on
    lda #$81
    sta SID_V1_CTRL
    
    ; Very brief
    ldx #5
turn_delay:
    dex
    bne turn_delay
    
    ; Gate off
    lda #$80
    sta SID_V1_CTRL
    rts

; -----------------------------------------------------------------------------
; DATA
; -----------------------------------------------------------------------------
.segment "RODATA"

score_text:
    .byte 19, 3, 15, 18, 5, 58, 0   ; "SCORE:" in screen codes

hex_chars:
    .byte 48, 49, 50, 51, 52, 53, 54, 55     ; 0-7
    .byte 56, 57, 1, 2, 3, 4, 5, 6           ; 8-9, A-F

press_text:
    .byte 16, 21, 19, 8, 32, 6, 9, 18, 5, 32, 20, 15, 32, 19, 20, 1, 18, 20, 0
    ; "PUSH FIRE TO START"

; -----------------------------------------------------------------------------
; SPRITE DATA (24x21 pixels = 63 bytes each, padded to 64)
; Placed directly at $2000 by linker
; -----------------------------------------------------------------------------
.segment "SPRITES"

; Snake Head Sprite - small 8x8 centered (Block 128 = $2000)
sprite_head:
    .byte $00, $00, $00   ; Row 0
    .byte $00, $00, $00   ; Row 1
    .byte $00, $00, $00   ; Row 2
    .byte $00, $00, $00   ; Row 3
    .byte $00, $00, $00   ; Row 4
    .byte $00, $00, $00   ; Row 5
    .byte $00, $3C, $00   ; Row 6  - top
    .byte $00, $7E, $00   ; Row 7
    .byte $00, $DB, $00   ; Row 8  - eyes
    .byte $00, $FF, $00   ; Row 9
    .byte $00, $FF, $00   ; Row 10
    .byte $00, $7E, $00   ; Row 11
    .byte $00, $3C, $00   ; Row 12
    .byte $00, $18, $00   ; Row 13 - tongue
    .byte $00, $00, $00   ; Row 14
    .byte $00, $00, $00   ; Row 15
    .byte $00, $00, $00   ; Row 16
    .byte $00, $00, $00   ; Row 17
    .byte $00, $00, $00   ; Row 18
    .byte $00, $00, $00   ; Row 19
    .byte $00, $00, $00   ; Row 20
    .byte $00            ; Padding to 64 bytes

; Apple Sprite - small 8x8 centered (Block 129 = $2040)
sprite_apple:
    .byte $00, $00, $00   ; Row 0
    .byte $00, $00, $00   ; Row 1
    .byte $00, $00, $00   ; Row 2
    .byte $00, $00, $00   ; Row 3
    .byte $00, $00, $00   ; Row 4
    .byte $00, $08, $00   ; Row 5  - stem
    .byte $00, $1C, $00   ; Row 6  - leaf
    .byte $00, $3C, $00   ; Row 7  - top
    .byte $00, $7E, $00   ; Row 8
    .byte $00, $7E, $00   ; Row 9
    .byte $00, $7E, $00   ; Row 10
    .byte $00, $3C, $00   ; Row 11
    .byte $00, $18, $00   ; Row 12
    .byte $00, $00, $00   ; Row 13
    .byte $00, $00, $00   ; Row 14
    .byte $00, $00, $00   ; Row 15
    .byte $00, $00, $00   ; Row 16
    .byte $00, $00, $00   ; Row 17
    .byte $00, $00, $00   ; Row 18
    .byte $00, $00, $00   ; Row 19
    .byte $00, $00, $00   ; Row 20
    .byte $00            ; Padding to 64 bytes

; Body Segment Sprite - small 6x6 centered (Block 130 = $2080)
sprite_body:
    .byte $00, $00, $00   ; Row 0
    .byte $00, $00, $00   ; Row 1
    .byte $00, $00, $00   ; Row 2
    .byte $00, $00, $00   ; Row 3
    .byte $00, $00, $00   ; Row 4
    .byte $00, $00, $00   ; Row 5
    .byte $00, $00, $00   ; Row 6
    .byte $00, $18, $00   ; Row 7  - top
    .byte $00, $3C, $00   ; Row 8
    .byte $00, $3C, $00   ; Row 9  - center
    .byte $00, $3C, $00   ; Row 10
    .byte $00, $18, $00   ; Row 11
    .byte $00, $00, $00   ; Row 12
    .byte $00, $00, $00   ; Row 13
    .byte $00, $00, $00   ; Row 14
    .byte $00, $00, $00   ; Row 15
    .byte $00, $00, $00   ; Row 16
    .byte $00, $00, $00   ; Row 17
    .byte $00, $00, $00   ; Row 18
    .byte $00, $00, $00   ; Row 19
    .byte $00, $00, $00   ; Row 20
    .byte $00            ; Padding to 64 bytes

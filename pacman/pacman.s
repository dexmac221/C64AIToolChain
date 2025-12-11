; -----------------------------------------------------------------------------
; PAC-MAN for Commodore 64
; A classic maze game with sprites and SID sound
; Built with the C64AIToolChain
; -----------------------------------------------------------------------------

; =============================================================================
; HARDWARE REGISTERS
; =============================================================================

; --- VIC-II Registers ---
VIC_BASE    = $D000
SPR0_X      = $D000
SPR0_Y      = $D001
SPR1_X      = $D002         ; Ghost 1 (Blinky - Red)
SPR1_Y      = $D003
SPR2_X      = $D004         ; Ghost 2 (Pinky - Pink)
SPR2_Y      = $D005
SPR3_X      = $D006         ; Ghost 3 (Inky - Cyan)
SPR3_Y      = $D007
SPR4_X      = $D008         ; Ghost 4 (Clyde - Orange)
SPR4_Y      = $D009
SPR_ENABLE  = $D015
SPR_X_MSB   = $D010
SPR_EXPAND_X = $D01D
SPR_EXPAND_Y = $D017
SPR_PRIORITY = $D01B
SPR_MULTICLR = $D01C
SPR_SPR_COL = $D01E
SPR_BG_COL  = $D01F
SPR0_COLOR  = $D027
SPR1_COLOR  = $D028
SPR2_COLOR  = $D029
SPR3_COLOR  = $D02A
SPR4_COLOR  = $D02B
SPR_PTRS    = $07F8

BORDER      = $D020
BG_COLOR    = $D021

; --- Screen Memory ---
SCREEN_RAM  = $0400
COLOR_RAM   = $D800

; --- Input ---
JOYSTICK    = $DC00

; --- SID Sound ---
SID_BASE    = $D400
SID_V1_FREQ_LO = $D400
SID_V1_FREQ_HI = $D401
SID_V1_PW_LO = $D402
SID_V1_PW_HI = $D403
SID_V1_CTRL = $D404
SID_V1_AD   = $D405
SID_V1_SR   = $D406
SID_V2_FREQ_LO = $D407
SID_V2_FREQ_HI = $D408
SID_V2_CTRL = $D40B
SID_V2_AD   = $D40C
SID_V2_SR   = $D40D
SID_VOLUME  = $D418

; =============================================================================
; GAME CONSTANTS
; =============================================================================

SPRITE_BASE  = $2000
SPRITE_BLOCK = 128          ; $2000 / 64

; Screen boundaries (sprite coordinates)
MAZE_LEFT    = 24           ; Left edge of maze
MAZE_RIGHT   = 255          ; Right edge
MAZE_TOP     = 50           ; Top edge
MAZE_BOTTOM  = 234          ; Bottom edge

; Movement
MOVE_SPEED   = 2            ; Pixels per frame

; Character codes for maze
CHAR_SPACE   = 32
CHAR_WALL    = 160          ; Solid block
CHAR_DOT     = 46           ; Period for dots
CHAR_POWER   = 81           ; Circle for power pellet
CHAR_EMPTY   = 32           ; Eaten dot space

; Colors
COL_YELLOW   = 7            ; Pac-Man
COL_RED      = 2            ; Blinky
COL_PINK     = 4            ; Pinky  
COL_CYAN     = 3            ; Inky
COL_ORANGE   = 8            ; Clyde
COL_BLUE     = 6            ; Frightened ghost
COL_WHITE    = 1            ; Eyes/dots
COL_MAZE     = 14           ; Light blue maze

; Game states
STATE_TITLE  = 0
STATE_PLAY   = 1
STATE_DYING  = 2
STATE_WON    = 3

; Ghost modes
MODE_CHASE   = 0
MODE_SCATTER = 1
MODE_FRIGHT  = 2

; =============================================================================
; ZERO PAGE VARIABLES
; =============================================================================

.segment "ZEROPAGE"

; Pac-Man state
pac_x:       .res 1         ; Pac-Man X position
pac_y:       .res 1         ; Pac-Man Y position
pac_dir:     .res 1         ; Direction: 0=R, 1=L, 2=U, 3=D
pac_next_dir:.res 1         ; Buffered next direction
pac_frame:   .res 1         ; Animation frame

; Ghost positions (4 ghosts)
ghost_x:     .res 4         ; X positions
ghost_y:     .res 4         ; Y positions
ghost_dir:   .res 4         ; Directions
ghost_mode:  .res 4         ; Current mode per ghost
ghost_timer: .res 4         ; AI timer per ghost

; Game state
game_state:  .res 1
score_lo:    .res 1
score_hi:    .res 1
lives:       .res 1
level:       .res 1
dots_left:   .res 1
power_timer: .res 1         ; Power pellet timer
fright_mode: .res 1         ; Global frightened flag
demo_mode:   .res 1         ; 1 = AI controls Pac-Man
demo_timer:  .res 1         ; Timer for direction changes

; Timing
frame_cnt:   .res 1
tick:        .res 1
speed:       .res 1
ghost_speed: .res 1

; Temporary variables
ptr_lo:      .res 1
ptr_hi:      .res 1
tmp1:        .res 1
tmp2:        .res 1
tmp3:        .res 1
tmp4:        .res 1

; Sound
waka_toggle: .res 1         ; Alternating waka sound

; =============================================================================
; BSS SEGMENT
; =============================================================================

.segment "BSS"
; Maze state (which dots are eaten) - 25 rows x 40 cols = 1000 bytes
; We'll use screen RAM directly for this

; =============================================================================
; BASIC HEADER
; =============================================================================

.segment "HEADER"
    .word $0801
    .word next_line
    .word 10
    .byte $9E
    .byte "2061", 0
next_line:
    .word 0

; =============================================================================
; MAIN CODE
; =============================================================================

.segment "CODE"

start:
    sei
    cld
    
    jsr init_sound
    jsr init_sprites
    
game_restart:
    lda #3
    sta lives
    lda #1
    sta level
    lda #0
    sta score_lo
    sta score_hi
    
    ; Enable demo mode
    lda #1
    sta demo_mode
    lda #30
    sta demo_timer
    
level_start:
    jsr init_screen
    jsr init_maze
    jsr init_positions
    jsr set_sprite_pointers
    jsr update_sprites        ; Position sprites immediately
    
    ; Make sure all sprites are enabled
    lda #%00011111
    sta SPR_ENABLE
    
    ; Brief pause before starting
    ldx #60
start_pause:
    jsr wait_frame
    jsr update_sprites        ; Keep sprites visible during pause
    dex
    bne start_pause
    
    lda #STATE_PLAY
    sta game_state

; -----------------------------------------------------------------------------
; MAIN GAME LOOP
; -----------------------------------------------------------------------------
game_loop:
    jsr wait_frame
    
    lda game_state
    cmp #STATE_PLAY
    bne check_dying
    
    ; Normal gameplay
    jsr read_input
    jsr move_pacman
    jsr check_dot_collision
    jsr update_ghosts
    jsr check_ghost_collision
    jsr update_power_timer
    jsr update_sprites
    jsr draw_score
    jsr animate_pacman
    
    ; Check if level complete
    lda dots_left
    beq level_complete
    
    jmp game_loop

check_dying:
    cmp #STATE_DYING
    bne check_won
    jsr death_animation
    jmp game_loop

check_won:
    cmp #STATE_WON
    bne game_loop
    ; Level complete handled elsewhere
    jmp game_loop

level_complete:
    ; Flash screen
    ldx #10
flash_loop:
    lda #1
    sta BG_COLOR
    jsr wait_frame
    jsr wait_frame
    lda #0
    sta BG_COLOR
    jsr wait_frame
    jsr wait_frame
    dex
    bne flash_loop
    
    ; Next level
    inc level
    lda level
    cmp #10
    bcs game_won
    jmp level_start
game_won:
    ; Game won!
    jmp game_restart

; -----------------------------------------------------------------------------
; INITIALIZATION
; -----------------------------------------------------------------------------

init_sound:
    ; Clear SID
    ldx #24
    lda #0
clear_sid:
    sta SID_BASE, x
    dex
    bpl clear_sid
    
    ; Set volume
    lda #$0F
    sta SID_VOLUME
    
    ; Voice 1 settings (waka waka)
    lda #$09
    sta SID_V1_AD
    lda #$00
    sta SID_V1_SR
    
    ; Voice 2 settings (effects)
    lda #$08
    sta SID_V2_AD
    lda #$04
    sta SID_V2_SR
    
    rts

init_sprites:
    ; Copy sprite data to $2000 (4 sprites, 64 bytes each = 256 bytes)
    ldx #0
copy_sprites:
    lda sprite_pacman, x
    sta $2000, x            ; Pac-Man open (block 128)
    inx
    cpx #63
    bne copy_sprites
    
    ldx #0
copy_sprites2:
    lda sprite_pacman_closed, x
    sta $2040, x            ; Pac-Man closed (block 129)
    inx
    cpx #63
    bne copy_sprites2
    
    ldx #0
copy_sprites3:
    lda sprite_ghost, x
    sta $2080, x            ; Ghost normal (block 130)
    inx
    cpx #63
    bne copy_sprites3
    
    ldx #0
copy_sprites4:
    lda sprite_ghost_fright, x
    sta $20C0, x            ; Ghost frightened (block 131)
    inx
    cpx #63
    bne copy_sprites4
    rts

set_sprite_pointers:
    ; Pac-Man = block 128
    lda #SPRITE_BLOCK
    sta SPR_PTRS
    
    ; Ghosts = block 130 (normal)
    lda #SPRITE_BLOCK + 2
    sta SPR_PTRS + 1
    sta SPR_PTRS + 2
    sta SPR_PTRS + 3
    sta SPR_PTRS + 4
    
    ; Enable 5 sprites (Pac-Man + 4 ghosts)
    lda #%00011111
    sta SPR_ENABLE
    
    ; Set colors
    lda #COL_YELLOW
    sta SPR0_COLOR
    lda #COL_RED
    sta SPR1_COLOR
    lda #COL_PINK
    sta SPR2_COLOR
    lda #COL_CYAN
    sta SPR3_COLOR
    lda #COL_ORANGE
    sta SPR4_COLOR
    
    ; No expansion
    lda #0
    sta SPR_EXPAND_X
    sta SPR_EXPAND_Y
    sta SPR_MULTICLR
    sta SPR_X_MSB
    
    rts

init_screen:
    ; Black background, blue border
    lda #0
    sta BG_COLOR
    lda #6
    sta BORDER
    
    ; Clear screen
    ldx #0
    lda #CHAR_SPACE
clear_screen:
    sta SCREEN_RAM, x
    sta SCREEN_RAM + 256, x
    sta SCREEN_RAM + 512, x
    sta SCREEN_RAM + 768, x
    lda #COL_MAZE
    sta COLOR_RAM, x
    sta COLOR_RAM + 256, x
    sta COLOR_RAM + 512, x
    sta COLOR_RAM + 768, x
    lda #CHAR_SPACE
    inx
    bne clear_screen
    rts

init_maze:
    ; Draw the maze centered on screen
    ; Maze is 20 chars wide, screen is 40, so start at column 10
    
    lda #0
    sta dots_left
    
    ; Set up source pointer to maze_data
    lda #<maze_data
    sta ptr_lo
    lda #>maze_data
    sta ptr_hi
    
    ; Set up destination - screen row 2, column 10 (offset = 80 + 10 = 90)
    lda #<(SCREEN_RAM + 90)
    sta tmp1
    lda #>(SCREEN_RAM + 90)
    sta tmp2
    
    ; Set up color pointer
    lda #<(COLOR_RAM + 90)
    sta tmp3
    lda #>(COLOR_RAM + 90)
    sta tmp4
    
    ldx #0              ; Row counter (max ~24 rows)
    
maze_row_loop:
    ldy #0              ; Column counter
    
maze_col_loop:
    ; Read source character
    lda (ptr_lo), y
    beq maze_done       ; Null terminator = done
    
    cmp #10             ; Newline?
    beq maze_next_row
    
    ; Check character type and convert
    cmp #35             ; '#' = wall
    bne @chk_dot
    lda #160            ; Solid block character
    pha
    lda #14             ; Light blue
    jmp @write_char
    
@chk_dot:
    cmp #46             ; '.' = dot
    bne @chk_power
    inc dots_left
    lda #46             ; Period character
    pha
    lda #1              ; White
    jmp @write_char
    
@chk_power:
    cmp #111            ; 'o' = power pellet
    bne @chk_space
    inc dots_left
    lda #81             ; Circle character
    pha
    lda #1              ; White
    jmp @write_char
    
@chk_space:
    lda #32             ; Space
    pha
    lda #0              ; Black
    
@write_char:
    ; A = color, top of stack = character
    ; Write color to color RAM
    sta (tmp3), y
    
    ; Write character to screen
    pla
    sta (tmp1), y
    
    ; Next column
    iny
    cpy #40             ; Screen width
    bcc maze_col_loop
    
    ; Row full, advance to next
    jmp maze_advance_row
    
maze_next_row:
    ; Newline in data - advance pointers
    iny                 ; Skip past the newline character
    
maze_advance_row:
    ; Advance source pointer by Y (chars read)
    tya
    clc
    adc ptr_lo
    sta ptr_lo
    lda #0
    adc ptr_hi
    sta ptr_hi
    
    ; Advance screen pointer by 40
    lda tmp1
    clc
    adc #40
    sta tmp1
    lda tmp2
    adc #0
    sta tmp2
    
    ; Advance color pointer by 40
    lda tmp3
    clc
    adc #40
    sta tmp3
    lda tmp4
    adc #0
    sta tmp4
    
    inx                 ; Next row
    cpx #24             ; Max rows
    bcc maze_row_loop
    
maze_done:
    rts

init_positions:
    ; Pac-Man starting position (center of maze area)
    ; Maze starts at screen column 10 + sprite offset 24
    ; Y position needs to be in a corridor, not a wall
    lda #160            ; X position (middle of maze)
    sta pac_x
    lda #220            ; Y position (bottom corridor - row 20)
    sta pac_y
    lda #0              ; Facing right
    sta pac_dir
    sta pac_next_dir
    sta pac_frame
    
    ; Ghost starting positions (in ghost house area - center of maze)
    ; Spread them out more
    lda #140
    sta ghost_x
    lda #150
    sta ghost_x + 1
    lda #160
    sta ghost_x + 2
    lda #170
    sta ghost_x + 3
    
    lda #140            ; Y position around middle
    sta ghost_y
    sta ghost_y + 1
    sta ghost_y + 2
    sta ghost_y + 3
    
    ; Initial directions (spread out)
    lda #0
    sta ghost_dir       ; Right
    lda #1
    sta ghost_dir + 1   ; Left
    lda #2
    sta ghost_dir + 2   ; Up
    lda #3
    sta ghost_dir + 3   ; Down
    
    ; All in chase mode
    lda #MODE_CHASE
    sta ghost_mode
    sta ghost_mode + 1
    sta ghost_mode + 2
    sta ghost_mode + 3
    
    ; Random timers
    lda $D012
    sta ghost_timer
    lsr
    sta ghost_timer + 1
    lsr
    sta ghost_timer + 2
    lsr
    sta ghost_timer + 3
    
    ; Game speed
    lda #4
    sta speed
    lda #6
    sta ghost_speed
    
    ; Clear power mode
    lda #0
    sta power_timer
    sta fright_mode
    sta waka_toggle
    
    rts

; -----------------------------------------------------------------------------
; INPUT HANDLING
; -----------------------------------------------------------------------------

read_input:
    ; Check if in demo mode
    lda demo_mode
    beq read_joystick
    
    ; DEMO MODE - AI controls Pac-Man
    jsr demo_ai
    rts
    
read_joystick:
    lda JOYSTICK
    
    ; Check Right (bit 3)
    lsr
    lsr
    lsr
    lsr
    bcs not_right
    lda #0
    sta pac_next_dir
    rts
not_right:
    
    lda JOYSTICK
    ; Check Left (bit 2)
    lsr
    lsr
    lsr
    bcs not_left
    lda #1
    sta pac_next_dir
    rts
not_left:

    lda JOYSTICK
    ; Check Up (bit 0)
    lsr
    bcs not_up
    lda #2
    sta pac_next_dir
    rts
not_up:

    lda JOYSTICK
    ; Check Down (bit 1)
    lsr
    lsr
    bcs not_down
    lda #3
    sta pac_next_dir
not_down:
    rts

; -----------------------------------------------------------------------------
; DEMO AI - Moves Pac-Man automatically
; -----------------------------------------------------------------------------

demo_ai:
    ; Decrease timer
    dec demo_timer
    bne demo_no_change
    
    ; Time to consider a new direction
    lda #20             ; Reset timer
    sta demo_timer
    
    ; Try to find a valid direction - prefer continuing or turning
    ; Use raster line as randomness source
    lda $D012
    and #$03            ; 0-3 direction
    sta tmp1
    
    ; Try this direction
    jsr can_move_dir
    bcc demo_try_next   ; Can't move, try next
    
    lda tmp1
    sta pac_next_dir
    rts
    
demo_try_next:
    ; Try next direction
    lda tmp1
    clc
    adc #1
    and #$03
    sta tmp1
    jsr can_move_dir
    bcc demo_try_next2
    
    lda tmp1
    sta pac_next_dir
    rts
    
demo_try_next2:
    ; Try another
    lda tmp1
    clc
    adc #1
    and #$03
    sta tmp1
    jsr can_move_dir
    bcc demo_try_last
    
    lda tmp1
    sta pac_next_dir
    rts
    
demo_try_last:
    ; Last try
    lda tmp1
    clc
    adc #1
    and #$03
    sta tmp1
    jsr can_move_dir
    bcc demo_no_change
    
    lda tmp1
    sta pac_next_dir
    
demo_no_change:
    rts

; -----------------------------------------------------------------------------
; PAC-MAN MOVEMENT
; -----------------------------------------------------------------------------

move_pacman:
    ; Try to change to buffered direction first
    lda pac_next_dir
    cmp pac_dir
    beq continue_move
    
    ; Check if we can turn
    sta tmp1            ; Save desired direction
    jsr can_move_dir
    bcc continue_move   ; Can't turn, continue current direction
    
    ; Can turn!
    lda tmp1
    sta pac_dir
    
continue_move:
    ; Move in current direction
    lda pac_dir
    jsr can_move_dir
    bcc move_done       ; Blocked
    
    ; Apply movement
    lda pac_dir
    cmp #0
    beq move_right
    cmp #1
    beq move_left
    cmp #2
    beq move_up
    ; else down
    
move_down:
    lda pac_y
    clc
    adc #MOVE_SPEED
    cmp #MAZE_BOTTOM
    bcs move_done
    sta pac_y
    jmp move_sound
    
move_up:
    lda pac_y
    sec
    sbc #MOVE_SPEED
    cmp #MAZE_TOP
    bcc move_done
    sta pac_y
    jmp move_sound
    
move_left:
    lda pac_x
    sec
    sbc #MOVE_SPEED
    cmp #MAZE_LEFT
    bcc move_done
    sta pac_x
    jmp move_sound
    
move_right:
    lda pac_x
    clc
    adc #MOVE_SPEED
    cmp #MAZE_RIGHT
    bcs move_done
    sta pac_x
    
move_sound:
    ; Waka waka sound on movement
    lda frame_cnt
    and #$07
    bne move_done
    jsr sound_waka
    
move_done:
    rts

; Check if Pac-Man can move in direction A
; Returns: Carry set if can move, clear if blocked
can_move_dir:
    sta tmp4            ; Save direction
    
    ; Calculate future position
    lda pac_x
    sta tmp1
    lda pac_y
    sta tmp2
    
    lda tmp4
    cmp #0              ; Right
    bne not_check_right
    lda tmp1
    clc
    adc #8
    sta tmp1
    jmp check_wall
not_check_right:
    cmp #1              ; Left
    bne not_check_left
    lda tmp1
    sec
    sbc #8
    sta tmp1
    jmp check_wall
not_check_left:
    cmp #2              ; Up
    bne not_check_up
    lda tmp2
    sec
    sbc #8
    sta tmp2
    jmp check_wall
not_check_up:
    ; Down
    lda tmp2
    clc
    adc #8
    sta tmp2
    
check_wall:
    ; Convert pixel position to screen coordinates
    ; Screen X = (pixel_x - 24) / 8
    ; Screen Y = (pixel_y - 50) / 8
    lda tmp1
    sec
    sbc #24
    lsr
    lsr
    lsr
    sta tmp3            ; Screen X
    
    lda tmp2
    sec
    sbc #50
    lsr
    lsr
    lsr
    sta tmp4            ; Screen Y
    
    ; Calculate screen offset: Y * 40 + X
    lda tmp4
    asl                 ; *2
    asl                 ; *4
    asl                 ; *8
    sta ptr_lo
    lda tmp4
    asl                 ; *2
    asl                 ; *4
    asl                 ; *8
    asl                 ; *16
    asl                 ; *32
    clc
    adc ptr_lo          ; *40
    adc tmp3            ; + X
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
    cmp #CHAR_WALL
    beq is_blocked
    
    sec                 ; Can move
    rts
    
is_blocked:
    clc                 ; Blocked
    rts

; -----------------------------------------------------------------------------
; DOT COLLISION
; -----------------------------------------------------------------------------

check_dot_collision:
    ; Convert Pac-Man position to screen coords
    lda pac_x
    sec
    sbc #24
    clc
    adc #4              ; Center of sprite
    lsr
    lsr
    lsr
    sta tmp1
    
    lda pac_y
    sec
    sbc #50
    clc
    adc #4
    lsr
    lsr
    lsr
    sta tmp2
    
    ; Calculate screen offset
    lda tmp2
    asl
    asl
    asl
    sta ptr_lo
    lda tmp2
    asl
    asl
    asl
    asl
    asl
    clc
    adc ptr_lo
    adc tmp1
    sta ptr_lo
    lda #0
    adc #0
    sta ptr_hi
    
    lda ptr_lo
    clc
    adc #<SCREEN_RAM
    sta ptr_lo
    lda ptr_hi
    adc #>SCREEN_RAM
    sta ptr_hi
    
    ; Check what's there
    ldy #0
    lda (ptr_lo), y
    
    cmp #CHAR_DOT
    beq eat_dot
    cmp #CHAR_POWER
    beq eat_power
    rts
    
eat_dot:
    lda #CHAR_SPACE
    sta (ptr_lo), y
    
    ; Add 10 points
    lda score_lo
    clc
    adc #10
    sta score_lo
    lda score_hi
    adc #0
    sta score_hi
    
    dec dots_left
    jsr sound_chomp
    rts
    
eat_power:
    lda #CHAR_SPACE
    sta (ptr_lo), y
    
    ; Add 50 points
    lda score_lo
    clc
    adc #50
    sta score_lo
    lda score_hi
    adc #0
    sta score_hi
    
    dec dots_left
    
    ; Activate power mode
    lda #255
    sta power_timer
    lda #1
    sta fright_mode
    
    ; Make ghosts frightened
    lda #MODE_FRIGHT
    sta ghost_mode
    sta ghost_mode + 1
    sta ghost_mode + 2
    sta ghost_mode + 3
    
    ; Change ghost sprites to frightened
    lda #SPRITE_BLOCK + 3
    sta SPR_PTRS + 1
    sta SPR_PTRS + 2
    sta SPR_PTRS + 3
    sta SPR_PTRS + 4
    
    ; Change colors to blue
    lda #COL_BLUE
    sta SPR1_COLOR
    sta SPR2_COLOR
    sta SPR3_COLOR
    sta SPR4_COLOR
    
    jsr sound_power
    rts

update_power_timer:
    lda power_timer
    beq power_done
    
    dec power_timer
    bne power_done
    
    ; Power mode ended
    lda #0
    sta fright_mode
    
    ; Restore ghost modes
    lda #MODE_CHASE
    sta ghost_mode
    sta ghost_mode + 1
    sta ghost_mode + 2
    sta ghost_mode + 3
    
    ; Restore ghost sprites
    lda #SPRITE_BLOCK + 2
    sta SPR_PTRS + 1
    sta SPR_PTRS + 2
    sta SPR_PTRS + 3
    sta SPR_PTRS + 4
    
    ; Restore colors
    lda #COL_RED
    sta SPR1_COLOR
    lda #COL_PINK
    sta SPR2_COLOR
    lda #COL_CYAN
    sta SPR3_COLOR
    lda #COL_ORANGE
    sta SPR4_COLOR
    
power_done:
    rts

; -----------------------------------------------------------------------------
; GHOST AI
; -----------------------------------------------------------------------------

update_ghosts:
    ldx #0
ghost_loop:
    jsr update_one_ghost
    inx
    cpx #4
    bne ghost_loop
    rts

update_one_ghost:
    ; X = ghost index
    
    ; Speed control
    lda frame_cnt
    and ghost_speed
    bne ghost_move_done
    
    ; Check if we should change direction
    dec ghost_timer, x
    bpl ghost_continue
    
    ; Time to reconsider direction
    jsr ghost_choose_direction
    
    ; Reset timer (random-ish)
    lda $D012
    and #$1F
    clc
    adc #10
    sta ghost_timer, x
    
ghost_continue:
    ; Move in current direction
    lda ghost_dir, x
    
    cmp #0              ; Right
    bne ghost_not_right
    lda ghost_x, x
    clc
    adc #2
    cmp #240
    bcs ghost_reverse
    sta ghost_x, x
    jmp ghost_move_done
ghost_not_right:
    
    cmp #1              ; Left
    bne ghost_not_left
    lda ghost_x, x
    sec
    sbc #2
    cmp #30
    bcc ghost_reverse
    sta ghost_x, x
    jmp ghost_move_done
ghost_not_left:

    cmp #2              ; Up
    bne ghost_not_up
    lda ghost_y, x
    sec
    sbc #2
    cmp #60
    bcc ghost_reverse
    sta ghost_y, x
    jmp ghost_move_done
ghost_not_up:

    ; Down
    lda ghost_y, x
    clc
    adc #2
    cmp #220
    bcs ghost_reverse
    sta ghost_y, x
    jmp ghost_move_done

ghost_reverse:
    ; Hit boundary, reverse direction
    lda ghost_dir, x
    eor #$01            ; Flip direction
    sta ghost_dir, x
    
ghost_move_done:
    rts

ghost_choose_direction:
    ; Simple AI: chase Pac-Man in normal mode, run away in fright mode
    
    lda ghost_mode, x
    cmp #MODE_FRIGHT
    beq ghost_flee
    
    ; Chase mode - move toward Pac-Man
    ; Compare X positions
    lda ghost_x, x
    cmp pac_x
    bcc ghost_go_right
    bne ghost_go_left
    jmp ghost_check_y
    
ghost_go_right:
    ; Check if we can go right (not reversing)
    lda ghost_dir, x
    cmp #1              ; Currently going left?
    beq ghost_check_y   ; Yes, try Y instead
    lda #0
    sta ghost_dir, x
    rts
    
ghost_go_left:
    lda ghost_dir, x
    cmp #0              ; Currently going right?
    beq ghost_check_y
    lda #1
    sta ghost_dir, x
    rts
    
ghost_check_y:
    lda ghost_y, x
    cmp pac_y
    bcc ghost_go_down
    bne ghost_go_up
    rts
    
ghost_go_down:
    lda ghost_dir, x
    cmp #2              ; Going up?
    beq ghost_ai_done
    lda #3
    sta ghost_dir, x
    rts
    
ghost_go_up:
    lda ghost_dir, x
    cmp #3              ; Going down?
    beq ghost_ai_done
    lda #2
    sta ghost_dir, x
    
ghost_ai_done:
    rts

ghost_flee:
    ; Frightened mode - move away from Pac-Man (opposite of chase)
    lda ghost_x, x
    cmp pac_x
    bcc ghost_flee_left
    bne ghost_flee_right
    jmp ghost_flee_y
    
ghost_flee_right:
    lda ghost_dir, x
    cmp #1
    beq ghost_flee_y
    lda #0
    sta ghost_dir, x
    rts
    
ghost_flee_left:
    lda ghost_dir, x
    cmp #0
    beq ghost_flee_y
    lda #1
    sta ghost_dir, x
    rts
    
ghost_flee_y:
    lda ghost_y, x
    cmp pac_y
    bcc ghost_flee_up
    bne ghost_flee_down
    rts
    
ghost_flee_up:
    lda ghost_dir, x
    cmp #3
    beq ghost_flee_done
    lda #2
    sta ghost_dir, x
    rts
    
ghost_flee_down:
    lda ghost_dir, x
    cmp #2
    beq ghost_flee_done
    lda #3
    sta ghost_dir, x
    
ghost_flee_done:
    rts

; -----------------------------------------------------------------------------
; GHOST COLLISION
; -----------------------------------------------------------------------------

check_ghost_collision:
    ldx #0
collision_loop:
    ; Check distance to ghost X
    lda pac_x
    sec
    sbc ghost_x, x
    bcs pos_diff_x
    eor #$FF
    clc
    adc #1
pos_diff_x:
    cmp #12             ; Within 12 pixels?
    bcs no_collision_this
    
    ; Check Y distance
    lda pac_y
    sec
    sbc ghost_y, x
    bcs pos_diff_y
    eor #$FF
    clc
    adc #1
pos_diff_y:
    cmp #12
    bcs no_collision_this
    
    ; Collision!
    lda ghost_mode, x
    cmp #MODE_FRIGHT
    beq eat_ghost
    
    ; Pac-Man dies
    jmp pacman_dies
    
eat_ghost:
    ; Eat the ghost!
    jsr sound_eat_ghost
    
    ; Add 200 points
    lda score_lo
    clc
    adc #200
    sta score_lo
    lda score_hi
    adc #0
    sta score_hi
    
    ; Reset ghost to home
    lda #140
    sta ghost_x, x
    lda #100
    sta ghost_y, x
    
    ; No longer frightened
    lda #MODE_CHASE
    sta ghost_mode, x
    
    ; Restore sprite (need to use correct register)
    txa
    clc
    adc #1              ; Sprite 1-4
    tay
    lda #SPRITE_BLOCK + 2
    sta SPR_PTRS, y
    
    ; Restore color
    lda ghost_colors, x
    sta SPR1_COLOR, x
    
no_collision_this:
    inx
    cpx #4
    bne collision_loop
    rts

pacman_dies:
    lda #STATE_DYING
    sta game_state
    jsr sound_death
    rts

death_animation:
    ; Flash Pac-Man
    lda frame_cnt
    and #$04
    beq death_visible
    lda #0
    sta SPR_ENABLE
    jmp death_continue
death_visible:
    lda #%00011111
    sta SPR_ENABLE
    
death_continue:
    ; Count down
    lda frame_cnt
    and #$7F
    bne death_not_done
    
    ; Animation complete
    dec lives
    beq game_over
    
    ; Reset positions
    jsr init_positions
    jsr set_sprite_pointers
    lda #STATE_PLAY
    sta game_state
    rts
    
death_not_done:
    rts

game_over:
    ; Display GAME OVER
    ldx #0
print_gameover:
    lda gameover_text, x
    beq gameover_done
    sta SCREEN_RAM + 12*40 + 15, x
    lda #COL_RED
    sta COLOR_RAM + 12*40 + 15, x
    inx
    bne print_gameover
gameover_done:
    
    ; Wait for fire button
wait_restart:
    jsr wait_frame
    lda JOYSTICK
    and #$10
    bne wait_restart
    
    jmp game_restart

; -----------------------------------------------------------------------------
; SPRITE UPDATES
; -----------------------------------------------------------------------------

update_sprites:
    ; Pac-Man position
    lda pac_x
    clc
    adc #24
    sta SPR0_X
    lda pac_y
    clc
    adc #50
    sta SPR0_Y
    
    ; Ghost positions
    lda ghost_x
    clc
    adc #24
    sta SPR1_X
    lda ghost_y
    clc
    adc #50
    sta SPR1_Y
    
    lda ghost_x + 1
    clc
    adc #24
    sta SPR2_X
    lda ghost_y + 1
    clc
    adc #50
    sta SPR2_Y
    
    lda ghost_x + 2
    clc
    adc #24
    sta SPR3_X
    lda ghost_y + 2
    clc
    adc #50
    sta SPR3_Y
    
    lda ghost_x + 3
    clc
    adc #24
    sta SPR4_X
    lda ghost_y + 3
    clc
    adc #50
    sta SPR4_Y
    
    ; X MSB (all sprites < 256 for now)
    lda #0
    sta SPR_X_MSB
    
    rts

animate_pacman:
    ; Animate every 4 frames
    lda frame_cnt
    and #$03
    bne no_animate
    
    lda pac_frame
    eor #1
    sta pac_frame
    
    beq mouth_open
    lda #SPRITE_BLOCK + 1   ; Closed
    jmp set_pac_sprite
mouth_open:
    lda #SPRITE_BLOCK       ; Open
set_pac_sprite:
    sta SPR_PTRS
    
no_animate:
    rts

; -----------------------------------------------------------------------------
; SCORE DISPLAY
; -----------------------------------------------------------------------------

draw_score:
    ; Draw "SCORE:" at top
    ldx #0
draw_score_label:
    lda score_label, x
    beq draw_score_value
    sta SCREEN_RAM + 1, x
    lda #COL_WHITE
    sta COLOR_RAM + 1, x
    inx
    bne draw_score_label
    
draw_score_value:
    ; Convert score to decimal
    lda score_hi
    sta tmp1
    lda score_lo
    sta tmp2
    
    ; Thousands
    ldx #0
    sec
thou_loop:
    lda tmp2
    sbc #<1000
    tay
    lda tmp1
    sbc #>1000
    bcc thou_done
    sta tmp1
    sty tmp2
    inx
    bne thou_loop
thou_done:
    txa
    clc
    adc #48
    sta SCREEN_RAM + 8
    lda #COL_WHITE
    sta COLOR_RAM + 8
    
    ; Hundreds
    ldx #0
hund_loop:
    lda tmp2
    sec
    sbc #100
    bcc hund_done
    sta tmp2
    inx
    bne hund_loop
hund_done:
    txa
    clc
    adc #48
    sta SCREEN_RAM + 9
    
    ; Tens
    lda tmp2
    ldx #0
tens_loop:
    sec
    sbc #10
    bcc tens_done
    sta tmp2
    inx
    bne tens_loop
tens_done:
    adc #10
    sta tmp2
    txa
    clc
    adc #48
    sta SCREEN_RAM + 10
    
    ; Ones
    lda tmp2
    clc
    adc #48
    sta SCREEN_RAM + 11
    
    ; Draw lives as a number
    lda lives
    clc
    adc #48             ; Convert to ASCII digit '0'-'9'
    sta SCREEN_RAM + 39
    lda #COL_RED
    sta COLOR_RAM + 39
    rts

; -----------------------------------------------------------------------------
; SOUND EFFECTS
; -----------------------------------------------------------------------------

sound_waka:
    ; Alternating waka sound
    lda waka_toggle
    eor #1
    sta waka_toggle
    
    bne waka_high
    lda #$20
    sta SID_V1_FREQ_HI
    lda #$00
    sta SID_V1_FREQ_LO
    jmp waka_play
waka_high:
    lda #$28
    sta SID_V1_FREQ_HI
    lda #$00
    sta SID_V1_FREQ_LO
waka_play:
    lda #$21            ; Sawtooth + gate
    sta SID_V1_CTRL
    lda #$20
    sta SID_V1_CTRL     ; Gate off
    rts

sound_chomp:
    lda #$40
    sta SID_V2_FREQ_HI
    lda #$00
    sta SID_V2_FREQ_LO
    lda #$11            ; Triangle + gate
    sta SID_V2_CTRL
    lda #$10
    sta SID_V2_CTRL
    rts

sound_power:
    ; Descending siren for power pellet
    lda #$60
    sta SID_V2_FREQ_HI
    lda #$21
    sta SID_V2_CTRL
    rts

sound_eat_ghost:
    ; Rising tone
    lda #$80
    sta SID_V2_FREQ_HI
    lda #$11
    sta SID_V2_CTRL
    ldx #20
eat_delay:
    dex
    bne eat_delay
    lda #$10
    sta SID_V2_CTRL
    rts

sound_death:
    ; Descending death sound
    lda #$40
    sta SID_V1_FREQ_HI
    lda #$21
    sta SID_V1_CTRL
    
    ldx #100
death_loop:
    lda SID_V1_FREQ_HI
    sec
    sbc #1
    sta SID_V1_FREQ_HI
    
    ldy #30
death_wait:
    dey
    bne death_wait
    dex
    bne death_loop
    
    lda #$20
    sta SID_V1_CTRL
    rts

; -----------------------------------------------------------------------------
; FRAME WAIT
; -----------------------------------------------------------------------------

wait_frame:
    inc tick
    inc frame_cnt
    
wait_raster:
    lda $D012
    cmp #250
    bne wait_raster
wait_raster2:
    lda $D012
    cmp #250
    beq wait_raster2
    rts

; =============================================================================
; DATA
; =============================================================================

.segment "RODATA"

score_label:
    .byte 19, 3, 15, 18, 5, 58, 0   ; "SCORE:"

gameover_text:
    .byte 7, 1, 13, 5, 32, 15, 22, 5, 18, 0  ; "GAME OVER"

ghost_colors:
    .byte COL_RED, COL_PINK, COL_CYAN, COL_ORANGE

; Maze layout - 20 chars wide, fits on screen
; # = wall, . = dot, o = power pellet, space = empty
; Screen is 40 chars, maze starts at column 10 (centered)
maze_data:
    .byte "####################", 10
    .byte "#........##........#", 10
    .byte "#.##.###.##.###.##.#", 10
    .byte "#o##.###.##.###.##o#", 10
    .byte "#..................#", 10
    .byte "#.##.#.####.#.##.#.#", 10
    .byte "#....#..##..#....#.#", 10
    .byte "####.## ## ##.#####", 10
    .byte "   #.#      #.#    ", 10
    .byte "   #.# #### #.#    ", 10
    .byte "####.# #  # #.#####", 10
    .byte "    .  #  #  .     ", 10
    .byte "####.# #  # #.#####", 10
    .byte "   #.# #### #.#    ", 10
    .byte "   #.#      #.#    ", 10
    .byte "####.# #### #.#####", 10
    .byte "#........##........#", 10
    .byte "#.##.###.##.###.##.#", 10
    .byte "#o.#.....  .....#.o#", 10
    .byte "##.#.##.####.##.#.##", 10
    .byte "#..................#", 10
    .byte "####################", 10
    .byte 0

; =============================================================================
; SPRITE DATA
; =============================================================================

.segment "SPRITES"

; Pac-Man open mouth (facing right) - 24x21 pixels
sprite_pacman:
    .byte %00000111, %11100000, %00000000   ; Row 0
    .byte %00011111, %11111000, %00000000   ; Row 1
    .byte %00111111, %11111100, %00000000   ; Row 2
    .byte %01111111, %11111110, %00000000   ; Row 3
    .byte %01111111, %11111000, %00000000   ; Row 4
    .byte %11111111, %11100000, %00000000   ; Row 5
    .byte %11111111, %11000000, %00000000   ; Row 6
    .byte %11111111, %10000000, %00000000   ; Row 7
    .byte %11111111, %10000000, %00000000   ; Row 8
    .byte %11111111, %11000000, %00000000   ; Row 9
    .byte %11111111, %11100000, %00000000   ; Row 10
    .byte %01111111, %11111000, %00000000   ; Row 11
    .byte %01111111, %11111110, %00000000   ; Row 12
    .byte %00111111, %11111100, %00000000   ; Row 13
    .byte %00011111, %11111000, %00000000   ; Row 14
    .byte %00000111, %11100000, %00000000   ; Row 15
    .byte %00000000, %00000000, %00000000   ; Row 16
    .byte %00000000, %00000000, %00000000   ; Row 17
    .byte %00000000, %00000000, %00000000   ; Row 18
    .byte %00000000, %00000000, %00000000   ; Row 19
    .byte %00000000, %00000000, %00000000   ; Row 20
    .byte %00                               ; Padding

; Pac-Man closed mouth
sprite_pacman_closed:
    .byte %00000111, %11100000, %00000000
    .byte %00011111, %11111000, %00000000
    .byte %00111111, %11111100, %00000000
    .byte %01111111, %11111110, %00000000
    .byte %01111111, %11111110, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %01111111, %11111110, %00000000
    .byte %01111111, %11111110, %00000000
    .byte %00111111, %11111100, %00000000
    .byte %00011111, %11111000, %00000000
    .byte %00000111, %11100000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00

; Ghost normal
sprite_ghost:
    .byte %00000111, %11100000, %00000000
    .byte %00011111, %11111000, %00000000
    .byte %00111111, %11111100, %00000000
    .byte %01111111, %11111110, %00000000
    .byte %01111001, %10011110, %00000000   ; Eyes
    .byte %11111001, %10011111, %00000000
    .byte %11111001, %10011111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11101110, %11101110, %00000000   ; Wavy bottom
    .byte %11000110, %01100011, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00

; Ghost frightened (scared)
sprite_ghost_fright:
    .byte %00000111, %11100000, %00000000
    .byte %00011111, %11111000, %00000000
    .byte %00111111, %11111100, %00000000
    .byte %01111111, %11111110, %00000000
    .byte %01100110, %01100110, %00000000   ; Scared eyes
    .byte %11100110, %01100111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11011001, %10011011, %00000000   ; Wavy mouth
    .byte %10100110, %01100101, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11111111, %11111111, %00000000
    .byte %11101110, %11101110, %00000000
    .byte %11000110, %01100011, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00000000, %00000000, %00000000
    .byte %00

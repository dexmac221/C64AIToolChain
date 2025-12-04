; -----------------------------------------------------------------------------
; TETRIS for C64 (cc65 Relocatable Version)
; A complete Tetris implementation with all 7 tetrominoes, rotation,
; collision detection, line clearing, and scoring.
; -----------------------------------------------------------------------------

; --- Constants ---
SCREEN_RAM  = $0400
COLOR_RAM   = $D800
JOYSTICK    = $DC00
BORDER      = $D020
BG_COLOR    = $D021

CHAR_SPACE  = 32
CHAR_WALL   = 160       ; Solid block for walls
CHAR_BLOCK  = 160       ; Solid block for pieces (same char, different color)

; Board dimensions (playfield is 10 wide, 20 tall)
BOARD_WIDTH     = 10
BOARD_HEIGHT    = 20
BOARD_X_START   = 15    ; Screen X offset for playfield
BOARD_Y_START   = 2     ; Screen Y offset for playfield

; Piece colors (C64 color codes)
COLOR_I     = 3         ; Cyan
COLOR_O     = 7         ; Yellow
COLOR_T     = 4         ; Purple
COLOR_S     = 5         ; Green
COLOR_Z     = 2         ; Red
COLOR_J     = 6         ; Blue
COLOR_L     = 8         ; Orange

; --- Zero Page Segment ---
.segment "ZEROPAGE"
ptr_lo:         .res 1      ; General purpose pointer
ptr_hi:         .res 1
col_ptr_lo:     .res 1      ; Color RAM pointer
col_ptr_hi:     .res 1
cur_piece:      .res 1      ; Current piece type (0-6)
cur_rot:        .res 1      ; Current rotation (0-3)
cur_x:          .res 1      ; Current piece X position
cur_y:          .res 1      ; Current piece Y position
frame_cnt:      .res 1      ; Frame counter for timing
drop_speed:     .res 1      ; Frames between drops
input_delay:    .res 1      ; Input repeat delay
tmp_x:          .res 1      ; Temp variables
tmp_y:          .res 1
tmp_lo:         .res 1
tmp_hi:         .res 1
piece_idx:      .res 1      ; Index into piece data
loop_i:         .res 1      ; Loop counters
loop_j:         .res 1
test_x:         .res 1      ; Collision test coords
test_y:         .res 1
score_lo:       .res 1      ; Score (16-bit)
score_hi:       .res 1
lines_cleared:  .res 1      ; Total lines cleared
level:          .res 1      ; Current level
rng_seed:       .res 1      ; Random seed
next_piece:     .res 1      ; Next piece preview
game_over:      .res 1      ; Game over flag
cur_color:      .res 1      ; Current piece color
check_row:      .res 1      ; Row being checked for clearing
piece_ptr_lo:   .res 1      ; Piece data pointer (preserved across calc_screen_pos)
piece_ptr_hi:   .res 1
demo_mode:      .res 1      ; 1 = AI playing, 0 = human playing
ai_target_x:    .res 1      ; Target X position for AI
ai_target_rot:  .res 1      ; Target rotation for AI

; --- BSS Segment (Uninitialized Data) ---
.segment "BSS"
board:          .res BOARD_WIDTH * BOARD_HEIGHT  ; Playfield buffer (0=empty, 1-7=piece color)

; --- BASIC Header ---
.segment "HEADER"
    .word $0801         ; Load address
    .word next_line     ; Pointer to next line
    .word 10            ; Line number
    .byte $9E           ; SYS token
    .byte "2061", 0     ; Address of 'start'
next_line:
    .word 0             ; End of BASIC program

; --- Main Code ---
.segment "CODE"
start:
    sei                 ; Disable interrupts
    cld                 ; Clear decimal mode
    
    ; Start in demo mode
    lda #1
    sta demo_mode
    
    jsr init_game
    jsr draw_demo_text
    
game_loop:
    lda game_over
    bne do_game_over
    
    ; Check if player wants to take control (fire button)
    lda demo_mode
    beq skip_demo_check
    lda JOYSTICK
    and #$10
    bne skip_demo_check
    ; Player pressed fire - switch to player mode
    lda #0
    sta demo_mode
    jsr clear_demo_text
    jsr init_game
    jmp game_loop
    
skip_demo_check:
    ; Wait for vertical blank to avoid flicker
    jsr wait_vblank
    
    ; Clear current piece from screen (before any movement)
    jsr clear_piece
    
    ; Handle input (AI or human)
    lda demo_mode
    beq do_human_input
    jsr ai_move
    jmp done_input
do_human_input:
    jsr read_input
done_input:
    
    ; Handle gravity drop
    jsr update_drop
    
    ; Redraw piece at new position
    jsr draw_piece
    
    ; Update score display
    jsr draw_score
    
    jmp game_loop

do_game_over:
    ; Switch back to demo mode
    lda #1
    sta demo_mode
    
    ; Flash border on game over
    inc BORDER
    jsr wait_frame
    jsr wait_frame
    jsr wait_frame
    dec BORDER
    jsr wait_frame
    jsr wait_frame
    jsr wait_frame
    
    ; Wait a moment then restart demo
    ldx #60
game_over_wait:
    jsr wait_frame
    ; Check for fire to start playing
    lda JOYSTICK
    and #$10
    beq start_player_mode
    dex
    bne game_over_wait
    
    ; Restart in demo mode
    jsr init_game
    jsr draw_demo_text
    jmp game_loop

start_player_mode:
    lda #0
    sta demo_mode
    jsr init_game
    jmp game_loop

; -----------------------------------------------------------------------------
; INITIALIZATION
; -----------------------------------------------------------------------------
init_game:
    ; Set colors
    lda #0
    sta BORDER
    sta BG_COLOR
    
    ; Clear score and level
    lda #0
    sta score_lo
    sta score_hi
    sta lines_cleared
    sta game_over
    lda #1
    sta level
    lda #15             ; Starting drop speed (frames)
    sta drop_speed
    lda #0
    sta frame_cnt
    sta input_delay
    
    ; Initialize RNG seed from timer
    lda $DC04
    sta rng_seed
    
    ; Clear board
    ldx #0
    lda #0
clear_board:
    sta board, x
    inx
    cpx #BOARD_WIDTH * BOARD_HEIGHT
    bne clear_board
    
    ; Clear screen
    jsr clear_screen
    
    ; Draw border
    jsr draw_border
    
    ; Draw UI
    jsr draw_ui
    
    ; Spawn first pieces
    jsr random_piece
    sta next_piece
    jsr spawn_piece
    
    rts

clear_screen:
    ldx #0
    lda #CHAR_SPACE
clear_loop:
    sta SCREEN_RAM, x
    sta SCREEN_RAM + 250, x
    sta SCREEN_RAM + 500, x
    sta SCREEN_RAM + 750, x
    dex
    bne clear_loop
    
    ; Set all colors to light gray
    lda #15
    ldx #0
set_colors:
    sta COLOR_RAM, x
    sta COLOR_RAM + 250, x
    sta COLOR_RAM + 500, x
    sta COLOR_RAM + 750, x
    dex
    bne set_colors
    rts

draw_border:
    ; Draw top wall
    ldx #0
draw_top:
    lda #CHAR_WALL
    sta SCREEN_RAM + (BOARD_Y_START - 1) * 40 + BOARD_X_START - 1, x
    lda #12             ; Gray
    sta COLOR_RAM + (BOARD_Y_START - 1) * 40 + BOARD_X_START - 1, x
    inx
    cpx #BOARD_WIDTH + 2
    bne draw_top
    
    ; Draw bottom wall
    ldx #0
draw_bottom:
    lda #CHAR_WALL
    sta SCREEN_RAM + (BOARD_Y_START + BOARD_HEIGHT) * 40 + BOARD_X_START - 1, x
    lda #12
    sta COLOR_RAM + (BOARD_Y_START + BOARD_HEIGHT) * 40 + BOARD_X_START - 1, x
    inx
    cpx #BOARD_WIDTH + 2
    bne draw_bottom
    
    ; Draw side walls
    ldx #0
draw_sides:
    ; Calculate row offset
    txa
    clc
    adc #BOARD_Y_START
    sta tmp_y
    
    ; Left wall
    lda #BOARD_X_START - 1
    sta tmp_x
    jsr calc_screen_pos
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y
    lda #12
    sta (col_ptr_lo), y
    
    ; Right wall
    lda #BOARD_X_START + BOARD_WIDTH
    sta tmp_x
    jsr calc_screen_pos
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y
    lda #12
    sta (col_ptr_lo), y
    
    inx
    cpx #BOARD_HEIGHT
    bne draw_sides
    
    rts

draw_ui:
    ; Draw "SCORE" label at column 1
    ldx #0
draw_score_label:
    lda score_text, x
    beq draw_lines_label
    sta SCREEN_RAM + 40 * 3, x
    lda #1              ; White
    sta COLOR_RAM + 40 * 3, x
    inx
    jmp draw_score_label

draw_lines_label:
    ldx #0
draw_ll:
    lda lines_text, x
    beq draw_level_label
    sta SCREEN_RAM + 40 * 7, x
    lda #1
    sta COLOR_RAM + 40 * 7, x
    inx
    jmp draw_ll

draw_level_label:
    ldx #0
draw_levl:
    lda level_text, x
    beq draw_next_label
    sta SCREEN_RAM + 40 * 11, x
    lda #1
    sta COLOR_RAM + 40 * 11, x
    inx
    jmp draw_levl

draw_next_label:
    ldx #0
draw_nl:
    lda next_text, x
    beq ui_done
    sta SCREEN_RAM + 40 * 3 + 28, x
    lda #1
    sta COLOR_RAM + 40 * 3 + 28, x
    inx
    jmp draw_nl
    
ui_done:
    rts

; Text data (PETSCII)
score_text:
    .byte 19, 3, 15, 18, 5, 0   ; "SCORE" in PETSCII screen codes
lines_text:
    .byte 12, 9, 14, 5, 19, 0   ; "LINES"
level_text:
    .byte 12, 5, 22, 5, 12, 0   ; "LEVEL"
next_text:
    .byte 14, 5, 24, 20, 0      ; "NEXT"

; -----------------------------------------------------------------------------
; PIECE DATA
; Each piece has 4 rotations, each rotation is 4 (x,y) offsets from center
; -----------------------------------------------------------------------------
; Piece 0: I (cyan)
piece_I_r0:  .byte 255, 0,   0, 0,   1, 0,   2, 0    ; Horizontal
piece_I_r1:  .byte 0, 255,   0, 0,   0, 1,   0, 2    ; Vertical
piece_I_r2:  .byte 255, 0,   0, 0,   1, 0,   2, 0    ; Horizontal
piece_I_r3:  .byte 0, 255,   0, 0,   0, 1,   0, 2    ; Vertical

; Piece 1: O (yellow)
piece_O_r0:  .byte 0, 0,   1, 0,   0, 1,   1, 1
piece_O_r1:  .byte 0, 0,   1, 0,   0, 1,   1, 1
piece_O_r2:  .byte 0, 0,   1, 0,   0, 1,   1, 1
piece_O_r3:  .byte 0, 0,   1, 0,   0, 1,   1, 1

; Piece 2: T (purple)
piece_T_r0:  .byte 255, 0,   0, 0,   1, 0,   0, 1
piece_T_r1:  .byte 0, 255,   0, 0,   0, 1,   1, 0
piece_T_r2:  .byte 255, 0,   0, 0,   1, 0,   0, 255
piece_T_r3:  .byte 0, 255,   0, 0,   0, 1,   255, 0

; Piece 3: S (green)
piece_S_r0:  .byte 0, 0,   1, 0,   255, 1,   0, 1
piece_S_r1:  .byte 0, 0,   0, 1,   1, 1,   1, 2
piece_S_r2:  .byte 0, 0,   1, 0,   255, 1,   0, 1
piece_S_r3:  .byte 0, 0,   0, 1,   1, 1,   1, 2

; Piece 4: Z (red)
piece_Z_r0:  .byte 255, 0,   0, 0,   0, 1,   1, 1
piece_Z_r1:  .byte 1, 0,   0, 1,   1, 1,   0, 2
piece_Z_r2:  .byte 255, 0,   0, 0,   0, 1,   1, 1
piece_Z_r3:  .byte 1, 0,   0, 1,   1, 1,   0, 2

; Piece 5: J (blue)
piece_J_r0:  .byte 255, 0,   0, 0,   1, 0,   1, 1
piece_J_r1:  .byte 0, 255,   0, 0,   0, 1,   255, 1
piece_J_r2:  .byte 255, 255,   255, 0,   0, 0,   1, 0
piece_J_r3:  .byte 1, 255,   0, 255,   0, 0,   0, 1

; Piece 6: L (orange)
piece_L_r0:  .byte 255, 0,   0, 0,   1, 0,   255, 1
piece_L_r1:  .byte 255, 255,   0, 255,   0, 0,   0, 1
piece_L_r2:  .byte 1, 255,   255, 0,   0, 0,   1, 0
piece_L_r3:  .byte 0, 255,   0, 0,   0, 1,   1, 1

; Piece data table (low bytes)
piece_table_lo:
    .byte <piece_I_r0, <piece_I_r1, <piece_I_r2, <piece_I_r3
    .byte <piece_O_r0, <piece_O_r1, <piece_O_r2, <piece_O_r3
    .byte <piece_T_r0, <piece_T_r1, <piece_T_r2, <piece_T_r3
    .byte <piece_S_r0, <piece_S_r1, <piece_S_r2, <piece_S_r3
    .byte <piece_Z_r0, <piece_Z_r1, <piece_Z_r2, <piece_Z_r3
    .byte <piece_J_r0, <piece_J_r1, <piece_J_r2, <piece_J_r3
    .byte <piece_L_r0, <piece_L_r1, <piece_L_r2, <piece_L_r3

; Piece data table (high bytes)
piece_table_hi:
    .byte >piece_I_r0, >piece_I_r1, >piece_I_r2, >piece_I_r3
    .byte >piece_O_r0, >piece_O_r1, >piece_O_r2, >piece_O_r3
    .byte >piece_T_r0, >piece_T_r1, >piece_T_r2, >piece_T_r3
    .byte >piece_S_r0, >piece_S_r1, >piece_S_r2, >piece_S_r3
    .byte >piece_Z_r0, >piece_Z_r1, >piece_Z_r2, >piece_Z_r3
    .byte >piece_J_r0, >piece_J_r1, >piece_J_r2, >piece_J_r3
    .byte >piece_L_r0, >piece_L_r1, >piece_L_r2, >piece_L_r3

; Color table for each piece type
piece_colors:
    .byte COLOR_I, COLOR_O, COLOR_T, COLOR_S, COLOR_Z, COLOR_J, COLOR_L

; -----------------------------------------------------------------------------
; PIECE SPAWNING
; -----------------------------------------------------------------------------
spawn_piece:
    ; Get next piece
    lda next_piece
    sta cur_piece
    
    ; Get new next piece
    jsr random_piece
    sta next_piece
    
    ; Set starting position
    lda #4              ; Middle X
    sta cur_x
    lda #0              ; Top Y
    sta cur_y
    lda #0              ; Rotation 0
    sta cur_rot
    
    ; Get piece color
    ldx cur_piece
    lda piece_colors, x
    sta cur_color
    
    ; Check if spawn position is valid
    jsr check_collision
    bcs spawn_game_over
    
    ; Draw next piece preview
    jsr draw_next_preview
    
    rts

spawn_game_over:
    lda #1
    sta game_over
    rts

random_piece:
    ; Simple LCG random: seed = seed * 5 + 1
    lda rng_seed
    asl
    asl
    clc
    adc rng_seed
    clc
    adc #1
    adc $DC04           ; Add timer for more entropy
    sta rng_seed
    
    ; Modulo 7
    and #$07
    cmp #7
    bcc rng_ok
    sbc #7
rng_ok:
    rts

; -----------------------------------------------------------------------------
; INPUT HANDLING
; -----------------------------------------------------------------------------
read_input:
    ; Decrement input delay
    lda input_delay
    beq input_ready
    dec input_delay
    rts

input_ready:
    lda JOYSTICK
    and #$1F
    
    ; Check left (bit 2)
    lsr
    lsr
    lsr
    bcs check_right
    jsr try_move_left
    lda #5
    sta input_delay
    rts

check_right:
    lda JOYSTICK
    and #$08            ; Right (bit 3)
    bne check_down
    jsr try_move_right
    lda #5
    sta input_delay
    rts

check_down:
    lda JOYSTICK
    and #$02            ; Down (bit 1)
    bne check_rotate
    jsr try_move_down
    lda #2
    sta input_delay
    rts

check_rotate:
    lda JOYSTICK
    and #$10            ; Fire (bit 4)
    bne input_done
    jsr try_rotate
    lda #10
    sta input_delay

input_done:
    rts

try_move_left:
    dec cur_x
    jsr check_collision
    bcc move_ok_l
    inc cur_x           ; Restore
move_ok_l:
    rts

try_move_right:
    inc cur_x
    jsr check_collision
    bcc move_ok_r
    dec cur_x           ; Restore
move_ok_r:
    rts

try_move_down:
    inc cur_y
    jsr check_collision
    bcc move_ok_d
    dec cur_y           ; Restore
    jsr lock_piece
move_ok_d:
    rts

try_rotate:
    lda cur_rot
    pha                 ; Save current rotation
    
    inc cur_rot
    lda cur_rot
    and #$03
    sta cur_rot
    
    jsr check_collision
    bcc rotate_ok
    
    ; Restore rotation
    pla
    sta cur_rot
    rts

rotate_ok:
    pla                 ; Discard saved rotation
    rts

; -----------------------------------------------------------------------------
; COLLISION DETECTION
; Returns: Carry set = collision, Carry clear = no collision
; -----------------------------------------------------------------------------
check_collision:
    jsr get_piece_ptr
    ; Save piece pointer
    lda ptr_lo
    sta piece_ptr_lo
    lda ptr_hi
    sta piece_ptr_hi
    
    lda #0
    sta loop_i

coll_loop:
    ; Get X offset
    ldy loop_i
    lda (piece_ptr_lo), y
    bmi coll_neg_x      ; Handle signed -1 (255)
    clc
    adc cur_x
    jmp coll_store_x
coll_neg_x:
    lda cur_x
    sec
    sbc #1
coll_store_x:
    sta test_x
    
    ; Get Y offset
    iny
    lda (piece_ptr_lo), y
    bmi coll_neg_y
    clc
    adc cur_y
    jmp coll_store_y
coll_neg_y:
    lda cur_y
    sec
    sbc #1
coll_store_y:
    sta test_y
    
    ; Check bounds
    lda test_x
    bmi coll_hit        ; X < 0
    cmp #BOARD_WIDTH
    bcs coll_hit        ; X >= width
    
    lda test_y
    bmi coll_skip       ; Y < 0 is OK (spawning)
    cmp #BOARD_HEIGHT
    bcs coll_hit        ; Y >= height
    
    ; Check board
    jsr get_board_cell
    cmp #0
    bne coll_hit
    
coll_skip:
    ; Next block
    lda loop_i
    clc
    adc #2
    sta loop_i
    cmp #8
    bcc coll_loop
    
    clc                 ; No collision
    rts

coll_hit:
    sec                 ; Collision
    rts

; Get pointer to current piece/rotation data
get_piece_ptr:
    ; Index = piece * 4 + rotation
    lda cur_piece
    asl
    asl
    clc
    adc cur_rot
    tax
    
    lda piece_table_lo, x
    sta ptr_lo
    lda piece_table_hi, x
    sta ptr_hi
    rts

; Get board cell at (test_x, test_y)
; Returns value in A
get_board_cell:
    ; Index = y * BOARD_WIDTH + x = y * 10 + x
    lda test_y
    asl                 ; *2
    asl                 ; *4
    clc
    adc test_y          ; *5
    asl                 ; *10
    clc
    adc test_x
    tax
    lda board, x
    rts

; Set board cell at (test_x, test_y) to value in A
set_board_cell:
    pha
    lda test_y
    asl
    asl
    clc
    adc test_y
    asl
    clc
    adc test_x
    tax
    pla
    sta board, x
    rts

; -----------------------------------------------------------------------------
; PIECE LOCKING AND LINE CLEARING
; -----------------------------------------------------------------------------
lock_piece:
    jsr get_piece_ptr
    ; Save piece pointer
    lda ptr_lo
    sta piece_ptr_lo
    lda ptr_hi
    sta piece_ptr_hi
    
    lda #0
    sta loop_i

lock_loop:
    ; Get X offset
    ldy loop_i
    lda (piece_ptr_lo), y
    bmi lock_neg_x
    clc
    adc cur_x
    jmp lock_store_x
lock_neg_x:
    lda cur_x
    sec
    sbc #1
lock_store_x:
    sta test_x
    
    ; Get Y offset
    iny
    lda (piece_ptr_lo), y
    bmi lock_neg_y
    clc
    adc cur_y
    jmp lock_store_y
lock_neg_y:
    lda cur_y
    sec
    sbc #1
lock_store_y:
    sta test_y
    
    ; Skip if Y < 0
    lda test_y
    bmi lock_next
    
    ; Set board cell to piece color index + 1
    lda cur_piece
    clc
    adc #1
    jsr set_board_cell

lock_next:
    lda loop_i
    clc
    adc #2
    sta loop_i
    cmp #8
    bcc lock_loop
    
    ; Check for completed lines
    jsr check_lines
    
    ; Redraw the board to show locked piece and cleared lines
    jsr draw_board
    
    ; Spawn new piece
    jsr spawn_piece
    rts

check_lines:
    lda #0
    sta tmp_hi          ; Lines cleared this turn
    
    lda #BOARD_HEIGHT - 1
    sta check_row

check_row_loop:
    lda #0
    sta loop_i          ; Column counter (use loop_i instead of X register)
    lda check_row
    sta test_y

check_col_loop:
    lda loop_i
    sta test_x
    jsr get_board_cell
    cmp #0
    beq row_not_complete
    inc loop_i
    lda loop_i
    cmp #BOARD_WIDTH
    bne check_col_loop
    
    ; Row is complete - clear it
    jsr clear_row
    inc tmp_hi
    jmp check_row_loop  ; Recheck same row (rows shifted down)

row_not_complete:
    dec check_row
    lda check_row
    bpl check_row_loop
    
    ; Add score based on lines cleared
    lda tmp_hi
    beq no_lines
    
    ; Add to total lines
    clc
    adc lines_cleared
    sta lines_cleared
    
    ; Check level up (every 10 lines)
    cmp #10
    bcc no_level_up
    
    sec
    sbc #10
    sta lines_cleared
    inc level
    
    ; Increase speed
    lda drop_speed
    cmp #3
    bcc no_level_up
    dec drop_speed
    dec drop_speed

no_level_up:
    ; Score: 1 line=100, 2=300, 3=500, 4=800
    lda tmp_hi
    cmp #1
    beq score_1
    cmp #2
    beq score_2
    cmp #3
    beq score_3
    ; 4 lines (tetris)
    lda #<800
    ldx #>800
    jmp add_score
score_3:
    lda #<500
    ldx #>500
    jmp add_score
score_2:
    lda #<300
    ldx #>300
    jmp add_score
score_1:
    lda #<100
    ldx #>100

add_score:
    clc
    adc score_lo
    sta score_lo
    txa
    adc score_hi
    sta score_hi

no_lines:
    rts

clear_row:
    ; Shift all rows down from check_row
    lda check_row
    sta loop_j

shift_loop:
    lda loop_j
    beq clear_top_row
    
    ; Copy row above to current row
    lda #0
    sta loop_i          ; Column counter
copy_row_loop:
    lda loop_i
    sta test_x
    
    ; Get cell from row above
    lda loop_j
    sec
    sbc #1
    sta test_y
    jsr get_board_cell
    pha                 ; Save cell value on stack
    
    ; Set cell in current row
    lda loop_j
    sta test_y
    lda loop_i
    sta test_x
    pla                 ; Retrieve cell value
    jsr set_board_cell
    
    inc loop_i
    lda loop_i
    cmp #BOARD_WIDTH
    bne copy_row_loop
    
    dec loop_j
    jmp shift_loop

clear_top_row:
    ; Clear top row
    lda #0
    sta test_y
    sta loop_i          ; Column counter
clear_top_loop:
    lda loop_i
    sta test_x
    lda #0
    jsr set_board_cell
    inc loop_i
    lda loop_i
    cmp #BOARD_WIDTH
    bne clear_top_loop
    
    rts

; -----------------------------------------------------------------------------
; DROP TIMING
; -----------------------------------------------------------------------------
update_drop:
    inc frame_cnt
    lda frame_cnt
    cmp drop_speed
    bcc no_drop
    
    lda #0
    sta frame_cnt
    
    ; Try to move down
    inc cur_y
    jsr check_collision
    bcc drop_ok
    
    ; Collision - restore and lock
    dec cur_y
    jsr lock_piece

drop_ok:
no_drop:
    rts

; -----------------------------------------------------------------------------
; SCREEN DRAWING
; -----------------------------------------------------------------------------
draw_screen:
    ; Only called after locking a piece or at init
    ; Draw entire board state
    jsr draw_board
    
    ; Draw current piece
    jsr draw_piece
    
    ; Draw score
    jsr draw_score
    
    rts

; Clear current piece from screen (draw spaces where piece is)
clear_piece:
    jsr get_piece_ptr
    ; Save piece pointer
    lda ptr_lo
    sta piece_ptr_lo
    lda ptr_hi
    sta piece_ptr_hi
    
    lda #0
    sta loop_i

clear_piece_loop:
    ; Get X offset
    ldy loop_i
    lda (piece_ptr_lo), y
    bmi clear_neg_x
    clc
    adc cur_x
    jmp clear_store_x
clear_neg_x:
    lda cur_x
    sec
    sbc #1
clear_store_x:
    clc
    adc #BOARD_X_START
    sta tmp_x
    
    ; Get Y offset
    iny
    lda (piece_ptr_lo), y
    bmi clear_neg_y
    clc
    adc cur_y
    jmp clear_store_y
clear_neg_y:
    lda cur_y
    sec
    sbc #1
clear_store_y:
    ; Skip if Y < 0
    bmi clear_piece_next
    
    clc
    adc #BOARD_Y_START
    sta tmp_y
    
    jsr calc_screen_pos
    
    lda #CHAR_SPACE
    ldy #0
    sta (ptr_lo), y

clear_piece_next:
    lda loop_i
    clc
    adc #2
    sta loop_i
    cmp #8
    bcc clear_piece_loop
    
    rts

draw_board:
    lda #0
    sta loop_j          ; Y
    
board_y_loop:
    lda #0
    sta loop_i          ; X
    
board_x_loop:
    lda loop_i
    sta test_x
    lda loop_j
    sta test_y
    
    jsr get_board_cell
    beq draw_empty
    
    ; Draw block with color
    tax
    dex                 ; Convert 1-7 to 0-6 for color lookup
    lda piece_colors, x
    pha                 ; Save color on stack (tmp_lo gets corrupted by calc_screen_pos)
    
    lda loop_i
    clc
    adc #BOARD_X_START
    sta tmp_x
    lda loop_j
    clc
    adc #BOARD_Y_START
    sta tmp_y
    jsr calc_screen_pos
    
    lda #CHAR_BLOCK
    ldy #0
    sta (ptr_lo), y
    pla                 ; Retrieve color from stack
    sta (col_ptr_lo), y
    jmp board_next_x

draw_empty:
    lda loop_i
    clc
    adc #BOARD_X_START
    sta tmp_x
    lda loop_j
    clc
    adc #BOARD_Y_START
    sta tmp_y
    jsr calc_screen_pos
    
    lda #CHAR_SPACE
    ldy #0
    sta (ptr_lo), y

board_next_x:
    inc loop_i
    lda loop_i
    cmp #BOARD_WIDTH
    bne board_x_loop
    
    inc loop_j
    lda loop_j
    cmp #BOARD_HEIGHT
    bne board_y_loop
    
    rts

draw_piece:
    jsr get_piece_ptr
    ; Save piece pointer
    lda ptr_lo
    sta piece_ptr_lo
    lda ptr_hi
    sta piece_ptr_hi
    
    lda #0
    sta loop_i

draw_piece_loop:
    ; Get X offset
    ldy loop_i
    lda (piece_ptr_lo), y
    bmi draw_neg_x
    clc
    adc cur_x
    jmp draw_store_x
draw_neg_x:
    lda cur_x
    sec
    sbc #1
draw_store_x:
    clc
    adc #BOARD_X_START
    sta tmp_x
    
    ; Get Y offset
    iny
    lda (piece_ptr_lo), y
    bmi draw_neg_y
    clc
    adc cur_y
    jmp draw_store_y
draw_neg_y:
    lda cur_y
    sec
    sbc #1
draw_store_y:
    ; Skip if Y < 0
    bmi draw_piece_next
    
    clc
    adc #BOARD_Y_START
    sta tmp_y
    
    jsr calc_screen_pos
    
    lda #CHAR_BLOCK
    ldy #0
    sta (ptr_lo), y
    lda cur_color
    sta (col_ptr_lo), y

draw_piece_next:
    lda loop_i
    clc
    adc #2
    sta loop_i
    cmp #8
    bcc draw_piece_loop
    
    rts

draw_next_preview:
    ; Clear preview area first
    ldx #0
clear_preview:
    lda #CHAR_SPACE
    sta SCREEN_RAM + 40 * 5 + 28, x
    sta SCREEN_RAM + 40 * 6 + 28, x
    sta SCREEN_RAM + 40 * 7 + 28, x
    sta SCREEN_RAM + 40 * 8 + 28, x
    inx
    cpx #6
    bne clear_preview
    
    ; Get next piece data (rotation 0)
    lda next_piece
    asl
    asl
    tax
    lda piece_table_lo, x
    sta piece_ptr_lo
    lda piece_table_hi, x
    sta piece_ptr_hi
    
    ; Get color
    ldx next_piece
    lda piece_colors, x
    pha                 ; Save color on stack
    
    ; Draw 4 blocks
    lda #0
    sta loop_i

preview_loop:
    ldy loop_i
    lda (piece_ptr_lo), y
    bmi preview_neg_x
    clc
    adc #30             ; X position for preview
    jmp preview_sx
preview_neg_x:
    lda #29
preview_sx:
    sta tmp_x
    
    iny
    lda (piece_ptr_lo), y
    bmi preview_neg_y
    clc
    adc #6              ; Y position for preview
    jmp preview_sy
preview_neg_y:
    lda #5
preview_sy:
    sta tmp_y
    
    jsr calc_screen_pos
    lda #CHAR_BLOCK
    ldy #0
    sta (ptr_lo), y
    pla                 ; Get color
    pha                 ; Keep it on stack
    sta (col_ptr_lo), y
    
    lda loop_i
    clc
    adc #2
    sta loop_i
    cmp #8
    bcc preview_loop
    
    pla                 ; Clean up stack
    rts

draw_score:
    ; Draw score value (5 digits)
    lda score_hi
    sta tmp_hi
    lda score_lo
    sta tmp_lo
    
    ; Simple BCD conversion - just show hex for now
    lda score_hi
    lsr
    lsr
    lsr
    lsr
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 4
    
    lda score_hi
    and #$0F
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 4 + 1
    
    lda score_lo
    lsr
    lsr
    lsr
    lsr
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 4 + 2
    
    lda score_lo
    and #$0F
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 4 + 3
    
    ; Draw lines
    lda lines_cleared
    lsr
    lsr
    lsr
    lsr
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 8
    
    lda lines_cleared
    and #$0F
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 8 + 1
    
    ; Draw level
    lda level
    lsr
    lsr
    lsr
    lsr
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 12
    
    lda level
    and #$0F
    jsr digit_to_char
    sta SCREEN_RAM + 40 * 12 + 1
    
    rts

digit_to_char:
    cmp #10
    bcs hex_letter
    clc
    adc #48             ; '0' = 48
    rts
hex_letter:
    sec
    sbc #9              ; A=1, B=2, etc. in screen codes
    rts

; -----------------------------------------------------------------------------
; UTILITY ROUTINES
; -----------------------------------------------------------------------------
calc_screen_pos:
    ; Calculate screen address for (tmp_x, tmp_y)
    ; Also sets up color RAM pointer
    
    lda #0
    sta ptr_hi
    sta col_ptr_hi
    lda tmp_y
    sta ptr_lo
    
    ; * 2
    asl ptr_lo
    rol ptr_hi
    ; * 4
    asl ptr_lo
    rol ptr_hi
    ; * 8
    asl ptr_lo
    rol ptr_hi
    
    ; Save y*8
    lda ptr_lo
    sta tmp_lo
    lda ptr_hi
    sta col_ptr_lo      ; Reuse temporarily
    
    ; * 16
    asl ptr_lo
    rol ptr_hi
    ; * 32
    asl ptr_lo
    rol ptr_hi
    
    ; Add y*8 to y*32
    lda ptr_lo
    clc
    adc tmp_lo
    sta ptr_lo
    lda ptr_hi
    adc col_ptr_lo
    sta ptr_hi
    
    ; Copy for color RAM
    lda ptr_lo
    sta col_ptr_lo
    lda ptr_hi
    sta col_ptr_hi
    
    ; Add Screen Base ($0400)
    lda ptr_hi
    clc
    adc #>SCREEN_RAM
    sta ptr_hi
    
    ; Add Color RAM Base ($D800)
    lda col_ptr_hi
    clc
    adc #>COLOR_RAM
    sta col_ptr_hi
    
    ; Add X pos
    lda ptr_lo
    clc
    adc tmp_x
    sta ptr_lo
    sta col_ptr_lo
    bcc done_pos
    inc ptr_hi
    inc col_ptr_hi
done_pos:
    rts

wait_vblank:
    ; Wait for raster to reach bottom of screen (line 250)
    lda #250
wait_r1:
    cmp $D012
    bne wait_r1
    ; Wait for it to pass
wait_r2:
    cmp $D012
    beq wait_r2
    rts

wait_frame:
    ; Wait for raster line $FF (used for timing delays)
    lda #$FF
wait_rf1:
    cmp $D012
    bne wait_rf1
wait_rf2:
    cmp $D012
    beq wait_rf2
    rts

; -----------------------------------------------------------------------------
; AI DEMO MODE
; -----------------------------------------------------------------------------
ai_move:
    ; Improved AI: Analyze board and find best placement
    ; Strategy: 
    ; 1. Pick a target rotation and X when piece spawns (cur_y is low)
    ; 2. Move toward target
    ; 3. Try to fill gaps and avoid creating holes
    
    ; Decrement input delay for AI too
    lda input_delay
    beq ai_ready
    dec input_delay
    rts

ai_ready:
    ; If piece just spawned (y <= 1), calculate new target
    lda cur_y
    cmp #2
    bcs ai_has_target
    
    ; Calculate best target position
    jsr ai_calc_target

ai_has_target:
    ; First, try to reach target rotation
    lda cur_rot
    cmp ai_target_rot
    beq ai_move_x
    
    ; Try to rotate toward target
    lda cur_rot
    pha
    inc cur_rot
    lda cur_rot
    and #$03
    sta cur_rot
    jsr check_collision
    bcc ai_rotate_ok
    ; Rotation failed, restore
    pla
    sta cur_rot
    jmp ai_move_x
ai_rotate_ok:
    pla                 ; Discard old rotation
    lda #4
    sta input_delay
    rts

ai_move_x:
    ; Move toward target X
    lda cur_x
    cmp ai_target_x
    beq ai_done
    bcc ai_move_right
    
    ; Move left
    dec cur_x
    jsr check_collision
    bcc ai_left_ok
    inc cur_x           ; Restore if collision
ai_left_ok:
    lda #2
    sta input_delay
    rts

ai_move_right:
    inc cur_x
    jsr check_collision
    bcc ai_right_ok
    dec cur_x           ; Restore if collision
ai_right_ok:
    lda #2
    sta input_delay

ai_done:
    rts

; Calculate the best target position for the current piece
ai_calc_target:
    ; Simple but effective strategy:
    ; - For I piece (0): prefer horizontal, find gaps
    ; - For O piece (1): no rotation needed, fill from sides
    ; - For others: prefer filling left side first, then right
    
    lda cur_piece
    cmp #0              ; I piece
    beq ai_target_i
    cmp #1              ; O piece  
    beq ai_target_o
    jmp ai_target_other

ai_target_i:
    ; I piece: horizontal (rot 0 or 2), scan for best column
    lda #0
    sta ai_target_rot
    ; Find column with highest stack (to fill gaps)
    jsr ai_find_lowest_column
    sta ai_target_x
    rts

ai_target_o:
    ; O piece: no rotation matters, find lowest column
    lda #0
    sta ai_target_rot
    ; Find best column to place
    jsr ai_find_lowest_column
    sta ai_target_x
    rts

ai_target_other:
    ; T, S, Z, J, L pieces
    ; Pick rotation based on piece type
    lda cur_piece
    and #$01
    sta ai_target_rot   ; 0 or 1 rotation
    
    ; Find best column - prefer filling from left
    jsr ai_find_lowest_column
    sta ai_target_x
    rts

; Find column with lowest gap (where we should place piece)
; Returns column in A
; Strategy: Find the column with the LOWEST stack to level the playfield
ai_find_lowest_column:
    ; Scan columns from left to right
    ; Find the column with lowest stack (most empty space) to level the field
    
    lda #0
    sta tmp_lo          ; Best column so far
    lda #BOARD_HEIGHT
    sta tmp_hi          ; Best height so far (start with max)
    
    lda #0
    sta loop_i          ; Column counter
    
ai_scan_col:
    lda loop_i
    sta test_x
    
    ; Scan from top down to find first filled cell
    lda #0
    sta test_y
    
ai_scan_row:
    jsr get_board_cell
    cmp #0
    bne ai_found_block  ; Found a block
    inc test_y
    lda test_y
    cmp #BOARD_HEIGHT
    bne ai_scan_row
    
    ; Column is empty - height is BOARD_HEIGHT (best!)
    lda loop_i
    sta tmp_lo
    lda #BOARD_HEIGHT
    sta tmp_hi
    jmp ai_next_col

ai_found_block:
    ; Found block at test_y in column loop_i
    ; test_y = how much empty space at top (lower = taller stack)
    ; We want the column with MOST empty space (highest test_y)
    lda test_y
    cmp tmp_hi
    bcc ai_next_col     ; test_y < tmp_hi, not better (taller stack)
    beq ai_next_col     ; Equal, keep first one found
    
    ; This column has more empty space (shorter stack)
    sta tmp_hi
    lda loop_i
    sta tmp_lo

ai_next_col:
    inc loop_i
    lda loop_i
    cmp #BOARD_WIDTH
    bne ai_scan_col
    
    ; Return best column (with bounds check)
    lda tmp_lo
    cmp #BOARD_WIDTH - 2
    bcc ai_col_ok
    lda #BOARD_WIDTH - 3  ; Don't go too far right
ai_col_ok:
    rts

; -----------------------------------------------------------------------------
; DEMO TEXT DISPLAY
; -----------------------------------------------------------------------------
draw_demo_text:
    ; Draw "DEMO" at top
    ldx #0
demo_loop1:
    lda demo_text, x
    beq draw_press_fire
    sta SCREEN_RAM + 2, x
    lda #1              ; White
    sta COLOR_RAM + 2, x
    inx
    jmp demo_loop1

draw_press_fire:
    ; Draw "PRESS FIRE" at bottom
    ldx #0
demo_loop2:
    lda press_fire_text, x
    beq demo_done
    sta SCREEN_RAM + 24 * 40 + 1, x
    lda #7              ; Yellow
    sta COLOR_RAM + 24 * 40 + 1, x
    inx
    jmp demo_loop2
demo_done:
    rts

clear_demo_text:
    ; Clear "DEMO" text
    ldx #0
    lda #CHAR_SPACE
clear_demo_loop1:
    sta SCREEN_RAM + 2, x
    inx
    cpx #10
    bne clear_demo_loop1
    
    ; Clear "PRESS FIRE" text
    ldx #0
clear_demo_loop2:
    sta SCREEN_RAM + 24 * 40 + 1, x
    inx
    cpx #12
    bne clear_demo_loop2
    rts

; Text data (PETSCII screen codes)
demo_text:
    .byte 4, 5, 13, 15, 0       ; "DEMO"
press_fire_text:
    .byte 16, 18, 5, 19, 19, 32, 6, 9, 18, 5, 0  ; "PRESS FIRE"

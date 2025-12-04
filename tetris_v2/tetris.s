; -----------------------------------------------------------------------------
; TETRIS for C64 (cc65 Relocatable Version)
; -----------------------------------------------------------------------------

; --- Constants ---
SCREEN_RAM = $0400
JOYSTICK   = $DC00
BORDER     = $D020
BG_COLOR   = $D021

; Screen Codes
CHAR_SPACE = 32
CHAR_WALL  = 160
CHAR_BLOCK = 160 ; Solid block for pieces

; Board Dimensions
BOARD_X    = 15  ; Screen X offset
BOARD_Y    = 2   ; Screen Y offset
BOARD_W    = 10
BOARD_H    = 20

; --- Zero Page Segment ---
.segment "ZEROPAGE"
cur_x:      .res 1
cur_y:      .res 1
cur_piece:  .res 1 ; 0-6
cur_rot:    .res 1 ; 0-3
next_piece: .res 1 ; Next piece type
target_x:   .res 1 ; AI Target X
target_rot: .res 1 ; AI Target Rotation
ai_timer:   .res 1 ; AI Speed Control
frame_cnt:  .res 1
drop_timer: .res 1
input_delay:.res 1
score:      .res 2 ; 16-bit score
lines:      .res 1
ptr_lo:     .res 1
ptr_hi:     .res 1
tmp_x:      .res 1
tmp_y:      .res 1
tmp_val:    .res 1
tmp_lo:     .res 1
tmp_hi:     .res 1
row_idx:    .res 1
col_idx:    .res 1
is_valid:   .res 1
tick:       .res 1
redraw_flag:.res 1
tmp_char:   .res 1
tmp_color:  .res 1

; --- BSS Segment ---
.segment "BSS"
board:      .res 200 ; 10x20 board (0=empty, 1=filled)

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
    jsr init_screen
    jsr init_vars
    
    ; Init Random
    lda $D012
    sta tick
    
    ; Pick first next piece
    jsr pick_random_piece
    sta next_piece
    
    jsr spawn_piece

game_loop:
    jsr wait_frame
    jsr clear_piece
    
    ; jsr ai_update    ; AI Control
    jsr handle_input ; Human Control
    
    jsr update_game
    
    lda redraw_flag
    beq skip_redraw
    jsr draw_board
    lda #0
    sta redraw_flag
skip_redraw:

    jsr draw_piece
    jsr draw_ghost_piece
    jsr draw_next_piece
    jsr draw_status
    jmp game_loop

; --- Subroutines ---

ai_update:
    inc ai_timer
    lda ai_timer
    cmp #4 ; AI Speed
    bne ai_done
    
    lda #0
    sta ai_timer
    
    ; Rotate towards target
    lda cur_rot
    cmp target_rot
    beq ai_check_x
    jsr try_rotate
    jmp ai_done

ai_check_x:
    ; Move towards target X
    lda cur_x
    cmp target_x
    beq ai_do_drop
    bcc ai_go_right
    jsr try_left
    jmp ai_done
ai_go_right:
    jsr try_right
    jmp ai_done

ai_do_drop:
    ; If aligned, drop faster
    jsr try_drop
    
ai_done:
    rts

init_vars:
    lda #0
    sta score
    sta score+1
    sta lines
    sta frame_cnt
    sta drop_timer
    sta input_delay
    sta redraw_flag
    
    ; Clear board array
    ldx #0
    lda #0
clear_board_loop:
    sta board, x
    inx
    cpx #200
    bne clear_board_loop
    rts

init_screen:
    lda #0
    sta BORDER
    sta BG_COLOR
    
    ; Init Color RAM (White text)
    ldx #0
    lda #1
color_loop:
    sta $D800, x
    sta $D900, x
    sta $DA00, x
    sta $DB00, x
    dex
    bne color_loop
    
    ; Clear Screen
    ldx #0
    lda #CHAR_SPACE
scr_clr:
    sta SCREEN_RAM, x
    sta SCREEN_RAM+250, x
    sta SCREEN_RAM+500, x
    sta SCREEN_RAM+750, x
    dex
    bne scr_clr
    
    ; Draw Walls
    lda #12 ; Grey
    sta tmp_color

    ; Left Wall
    ldy #0
wall_loop:
    sty row_idx ; Save Y loop counter
    
    lda #CHAR_WALL
    
    ; Calculate screen pos for left wall (BOARD_X-1, BOARD_Y + y)
    ldx #BOARD_X
    dex
    stx tmp_x
    tya
    clc
    adc #BOARD_Y
    sta tmp_y
    lda #CHAR_WALL ; Reload char
    jsr plot_char
    
    ; Right Wall (BOARD_X + BOARD_W, BOARD_Y + y)
    ldy row_idx ; Restore Y for calculation
    ldx #BOARD_X
    txa
    clc
    adc #BOARD_W
    sta tmp_x
    tya
    clc
    adc #BOARD_Y
    sta tmp_y
    lda #CHAR_WALL ; Reload char
    jsr plot_char
    
    ldy row_idx ; Restore Y for loop
    iny
    cpy #BOARD_H
    bne wall_loop
    
    ; Bottom Wall
    ldx #0
bot_wall:
    stx tmp_val ; save x loop
    
    txa
    clc
    adc #BOARD_X
    sta tmp_x
    
    lda #BOARD_Y
    clc
    adc #BOARD_H
    sta tmp_y
    
    lda #CHAR_WALL ; Reload char
    jsr plot_char
    
    ldx tmp_val
    inx
    cpx #BOARD_W
    bne bot_wall
    
    ; Draw Status Text
    ldx #0
dtxt1:
    lda txt_score, x
    beq dtxt2
    sta SCREEN_RAM + 5*40 + 28, x
    inx
    jmp dtxt1
dtxt2:
    ldx #0
dtxt3:
    lda txt_lines, x
    beq dtxt_end
    sta SCREEN_RAM + 8*40 + 28, x
    inx
    jmp dtxt3
dtxt_end:
    rts

draw_status:
    ; Draw Score (Hex)
    lda score
    jsr print_hex_fixed
    sta SCREEN_RAM + 6*40 + 28
    txa
    sta SCREEN_RAM + 6*40 + 29
    
    ; Draw Lines (Hex)
    lda lines
    jsr print_hex_fixed
    sta SCREEN_RAM + 9*40 + 28
    txa
    sta SCREEN_RAM + 9*40 + 29
    rts

hex_digit:
    cmp #10
    bcs is_letter
    adc #48
    rts
is_letter:
    sbc #9
    rts

print_hex_fixed:
    pha
    lsr
    lsr
    lsr
    lsr
    jsr hex_digit
    sta tmp_val ; High Char
    pla
    and #$0F
    jsr hex_digit
    tax ; Low Char
    lda tmp_val ; High Char
    rts

spawn_piece:
    lda next_piece
    sta cur_piece
    
    jsr pick_random_piece
    sta next_piece
    
    lda #0
    sta cur_rot
    
    lda #4 ; Center X
    sta cur_x
    lda #0 ; Top Y
    sta cur_y
    
    ; AI: Pick Random Target X (0-6) and Rot (0-3)
    lda $D012
    adc tick
    and #$07
    cmp #7
    bcs ai_rnd_x
    sta target_x
ai_rnd_x:
    lda $D012
    and #$03
    sta target_rot
    
    ; Check game over
    jsr check_collision
    lda is_valid
    bne spawn_ok
    jmp game_over
spawn_ok:
    rts

pick_random_piece:
    lda $D012
    adc tick
    and #$07
    cmp #7
    bcs pick_random_piece
    rts

draw_next_piece:
    ; Draw "NEXT" text
    ldx #0
dn_txt:
    lda txt_next, x
    beq dn_draw
    sta SCREEN_RAM + 12*40 + 28, x
    inx
    jmp dn_txt
    
dn_draw:
    ; Clear Next Area (4x4)
    ; Row 14
    ldx #0
    lda #CHAR_SPACE
dn_clr_1:
    sta SCREEN_RAM + 588, x
    inx
    cpx #4
    bne dn_clr_1
    
    ; Row 15
    ldx #0
dn_clr_2:
    sta SCREEN_RAM + 628, x
    inx
    cpx #4
    bne dn_clr_2
    
    ; Row 16
    ldx #0
dn_clr_3:
    sta SCREEN_RAM + 668, x
    inx
    cpx #4
    bne dn_clr_3
    
    ; Row 17
    ldx #0
dn_clr_4:
    sta SCREEN_RAM + 708, x
    inx
    cpx #4
    bne dn_clr_4
    
    ; Draw Piece
    ; We need to use get_block_offset with next_piece and rot 0
    lda cur_piece
    pha
    lda cur_rot
    pha
    
    lda next_piece
    sta cur_piece
    lda #0
    sta cur_rot
    
    ; Set Color
    ldx next_piece
    lda PIECE_COLORS, x
    sta tmp_color
    
    ldx #0
dn_loop:
    stx tmp_val
    jsr get_block_offset
    
    ; Draw at 28, 14
    txa
    clc
    adc #28
    sta tmp_x
    
    tya
    clc
    adc #14
    sta tmp_y
    
    lda #160 ; Solid Block
    jsr plot_char
    
    ldx tmp_val
    inx
    cpx #4
    bne dn_loop
    
    pla
    sta cur_rot
    pla
    sta cur_piece
    rts

draw_ghost_piece:
    ; Save current Y
    lda cur_y
    pha
    
    ; Set Ghost Color (White)
    lda #1
    sta tmp_color
    
    ; Drop until collision
ghost_drop:
    inc cur_y
    jsr check_collision
    lda is_valid
    bne ghost_drop
    
    ; Move back up one
    dec cur_y
    
    ; Draw ghost
    lda #46 ; '.' char
    sta tmp_char
    lda #1 ; White
    sta tmp_color
    
    ldx #0
dg_loop:
    stx tmp_val
    jsr get_block_offset
    
    txa
    clc
    adc cur_x
    clc
    adc #BOARD_X
    sta tmp_x
    
    tya
    clc
    adc cur_y
    clc
    adc #BOARD_Y
    sta tmp_y
    
    lda tmp_char
    jsr plot_char
    
    ldx tmp_val
    inx
    cpx #4
    bne dg_loop
    
    ; Restore Y
    pla
    sta cur_y
    rts

game_over:
    inc BORDER
    jmp start

wait_frame:
    lda #$FF
w1: cmp $D012
    bne w1
w2: cmp $D012
    beq w2
    inc tick
    rts

handle_input:
    lda input_delay
    beq read_joy
    dec input_delay
    rts

read_joy:
    lda JOYSTICK
    and #$1F
    cmp #$1F
    beq no_input
    
    ; Reset delay
    lda #5
    sta input_delay
    
    ; Check Up (Rotate)
    lda JOYSTICK
    and #$01
    bne chk_left_joy
    jsr try_rotate
    rts

chk_left_joy:
    lda JOYSTICK
    and #$04
    bne chk_right_joy
    jsr try_left
    rts

chk_right_joy:
    lda JOYSTICK
    and #$08
    bne chk_down_joy
    jsr try_right
    rts

chk_down_joy:
    lda JOYSTICK
    and #$02
    bne no_input
    jsr try_drop
    rts

no_input:
    rts

update_game:
    inc drop_timer
    lda drop_timer
    cmp #30 ; Drop speed
    bne no_drop
    
    lda #0
    sta drop_timer
    jsr try_drop
    
no_drop:
    rts

try_rotate:
    lda cur_rot
    pha ; Save old rot
    
    clc
    adc #1
    and #3
    sta cur_rot
    
    jsr check_collision
    lda is_valid
    bne rot_ok
    
    pla ; Restore old rot
    sta cur_rot
    rts
rot_ok:
    pla ; Discard saved
    rts

try_left:
    dec cur_x
    jsr check_collision
    lda is_valid
    bne left_ok
    inc cur_x
left_ok:
    rts

try_right:
    inc cur_x
    jsr check_collision
    lda is_valid
    bne right_ok
    dec cur_x
right_ok:
    rts

try_drop:
    inc cur_y
    jsr check_collision
    lda is_valid
    bne drop_ok
    
    dec cur_y
    jsr lock_piece
    jsr check_lines
    jsr spawn_piece
drop_ok:
    rts

check_collision:
    lda #1
    sta is_valid
    
    ; Iterate 4 blocks of current piece
    ldx #0
col_loop:
    stx tmp_val ; Save block index
    
    jsr get_block_offset ; Returns X in X, Y in Y (relative)
    
    ; Add global pos
    txa
    clc
    adc cur_x
    tax ; Abs X
    
    tya
    clc
    adc cur_y
    tay ; Abs Y
    
    ; Check bounds
    cpx #0
    bmi col_fail
    cpx #BOARD_W
    bpl col_fail
    cpy #BOARD_H
    bpl col_fail
    
    ; Check board array
    ; Index = y * 10 + x
    tya
    asl
    sta ptr_lo ; 2y
    asl
    asl ; 8y
    clc
    adc ptr_lo ; 10y
    stx ptr_lo ; +x (temp use of ptr_lo)
    adc ptr_lo ; 10y + x
    tay
    
    lda board, y
    bne col_fail
    
    ldx tmp_val
    inx
    cpx #4
    bne col_loop
    rts

col_fail:
    lda #0
    sta is_valid
    rts

lock_piece:
    ldx #0
lock_loop:
    stx tmp_val
    jsr get_block_offset
    
    txa
    clc
    adc cur_x
    tax
    
    tya
    clc
    adc cur_y
    tay
    
    ; Index = y * 10 + x
    tya
    asl
    sta ptr_lo
    asl
    asl
    clc
    adc ptr_lo
    stx ptr_lo
    adc ptr_lo
    tay
    
    ; Store piece type + 1 (so 0 is still empty)
    lda cur_piece
    clc
    adc #1
    sta board, y
    
    ldx tmp_val
    inx
    cpx #4
    bne lock_loop
    
    lda #1
    sta redraw_flag
    rts

check_lines:
    ; Scan from bottom up
    ldy #19 ; Row 19
line_loop:
    sty row_idx
    
    ; Check if row is full
    ldx #0
check_row:
    stx col_idx
    
    ; Calc index
    tya
    asl
    sta ptr_lo
    asl
    asl
    clc
    adc ptr_lo
    adc col_idx
    tax
    
    lda board, x
    beq line_not_full
    
    ldx col_idx
    inx
    cpx #10
    bne check_row
    
    ; Line is full!
    jsr remove_line
    ; Don't decrement Y, check this row again (it's now the one from above)
    jmp line_loop

line_not_full:
    ldy row_idx
    dey
    bpl line_loop
    rts

remove_line:
    inc score
    
    ; Move everything down from row_idx-1
    ldy row_idx
move_down_loop:
    dey
    bmi clear_top_line ; If we went past 0
    
    ; Copy row Y to Y+1
    ldx #0
copy_row:
    stx col_idx
    
    ; Src Index (Y)
    tya
    asl
    sta ptr_lo
    asl
    asl
    clc
    adc ptr_lo
    adc col_idx
    tax
    lda board, x
    sta tmp_val ; Save src pixel
    
    ; Dst Index (Y+1)
    tya
    clc
    adc #1
    asl
    sta ptr_lo
    asl
    asl
    clc
    adc ptr_lo
    adc col_idx
    tax
    
    lda tmp_val
    sta board, x
    
    ldx col_idx
    inx
    cpx #10
    bne copy_row
    
    jmp move_down_loop

clear_top_line:
    ; Clear row 0
    ldx #0
    lda #0
clr_top:
    sta board, x
    inx
    cpx #10
    bne clr_top
    rts

draw_board:
    ldy #0
draw_y:
    sty row_idx
    ldx #0
draw_x:
    stx col_idx
    
    ; Get board val
    tya
    asl
    sta ptr_lo
    asl
    asl
    clc
    adc ptr_lo
    adc col_idx
    tax
    lda board, x
    beq draw_empty
    
    ; It's a block!
    ; A = piece_type + 1
    sec
    sbc #1
    tax
    lda PIECE_COLORS, x
    sta tmp_color
    
    lda #160 ; Solid Block
    jmp do_plot
draw_empty:
    lda #CHAR_SPACE
    ; sta tmp_color ; Don't set color for space, it's irrelevant
do_plot:
    pha ; Save char
    
    ; Calc screen pos
    lda col_idx
    clc
    adc #BOARD_X
    sta tmp_x
    
    lda row_idx
    clc
    adc #BOARD_Y
    sta tmp_y
    
    pla ; Restore char
    jsr plot_char
    
    ldx col_idx
    inx
    cpx #10
    bne draw_x
    
    ldy row_idx
    iny
    cpy #20
    bne draw_y
    rts

draw_piece:
    ldx cur_piece
    lda PIECE_COLORS, x
    sta tmp_color
    lda #160 ; Solid Block
    jmp draw_piece_common

clear_piece:
    lda #CHAR_SPACE
    ; Fall through

draw_piece_common:
    sta tmp_char
    ldx #0
dp_loop:
    stx tmp_val
    jsr get_block_offset
    
    txa
    clc
    adc cur_x
    clc
    adc #BOARD_X
    sta tmp_x
    
    tya
    clc
    adc cur_y
    clc
    adc #BOARD_Y
    sta tmp_y
    
    lda tmp_char
    jsr plot_char
    
    ldx tmp_val
    inx
    cpx #4
    bne dp_loop
    rts

plot_char:
    ; Plot A at tmp_x, tmp_y with color tmp_color
    pha ; Save char
    
    lda #0
    sta ptr_hi
    lda tmp_y
    sta ptr_lo
    
    ; * 40
    asl ptr_lo
    rol ptr_hi ; *2
    asl ptr_lo
    rol ptr_hi ; *4
    
    lda ptr_lo
    sta tmp_lo ; Save *4
    lda ptr_hi
    sta tmp_hi
    
    asl ptr_lo
    rol ptr_hi ; *8
    
    lda ptr_lo
    clc
    adc tmp_lo
    sta ptr_lo
    lda ptr_hi
    adc tmp_hi
    sta ptr_hi ; *10? No wait.
    
    ; 40 = 32 + 8
    ; Start over
    lda #0
    sta ptr_hi
    lda tmp_y
    sta ptr_lo
    
    asl ptr_lo
    rol ptr_hi ; *2
    asl ptr_lo
    rol ptr_hi ; *4
    asl ptr_lo
    rol ptr_hi ; *8
    
    lda ptr_lo
    sta tmp_lo
    lda ptr_hi
    sta tmp_hi ; Save *8
    
    asl ptr_lo
    rol ptr_hi ; *16
    asl ptr_lo
    rol ptr_hi ; *32
    
    lda ptr_lo
    clc
    adc tmp_lo
    sta ptr_lo
    lda ptr_hi
    adc tmp_hi
    sta ptr_hi ; *40
    
    ; Add Base
    lda ptr_hi
    clc
    adc #>SCREEN_RAM
    sta ptr_hi
    
    lda ptr_lo
    clc
    adc tmp_x
    sta ptr_lo
    bcc pc_ok
    inc ptr_hi
pc_ok:
    ; Write Char
    pla
    ldy #0
    sta (ptr_lo), y
    
    ; Write Color
    ; Color RAM is at Screen RAM + $D400
    ; But we need to be careful about carry if we just add $D4 to ptr_hi
    ; ptr_hi is $04-$07. $04+$D4 = $D8. $07+$D4 = $DB.
    ; No carry issues for this range.
    lda ptr_hi
    clc
    adc #$D4
    sta ptr_hi
    
    lda tmp_color
    sta (ptr_lo), y
    
    rts

get_block_offset:
    ; Input: cur_piece, cur_rot, X (block index 0-3)
    ; Output: X (x offset), Y (y offset)
    
    ; Offset = (piece * 16) + (rot * 4) + index
    lda cur_piece
    asl
    asl
    asl
    asl ; *16
    sta tmp_lo
    
    lda cur_rot
    asl
    asl ; *4
    clc
    adc tmp_lo
    stx tmp_lo ; temp save
    adc tmp_lo ; add index (X)
    tax
    
    lda PIECES_X, x
    tay ; Save X to Y temporarily
    lda PIECES_Y, x
    pha ; Save Y
    tya
    tax ; Restore X
    pla
    tay ; Restore Y
    rts

; --- Data ---
.segment "RODATA"

txt_score: .byte 19, 3, 15, 18, 5, 0 ; SCORE
txt_lines: .byte 12, 9, 14, 5, 19, 0 ; LINES
txt_next:  .byte 14, 5, 24, 20, 0 ; NEXT (N=14, E=5, X=24, T=20)

; Piece Colors (Standard Tetris Colors)
; 0: I - Cyan (3)
; 1: J - Blue (6)
; 2: L - Orange (8 - using Orange/Brown)
; 3: O - Yellow (7)
; 4: S - Green (5)
; 5: T - Purple (4)
; 6: Z - Red (2)
PIECE_COLORS:
    .byte 3, 6, 8, 7, 5, 4, 2

; Piece Definitions (4 blocks per piece, relative X,Y)
; 7 Pieces, 4 Rotations, 4 Blocks per rotation
; Format: X, Y relative to pivot

PIECES_X:
    ; I Piece
    .byte 0, 1, 2, 3,  0, 0, 0, 0,  0, 1, 2, 3,  0, 0, 0, 0 ; Rot 0-3
    ; J Piece
    .byte 0, 0, 1, 2,  1, 2, 1, 1,  0, 1, 2, 2,  1, 1, 0, 1
    ; L Piece
    .byte 2, 0, 1, 2,  1, 1, 1, 2,  0, 1, 2, 0,  0, 1, 1, 1
    ; O Piece (No rotation change really)
    .byte 1, 2, 1, 2,  1, 2, 1, 2,  1, 2, 1, 2,  1, 2, 1, 2
    ; S Piece
    .byte 1, 2, 0, 1,  1, 1, 2, 2,  1, 2, 0, 1,  1, 1, 2, 2
    ; T Piece
    .byte 1, 0, 1, 2,  1, 1, 2, 1,  0, 1, 2, 1,  1, 0, 1, 1
    ; Z Piece
    .byte 0, 1, 1, 2,  2, 1, 2, 1,  0, 1, 1, 2,  2, 1, 2, 1

PIECES_Y:
    ; I Piece
    .byte 1, 1, 1, 1,  0, 1, 2, 3,  1, 1, 1, 1,  0, 1, 2, 3
    ; J Piece
    .byte 0, 1, 1, 1,  0, 0, 1, 2,  1, 1, 1, 2,  0, 1, 2, 2
    ; L Piece
    .byte 0, 1, 1, 1,  0, 1, 2, 2,  1, 1, 1, 2,  0, 0, 1, 2
    ; O Piece
    .byte 0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1
    ; S Piece
    .byte 0, 0, 1, 1,  0, 1, 1, 2,  0, 0, 1, 1,  0, 1, 1, 2
    ; T Piece
    .byte 0, 1, 1, 1,  0, 1, 1, 2,  1, 1, 1, 2,  0, 1, 1, 2
    ; Z Piece
    .byte 0, 0, 1, 1,  0, 1, 1, 2,  0, 0, 1, 1,  0, 1, 1, 2


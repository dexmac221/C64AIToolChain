; -----------------------------------------------------------------------------
; TETRIS for C64 (cc65 Relocatable Version)
; -----------------------------------------------------------------------------

; --- Constants ---
SCREEN_RAM = $0400
JOYSTICK   = $DC00
BORDER     = $D020
BG_COLOR   = $D021

CHAR_SPACE = 32    
CHAR_WALL  = 160   
CHAR_BLOCK = 81    ; Ball symbol for now

; Board Dimensions
BOARD_X_START = 15
BOARD_X_END   = 26
BOARD_Y_START = 2
BOARD_Y_END   = 23

; --- Zero Page Segment ---
.segment "ZEROPAGE"
ptr_lo:     .res 1
ptr_hi:     .res 1
tmp_x:      .res 1
tmp_y:      .res 1
cur_x:      .res 1
cur_y:      .res 1
frame_cnt:  .res 1
input_cnt:  .res 1
tmp_lo:     .res 1
tmp_hi:     .res 1
shape_idx:  .res 1
loop_idx:   .res 1
shape_ptr:  .res 1 ; Index into shape table (shape_idx * 4)

; --- BASIC Header ---
.segment "HEADER"
    .word $0801       
    .word next_line   
    .word 10          
    .byte $9E         
    .byte "2061", 0   ; Address of 'start'
next_line:
    .word 0           

; --- Main Code ---
.segment "CODE"
start:
    sei         
    cld         
    jsr init_screen
    lda #0
    sta shape_idx
    jsr spawn_block
    
game_loop:
    jsr wait_frame
    
    jsr clear_block
    
    ; Try moving down
    inc cur_y
    jsr check_collision
    bcs collision_down
    
    ; No collision, draw and continue
    jsr draw_block
    jmp game_loop

collision_down:
    ; Restore Y
    dec cur_y
    jsr draw_block
    
lock_block:
    ; Block reached bottom, leave it there and spawn new one
    jsr spawn_block
    jmp game_loop

; --- Subroutines ---

check_collision:
    ; Returns Carry Set if collision
    lda #0
    sta loop_idx
    
coll_loop:
    ldx loop_idx
    
    ; Calc X = cur_x + shape_x[shape_ptr + loop_idx]
    lda shape_ptr
    clc
    adc loop_idx
    tay
    
    lda cur_x
    clc
    adc SHAPES_X, y
    sta tmp_x
    
    ; Calc Y = cur_y + shape_y[shape_ptr + loop_idx]
    lda cur_y
    clc
    adc SHAPES_Y, y
    sta tmp_y
    
    jsr calc_screen_pos
    
    ldy #0
    lda (ptr_lo), y
    cmp #CHAR_SPACE
    bne coll_found
    
    inc loop_idx
    lda loop_idx
    cmp #4
    bne coll_loop
    
    clc ; No collision
    rts

coll_found:
    sec ; Collision
    rts

wait_frame:
    ; Wait for raster line $FF
    lda #$FF
wait_r1:
    cmp $D012
    bne wait_r1
wait_r2:
    cmp $D012
    beq wait_r2
    
    inc frame_cnt
    lda frame_cnt
    cmp #10      ; Slow fall speed
    bne wait_frame
    
    lda #0
    sta frame_cnt
    rts

spawn_block:
    lda #20      ; Center X
    sta cur_x
    lda #BOARD_Y_START + 1
    sta cur_y
    
    ; Cycle shape
    inc shape_idx
    lda shape_idx
    cmp #7 ; 7 shapes
    bne calc_ptr
    lda #0
    sta shape_idx
calc_ptr:
    ; shape_ptr = shape_idx * 4
    lda shape_idx
    asl
    asl
    sta shape_ptr
    rts

draw_block:
    lda #CHAR_BLOCK
    sta tmp_lo ; Store char to draw
    jmp common_draw

clear_block:
    lda #CHAR_SPACE
    sta tmp_lo
    
common_draw:
    lda #0
    sta loop_idx
    
draw_loop:
    ; Calc X
    lda shape_ptr
    clc
    adc loop_idx
    tay
    
    lda cur_x
    clc
    adc SHAPES_X, y
    sta tmp_x
    
    ; Calc Y
    lda cur_y
    clc
    adc SHAPES_Y, y
    sta tmp_y
    
    jsr calc_screen_pos
    
    lda tmp_lo
    ldy #0
    sta (ptr_lo), y
    
    inc loop_idx
    lda loop_idx
    cmp #4
    bne draw_loop
    rts

SHAPES_X:
    .byte 0, 1, 0, 1     ; O
    .byte 255, 0, 1, 2   ; I
    .byte 255, 0, 1, 0   ; T
    .byte 0, 1, 255, 0   ; S
    .byte 255, 0, 0, 1   ; Z
    .byte 255, 0, 1, 1   ; J
    .byte 255, 0, 1, 255 ; L

SHAPES_Y:
    .byte 0, 0, 1, 1     ; O
    .byte 0, 0, 0, 0     ; I
    .byte 0, 0, 0, 1     ; T
    .byte 0, 0, 1, 1     ; S
    .byte 0, 0, 1, 1     ; Z
    .byte 0, 0, 0, 1     ; J
    .byte 0, 0, 0, 1     ; L

init_screen:
    lda #0
    sta BORDER
    sta BG_COLOR
    
    ; Clear Screen
    ldx #0
    lda #CHAR_SPACE
clear_loop:
    sta SCREEN_RAM, x
    sta SCREEN_RAM + 250, x
    sta SCREEN_RAM + 500, x
    sta SCREEN_RAM + 750, x
    dex
    bne clear_loop
    
    ; Draw Board Borders
    ; Top and Bottom
    ldx #BOARD_X_START
draw_tb:
    lda #CHAR_WALL
    
    ; Calculate Top Row Address
    stx tmp_x
    lda #BOARD_Y_START
    sta tmp_y
    jsr calc_screen_pos
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y
    
    ; Calculate Bottom Row Address
    lda #BOARD_Y_END
    sta tmp_y
    jsr calc_screen_pos
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y
    
    ldx tmp_x
    inx
    cpx #BOARD_X_END + 1
    bne draw_tb

    ; Side Walls
    ldx #BOARD_Y_START
draw_sides:
    stx tmp_y
    
    ; Left Wall
    lda #BOARD_X_START
    sta tmp_x
    jsr calc_screen_pos
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y
    
    ; Right Wall
    lda #BOARD_X_END
    sta tmp_x
    jsr calc_screen_pos
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y
    
    ldx tmp_y
    inx
    cpx #BOARD_Y_END
    bne draw_sides
    
    rts

calc_screen_pos:
    ; Calculate address for (tmp_x, tmp_y)
    ; Addr = SCREEN_RAM + y*40 + x
    
    lda #0
    sta ptr_hi
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
    sta tmp_hi
    
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
    adc tmp_hi
    sta ptr_hi
    
    ; Add Screen Base
    lda ptr_hi
    clc
    adc #>SCREEN_RAM
    sta ptr_hi
    
    ; Add X
    lda ptr_lo
    clc
    adc tmp_x
    sta ptr_lo
    bcc done_calc
    inc ptr_hi
done_calc:
    rts

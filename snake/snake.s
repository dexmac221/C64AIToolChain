; -----------------------------------------------------------------------------
; SNAKE for C64 (cc65 Relocatable Version)
; -----------------------------------------------------------------------------

; --- Constants ---
SCREEN_RAM = $0400
JOYSTICK   = $DC00
BORDER     = $D020
BG_COLOR   = $D021

CHAR_SNAKE = 81    
CHAR_SPACE = 32    
CHAR_WALL  = 160   
CHAR_APPLE = 83    ; Heart symbol
CHAR_TREE  = 65    ; Spade symbol (Grove)

; --- Zero Page Segment ---
; The linker places these in addresses $0002+ automatically
.segment "ZEROPAGE"
head_x:     .res 1
head_y:     .res 1
dir:        .res 1   ; 0=N, 1=S, 2=W, 3=E
tail_idx:   .res 1
head_idx:   .res 1
ptr_lo:     .res 1   ; Pointer for screen calc
ptr_hi:     .res 1
frame_cnt:  .res 1   ; Frame counter for speed control
sim_joy:    .res 1   ; Simulated joystick input (for AI control)
apple_x:    .res 1
apple_y:    .res 1
score:      .res 1
grow:       .res 1   ; Flag to grow snake
tick:       .res 1   ; Free running counter for RNG
tmp_lo:     .res 1   ; Temp for math
tmp_hi:     .res 1

; --- BSS Segment (Uninitialized Data) ---
; The linker places these in RAM after the code
.segment "BSS"
tail_x_buf: .res 256
tail_y_buf: .res 256

; --- BASIC Header ---
.segment "HEADER"
    .word $0801       ; Load address (first 2 bytes of PRG file)
    .word next_line   ; Pointer to next line
    .word 10          ; Line number
    .byte $9E         ; SYS token
    .byte "2061", 0   ; "2061" (Address of 'start' label below)
next_line:
    .word 0           ; End of BASIC program

; --- Main Code ---
.segment "CODE"
start:
    sei         ; Disable interrupts to stop KERNAL cursor/keys
    cld         ; Clear decimal mode (just in case)
    jsr init_screen
    jsr init_vars

game_loop:
    jsr wait_frame
    jsr ai_move      ; Demo Mode: AI controls snake
    ; jsr read_joy   ; Human control
    jsr move_snake
    jsr check_collision
    jsr update_screen
    jmp game_loop

; --- Subroutines ---

ai_move:
    ; Simple Apple Convergent Algorithm
    ; Try to minimize distance to apple
    
    ; Try X axis
    lda head_x
    cmp apple_x
    beq ai_try_y
    bcc ai_want_right ; head < apple
    
    ; ai_want_left
    lda dir
    cmp #3 ; If East, can't go West
    beq ai_try_y
    lda #2 ; West
    sta dir
    rts
    
ai_want_right:
    lda dir
    cmp #2 ; If West, can't go East
    beq ai_try_y
    lda #3 ; East
    sta dir
    rts

ai_try_y:
    lda head_y
    cmp apple_y
    beq ai_keep_going
    bcc ai_want_down ; head < apple
    
    ; ai_want_up
    lda dir
    cmp #1 ; If South, can't go North
    beq ai_avoid_y_lock
    lda #0 ; North
    sta dir
    rts
    
ai_want_down:
    lda dir
    cmp #0 ; If North, can't go South
    beq ai_avoid_y_lock
    lda #1 ; South
    sta dir
    rts

ai_avoid_y_lock:
    ; We want to go Y but are locked in opposite direction.
    ; Force a turn to X axis to break the lock.
    ; Check if we can go East (3)
    lda dir
    cmp #2 ; If West, can't go East
    beq ai_force_west
    lda #3
    sta dir
    rts
ai_force_west:
    lda #2
    sta dir
    rts

ai_keep_going:
    rts

init_screen:
    lda #0
    sta BORDER
    sta BG_COLOR
    
    ldx #0
    lda #CHAR_SPACE
clear_loop:
    sta SCREEN_RAM, x
    sta SCREEN_RAM + 250, x
    sta SCREEN_RAM + 500, x
    sta SCREEN_RAM + 750, x
    dex
    bne clear_loop
    
    ; Draw Top and Bottom Walls
    ldx #39
draw_tb_walls:
    lda #CHAR_WALL
    sta SCREEN_RAM, x           
    sta SCREEN_RAM + 960, x     
    dex
    bpl draw_tb_walls
    
    ; Draw Side Walls
    lda #<SCREEN_RAM
    sta ptr_lo
    lda #>SCREEN_RAM
    sta ptr_hi
    ldx #25 ; 25 Rows
draw_side_loop:
    lda #CHAR_WALL
    ldy #0
    sta (ptr_lo), y ; Left side
    ldy #39
    sta (ptr_lo), y ; Right side
    
    ; Advance pointer by 40
    lda ptr_lo
    clc
    adc #40
    sta ptr_lo
    bcc skip_inc_hi_init_screen_2
    inc ptr_hi
skip_inc_hi_init_screen_2:
    dex
    bne draw_side_loop
    
    rts

init_vars:
    lda #20
    sta head_x
    lda #12
    sta head_y
    lda #3      
    sta dir
    lda #0
    sta tail_idx
    sta frame_cnt
    sta score
    sta grow
    lda #$1F    ; Init simulated joystick to "no input" (all bits high)
    sta sim_joy
    lda #4      
    sta head_idx

    ; Initialize snake body in buffer to avoid clearing garbage (and potentially code!)
    ldx #0
init_body_loop:
    lda #20
    sta tail_x_buf, x
    lda #12
    sta tail_y_buf, x
    inx
    cpx #4
    bne init_body_loop
    
    jsr spawn_obstacles
    jsr spawn_apple
    rts

wait_frame:
    ; Wait for raster line $FF to stabilize speed
    lda #$FF
wait_raster_1:
    cmp $D012
    bne wait_raster_1
    
    ; Wait for raster to move PAST line $FF
wait_raster_2:
    cmp $D012
    beq wait_raster_2
    
    inc tick ; Increment entropy counter every frame
    
    ; Speed control: only return every 5th frame
    inc frame_cnt
    lda frame_cnt
    cmp #5
    bne wait_frame ; If not 5, wait for next frame
    
    lda #0
    sta frame_cnt
    rts

read_joy:
    lda JOYSTICK ; Real hardware
    ; lda sim_joy    ; AI control
    and #$1F    
    
    lsr         ; Bit 0 (Up)
    bcs check_down
    lda #0
    sta dir
    rts
check_down:
    lsr         ; Bit 1 (Down)
    bcs check_left
    lda #1
    sta dir
    rts
check_left:
    lsr         ; Bit 2 (Left)
    bcs check_right
    lda #2
    sta dir
    rts
check_right:
    lsr         ; Bit 3 (Right)
    bcs no_input
    lda #3
    sta dir
no_input:
    rts

move_snake:
    lda dir
    cmp #0
    beq move_up
    cmp #1
    beq move_down
    cmp #2
    beq move_left
    cmp #3
    beq move_right
    rts

move_up:
    dec head_y
    rts
move_down:
    inc head_y
    rts
move_left:
    dec head_x
    rts
move_right:
    inc head_x
    rts

check_collision:
    ; Calculate pointer to new head position
    jsr calc_screen_pos
    
    ldy #0
    lda (ptr_lo), y
    
    cmp #CHAR_APPLE
    beq eat_apple
    
    cmp #CHAR_SPACE
    bne game_over
    
    rts

eat_apple:
    inc score
    inc grow
    inc BORDER ; Flash border
    jsr spawn_apple
    dec BORDER
    rts

game_over:
    inc BORDER
    jmp start

update_screen:
    ldx head_idx
    lda head_x
    sta tail_x_buf, x
    lda head_y
    sta tail_y_buf, x
    
    jsr calc_screen_pos
    ldy #0
    lda #CHAR_SNAKE
    sta (ptr_lo), y
    
    ; Check if growing
    lda grow
    beq do_tail
    dec grow
    jmp skip_tail ; Don't clear tail, don't inc tail_idx -> snake grows
    
do_tail:
    ldx tail_idx
    lda tail_x_buf, x
    sta head_x     
    lda tail_y_buf, x
    sta head_y
    
    jsr calc_screen_pos
    ldy #0
    lda #CHAR_SPACE
    sta (ptr_lo), y
    
    inc tail_idx

skip_tail:
    inc head_idx
    
    ; Restore actual head pos
    ldx head_idx
    dex
    lda tail_x_buf, x
    sta head_x
    lda tail_y_buf, x
    sta head_y
    
    ; Draw Score (simple hex at center top)
    lda score
    and #$0F ; Lower nibble
    cmp #10
    bcs hex_digit_1
    adc #48 ; 0-9
    jmp store_digit_1
hex_digit_1:
    sbc #9 ; 10-15 -> 1-6 (A-F)
store_digit_1:
    sta SCREEN_RAM + 20
    
    lda score
    lsr
    lsr
    lsr
    lsr
    cmp #10
    bcs hex_digit_2
    adc #48
    jmp store_digit_2
hex_digit_2:
    sbc #9
store_digit_2:
    sta SCREEN_RAM + 19
    
    rts

calc_screen_pos:
    ; Calculate y * 40 = y * 32 + y * 8
    lda #0
    sta ptr_hi
    lda head_y
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
    
    ; Add X pos
    lda ptr_lo
    clc
    adc head_x
    sta ptr_lo
    bcc done_calc
    inc ptr_hi
done_calc:
    rts

spawn_apple:
    ; Improved random using CIA Timer A (1MHz)
    lda $DC04   ; CIA1 Timer A Lo (changes very fast)
    eor tick    ; Mix with frame counter
    adc $D012   ; Mix with raster
    and #$1F    ; 0-31
    cmp #25
    bcs spawn_apple ; Retry if >= 25
    sta apple_y
    
    lda $DC04
    adc tick
    eor $D012
    ror
    and #$3F ; 0-63
    cmp #40
    bcs spawn_apple ; Retry if >= 40
    sta apple_x
    
    ; Save head pos
    lda head_x
    pha
    lda head_y
    pha
    
    lda apple_x
    sta head_x
    lda apple_y
    sta head_y
    
    jsr calc_screen_pos
    ldy #0
    lda (ptr_lo), y
    cmp #CHAR_SPACE
    bne spawn_apple_restore ; If not space, retry
    
    lda #CHAR_APPLE
    sta (ptr_lo), y
    
    pla
    sta head_y
    pla
    sta head_x
    rts

spawn_apple_restore:
    pla
    sta head_y
    pla
    sta head_x
    jmp spawn_apple

spawn_obstacles:
    ldx #15 ; Number of obstacles
obs_loop:
    ; Random Y
    lda $DC04
    eor tick
    adc $D012
    and #$1F
    cmp #25
    bcs obs_loop
    sta tmp_lo ; Y

    ; Random X
    lda $DC04
    adc tick
    eor $D012
    ror
    and #$3F
    cmp #40
    bcs obs_loop
    sta tmp_hi ; X
    
    ; Save current head
    lda head_x
    pha
    lda head_y
    pha
    
    ; Set head to random pos for calc
    lda tmp_hi
    sta head_x
    lda tmp_lo
    sta head_y
    
    jsr calc_screen_pos
    
    ldy #0
    lda (ptr_lo), y
    cmp #CHAR_SPACE
    bne obs_skip
    
    ; Don't spawn too close to center (start pos 20,12)
    ; Simple check: if x is between 15 and 25 AND y is between 10 and 14
    lda head_x
    cmp #15
    bcc place_obs
    cmp #25
    bcs place_obs
    lda head_y
    cmp #10
    bcc place_obs
    cmp #14
    bcs place_obs
    jmp obs_skip ; Skip if in center safe zone

place_obs:
    lda #CHAR_TREE
    sta (ptr_lo), y
    
obs_skip:
    pla
    sta head_y
    pla
    sta head_x
    
    dex
    bne obs_loop
    rts

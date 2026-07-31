; irq.s - raster IRQ engine + sprite multiplexer for GHOST KEEP
;
; Three kinds of interrupt per frame:
;   irq_top   (line 40)  apply the buffer flip ($D018) latched together
;                        with the fine scroll, lock the HUD rows steady,
;                        park the multiplexed sprites off screen, bump
;                        the frame flag. Chains into the full KERNAL IRQ
;                        once per frame so jiffy/keyboard stay alive.
;   irq_split (line 73)  apply the playfield fine scroll in 38-column
;                        multicolor mode, then hand over to the mux.
;   irq_mux   (variable) reprogram one hardware sprite as the beam comes
;                        down, then schedule itself for the next entry.
;                        Hardware sprites 3-7 are recycled this way, so
;                        far more enemies can be on screen than the VIC
;                        has sprites. Sprites 0-2 (ship, bolt, orb) are
;                        never multiplexed.
;
; The mux tables are filled by C, sorted by Y, and terminated with $FF.
;
; C interface:
;   void irq_init(void);
;   extern unsigned char fine_next, d018_next, vsync_flag;
;   extern unsigned char mux_y[], mux_xlo[], mux_slot[], mux_slot2[],
;                        mux_ptr[], mux_col[], mux_xand[], mux_xor[];

.export _irq_init
.export _fine_next, _d018_next, _vsync_flag
.export _mux_y, _mux_xlo, _mux_slot, _mux_slot2
.export _mux_ptr, _mux_col, _mux_xand, _mux_xor, _mux_count

LINE_TOP   = 40
LINE_SPLIT = 65                 ; raster line after the 2 HUD rows
MUX_MAX    = 17                 ; 16 virtual sprites + terminator
SCR_A_PTR  = $47F8              ; sprite pointers, screen A
SCR_B_PTR  = $4BF8              ; sprite pointers, screen B

; The main loop runs early in the frame (before LINE_SPLIT), so a fine
; scroll value written there would show THIS frame while the $D018
; buffer flip it belongs with is latched one frame later - a 7px
; back-jump once per coarse step. Both are latched together at the top.
.data
_fine_next:  .byte 7            ; written by C, latched at frame top
disp_fine:   .byte 7            ; value actually displayed this frame
_d018_next:  .byte $14          ; screen $4400, charset $5000
_vsync_flag: .byte 0
_mux_count:  .byte 0
mux_idx:     .byte 0

.bss
_mux_y:     .res MUX_MAX        ; sorted ascending, $FF terminates
_mux_xlo:   .res MUX_MAX
_mux_slot:  .res MUX_MAX        ; hardware sprite 3..7
_mux_slot2: .res MUX_MAX        ; the same doubled, for $D000,x
_mux_ptr:   .res MUX_MAX        ; sprite pointer value
_mux_col:   .res MUX_MAX
_mux_xand:  .res MUX_MAX        ; $D010 mask: clear this sprite's bit
_mux_xor:   .res MUX_MAX        ; $D010 mask: set it when x > 255

.code

_irq_init:
        sei
        lda #$7f
        sta $dc0d               ; disable CIA1 timer IRQ
        lda $dc0d               ; ack pending
        lda #<irq_top
        sta $0314
        lda #>irq_top
        sta $0315
        lda #LINE_TOP
        sta $d012
        lda $d011
        and #$7f                ; raster high bit = 0
        sta $d011
        lda #$01
        sta $d01a               ; enable raster IRQ
        lda #$ff
        sta $d019               ; ack anything pending
        cli
        rts

irq_top:
        lda #$01
        sta $d019               ; ack raster IRQ
        lda _d018_next
        sta $d018               ; double buffer flip at frame top
        lda _fine_next
        sta disp_fine           ; latch fine scroll with the flip
        lda #$18                ; HUD: MCM on, 40 col, fine scroll 0
        sta $d016
        ; park every multiplexed sprite below the visible area: a slot
        ; the mux never reaches this frame must not show last frame's
        ; ghost at last frame's position
        lda #$ff
        sta $d007               ; sprite 3 Y
        sta $d009               ; sprite 4 Y
        sta $d00b               ; sprite 5 Y
        sta $d00d               ; sprite 6 Y
        sta $d00f               ; sprite 7 Y
        inc _vsync_flag
        lda #LINE_SPLIT
        sta $d012
        lda #<irq_split
        sta $0314
        lda #>irq_split
        sta $0315
        jmp $ea31               ; full KERNAL IRQ tail (jiffy, keyboard)

irq_split:
        lda #$01
        sta $d019
        lda disp_fine
        ora #$10                ; MCM on, 38 col, playfield fine scroll
        sta $d016
        lda #$00
        sta mux_idx
        lda _mux_count
        beq mux_done            ; nothing to multiplex this frame
        lda _mux_y              ; first entry: schedule a few lines early
        sec
        sbc #4
        cmp #LINE_SPLIT+2
        bcs :+
        lda #LINE_SPLIT+2       ; never behind the split we are servicing
:       sta $d012
        lda #<irq_mux
        sta $0314
        lda #>irq_mux
        sta $0315
        jmp $ea81               ; minimal KERNAL IRQ exit

irq_mux:
        lda #$01
        sta $d019
mux_body:
        ldy mux_idx
        lda _mux_y,y
        cmp #$ff
        beq mux_done
        ; --- reprogram one hardware sprite ---
        ldx _mux_slot2,y
        lda _mux_xlo,y
        sta $d000,x
        lda _mux_y,y
        sta $d001,x
        lda $d010
        and _mux_xand,y
        ora _mux_xor,y
        sta $d010
        ldx _mux_slot,y
        lda _mux_col,y
        sta $d027,x
        lda _mux_ptr,y
        sta SCR_A_PTR,x         ; both buffers: the flip must not matter
        sta SCR_B_PTR,x
        ; --- schedule the next entry ---
        iny
        sty mux_idx
        lda _mux_y,y
        cmp #$ff
        beq mux_done
        sec
        sbc #4
        sta $d012
        cmp $d012               ; A = target, $D012 reads the current line
        bcc mux_body            ; already past it: program it right now
        beq mux_body
        lda #<irq_mux
        sta $0314
        lda #>irq_mux
        sta $0315
        jmp $ea81

mux_done:
        lda #LINE_TOP
        sta $d012
        lda #<irq_top
        sta $0314
        lda #>irq_top
        sta $0315
        jmp $ea81

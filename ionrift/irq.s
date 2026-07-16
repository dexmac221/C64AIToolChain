; irq.s - raster IRQ engine for ION RIFT
;
; Two raster interrupts per frame:
;   LINE_TOP   (above visible area): apply buffer flip ($D018), lock the
;              HUD rows steady (fine scroll 0), bump the frame flag.
;              Chains into the full KERNAL IRQ once per frame so the
;              jiffy clock and keyboard scan stay alive.
;   LINE_SPLIT (after the 3 HUD rows): apply the playfield fine scroll
;              in 38-column multicolor mode. Minimal exit.
;
; C interface:
;   void irq_init(void);
;   extern unsigned char fine_x;     playfield fine scroll 0..7
;   extern unsigned char d018_next;  $D018 value (buffer flip) at frame top
;   extern unsigned char vsync_flag; incremented once per frame

.export _irq_init
.export _fine_x, _d018_next, _vsync_flag

LINE_TOP   = 40
LINE_SPLIT = 73                 ; raster line after HUD rows 0-2 (50+24-1)

.data
_fine_x:     .byte 7
_d018_next:  .byte $14          ; screen $4400, charset $5000
_vsync_flag: .byte 0

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
        lda #$18                ; HUD: MCM on, 40 col, fine scroll 0
        sta $d016
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
        lda _fine_x
        ora #$10                ; MCM on, 38 col, playfield fine scroll
        sta $d016
        lda #LINE_TOP
        sta $d012
        lda #<irq_top
        sta $0314
        lda #>irq_top
        sta $0315
        jmp $ea81               ; minimal KERNAL IRQ exit

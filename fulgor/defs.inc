; defs.inc - constants and shared symbols for FULGOR

SCREEN_A    = $4400
SCREEN_B    = $4800
SCREEN_C    = $4C00
COLRAM      = $D800
PF_OFF      = 120               ; playfield starts on screen row 3
PF_ROWS     = 21                ; rows 3..23: the 22nd never reaches the window
D018_A      = $14               ; screen $4400, charset $5000
D018_B      = $24               ; screen $4800, charset $5000
D018_C      = $34               ; screen $4C00, charset $5000
SPR_BASE    = 96                ; sprite pointer of frame 0 ($5800)

LINE_TOP    = $F8               ; frame interrupt, in the bottom border
LINE_SPLIT  = $46               ; last line of HUD row 1
MUX_BANK    = 32                ; entries per multiplexer bank
MUX_LEAD    = 3                 ; lines between a mux interrupt and its sprite
MUX_GAP     = 25                ; lines before a hardware sprite is reused
NUM_OBJ     = 16

SCR_LEFT_X  = 31                ; sprite X of world pixel cam_x
SCR_TOP_Y   = $55               ; sprite Y of world pixel cam_y
CLIP_TOP    = $46               ; sprites above this would cover the HUD
CLIP_BOT    = $F7

CAM_X_MAX   = MAP_W * 32 - 320
CAM_Y_MAX   = MAP_H * 32 - PF_ROWS * 8

; prep units (bits of p_units), run in this order: the two edges, then
; the copy in four bands of ten columns. The copy never touches the
; entering row or column, so the order is free and the edges go first.
U_EC        = 1                 ; the entering column
U_ER        = 2                 ; the entering row
U_CA        = 4                 ; copy, columns 39..30
U_CB        = 8                 ; copy, columns 29..20
U_CC        = 16                ; copy, columns 19..10
U_CD        = 32                ; copy, columns 9..0

; joystick bits, as in the toolchain's agent input bytes
J_UP        = 1
J_DOWN      = 2
J_LEFT      = 4
J_RIGHT     = 8
J_FIRE      = 16

; telemetry, read through the monitor
T_FRAMES    = $0350             ; 16-bit: frames committed
T_LATE      = $0352             ; 16-bit: frames whose work missed the vsync
T_LOADMAX   = $0354             ; worst commit time, in raster lines after vsync
T_LOAD      = $0355             ; last commit time
T_STALLS    = $0356             ; 16-bit: camera held at a boundary
T_FLIPS     = $0358             ; 16-bit: coarse steps
T_HIST      = $0360             ; 8 bins: commit time / 32 lines
T_PROF      = $0370             ; 8 checkpoints: last, then 8 worst
T_DBG       = $0380             ; scratch for calibration
T_CAM       = $0388             ; camera x, y of the picture on screen (16-bit each)

.globalzp   irq_a, irq_x, irq_y, nmi_a, er_mp, er_dst, cp, px, py, zt
.global     cam_x, cam_y, front, s_v, p_active
.global     o_st, o_xl, o_xh, o_yl, o_yh, o_ptr, o_col
.global     solid_at, floor_at, fl_ow, side_solid
.global     _teleport, hero_sprites, camera_place, obj_add, obj_add_at
.global     flags_at, take_cell, s_y, s_x2, m_n, o_in, snd_play, _sfx_req
.global     _joy, _frame, _prof
.global     _scroll_init, _scroll_step, _build_mux, _commit, _wait_commit
.global     hero_x, hero_y

; PROF n: record how far down the frame we are (only in profile builds)
.macro PROF n
    .ifdef PROFILE
        ldy #n
        jsr _prof
    .endif
.endmacro

; sound effects (game code writes one of these to _sfx_req)
SFX_SHOT    = 1
SFX_GEM     = 2
SFX_POWER   = 3
SFX_HURT    = 4
SFX_BOOM    = 5
SFX_HIT     = 6
SFX_ESHOT   = 7

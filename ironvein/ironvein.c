/*
 * IRON VEIN - 8-way scrolling engine spike for the Commodore 64
 *
 * Nothing here is a game yet. This is the question "can this toolchain
 * scroll in eight directions, with colour, at 50 frames a second, with a
 * dozen multiplexed sprites on screen?" written as a program that
 * answers it with numbers.
 *
 * Engine:
 *   - screen RAM double-buffered ($4400/$4800), flipped in the IRQ at
 *     the bottom border together with both fine scroll values
 *   - colour RAM cannot be double-buffered, so colour is PER ROW: a
 *     horizontal step moves no colour at all, a vertical step repaints
 *     the 22 rows with constants ahead of the beam (col_fill, ~4k)
 *   - the camera commits to a step direction; the shifted back buffer
 *     is prepared over four frames (three copy chunks, then the edge
 *     cells fetched from the metatile map); a boundary may not be
 *     crossed until the buffer for that exact crossing is ready
 *   - HUD rows 0-1, blank spacer row 2, playfield rows 3-24; 24-row and
 *     38-column modes hide the ragged edges; YSCROLL changes in the
 *     spacer row
 *   - the per-object work (drones, multiplexer table, edge fetch) is
 *     assembly: cc65 promotes byte arithmetic to int and the same code
 *     in C cost four times as much
 *
 * Probes (read once through the monitor, never while someone watches):
 *   $033D missed frames
 *   $0350/1 $0352/3 $0354/5 $0356/7  worst cycles: fill, camera+prep,
 *                                    actors+mux, whole frame (16-bit)
 *   $0358/9 drones only   $035A/B write_edges only
 *   $035C/D self-test: 200 empty loop turns   $035E/F self-test: nothing
 *   $0360 stalls  $0361 crossings  $0362/3 frames
 *
 * AI Toolchain Project 2026
 */

#include <c64.h>
#include <string.h>
#include <stdlib.h>
#include <joystick.h>
#include <peekpoke.h>

#include "tiles_mc.h"
#include "sprites_mc.h"
#include "level.h"

/* --- video memory (VIC bank 1) --- */
#define SCREEN_A ((unsigned char*)0x4400)
#define SCREEN_B ((unsigned char*)0x4800)
#define CHARSET  0x5000
#define SPRDATA  0x5800
#define SPR_PTR_BASE 96
#define COLRAM   ((unsigned char*)0xD800)
#define D018_A 0x14
#define D018_B 0x24

#define VIC_SPR_X(n)   (*(unsigned char*)(0xD000 + (n)*2))
#define VIC_SPR_Y(n)   (*(unsigned char*)(0xD001 + (n)*2))
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_MC     (*(unsigned char*)0xD01C)
#define VIC_SPR_PRIO   (*(unsigned char*)0xD01B)
#define VIC_SPR_MC0    (*(unsigned char*)0xD025)
#define VIC_SPR_MC1    (*(unsigned char*)0xD026)
#define VIC_SPR_COL(n) (*(unsigned char*)(0xD027 + (n)))
#define VIC_BORDER     (*(unsigned char*)0xD020)
#define VIC_BG         (*(unsigned char*)0xD021)
#define VIC_MC1        (*(unsigned char*)0xD022)
#define VIC_MC2        (*(unsigned char*)0xD023)
#define CIA2_PRA       (*(unsigned char*)0xDD00)

/* --- engine hooks --- */
extern volatile unsigned char finex_next, finey_next, d018_next, vsync_flag;
extern volatile unsigned char mux_next, irq_count;
extern volatile unsigned int vsync_count;
void irq_init(void);
extern unsigned char *scr_src, *scr_dst;
extern unsigned char scr_rows;
void scroll_rows(void);
extern unsigned char mux_y[], mux_xlo[], mux_slot[], mux_slot2[];
extern unsigned char mux_ptr[], mux_col[], mux_xand[], mux_xor[];

/* --- geometry --- */
#define PF_TOP_ROW 3
#define PF_ROWS 22
#define PF_OFFSET (PF_TOP_ROW * 40)
#define PF_CELLS (PF_ROWS * 40)
#define SCREEN_LEFT_X 31         /* sprite X of world pixel cam_px */
#define SCREEN_TOP_Y  0x56       /* sprite Y of world pixel cam_py */
#define MUX_BANK 32

/* --- probes --- */
#define MISS_COUNTER 0x033D
#define PROBE_BASE   0x0350
#define P_STALLS     0x0360
#define P_CROSS      0x0361
#define P_FRAMES     0x0362
#define PROFILE_ON   0x033F      /* non-zero: paint the border per phase */
#define BORDER(c)    if (PEEK(PROFILE_ON)) VIC_BORDER = (c)

unsigned char rowcol[PF_ROWS];         /* colour of each playfield row */
unsigned char rowcol_new[PF_ROWS];
const unsigned int row_ofs[PF_ROWS] = {
      0,  40,  80, 120, 160, 200, 240, 280, 320, 360, 400, 440,
    480, 520, 560, 600, 640, 680, 720, 760, 800, 840
};

unsigned int cia_now(void);
void probe(unsigned char slot, unsigned int start);
/* The probes cost ~450 cycles each, six a frame: 14% of the budget.
   They are what found the problems, and they are the first thing to
   switch off when checking whether the problems are fixed. */
#ifndef PROFILE
#define PROFILE 0
#endif
#if PROFILE
#define PROBE(slot, start) probe(slot, start)
#define NOW() cia_now()
#else
#define PROBE(slot, start)
#define NOW() 0
#endif

/* --- camera --- */
unsigned char cam_cx = 4, cam_cy = 46;    /* char column/row at screen origin */
unsigned char fx = 0, fy = 0;             /* fine position 0..7 inside it */
unsigned char front = 0;
unsigned char fill_pending = 0;   /* a vertical step: repaint colour RAM */
unsigned char skip_prep = 0;

/* --- back buffer preparation --- */
signed char prep_dx = 0, prep_dy = 0;
unsigned char prep_step = 0;              /* 0 idle, 1-4 working, 5 ready */
#define PREP_READY 8

unsigned char frame = 0;
unsigned int frames = 0;

unsigned char *front_scr(void) { return front ? SCREEN_B : SCREEN_A; }
unsigned char *back_scr(void)  { return front ? SCREEN_A : SCREEN_B; }

/* ================= world ================= */

unsigned char world_char(unsigned char cx, unsigned char cy) {
    unsigned char mt = level_map[((unsigned int)(cy >> 2) << 6) + (cx >> 2)];
    return mt_chars[(mt << 4) + ((cy & 3) << 2) + (cx & 3)];
}

/* Paint one full screen + colour RAM for the current camera. Slow;
   used once at start. */
void draw_full(unsigned char *scr) {
    unsigned char r, c;
    unsigned int o = PF_OFFSET;
    for (r = 0; r < PF_ROWS; r++) {
        for (c = 0; c < 40; c++) {
            scr[o] = world_char(cam_cx + c, cam_cy + r);
            COLRAM[o] = row_col[cam_cy + r];
            o++;
        }
    }
}

/* ================= preparation ================= */

void prep_start(signed char dx, signed char dy) {
    prep_dx = dx;
    prep_dy = dy;
    prep_step = 1;
}

/* The cells that enter the screen: a column, a row, or both. The
   fetch itself is assembly (edges.s); this only points it at the map. */
extern unsigned char *edge_bs;
extern const unsigned char *edge_mp;
extern unsigned char edge_cx, edge_cy;
void edge_row(void);
void edge_col(void);

void write_edge_col(void) {
    unsigned char ncx = cam_cx + prep_dx, ncy = cam_cy + prep_dy;
    if (prep_dx) {
        unsigned char c = (prep_dx > 0) ? 39 : 0;
        edge_bs = back_scr() + PF_OFFSET + c;
        edge_cx = ncx + c;
        edge_cy = ncy;
        edge_mp = level_map + ((unsigned int)(ncy >> 2) << 6);
        edge_col();
    }
}

void write_edge_row(void) {
    unsigned char ncx = cam_cx + prep_dx, ncy = cam_cy + prep_dy;
    if (prep_dy) {
        unsigned char r = (prep_dy > 0) ? PF_ROWS - 1 : 0;
        edge_bs = back_scr() + PF_OFFSET + row_ofs[r];
        edge_cx = ncx;
        edge_cy = ncy + r;
        edge_mp = level_map + ((unsigned int)((ncy + r) >> 2) << 6);
        edge_row();
    }
}

/* Shift n rows starting at r0 from the front buffer (and colour RAM)
   into the back buffer (and the shadow), displaced by the step. */
void shift_chunk(unsigned char r0, unsigned char n) {
    int off = (prep_dy > 0 ? 40 : (prep_dy < 0 ? -40 : 0)) + prep_dx;
    unsigned int o = PF_OFFSET + row_ofs[r0];
    scr_src = front_scr() + o + off;
    scr_dst = back_scr() + o;
    scr_rows = n;
    scroll_rows();
}

/* The cells that enter the screen: one column, one row, or both.
   world_char() per cell cost 24 000 cycles for a row - six 16-bit
   runtime calls each. Here the map-row pointer is computed once per
   row and everything else is a byte index, which is what the 6502 is
   good at. NUM_MT must stay <= 16 so mt<<4 fits a byte. */

void prep_run(void) {
    switch (prep_step) {
    /* a crossing is at least eight frames away, so the copy is spread
       as thin as it goes: five chunks, the column edge, the row edge -
       seven frames, ready with one to spare */
    case 1: shift_chunk(0, 5);  prep_step = 2; break;
    case 2: shift_chunk(5, 5);  prep_step = 3; break;
    case 3: shift_chunk(10, 4); prep_step = 4; break;
    case 4: shift_chunk(14, 4); prep_step = 5; break;
    case 5: shift_chunk(18, 4); prep_step = 6; break;
    case 6: { unsigned int t = NOW(); write_edge_col(); PROBE(5, t); }
            prep_step = 7; break;
    case 7: write_edge_row(); prep_step = PREP_READY; break;
    default: break;
    }
}

/* ================= camera ================= */

/* Move the camera by (vx, vy) in {-1,0,1} pixels, honouring the rule
   that a coarse boundary is crossed only with a prepared buffer. */
void camera_step(signed char vx, signed char vy) {
    unsigned char tx = 255, ty = 255;     /* frames until each crossing */
    unsigned char crossx, crossy;

    /* keep the window inside the map */
    if (vx < 0 && cam_cx == 0 && fx == 0) vx = 0;
    if (vx > 0 && cam_cx >= (MAP_W * 4 - 40) && fx == 7) vx = 0;
    if (vy < 0 && cam_cy == 0 && fy == 0) vy = 0;
    if (vy > 0 && cam_cy >= (MAP_H * 4 - PF_ROWS) && fy == 7) vy = 0;

    if (vx > 0) tx = 7 - fx; else if (vx < 0) tx = fx;
    if (vy > 0) ty = 7 - fy; else if (vy < 0) ty = fy;

    /* Diagonal motion with the two fine positions out of phase would
       cross a boundary every four frames, alternating axes, and every
       crossing costs a full preparation. Instead the nearer axis waits
       for the other to catch up - a one-off hitch of at most seven
       pixels on one axis when a diagonal begins - and from then on the
       two cross together, once every eight frames, with one shift. */
    if (tx != 255 && ty != 255 && tx != ty) {
        if (tx < ty) { vx = 0; tx = 255; }
        else         { vy = 0; ty = 255; }
        POKE(0x0374, PEEK(0x0374) + 1);               /* aligns */
    }

    /* prepare for the nearest crossing (both axes if they coincide) */
    if (tx != 255 || ty != 255) {
        signed char wdx = 0, wdy = 0;
        if (tx <= ty) wdx = vx;
        if (ty <= tx) wdy = vy;
        if (prep_step == 0 || wdx != prep_dx || wdy != prep_dy)
            prep_start(wdx, wdy);
    }
    if (!skip_prep) prep_run();      /* not on top of a colour fill */

    crossx = (tx == 0);
    crossy = (ty == 0);
    if (crossx || crossy) {
        signed char ndx = crossx ? vx : 0, ndy = crossy ? vy : 0;
        if (prep_step == PREP_READY && prep_dx == ndx && prep_dy == ndy) {
            cam_cx += ndx;
            cam_cy += ndy;
            if (crossx) fx = (vx > 0) ? 0 : 7; else fx += vx;
            if (crossy) fy = (vy > 0) ? 0 : 7; else fy += vy;
            front ^= 1;
            d018_next = front ? D018_B : D018_A;
            if (ndy) {                        /* rows moved: new colours */
                unsigned char r;
                for (r = 0; r < PF_ROWS; r++) rowcol_new[r] = row_col[cam_cy + r];
                fill_pending = 1;
            }
            prep_step = 0;
            POKE(P_CROSS, PEEK(P_CROSS) + 1);
        } else {
            POKE(P_STALLS, PEEK(P_STALLS) + 1);   /* frozen this frame */
        }
    } else {
        fx += vx;
        fy += vy;
    }
    finex_next = 7 - fx;
    finey_next = (6 - fy) & 7;
}

/* ================= sprites ================= */

#define NUM_WK 8
/* Walkers: structure of arrays, bytes only. World position as lo/hi,
   signed velocities, on = standing on ground. Their physics is in
   physics.s; C only spawns them. */
unsigned char dr_xlo[NUM_WK], dr_xhi[NUM_WK];
unsigned char dr_ylo[NUM_WK], dr_yhi[NUM_WK];
signed char dr_vx[NUM_WK], dr_vy[NUM_WK];
unsigned char dr_on[NUM_WK], dr_col[NUM_WK];
extern unsigned int cam_px, cam_py;
extern unsigned char mux_bank;
void update_walkers(void);
void build_mux(void);
extern unsigned int col_x, col_y;
unsigned char solid_at(void);
unsigned int g_t;

/* A walker drops in from above the view, just outside it to one side,
   and gravity does the rest. */
void spawn_walker(unsigned char i) {
    unsigned char r = (unsigned char)rand();
    g_t = cam_px + ((r & 1) ? (unsigned int)-28 : 330u);
    dr_xlo[i] = g_t & 0xFF;
    dr_xhi[i] = g_t >> 8;
    g_t = cam_py + (r & 0x3F);
    dr_ylo[i] = g_t & 0xFF;
    dr_yhi[i] = g_t >> 8;
    dr_vx[i] = (r & 1) ? 1 : -1;
    dr_vy[i] = 0;
    dr_on[i] = 0;
    dr_col[i] = (i & 1) ? 10 : 13;
}

void init_walkers(void) {
    unsigned char i;
    cam_px = (unsigned int)cam_cx * 8 + fx;
    cam_py = (unsigned int)cam_cy * 8 + fy;
    for (i = 0; i < NUM_WK; i++) {
        spawn_walker(i);
        g_t = cam_px + 40 + i * 32;             /* start on screen */
        dr_xlo[i] = g_t & 0xFF;
        dr_xhi[i] = g_t >> 8;
    }
}

/* ================= hero ================= */

/* World position of the sprite's top-left; the collision box inside
   the 24x21 sprite is columns 6..17, rows 2..20 (feet on row 20). */
unsigned int hero_x, hero_y;
unsigned char hero_blocked = 0;
signed char hero_vy = 0;
unsigned char hero_on = 0, hero_right = 1, hero_frm = 0;
#define H_L 6
#define H_R 17
#define H_T 2
#define H_B 20
#define JUMP_V (-6)

unsigned char solid(unsigned int x, unsigned int y) {
    col_x = x;
    col_y = y;
    return solid_at();
}

void update_hero(unsigned char joy) {
    unsigned char blocked = 0;

    /* gravity, every other frame, terminal speed 4 */
    if (!(frame & 1) && hero_vy < 4) hero_vy++;

    /* vertical */
    hero_y += hero_vy;
    hero_on = 0;
    if (hero_vy > 0) {
        if (solid(hero_x + H_L, hero_y + H_B + 1) ||
            solid(hero_x + H_R, hero_y + H_B + 1)) {
            hero_y = ((hero_y + H_B + 1) & 0xFFE0) - (H_B + 1);   /* snap */
            hero_vy = 0;
            hero_on = 1;
        }
    } else if (hero_vy < 0) {
        if (solid(hero_x + H_L, hero_y + H_T) ||
            solid(hero_x + H_R, hero_y + H_T)) {
            hero_y -= hero_vy;
            hero_vy = 0;
        }
    }

    /* horizontal, one pixel a frame, walls at knee and chest */
    if (JOY_LEFT(joy) || JOY_RIGHT(joy)) {
        unsigned int nx = hero_x + (JOY_RIGHT(joy) ? 1 : -1);
        unsigned int probe = JOY_RIGHT(joy) ? nx + H_R + 1 : nx + H_L - 1;
        hero_right = JOY_RIGHT(joy) ? 1 : 0;
        if (solid(probe, hero_y + (H_T + H_B) / 2))
            blocked = 1;
        else
            hero_x = nx;
        if (hero_on) hero_frm = (frame >> 3) & 1;
    } else {
        hero_frm = 0;
    }

    /* jump: only from the ground */
    if (JOY_UP(joy) && hero_on) {
        hero_vy = JUMP_V;
        hero_on = 0;
    }
    hero_blocked = blocked;
}

/* The camera follows the hero through a dead zone: it moves only when
   he leaves a box in the middle of the view, one pixel a frame. */
void camera_follow(signed char *vx, signed char *vy) {
    int sx = (int)hero_x - (int)cam_px;    /* hero on screen, 0..319 */
    int sy = (int)hero_y - (int)cam_py;
    *vx = 0; *vy = 0;
    if (sx > 176) *vx = 1; else if (sx < 120) *vx = -1;
    if (sy > 112) *vy = 1; else if (sy < 56) *vy = -1;
}

/* Autopilot for the hero: run, jump at walls and now and then, turn
   back at the ends of the map. */
unsigned char ai_dir = JOY_RIGHT_MASK, ai_t = 0;
unsigned char hero_ai(void) {
    unsigned char joy = ai_dir;
    if (hero_blocked && hero_on) joy |= JOY_UP_MASK;
    if (hero_on && (rand() & 63) == 0) joy |= JOY_UP_MASK;
    if (hero_x < 48) ai_dir = JOY_RIGHT_MASK;
    if (hero_x > 1950) ai_dir = JOY_LEFT_MASK;
    if (++ai_t == 0 && (rand() & 1)) ai_dir ^= JOY_LEFT_MASK | JOY_RIGHT_MASK;
    return joy;
}

void set_hw_sprite(unsigned char spr, unsigned int x, unsigned char y) {
    VIC_SPR_X(spr) = x & 0xFF;
    VIC_SPR_Y(spr) = y;
    if (x > 255) VIC_SPR_HI_X |= 1 << spr;
    else         VIC_SPR_HI_X &= ~(1 << spr);
}

/* Screen-space the visible drones, sort by Y, hand them to the
   multiplexer in the bank the IRQ is not reading. Sprites 2-7 rotate. */

/* ================= text ================= */

void put_text(unsigned char *scr, unsigned char x, unsigned char y,
              const char *s) {
    unsigned int o = y * 40 + x;
    unsigned char c;
    while ((c = (unsigned char)*s++) != 0) {
        if (c >= 0xC1 && c <= 0xDA) c -= 0x80;
        scr[o++] = c;
    }
}

void put_text2(unsigned char x, unsigned char y, const char *s) {
    put_text(SCREEN_A, x, y, s);
    put_text(SCREEN_B, x, y, s);
}

/* ================= video ================= */

void install_video(void) {
    unsigned char i;
    CIA2_PRA = (CIA2_PRA & 0xFC) | 0x02;         /* VIC bank 1 */
    __asm__("sei");
    POKE(1, PEEK(1) & 0xFB);
    memcpy((void*)CHARSET, (void*)0xD800, 96 * 8);   /* ROM glyphs */
    POKE(1, PEEK(1) | 0x04);
    __asm__("cli");
    memset((void*)(CHARSET + 96 * 8), 0, 32 * 8);
    memcpy((void*)(CHARSET + 128 * 8), tile_gfx, sizeof(tile_gfx));
    memcpy((void*)SPRDATA, sprite_gfx, sizeof(sprite_gfx));
    POKE(0x7FFF, 0);                             /* idle graphics: blank */

    memset(SCREEN_A, 32, 1000);
    memset(SCREEN_B, 32, 1000);
    memset(COLRAM, 1, 120);                      /* HUD + spacer: hires white */

    VIC_BG = 0;
    VIC_BORDER = 0;
    VIC_MC1 = 11;                /* shared 01: dark grey */
    VIC_MC2 = 12;                /* shared 10: mid grey */
    VIC_SPR_MC = 0xFF;
    VIC_SPR_PRIO = 0x00;
    VIC_SPR_MC0 = 11;
    VIC_SPR_MC1 = 1;
    VIC_SPR_COL(0) = 14;
    VIC_SPR_COL(1) = 7;
    for (i = 2; i < 8; i++) VIC_SPR_COL(i) = 13;
    VIC_SPR_ENA = 0xFF;
    SCREEN_A[0x3F8] = SPR_PTR_BASE + SF_HERO;
    SCREEN_B[0x3F8] = SPR_PTR_BASE + SF_HERO;
}

/* ================= frame ================= */

unsigned char wait_frame(void) {
    unsigned char n;
    /* arriving with the flag already set means this iteration ended
       after the vsync it was meant to beat: the frame is late, though
       not lost. $036A counts those; $033D counts only lost ones. */
    if (vsync_flag) POKE(0x036A, PEEK(0x036A) + 1);
    while (!vsync_flag) ;
    n = vsync_flag;
    vsync_flag = 0;
    if (n > 1) POKE(MISS_COUNTER, PEEK(MISS_COUNTER) + n - 1);
    return n;
}

/* CIA2 timer A free-runs from $FFFF downwards at one count per cycle;
   the difference between two readings is exact cycles, and a frame is
   19 656 of them. Raster-line deltas were what we had before, and they
   alias above line 255. */
void profiler_init(void) {
    POKE(0xDD0E, 0);
    POKE(0xDD04, 0xFF);
    POKE(0xDD05, 0xFF);
    POKE(0xDD0E, 0x11);              /* force load, start, continuous */
}

unsigned int cia_now(void) {
    unsigned char hi = PEEK(0xDD05), lo = PEEK(0xDD04);
    if (PEEK(0xDD05) != hi) { hi = PEEK(0xDD05); lo = PEEK(0xDD04); }
    return ((unsigned int)hi << 8) | lo;
}

/* slot n: worst cycles seen, 16-bit at PROBE_BASE + 2n */
void probe(unsigned char slot, unsigned int start) {
    unsigned int d = start - cia_now();
    unsigned int *p = (unsigned int*)(PROBE_BASE + (slot << 1));
    if (d > *p) *p = d;              /* exact mod 65536: the reload is -1 */
}

void main(void) {
    signed char vx, vy;
    unsigned char joy;
    unsigned int t0, tf;

    joy_install(joy_static_stddrv);
    install_video();
    memset((void*)PROBE_BASE, 0, 32);
    POKE(MISS_COUNTER, 0);
    put_text2(1, 0, "IRON VEIN    8-WAY ENGINE SPIKE");
    put_text2(1, 1, "JOYSTICK 2  UP = JUMP   IDLE = DEMO");
    draw_full(SCREEN_A);
    draw_full(SCREEN_B);
    { unsigned char r; for (r = 0; r < PF_ROWS; r++) rowcol[r] = row_col[cam_cy + r]; }
    hero_x = (unsigned int)cam_cx * 8 + 150;
    hero_y = (unsigned int)cam_cy * 8 + 40;
    init_walkers();
    memset((void*)0x0368, 0, 8);
    /* an unterminated multiplexer table is walked into the rest of BSS
       and sprays whatever it finds over the VIC registers */
    memset(mux_y, 0xFF, 2 * MUX_BANK);
    finex_next = 7 - fx;
    finey_next = (6 - fy) & 7;
    profiler_init();
    {   /* calibration, interrupts off: what do the hot functions cost
           on a quiet machine? $0368 chunk  $036A edges  $036C build_mux
           $036E update_drones */
        unsigned int t;
        prep_dx = 1; prep_dy = 1;
        cam_px = (unsigned int)cam_cx * 8 + fx;
        cam_py = (unsigned int)cam_cy * 8 + fy;
        t = cia_now(); shift_chunk(0, 8); t -= cia_now(); POKE(0x0368, t & 0xFF); POKE(0x0369, t >> 8);
        t = cia_now(); write_edge_col(); write_edge_row(); t -= cia_now(); POKE(0x036A, t & 0xFF); POKE(0x036B, t >> 8);
        t = cia_now(); build_mux(); t -= cia_now(); POKE(0x036C, t & 0xFF); POKE(0x036D, t >> 8);
        t = cia_now(); update_walkers(); t -= cia_now(); POKE(0x036E, t & 0xFF); POKE(0x036F, t >> 8);
        t = cia_now(); update_hero(JOY_RIGHT_MASK); t -= cia_now(); POKE(0x0370, t & 0xFF); POKE(0x0371, t >> 8);
        { signed char a, b; t = cia_now(); camera_follow(&a, &b); t -= cia_now(); POKE(0x0372, t & 0xFF); POKE(0x0373, t >> 8); }
        prep_dx = 0; prep_dy = 0;
        memset(mux_y, 0xFF, 2 * MUX_BANK);
        mux_bank = 0;
    }
    {   /* self-test the profiler against known work */
        unsigned int t;
        volatile unsigned char k;
        t = cia_now();
        for (k = 0; k < 200; k++) ;
        probe(6, t);
        t = cia_now();
        probe(7, t);
    }
    irq_init();

    for (;;) {
        wait_frame();
        frame++;
        frames++;
        POKE(P_FRAMES, frames & 0xFF);
        POKE(P_FRAMES + 1, frames >> 8);
        POKE(0x0364, vsync_count & 0xFF);
        POKE(0x0365, vsync_count >> 8);
        if (irq_count > PEEK(0x0366)) POKE(0x0366, irq_count);
        irq_count = 0;

        /* 1. colour fill after a vertical step, ahead of the beam */
        tf = t0 = NOW();
        skip_prep = 0;
        if (fill_pending) {
            /* Only rows whose colour actually changes get painted: with
               four colour bands that is a row or two at the band edges,
               not 880 stores. Ahead of the beam, so first. */
            unsigned char r;
            skip_prep = 1;
            BORDER(2);
            for (r = 0; r < PF_ROWS; r++) {
                if (rowcol_new[r] != rowcol[r]) {
                    rowcol[r] = rowcol_new[r];
                    memset(COLRAM + PF_OFFSET + row_ofs[r], rowcol[r], 40);
                }
            }
            fill_pending = 0;
            PROBE(0, t0);
        }

        /* 2. the hero: joystick, or his own autopilot when idle */
        joy = joy_read(JOY_2) | PEEK(0x033C) | PEEK(0x033E);
        POKE(0x033C, 0);                 /* $033E is a hold, $033C an edge */
        if (!(joy & (JOY_LEFT_MASK | JOY_RIGHT_MASK | JOY_UP_MASK)))
            joy = hero_ai();
        cam_px = (unsigned int)cam_cx * 8 + fx;
        cam_py = (unsigned int)cam_cy * 8 + fy;
        update_hero(joy);
        camera_follow(&vx, &vy);
        if (PEEK(0x0340)) {              /* test hook: pin the vertical
                                            fine position, freeze the camera */
            vx = vy = 0;
            fy = PEEK(0x0340) - 1;
            if (fy > 7) fy = 7;
        }

        /* 3. camera + preparation */
        BORDER(6);
        t0 = NOW();
        camera_step(vx, vy);
        PROBE(1, t0);

        /* 4. actors and multiplexer */
        BORDER(5);
        t0 = NOW();
        cam_px = (unsigned int)cam_cx * 8 + fx;
        cam_py = (unsigned int)cam_cy * 8 + fy;
        update_walkers();
        PROBE(6, t0);
        set_hw_sprite(0, hero_x - cam_px + SCREEN_LEFT_X,
                      (unsigned char)(hero_y - cam_py + SCREEN_TOP_Y));
        SCREEN_A[0x3F8] = SPR_PTR_BASE + SF_HERO;
        SCREEN_B[0x3F8] = SPR_PTR_BASE + SF_HERO;
        { unsigned int t = NOW(); build_mux(); PROBE(7, t); }
        PROBE(2, t0);
        PROBE(3, tf);                     /* whole frame's work */
        BORDER(0);
    }
}

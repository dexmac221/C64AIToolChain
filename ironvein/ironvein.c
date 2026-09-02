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
 *   $0358/9 game logic (hero, boss, shots, HUD)   $035A/B write_edges only
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
unsigned char cam_cx = 0, cam_cy = 12;    /* char column/row at screen origin:
                                             where the dead zone puts the hero's start */
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

#define NUM_WK 6
#define BOSS0 6
#define SHOT0 10
#define NUM_OBJ 14
#define DYING 17
/* Objects: structure of arrays, bytes only. 0..5 are walkers, 6..9 the
   boss's four parts, 10..13 the hero's shots. Eight walkers plus shots,
   boss and contact tests ran the frame over budget on most frames of
   the plateau (62% late); the spike's verdict was "eight objects yes,
   twelve no" and the game had crept to eleven. World position as lo/hi, signed velocities, on =
   standing on ground, st = 0 free / 1 alive / 2..DYING exploding,
   ptr = sprite pointer, life = frames left (shots). All of their
   per-frame logic is in physics.s; C only spawns them. */
unsigned char dr_xlo[NUM_OBJ], dr_xhi[NUM_OBJ];
unsigned char dr_ylo[NUM_OBJ], dr_yhi[NUM_OBJ];
signed char dr_vx[NUM_OBJ], dr_vy[NUM_OBJ];
unsigned char dr_on[NUM_OBJ], dr_col[NUM_OBJ];
unsigned char dr_st[NUM_OBJ], dr_ptr[NUM_OBJ], dr_life[NUM_OBJ];
unsigned char kills = 0;                 /* bumped by update_shots */
unsigned char boss_hits = 0;             /* likewise, shots on the boss */
extern unsigned int cam_px, cam_py;
extern unsigned char mux_bank;
void update_walkers(void);
void update_shots(void);
unsigned char hero_touch(void);
void build_mux(void);
extern unsigned int col_x, col_y;
unsigned char solid_at(void);
unsigned int g_t;

/* A walker drops in from above the view, just outside it to one side,
   and gravity does the rest. */
/* The arena at the bottom right belongs to the boss: no walker spawns
   while the hero is near it, and the ones that leave stay gone until
   he comes back out (respawn tick in the main loop). */
extern unsigned int hero_x, hero_y;
unsigned char near_arena(void) {
    return hero_x > 1250 && hero_y > 700;
}

void spawn_walker(unsigned char i) {
    unsigned char r = (unsigned char)rand();
    if (near_arena()) { dr_st[i] = 0; return; }
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
    dr_ptr[i] = SPR_PTR_BASE + SF_DRONE1;
    dr_st[i] = 1;
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
unsigned char hero_on = 0, hero_right = 1, hero_frm = 0, hero_moving = 0;
unsigned char hero_inv = 0;       /* frames of invulnerability after a hit */
unsigned char energy = 8, fire_cool = 0;
/* The score lives as five decimal digits: cc65's 16-bit division costs
   ~700 cycles and the HUD needed ten of them on every kill - 8 000
   cycles, a frame lost each time the player did well. A digit-wise
   add with carry is fifty. */
unsigned char score_d[5] = { 0, 0, 0, 0, 0 };   /* most significant first */
unsigned char score_dirty = 1;
void add_score(unsigned char digit, unsigned char n) {
    score_dirty = 1;
    while (n--) {
        unsigned char d = digit;
        for (;;) {
            if (++score_d[d] < 10) break;
            score_d[d] = 0;
            if (d == 0) break;
            d--;
        }
    }
}
#define SCORE_TENS 3
#define SCORE_HUNDREDS 2
#define ENERGY_MAX 8
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
        hero_moving = 1;
    } else {
        hero_moving = 0;
    }

    /* jump: only from the ground */
    if (JOY_UP(joy) && hero_on) {
        hero_vy = JUMP_V;
        hero_on = 0;
    }
    hero_blocked = blocked;

    /* the frame: jumping, running (four frames, four ticks each), or
       standing; the left-facing set follows the right-facing one */
    if (!hero_on)          hero_frm = SF_HERO_JUMP;
    else if (hero_moving)  hero_frm = SF_HERO_RUN1 + ((frame >> 2) & 3);
    else                   hero_frm = SF_HERO_STAND;
    if (!hero_right) hero_frm += HERO_NFRM;

    /* fire: a bolt from the gun, four pixels a frame, three at a time */
    if (fire_cool) fire_cool--;
    if (JOY_BTN_1(joy) && !fire_cool) {
        unsigned char i;
        for (i = SHOT0; i < NUM_OBJ; i++) if (!dr_st[i]) break;
        if (i < NUM_OBJ) {
            g_t = hero_x + (hero_right ? 12 : (unsigned int)-12);
            dr_xlo[i] = g_t & 0xFF;
            dr_xhi[i] = g_t >> 8;
            dr_ylo[i] = hero_y & 0xFF;
            dr_yhi[i] = hero_y >> 8;
            dr_vx[i] = hero_right ? 4 : -4;
            dr_life[i] = 40;
            dr_col[i] = 7;
            dr_ptr[i] = SPR_PTR_BASE + SF_SHOT;
            dr_st[i] = 1;
            fire_cool = 8;
        }
    }
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
    if ((frame & 15) < 8) joy |= JOY_BTN_1_MASK;   /* a shot every 16 frames */
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

/* HUD row 1: energy as yellow bars (a hires-drawn tile in yellow colour
   RAM), score as five digits. Rewritten only when something changed. */
#define HUD_BAR_X 7
#define HUD_SCORE_X 28
unsigned char hud_energy = 255;
void draw_hud(void) {
    unsigned char i;
    if (energy != hud_energy) {
        for (i = 0; i < ENERGY_MAX; i++) {
            unsigned char c = (i < energy) ? T_BAR : 32;
            SCREEN_A[40 + HUD_BAR_X + i] = c;
            SCREEN_B[40 + HUD_BAR_X + i] = c;
        }
        hud_energy = energy;
    }
    if (score_dirty) {
        for (i = 0; i < 5; i++) {
            unsigned char c = 48 + score_d[i];
            SCREEN_A[40 + HUD_SCORE_X + i] = c;
            SCREEN_B[40 + HUD_SCORE_X + i] = c;
        }
        score_dirty = 0;
    }
}

/* A hit: energy down, a second of blinking. At zero the hero drops back
   in from the top of the view and everything near him is blown away. */
void hero_hit(void) {
    unsigned char i;
    hero_inv = 60;
    if (energy) energy--;
    if (energy) return;
    energy = ENERGY_MAX;
    memset(score_d, 0, 5);
    score_dirty = 1;
    hero_x = cam_px + 150;
    hero_y = cam_py + 8;
    hero_vy = 0;
    /* staggered countdowns: eight respawns in one frame cost 5 000 cycles */
    for (i = 0; i < NUM_WK; i++) if (dr_st[i] == 1) dr_st[i] = DYING - i;
}

/* ================= boss ================= */

/* A mech of four stacked sprites pacing the arena floor, jumping toward
   the hero every so often. Contact hurts, every shot on any part is a
   hit point; at zero the four parts burst for a second. One object as
   far as C is concerned, four as far as the multiplexer is. */
#define BOSS_X0 1456
#define BOSS_X1 1880
#define BOSS_FLOOR_Y 894          /* origin y with the feet on row 29's top */
#define BOSS_HP 24
unsigned int boss_x = 1720, boss_y = BOSS_FLOOR_Y;
signed char boss_vx = -1, boss_vy = 0;
unsigned char boss_hp = BOSS_HP, boss_flash = 0, boss_t = 0, boss_on = 0;
unsigned char boss_dead = 0;      /* 0 alive, 255 gone, else burst countdown */
const unsigned char boss_ox[4] = { 0, 24, 0, 24 };
const unsigned char boss_oy[4] = { 0, 0, 21, 21 };

void place_boss(unsigned char st) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        unsigned char o = BOSS0 + i;
        g_t = boss_x + boss_ox[i];
        dr_xlo[o] = g_t & 0xFF;
        dr_xhi[o] = g_t >> 8;
        g_t = boss_y + boss_oy[i];
        dr_ylo[o] = g_t & 0xFF;
        dr_yhi[o] = g_t >> 8;
        dr_st[o] = st;
    }
}

void update_boss(void) {
    unsigned char i;
    if (boss_dead == 255) return;
    if (boss_dead) {                       /* bursting; state 2 hits nothing */
        unsigned char f = SF_BOOM1 + ((48 - boss_dead) >> 4);
        for (i = 0; i < 4; i++) {
            dr_ptr[BOSS0 + i] = SPR_PTR_BASE + f;
            dr_col[BOSS0 + i] = 7;
        }
        if (--boss_dead == 0) {
            for (i = 0; i < 4; i++) dr_st[BOSS0 + i] = 0;
            boss_dead = 255;
        }
        return;
    }
    if (!near_arena()) {                   /* waits, unseen */
        for (i = 0; i < 4; i++) dr_st[BOSS0 + i] = 0;
        return;
    }
    if (boss_hits) {
        boss_flash = 4;
        if (boss_hp > boss_hits) boss_hp -= boss_hits;
        else { boss_hp = 0; boss_dead = 48; add_score(SCORE_HUNDREDS, 5); place_boss(2); return; }
        boss_hits = 0;
    }
    if (boss_flash) boss_flash--;

    if (!(frame & 1) && boss_vy < 4) boss_vy++;
    boss_y += boss_vy;
    boss_on = 0;
    if (boss_y >= BOSS_FLOOR_Y) { boss_y = BOSS_FLOOR_Y; boss_vy = 0; boss_on = 1; }
    boss_x += boss_vx;
    if (boss_x <= BOSS_X0) boss_vx = 1;
    if (boss_x >= BOSS_X1) boss_vx = -1;
    if (boss_on && ++boss_t >= 90) {       /* a leap toward the hero */
        boss_t = 0;
        boss_vy = -7;
        boss_vx = (hero_x > boss_x) ? 1 : -1;
    }
    place_boss(1);
    for (i = 0; i < 4; i++) {
        unsigned char f = SF_BOSS_TL + i;
        if (i >= 2 && (frame & 8)) f += 2;   /* the legs' second frame */
        dr_ptr[BOSS0 + i] = SPR_PTR_BASE + f;
        dr_col[BOSS0 + i] = boss_flash ? 1 : 8;
    }
}

/* ================= camera relocation ================= */

/* Put the camera where its dead zone would have it for the hero and
   redraw everything: at the start, and on the $0341 teleport hook. */
void relocate(void) {
    unsigned char r;
    int px = (int)hero_x - 150, py = (int)hero_y - 80;
    if (px < 0) px = 0;
    if (px > MAP_W * 32 - 320) px = MAP_W * 32 - 320;
    if (py < 0) py = 0;
    if (py > MAP_H * 32 - PF_ROWS * 8) py = MAP_H * 32 - PF_ROWS * 8;
    cam_cx = px >> 3; fx = px & 7;
    cam_cy = py >> 3; fy = py & 7;
    draw_full(SCREEN_A);
    draw_full(SCREEN_B);
    for (r = 0; r < PF_ROWS; r++) rowcol[r] = row_col[cam_cy + r];
    prep_step = 0;
    fill_pending = 0;
    finex_next = 7 - fx;
    finey_next = (6 - fy) & 7;
    cam_px = (unsigned int)cam_cx * 8 + fx;
    cam_py = (unsigned int)cam_cy * 8 + fy;
}

/* $0341 = n: drop the hero at spot n (1 plateau, 2 crystal cave, 3 hall,
   4 the arena). For tests and for filming. */
const unsigned int spot_x[5] = { 0, HERO_START_X, 200, 300, 1500 };
const unsigned int spot_y[5] = { 0, HERO_START_Y, 427, 683, 907 };
void teleport(unsigned char n) {
    hero_x = spot_x[n];
    hero_y = spot_y[n];
    hero_vy = 0;
    relocate();
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
    memset(COLRAM + 40 + HUD_BAR_X, 7, ENERGY_MAX);   /* energy: hires yellow */

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
    VIC_SPR_ENA = 0xFD;                          /* sprite 1 is unused */
    SCREEN_A[0x3F8] = SPR_PTR_BASE + SF_HERO_STAND;
    SCREEN_B[0x3F8] = SPR_PTR_BASE + SF_HERO_STAND;
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
    put_text2(1, 0, "IRON VEIN     FIRE SHOOTS   UP JUMPS");
    put_text2(1, 1, "ENERGY");
    put_text2(HUD_SCORE_X - 6, 1, "SCORE");
    hero_x = HERO_START_X;
    hero_y = HERO_START_Y;
    relocate();
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
        if (!(joy & (JOY_LEFT_MASK | JOY_RIGHT_MASK | JOY_UP_MASK | JOY_BTN_1_MASK)))
            joy = hero_ai();         /* any input at all silences the autopilot */
        cam_px = (unsigned int)cam_cx * 8 + fx;
        cam_py = (unsigned int)cam_cy * 8 + fy;
        t0 = NOW();
        update_hero(joy);
        update_boss();
        update_shots();
        if (hero_inv) hero_inv--;
        else if (hero_touch()) hero_hit();
        if (kills) { add_score(SCORE_TENS, kills); kills = 0; }
        draw_hud();
        camera_follow(&vx, &vy);
        PROBE(4, t0);                     /* the game's own logic */
        if (PEEK(0x0341)) {              /* teleport hook */
            teleport(PEEK(0x0341) & 7);
            POKE(0x0341, 0);
            continue;
        }
        if (!(frame & 31) && !near_arena()) {   /* walkers that stayed away */
            unsigned char i;
            for (i = 0; i < NUM_WK; i++)
                if (!dr_st[i]) { spawn_walker(i); break; }
        }
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
        SCREEN_A[0x3F8] = SPR_PTR_BASE + hero_frm;
        SCREEN_B[0x3F8] = SPR_PTR_BASE + hero_frm;
        VIC_SPR_ENA = (hero_inv & 4) ? 0xFC : 0xFD;   /* blink when hit */
        { unsigned int t = NOW(); build_mux(); PROBE(7, t); }
        PROBE(2, t0);
        PROBE(3, tf);                     /* whole frame's work */
        BORDER(0);
    }
}

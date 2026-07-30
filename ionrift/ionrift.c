/*
 * ION RIFT - horizontally smooth-scrolling shoot-em-up for the C64
 *
 * Technical targets (one step beyond Dreadline):
 *   - pixel-smooth hardware scrolling ($D016) at 50 fps
 *   - raster IRQ split: steady 3-row HUD above a scrolling playfield
 *   - double-buffered screen RAM (flip via $D018 in the IRQ)
 *   - procedural multicolor character terrain, generated column by column
 *   - multicolor sprites from the assetgen.py pipeline
 *   - SID music engine (2 voices) + sfx on voice 3
 *
 * Memory map (VIC bank 1):
 *   $4400 screen A   $4800 screen B   $5000 charset   $5800 sprites
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

/* --- Video memory (VIC bank 1) --- */
#define SCREEN_A ((unsigned char*)0x4400)
#define SCREEN_B ((unsigned char*)0x4800)
#define CHARSET  0x5000
#define SPRDATA  0x5800
#define SPR_PTR_BASE 96          /* $5800 / 64 */
#define COLRAM   ((unsigned char*)0xD800)

#define D018_A 0x14              /* screen $4400, charset $5000 */
#define D018_B 0x24              /* screen $4800, charset $5000 */

/* --- VIC / SID registers --- */
#define VIC_SPR_X(n)   (*(unsigned char*)(0xD000 + (n)*2))
#define VIC_SPR_Y(n)   (*(unsigned char*)(0xD001 + (n)*2))
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_MC     (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X  (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y  (*(unsigned char*)0xD017)
#define VIC_SPR_MC0    (*(unsigned char*)0xD025)
#define VIC_SPR_MC1    (*(unsigned char*)0xD026)
#define VIC_SPR_COL(n) (*(unsigned char*)(0xD027 + (n)))
#define VIC_BORDER     (*(unsigned char*)0xD020)
#define VIC_BG         (*(unsigned char*)0xD021)
#define VIC_MC1        (*(unsigned char*)0xD022)
#define VIC_MC2        (*(unsigned char*)0xD023)
#define CIA2_PRA       (*(unsigned char*)0xDD00)

#define SID_V1F  0xD400
#define SID_V1PW 0xD402
#define SID_V1CR 0xD404
#define SID_V1AD 0xD405
#define SID_V1SR 0xD406
#define SID_V2F  0xD407
#define SID_V2CR 0xD40B
#define SID_V2AD 0xD40C
#define SID_V2SR 0xD40D
#define SID_V3F  0xD40E
#define SID_V3CR 0xD412
#define SID_V3AD 0xD413
#define SID_V3SR 0xD414
#define SID_VOL  0xD418

/* --- IRQ engine (irq.s) --- */
extern volatile unsigned char fine_next;   /* latched at frame top */
extern volatile unsigned char d018_next;
extern volatile unsigned char vsync_flag;
void irq_init(void);

unsigned char fine_pos = 7;    /* game-side scroll position 0..7 */

/* --- Row copier (scroll.s) --- */
extern unsigned char *scr_src;
extern unsigned char *scr_dst;
extern unsigned char scr_rows;
void scroll_rows(void);

/* Playfield accent colour: colour RAM is STATIC (never scrolled).
   The classic C64 compromise — scrolling 1000 bytes of colour RAM
   costs more than a frame allows, so all '11' pixels share one
   accent colour and the terrain detail lives in the char patterns */
#define PF_COLOR 14              /* light blue accents */

/* Debug: missed-frame counter, readable at $033D from the monitor */
#define MISS_COUNTER 0x033D

/* --- Playfield geometry --- */
#define PF_TOP_ROW 3             /* first playfield char row */
#define PF_ROWS 22
#define PF_OFFSET (PF_TOP_ROW * 40)

/* --- Agent I/O (toolchain convention) ---
   $033C edge-triggered input: consumed and cleared each poll (fire)
   $033E hold input: persists until the agent rewrites it (steering)
   $0340+ telemetry block, written every play frame:
     +0 ship_y  +1 gap top px  +2 gap bottom px
     +3 nearest enemy y ($FF none)  +4 enemy distance/2  +5 state */
#define AGENT_INPUT 0x033C
#define AGENT_HOLD  0x033E
#define AGENT_TELE  0x0340
#define ST_TITLE 0
#define ST_PLAY  1
#define ST_OVER  2

/* Hold input carries a watchdog: it survives ~25 frames and then
   expires unless the agent rewrites the byte. The agent toggles bit 7
   as a heartbeat so rewriting the same direction still refreshes it.
   A dead agent can never leave the ship pinned to a wall. */
unsigned char hold_ttl = 0;
unsigned char hold_last = 0;

unsigned char read_input(void) {
    unsigned char h = PEEK(AGENT_HOLD);
    unsigned char v = joy_read(JOY_2) | PEEK(AGENT_INPUT) | (h & 0x1F);
    POKE(AGENT_INPUT, 0);
    if (h != hold_last) { hold_last = h; hold_ttl = 25; }
    else if (hold_ttl)  hold_ttl--;
    else                POKE(AGENT_HOLD, 0);
    return v;
}

/* --- Terrain model: ring buffer of column descriptors --- */
#define RING 64
#define RMASK 63
unsigned char ceil_h[RING];      /* ceiling thickness in rows (1..5) */
unsigned char floor_h[RING];     /* floor thickness in rows (1..5)  */
unsigned char tow_h[RING];       /* tower height above floor (0..8) */
unsigned char head = 0;          /* ring index of screen column 0 */

/* --- Parallax star field ---
   The stars are NOT part of the terrain: they live in their own model
   and are repainted into the back buffer once per coarse-scroll cycle.
   The row copier drags every star one char left along with the terrain,
   so a star only has to be pushed back to its old column on the cycles
   where it should NOT move - that is what makes it slower than the
   foreground. Three planes at 1/2, 1/3 and 1/4 of the terrain speed.
   Depth is cued by colour without touching the static colour RAM:
   near stars use the '11' bit pair (colour RAM, light blue), mid stars
   '10' ($D023 mid grey), far stars '01' ($D022 dark grey). */
#define NUM_STARS 15
unsigned char st_col[NUM_STARS];  /* screen column, model-tracked */
unsigned char st_row[NUM_STARS];  /* playfield row */
unsigned char st_ch[NUM_STARS];   /* tile, encodes the depth plane */
unsigned char st_div[NUM_STARS];  /* cycles per character step */
unsigned char st_cnt[NUM_STARS];
void init_stars(void);            /* defined with the scroll engine */
void update_stars(unsigned char lo, unsigned char hi);

unsigned char cur_ceil = 2, cur_floor = 2;

/* --- Game state --- */
unsigned char front = 0;         /* 0 = SCREEN_A visible */
unsigned int  ship_x = 60;
unsigned char ship_y = 140;
unsigned char shields = 3;
unsigned char invuln = 0;
unsigned int  score = 0, hi_score = 0;
unsigned char demo_mode = 0;
unsigned char game_over_flag = 0;
unsigned char frame = 0;
unsigned char anim = 0;
unsigned char spawn_timer = 90;
unsigned char speed_lvl = 0;
unsigned char hud_dirty = 1;
unsigned char ship_boom = 0;     /* death blast countdown */
unsigned char pending_over = 0;  /* game over held back until it ends */

struct { unsigned int x; unsigned char y; unsigned char active; } bolt;
struct { unsigned int x; unsigned char y; unsigned char active; } orb;

/* Hardware sprites 3..7 are recycled by the raster multiplexer in
   irq.s, so the model holds far more enemies than the VIC has sprites.
   Each entry carries its own frame and colour: nothing here touches
   the VIC directly any more, build_mux() hands the sorted list over. */
/* Twelve was over budget: the enemy pass is linear but the multiplexer
   sort is quadratic, so the last few craft cost far more than the first
   few. Eight keeps every frame inside 50 Hz with margin to spare - the
   VIC could carry more, hand-written assembly games did, but this game
   spends its cycles in C and fluidity is worth more than density. */
#define MAX_EN 8
#define MUX_SLOTS 5              /* hardware sprites 3..7 */

/* Structure of ARRAYS, not an array of structures: cc65 indexes a
   struct array with a multiply by the struct size, and these loops run
   twelve times every frame inside the multiplexer sort. X is kept in
   2-pixel units so that every comparison stays 8-bit as well - the
   16-bit arithmetic cc65 emits for an unsigned int is what made the
   first version eat half the frame. */
unsigned char en_x[MAX_EN];      /* 2-pixel units: 0..200 */
unsigned char en_y[MAX_EN];
unsigned char en_base[MAX_EN];
unsigned char en_type[MAX_EN];   /* 0 drone (sine), 1 dart (fast) */
unsigned char en_act[MAX_EN];
unsigned char en_die[MAX_EN];    /* explosion countdown */
unsigned char en_frm[MAX_EN];
unsigned char en_col[MAX_EN];

/* Tables consumed by the multiplexer IRQ (defined in irq.s) */
extern unsigned char mux_y[], mux_xlo[], mux_slot[], mux_slot2[];
extern unsigned char mux_ptr[], mux_col[], mux_xand[], mux_xor[];
extern unsigned char mux_count;

const unsigned char sine[32] = {
    0, 2, 5, 8, 11, 13, 15, 16, 16, 15, 13, 11, 8, 5, 2, 0,
    0, 254, 251, 248, 245, 243, 241, 240, 240, 241, 243, 245, 248, 251, 254, 0
};

/* ================= SID: music engine + sfx ================= */

const unsigned int FREQ[25] = {
    2228, 2360, 2500, 2649, 2806, 2973, 3150, 3338, 3536, 3747, 3970, 4206,
    4455, 4720, 5001, 5298, 5613, 5947, 6300, 6676, 7072, 7493, 7939, 8412,
    8910
};

#define REST 255
/* Tense R-Type style drive: octave-hammering minor bass,
   Am arpeggio lead with a dominant (E major) turn for tension */
const unsigned char bass_pat[16] = {
    9, 9, 21, 9, 9, 9, 21, 9,
    5, 5, 17, 5, 7, 7, 19, 7
};
const unsigned char lead_pat[32] = {
    12, 16, 21, 16, 12, 16, 21, 16,
    8, 12, 17, 12, 8, 12, 17, 12,
    10, 14, 19, 14, 10, 14, 19, 14,
    16, 20, 23, 20, 16, 20, 23, 20
};

unsigned char mframe = 0, bstep = 0, lstep = 0;
unsigned char sfx_timer = 0;
unsigned char music_on = 1;

void music_init(void) {
    unsigned char i;
    for (i = 0; i < 25; i++) POKE(SID_V1F + i, 0);
    POKE(SID_VOL, 15);
    POKE(SID_V1AD, 0x18); POKE(SID_V1SR, 0x00);   /* bass: punchy */
    POKE(SID_V1PW, 0x00); POKE(SID_V1PW + 1, 0x08);
    POKE(SID_V2AD, 0x29); POKE(SID_V2SR, 0x00);   /* lead: soft   */
    POKE(SID_V3AD, 0x0A); POKE(SID_V3SR, 0x00);   /* sfx          */
}

void music_tick(void) {
    unsigned char n;
    unsigned int f;
    if (!music_on) return;
    mframe++;
    if (mframe == 3) {                       /* staccato gate off */
        POKE(SID_V1CR, 0x40);
        POKE(SID_V2CR, 0x20);
    }
    if (mframe < 5) return;
    mframe = 0;
    n = bass_pat[bstep];
    bstep = (bstep + 1) & 15;
    if (n != REST) {
        f = FREQ[n] >> 1;                    /* bass one octave down */
        POKE(SID_V1F, f & 0xFF); POKE(SID_V1F + 1, f >> 8);
        POKE(SID_V1CR, 0x41);                /* pulse + gate */
    }
    n = lead_pat[lstep];
    lstep = (lstep + 1) & 31;
    if (n != REST) {
        f = FREQ[n];
        POKE(SID_V2F, f & 0xFF); POKE(SID_V2F + 1, f >> 8);
        POKE(SID_V2CR, 0x21);                /* sawtooth + gate */
    }
    if (sfx_timer) {
        sfx_timer--;
        if (!sfx_timer) POKE(SID_V3CR, 0x80);
    }
}

void sfx_shoot(void) {
    POKE(SID_V3CR, 0x80);
    POKE(SID_V3F, 0x00); POKE(SID_V3F + 1, 0x28);
    POKE(SID_V3CR, 0x81);
    sfx_timer = 2;
}

void sfx_boom(void) {
    POKE(SID_V3CR, 0x80);
    POKE(SID_V3F, 0x00); POKE(SID_V3F + 1, 0x06);
    POKE(SID_V3CR, 0x81);
    sfx_timer = 4;
}

/* ================= Video init ================= */

void install_video(void) {
    unsigned int i;

    /* VIC bank 1 ($4000-$7FFF) */
    CIA2_PRA = (CIA2_PRA & 0xFC) | 0x02;

    /* Charset: ROM lower/upper glyphs for text (codes 0-95),
       generated tiles at 128+ */
    __asm__("sei");
    POKE(1, PEEK(1) & 0xFB);
    memcpy((void*)CHARSET, (void*)0xD800, 96 * 8);
    POKE(1, PEEK(1) | 0x04);
    __asm__("cli");
    memset((void*)(CHARSET + 96 * 8), 0, 32 * 8);
    memcpy((void*)(CHARSET + 128 * 8), tile_gfx, sizeof(tile_gfx));

    /* Sprite frames into bank 1 */
    memcpy((void*)SPRDATA, sprite_gfx, sizeof(sprite_gfx));

    /* Clear both screens */
    memset(SCREEN_A, 32, 1000);
    memset(SCREEN_B, 32, 1000);

    /* HUD colour rows (hires white / cyan separator) */
    for (i = 0; i < 80; i++) COLRAM[i] = 1;
    for (i = 80; i < 120; i++) COLRAM[i] = 3;

    VIC_BG = 0;
    VIC_BORDER = 0;
    VIC_MC1 = 11;                /* shared '01' dark gray  */
    VIC_MC2 = 12;                /* shared '10' mid gray   */

    /* Sprites: all multicolor, none expanded until a blast needs it */
    VIC_SPR_MC = 0xFF;
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_MC0 = 8;             /* shared: orange glow */
    VIC_SPR_MC1 = 1;             /* shared: white             */
    VIC_SPR_COL(0) = 3;          /* ship cyan     */
    VIC_SPR_COL(1) = 1;          /* bolt white    */
    VIC_SPR_COL(2) = 8;          /* orb orange    */
    VIC_SPR_COL(3) = 4;          /* enemies...    */
    VIC_SPR_COL(4) = 4;
    VIC_SPR_COL(5) = 4;
    VIC_SPR_COL(6) = 4;
    VIC_SPR_COL(7) = 4;
}

void set_sprite_frame(unsigned char spr, unsigned char frm) {
    SCREEN_A[0x3F8 + spr] = SPR_PTR_BASE + frm;
    SCREEN_B[0x3F8 + spr] = SPR_PTR_BASE + frm;
}

void set_sprite_pos(unsigned char spr, unsigned int x, unsigned char y) {
    VIC_SPR_X(spr) = x & 0xFF;
    VIC_SPR_Y(spr) = y;
    if (x > 255) VIC_SPR_HI_X |= 1 << spr;
    else         VIC_SPR_HI_X &= ~(1 << spr);
}

/* ================= Text (own renderer, screens at $4400/$4800) ===== */

void put_text(unsigned char *scr, unsigned char x, unsigned char y,
              const char *s) {
    unsigned int o = y * 40 + x;
    unsigned char c;
    while ((c = (unsigned char)*s++) != 0) {
        /* cc65 string literals are PETSCII: uppercase letters arrive
           as $C1-$DA and must become screen codes $41-$5A */
        if (c >= 0xC1 && c <= 0xDA) c -= 0x80;
        scr[o++] = c;
    }
}

void put_text2(unsigned char x, unsigned char y, const char *s) {
    put_text(SCREEN_A, x, y, s);
    put_text(SCREEN_B, x, y, s);
}

void put_num2(unsigned char x, unsigned char y, unsigned int v,
              unsigned char w) {
    char buf[6];
    signed char i;
    for (i = w - 1; i >= 0; i--) { buf[i] = '0' + (v % 10); v /= 10; }
    buf[w] = 0;
    put_text2(x, y, buf);
}

/* ================= Terrain generation ================= */

void gen_column(unsigned char idx) {
    unsigned char r = rand() & 15;
    unsigned char gap;

    if (r < 4 && cur_ceil > 1) cur_ceil--;
    else if (r > 11 && cur_ceil < 5) cur_ceil++;
    r = rand() & 15;
    if (r < 4 && cur_floor > 1) cur_floor--;
    else if (r > 11 && cur_floor < 5) cur_floor++;

    /* keep a flyable corridor of at least 10 rows */
    gap = PF_ROWS - cur_ceil - cur_floor;
    while (gap < 10) {
        if (cur_ceil > 1) cur_ceil--;
        else cur_floor--;
        gap++;
    }

    ceil_h[idx] = cur_ceil;
    floor_h[idx] = cur_floor;

    /* occasional tower on the floor */
    if ((rand() & 31) == 0) {
        tow_h[idx] = 2 + (rand() & 3);
        if (tow_h[idx] > gap - 6) tow_h[idx] = gap - 6;
    } else tow_h[idx] = 0;
}

/* Char for playfield row r (0..21) of ring column idx */
unsigned char column_char(unsigned char idx, unsigned char r) {
    unsigned char fl_top = PF_ROWS - floor_h[idx];

    if (r < ceil_h[idx]) {
        if (r == ceil_h[idx] - 1) return T_CEIL_BOT;
        /* the ring index is a stable per-column identity, so this
           greeble stays put on its column while the deck scrolls */
        return ((idx & 3) == 2 && r == 0) ? T_GREEBLE : T_CEIL_FILL;
    }
    if (r >= fl_top) {
        if (r == fl_top) return T_FLOOR_TOP;
        return ((idx & 3) == 1 && r == fl_top + 1) ? T_GREEBLE : T_FLOOR_FILL;
    }
    if (tow_h[idx] && r >= fl_top - tow_h[idx])
        return (r == fl_top - tow_h[idx]) ? T_TOWER_TOP : T_TOWER;
    return 32;
}

void write_column_chars(unsigned char *scr, unsigned char scr_col,
                        unsigned char idx) {
    unsigned char r;
    unsigned int o = PF_OFFSET + scr_col;
    for (r = 0; r < PF_ROWS; r++) {
        scr[o] = column_char(idx, r);
        o += 40;
    }
}

void init_colors(void) {
    unsigned char r;
    for (r = PF_TOP_ROW; r < PF_TOP_ROW + PF_ROWS; r++)
        memset(COLRAM + r * 40, PF_COLOR, 40);
}

void init_terrain(void) {
    unsigned char c;
    head = 0;
    cur_ceil = 2;
    cur_floor = 2;
    for (c = 0; c <= 40; c++) gen_column(c & RMASK);
    for (c = 0; c < 40; c++) {
        write_column_chars(SCREEN_A, c, c);
        write_column_chars(SCREEN_B, c, c);
    }
    init_colors();
    init_stars();
}

/* ================= Scroll engine ================= */

unsigned char *front_scr(void) { return front ? SCREEN_B : SCREEN_A; }
unsigned char *back_scr(void)  { return front ? SCREEN_A : SCREEN_B; }

/* r * 40 without a multiply: cc65 calls a subroutine for every one and
   the star layer needs two per star per coarse step */
const unsigned int row_ofs[PF_ROWS] = {
      0,  40,  80, 120, 160, 200, 240, 280, 320, 360, 400,
    440, 480, 520, 560, 600, 640, 680, 720, 760, 800, 840
};

void init_stars(void) {
    unsigned char i, d;
    for (i = 0; i < NUM_STARS; i++) {
        d = i % 3;                       /* three depth planes */
        st_col[i] = rand() % 39;
        st_row[i] = 1 + (rand() % (PF_ROWS - 2));
        st_div[i] = d + 2;               /* 1/2, 1/3, 1/4 of terrain speed */
        st_ch[i] = (d == 0) ? T_STAR_NEAR :
                   (d == 1) ? T_STAR_MID : T_STAR_FAR;
        st_cnt[i] = rand() & 3;
    }
}

/* Repaint the star layer into the back buffer. Runs once per coarse
   cycle, right after the new terrain column is written, using the ring
   index the buffer will be displayed with (head has not advanced yet).
   A star is drawn only where the terrain model says the sky is empty,
   so the foreground occludes the background for free. */
/* Repaint one horizontal band of the star layer. Called right after the
   row copier has dealt with those rows, so the cost rides along with the
   four copy frames instead of landing on one. */
void update_stars(unsigned char lo, unsigned char hi) {
    unsigned char i, oldc, r;
    unsigned char nh = (head + 1) & RMASK;
    unsigned char *scr = back_scr() + PF_OFFSET;
    unsigned int ro;

    for (i = 0; i < NUM_STARS; i++) {
        r = st_row[i];
        if (r < lo || r >= hi) continue;     /* another band's turn */
        ro = row_ofs[r];
        if (st_col[i] == 0) {
            /* dragged off the left edge: respawn on the right, inside the
               same band so no band gets painted twice in one cycle */
            st_col[i] = 38;                  /* 39 is the new terrain column */
            r = st_row[i] = lo + (rand() % (hi - lo));
            ro = row_ofs[r];
        } else {
            oldc = st_col[i] - 1;            /* where the copier left it */
            scr[ro + oldc] = column_char((nh + oldc) & RMASK, r);
            if (++st_cnt[i] >= st_div[i]) { st_cnt[i] = 0; st_col[i] = oldc; }
        }
        if (column_char((nh + st_col[i]) & RMASK, r) == 32)
            scr[ro + st_col[i]] = st_ch[i];
    }
}

/* The coarse-scroll work is spread across the fine-scroll cycle so that
   no single frame carries a heavy job:
     fine 6:   generate the next column and cache its characters
     fine 5-2: copy 5-6 playfield rows, then repaint that band of stars
     fine 1:   write the cached column into the back buffer
     fine 0:   flip only - colour RAM is static, so this frame is nearly free */
unsigned char char_cache[PF_ROWS];

void copy_row_chunk(unsigned char first, unsigned char count) {
    unsigned int o = PF_OFFSET + first * 40;
    scr_src = front_scr() + o + 1;
    scr_dst = back_scr() + o;
    scr_rows = count;
    scroll_rows();
}

unsigned char scroll_step(void) {
    unsigned char r;
    unsigned int o;

    if (fine_pos == 0) {
        fine_pos = 7;
        front ^= 1;
        d018_next = front ? D018_B : D018_A;
        fine_next = 7;          /* latched together with the flip */
        head = (head + 1) & RMASK;
        return 1;
    }
    fine_pos--;
    fine_next = fine_pos;
    switch (fine_pos) {
    case 6:
        gen_column((head + 40) & RMASK);
        for (r = 0; r < PF_ROWS; r++)
            char_cache[r] = column_char((head + 40) & RMASK, r);
        break;
    case 5: copy_row_chunk(0, 6);  update_stars(0, 6);   break;
    case 4: copy_row_chunk(6, 6);  update_stars(6, 12);  break;
    case 3: copy_row_chunk(12, 5); update_stars(12, 17); break;
    case 2: copy_row_chunk(17, 5); update_stars(17, 22); break;
    case 1: {
        unsigned char *bs = back_scr();      /* was called once per row */
        o = PF_OFFSET + 39;
        for (r = 0; r < PF_ROWS; r++) {
            bs[o] = char_cache[r];
            o += 40;
        }
        break;
    }
    }
    return 0;
}

/* Full model-driven repaint of both buffers (mode transitions) */
void full_redraw(void) {
    unsigned char c, idx;
    for (c = 0; c < 40; c++) {
        idx = (head + c) & RMASK;
        write_column_chars(SCREEN_A, c, idx);
        write_column_chars(SCREEN_B, c, idx);
    }
    init_colors();
    init_stars();
}

/* ================= HUD ================= */

void draw_hud_static(void) {
    unsigned char c;
    put_text2(1, 0, "SCORE 00000   HI 00000    SHIELDS 3");
    put_text2(1, 1, "I O N   R I F T");
    put_num2(28, 1, (unsigned int)speed_lvl + 1, 1);
    put_text2(22, 1, "SPEED");
    for (c = 0; c < 40; c++) {
        SCREEN_A[80 + c] = T_HUDLINE;
        SCREEN_B[80 + c] = T_HUDLINE;
    }
}

void update_hud(void) {
    put_num2(7, 0, score, 5);
    put_num2(18, 0, hi_score, 5);
    put_num2(35, 0, shields, 1);
    put_num2(28, 1, (unsigned int)speed_lvl + 1, 1);
}

/* ================= Collisions ================= */

unsigned char terrain_hit(unsigned int x, unsigned char y) {
    unsigned char c, r, idx, fl_top;
    /* sprite centre to playfield cell */
    if (x < 28) return 0;
    c = (x - 20) / 8;
    if (c > 39) return 0;
    r = (y - 46) / 8;
    if (r < PF_TOP_ROW) return 1;
    r -= PF_TOP_ROW;
    if (r >= PF_ROWS) return 1;
    idx = (head + c) & RMASK;
    fl_top = PF_ROWS - floor_h[idx];
    if (r < ceil_h[idx]) return 1;
    if (r >= fl_top) return 1;
    if (tow_h[idx] && r >= fl_top - tow_h[idx]) return 1;
    return 0;
}

/* ================= Agent telemetry ================= */

void write_telemetry(void) {
    unsigned char idx = (head + (ship_x - 20) / 8) & RMASK;
    unsigned char su = (unsigned char)(ship_x >> 1);
    unsigned char best = 0xFF, ey = 0xFF, i;

    POKE(AGENT_TELE + 0, ship_y);
    POKE(AGENT_TELE + 1, 74 + (ceil_h[idx] << 3));
    POKE(AGENT_TELE + 2,
         74 + ((PF_ROWS - floor_h[idx] - tow_h[idx]) << 3));
    for (i = 0; i < MAX_EN; i++) {
        if (en_act[i] && en_x[i] > su) {
            if (en_x[i] - su < best) { best = en_x[i] - su; ey = en_y[i]; }
        }
    }
    POKE(AGENT_TELE + 3, ey);
    POKE(AGENT_TELE + 4, best);
    POKE(AGENT_TELE + 5, ST_PLAY);
}

/* ================= Entities ================= */

/* --- Explosion playback, shared by enemies and the ship ---
   Four phases over 20 frames: white flash, expanded fireball,
   breaking ring, cooling debris. The per-sprite colour cycles
   white -> yellow -> orange -> red while the shared registers keep
   the orange glow and white hot-spots, and the VIC doubles the
   sprite during the fireball so the blast dwarfs what it came from. */
#define BOOM_FRAMES 20

unsigned char boom_frame(unsigned char d) {
    if (d > 14) return SF_EXPL0;         /* white flash      */
    if (d > 9)  return SF_EXPL1;         /* fireball         */
    if (d > 4)  return SF_EXPL2;         /* breaking ring    */
    return SF_EXPL3;                     /* cooling debris   */
}

unsigned char boom_color(unsigned char d) {
    if (d > 14) return 1;
    if (d > 9)  return 7;
    if (d > 4)  return 8;
    return 2;
}

/* Only the ship gets hardware expansion: $D017/$D01D are per hardware
   sprite, and a multiplexed slot would drag the stretch onto whatever
   enemy recycles it further down the screen. */
void draw_ship_boom(unsigned char d) {
    set_sprite_frame(0, boom_frame(d));
    VIC_SPR_COL(0) = boom_color(d);
    if (d > 9 && d <= 14) {
        VIC_SPR_DBL_X |= 1;
        VIC_SPR_DBL_Y |= 1;
        set_sprite_pos(0, ship_x > 36 ? ship_x - 12 : ship_x,
                          ship_y > 60 ? ship_y - 10 : ship_y);
    } else {
        VIC_SPR_DBL_X &= ~1;
        VIC_SPR_DBL_Y &= ~1;
        set_sprite_pos(0, ship_x, ship_y);
    }
}

/* Sort the live enemies by Y and pack them into the multiplexer tables.
   An entry is dropped when it would recycle a hardware slot before the
   raster cleared the previous occupant - graceful degradation instead
   of a torn sprite. */
void build_mux(void) {
    unsigned char n = 0, i, j, k, t, y, u, slot, mask;
    unsigned char ord[MAX_EN], ys[MAX_EN];

    for (i = 0; i < MAX_EN; i++) {
        if (en_act[i] | en_die[i]) {
            y = en_y[i];
            if (y >= 240) continue;      /* off screen, and $FF is the marker */
            ord[n] = i; ys[n] = y; n++;
        }
    }
    if (!n) { mux_y[0] = 0xFF; mux_count = 0; return; }
    for (i = 1; i < n; i++) {            /* insertion sort, ascending Y */
        y = ys[i]; t = ord[i]; j = i;
        while (j && ys[j - 1] > y) {
            ys[j] = ys[j - 1]; ord[j] = ord[j - 1]; j--;
        }
        ys[j] = y; ord[j] = t;
    }
    k = 0; slot = 3;
    for (i = 0; i < n; i++) {
        if (k >= MUX_SLOTS && ys[i] < mux_y[k - MUX_SLOTS] + 22) continue;
        t = ord[i];
        mask = 1 << slot;
        u = en_x[t];
        mux_y[k] = ys[i];
        if (u >= 128) { mux_xlo[k] = (u - 128) << 1; mux_xor[k] = mask; }
        else          { mux_xlo[k] = u << 1;         mux_xor[k] = 0;    }
        mux_slot[k] = slot;
        mux_slot2[k] = slot << 1;
        mux_ptr[k] = SPR_PTR_BASE + en_frm[t];
        mux_col[k] = en_col[t];
        mux_xand[k] = ~mask;
        k++;
        if (++slot > 7) slot = 3;
    }
    mux_y[k] = 0xFF;                     /* terminator for the IRQ */
    mux_count = k;
}

void hide_enemy(unsigned char i) {
    en_act[i] = 0;
    en_die[i] = 0;
    en_y[i] = 0xFF;              /* parked: build_mux skips it */
}

void spawn_one(unsigned char type, unsigned char xu, unsigned char y) {
    unsigned char i;
    for (i = 0; i < MAX_EN; i++) {
        if (!en_act[i] && !en_die[i]) {
            /* keep the sine bob inside the playfield: a Y reaching $FF
               would read as the multiplexer's end-of-list marker */
            if (y < 96)  y = 96;
            if (y > 200) y = 200;
            en_act[i] = 1;
            en_type[i] = type;
            en_x[i] = xu;
            en_base[i] = y;
            en_y[i] = y;
            en_frm[i] = type ? SF_DART : SF_DRONE0;
            en_col[i] = type ? 7 : 4;
            return;
        }
    }
}

/* A wave is what the multiplexer buys: formations instead of singles */
void spawn_wave(void) {
    unsigned char n = 3 + (rand() & 1);          /* 3..4 craft */
    unsigned char shape = rand() & 3;
    unsigned char base = 104 + (rand() % 60);
    unsigned char k, xu;

    for (k = 0; k < n; k++) {
        xu = 172 + k * 9;                        /* 2-pixel units */
        switch (shape) {
        case 0:  spawn_one(0, xu, base + k * 12); break;   /* descending */
        case 1:  spawn_one(0, xu, base + (n - k) * 12); break; /* climbing */
        case 2:  spawn_one(0, xu, base); break;            /* line abreast */
        default: spawn_one(k < 2 ? 1 : 0, xu, base + k * 14); break;
        }
    }
}

void kill_enemy(unsigned char i) {
    en_act[i] = 0;
    en_die[i] = BOOM_FRAMES;
    score += en_type[i] ? 50 : 25;
    hud_dirty = 1;
    sfx_boom();
}

void ship_hit(void) {
    if (invuln) return;
    sfx_boom();
    VIC_BORDER = 2;
    shields--;
    hud_dirty = 1;
    invuln = 60;
    ship_boom = BOOM_FRAMES;     /* blast plays where the ship died */
    if (shields == 0) pending_over = 1;
}

void update_enemies(void) {
    unsigned char i, ex, ey, d;
    unsigned char su = (unsigned char)(ship_x >> 1);   /* ship in units */
    unsigned char bu = (unsigned char)(bolt.x >> 1);

    for (i = 0; i < MAX_EN; i++) {
        if (en_die[i]) {
            d = --en_die[i];
            en_frm[i] = boom_frame(d);
            en_col[i] = boom_color(d);
            if (!d) hide_enemy(i);
            continue;
        }
        if (!en_act[i]) continue;

        ex = en_x[i];
        d = en_type[i] ? 2 : 1;                  /* 4px or 2px per frame */
        if (ex <= d + 5) { hide_enemy(i); continue; }
        ex -= d;
        en_x[i] = ex;

        if (en_type[i] == 0) {
            ey = en_base[i] + (signed char)sine[((frame >> 2) + (i << 2)) & 31];
            en_y[i] = ey;
            en_frm[i] = (frame & 8) ? SF_DRONE0 : SF_DRONE1;
        } else ey = en_y[i];

        /* enemy fires the shared orb */
        if (!orb.active && ex > su + 20 && (rand() & 63) == 0) {
            orb.active = 1;
            orb.x = (unsigned int)ex << 1;
            orb.y = ey + 4;
        }

        /* bolt vs enemy (all 8-bit now) */
        if (bolt.active && bu + 10 >= ex && bu <= ex + 8 &&
            bolt.y + 6 >= ey && bolt.y <= ey + 12) {
            kill_enemy(i);
            bolt.active = 0;
            set_sprite_pos(1, 0, 0);
            continue;
        }

        /* enemy vs ship */
        if (!invuln && su + 10 >= ex && su <= ex + 8 &&
            ship_y + 10 >= ey && ship_y <= ey + 12) {
            kill_enemy(i);
            ship_hit();
        }
    }
}

/* ================= Game loop ================= */

/* Frames elapsed since last call; if a frame overran, the caller
   catches up by running the scroll that many times (2px step beats
   a visible stall) */
unsigned char wait_frames(void) {
    unsigned char n;
    while (!vsync_flag) ;
    n = vsync_flag;
    vsync_flag = 0;
    if (n > 1) POKE(MISS_COUNTER, PEEK(MISS_COUNTER) + n - 1);
    /* Never replay the scroll to catch up: doing two coarse steps in one
       frame costs twice the row copy, which guarantees another overrun -
       an avalanche. Losing a frame simply means the world moved a little
       slower for one frame, which nobody can see. */
    return 1;
}

/* --- Solid text panel over the scrolling terrain (no ghosting):
   the whole band is repainted every coarse-scroll cycle --- */
#define PANEL_TOP 7
#define PANEL_BOT 18             /* exclusive */

void panel_colors(void) {
    unsigned char r;
    for (r = PANEL_TOP; r < PANEL_BOT; r++)
        memset(COLRAM + r * 40, 1, 40);
}

void reset_game(void) {
    unsigned char i;
    score = 0;
    shields = 3;
    invuln = 50;
    ship_x = 60;
    ship_y = 140;
    game_over_flag = 0;
    pending_over = 0;
    ship_boom = 0;
    spawn_timer = 90;
    speed_lvl = 0;
    bolt.active = 0;
    orb.active = 0;
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_COL(0) = 3;
    for (i = 0; i < MAX_EN; i++) hide_enemy(i);
    mux_y[0] = 0xFF;
    mux_count = 0;
    set_sprite_pos(1, 0, 0);
    set_sprite_pos(2, 0, 0);
}

void play(void) {
    unsigned char joy, t;
    unsigned int demo_timer = 0;

    reset_game();
    full_redraw();               /* wipe the title panel, restore colours */
    draw_hud_static();
    update_hud();
    VIC_SPR_ENA = 0xFF;
    set_sprite_frame(0, SF_SHIP0);

    while (!game_over_flag) {
        {
            unsigned char n = wait_frames();
            while (n--) {
                frame++;
                music_tick();
                scroll_step();
            }
        }

        if (invuln) {
            invuln--;
            /* the respawn blink must never hide the death blast */
            VIC_SPR_ENA = (!ship_boom && (invuln & 2)) ? 0xFE : 0xFF;
            if (invuln == 52) VIC_BORDER = 0;   /* short damage flash */
            if (!invuln) { VIC_BORDER = 0; VIC_SPR_ENA = 0xFF; }
        }

        joy = read_input();
        if (demo_mode && joy) { demo_mode = 0; }

        if (ship_boom) {
            /* controls are dead while the wreck burns */
        } else if (demo_mode) {
            /* autopilot: stay mid-gap, dodge, shoot */
            unsigned char idx = (head + (ship_x - 20) / 8) & RMASK;
            unsigned char target =
                74 + ((ceil_h[idx] + PF_ROWS - floor_h[idx]) << 2);
            if (ship_y < target - 2) ship_y += 1;
            else if (ship_y > target + 2) ship_y -= 1;
            if (!bolt.active && (rand() & 7) == 0) joy = 0x10;
            demo_timer++;
            if (demo_timer > 900) return;
        } else {
            if (JOY_UP(joy) && ship_y > 78) ship_y -= 2;
            if (JOY_DOWN(joy) && ship_y < 230) ship_y += 2;
            if (JOY_LEFT(joy) && ship_x > 28) ship_x -= 2;
            if (JOY_RIGHT(joy) && ship_x < 300) ship_x += 2;
        }

        if (JOY_BTN_1(joy) && !bolt.active && !ship_boom) {
            bolt.active = 1;
            bolt.x = ship_x + 18;
            bolt.y = ship_y + 6;
            sfx_shoot();
        }

        /* ship, or what is left of it */
        if (ship_boom) {
            ship_boom--;
            draw_ship_boom(ship_boom);
            if (!ship_boom) {
                VIC_SPR_COL(0) = 3;
                if (pending_over) game_over_flag = 1;
                else { ship_x = 60; ship_y = 140; }
            }
        } else {
            set_sprite_frame(0, (frame & 4) ? SF_SHIP0 : SF_SHIP1);
            set_sprite_pos(0, ship_x, ship_y);
            if (!invuln && terrain_hit(ship_x + 10, ship_y + 10)) {
                sfx_boom();
                ship_hit();
            }
        }

        /* bolt */
        if (bolt.active) {
            bolt.x += 6;
            if (bolt.x > 340) { bolt.active = 0; set_sprite_pos(1, 0, 0); }
            else {
                set_sprite_frame(1, SF_PBOLT);
                set_sprite_pos(1, bolt.x, bolt.y);
            }
        }

        /* enemy orb */
        if (orb.active) {
            orb.x -= 3;
            if (orb.x < 12) { orb.active = 0; set_sprite_pos(2, 0, 0); }
            else {
                set_sprite_frame(2, SF_EORB);
                set_sprite_pos(2, orb.x, orb.y);
                if (!invuln &&
                    ship_x + 18 >= orb.x && ship_x + 4 <= orb.x + 6 &&
                    ship_y + 12 >= orb.y && ship_y + 2 <= orb.y + 6) {
                    orb.active = 0;
                    set_sprite_pos(2, 0, 0);
                    ship_hit();
                }
            }
        }

        update_enemies();

        /* wave spawning, tighter over time */
        t = 150 - (speed_lvl << 5);
        if (--spawn_timer == 0) {
            spawn_timer = t < 60 ? 60 : t;
            spawn_wave();
        }
        if (score >= 500 && speed_lvl < 1) { speed_lvl = 1; hud_dirty = 1; }
        if (score >= 1500 && speed_lvl < 2) { speed_lvl = 2; hud_dirty = 1; }
        if (score >= 3000 && speed_lvl < 3) { speed_lvl = 3; hud_dirty = 1; }

        build_mux();             /* sorted hand-off to the raster IRQ */
        write_telemetry();

        if (hud_dirty) { update_hud(); hud_dirty = 0; }
    }

    if (score > hi_score) hi_score = score;
}

/* ================= Screens ================= */

void stamp_title(unsigned char *scr, unsigned char blink) {
    unsigned char r;
    for (r = PANEL_TOP; r < PANEL_BOT; r++)
        memset(scr + r * 40, 32, 40);
    put_text(scr, 9,  8,  "*  I O N   R I F T  *");
    put_text(scr, 7,  11, "SMOOTH SCROLL + RASTER IRQ");
    put_text(scr, 7,  12, "DOUBLE BUFFER + SID MUSIC");
    if (blink)
        put_text(scr, 10, 15, "PRESS FIRE TO START");
    put_text(scr, 6,  17, "(C) 2026 AI TOOLCHAIN FABLE");
}

/* Title and game over are STATIC screens: text inside the fine-scroll
   zone would wobble with the terrain, so the scroll engine idles here
   and ignition happens when the game starts */
void title_screen(void) {
    unsigned int timer = 0;
    unsigned char joy;

    VIC_SPR_ENA = 0x00;
    VIC_BORDER = 0;
    mux_count = 0;               /* retire the multiplexer IRQ chain: the
                                    sprites are off, and leaving the old
                                    list live keeps firing interrupts down
                                    the lower half of a static screen */
    mux_y[0] = 0xFF;
    POKE(AGENT_TELE + 5, ST_TITLE);
    POKE(AGENT_HOLD, 0);
    draw_hud_static();
    update_hud();
    stamp_title(SCREEN_A, 1);
    stamp_title(SCREEN_B, 1);
    panel_colors();

    while (1) {
        wait_frames();
        frame++;
        music_tick();
        timer++;
        if ((timer & 0x1F) == 0) {
            if (timer & 0x20) put_text2(10, 15, "PRESS FIRE TO START");
            else              put_text2(10, 15, "                   ");
        }
        joy = read_input();
        if (JOY_BTN_1(joy)) { demo_mode = 0; break; }
        if (timer > 700) { demo_mode = 1; break; }
    }
}

void stamp_gameover(unsigned char *scr, unsigned char blink) {
    unsigned char r;
    char buf[6];
    signed char i;
    unsigned int v = score;
    for (r = PANEL_TOP; r < PANEL_BOT; r++)
        memset(scr + r * 40, 32, 40);
    put_text(scr, 11, 9, "G A M E   O V E R");
    for (i = 4; i >= 0; i--) { buf[i] = '0' + (v % 10); v /= 10; }
    buf[5] = 0;
    put_text(scr, 12, 12, "SCORE");
    put_text(scr, 18, 12, buf);
    if (blink)
        put_text(scr, 9, 15, "PRESS FIRE TO CONTINUE");
}

void game_over_screen(void) {
    unsigned int timer = 0;

    VIC_SPR_ENA = 0x00;
    VIC_BORDER = 0;              /* clear a mid-flash red border */
    mux_count = 0;               /* static screen: no sprites to multiplex */
    mux_y[0] = 0xFF;
    POKE(AGENT_TELE + 5, ST_OVER);
    POKE(AGENT_HOLD, 0);
    stamp_gameover(SCREEN_A, 1);
    stamp_gameover(SCREEN_B, 1);
    panel_colors();

    while (1) {
        wait_frames();
        frame++;
        music_tick();
        timer++;
        if ((timer & 0x1F) == 0) {
            if (timer & 0x20) put_text2(9, 15, "PRESS FIRE TO CONTINUE");
            else              put_text2(9, 15, "                      ");
        }
        if (JOY_BTN_1(read_input())) break;
        if (timer > 800) break;
    }
}

int main(void) {
    srand(0xC64);
    joy_install(joy_static_stddrv);
    POKE(AGENT_INPUT, 0);
    POKE(AGENT_HOLD, 0);

    install_video();
    music_init();
    init_terrain();
    irq_init();

    while (1) {
        title_screen();
        if (!demo_mode) { /* player game */ }
        play();
        if (!demo_mode) game_over_screen();
    }
    return 0;
}

/*
 * GHOST KEEP - a Ghosts'n Goblins tribute for the Commodore 64
 *
 * Level 1: the graveyard. Arthur runs, jumps the arcade's rigid arc,
 * throws lances, loses his armour on the first hit and his life on the
 * second, while zombies claw their way out of the turf.
 *
 * Engine carried over from ION RIFT:
 *   - pixel-smooth $D016 scrolling with double-buffered screen RAM
 *   - raster split: steady HUD above a scrolling playfield
 *   - sprite multiplexer recycling hardware sprites 3-7
 * New here:
 *   - a hand-designed tile map instead of procedural terrain
 *   - platform physics: ground following, fixed-arc jump, landing
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
#include "level.h"

/* --- Video memory (VIC bank 1) --- */
#define SCREEN_A ((unsigned char*)0x4400)
#define SCREEN_B ((unsigned char*)0x4800)
#define CHARSET  0x5000
#define SPRDATA  0x5800
#define SPR_PTR_BASE 96
#define COLRAM   ((unsigned char*)0xD800)
#define D018_A 0x14
#define D018_B 0x24

/* --- VIC / SID --- */
#define VIC_SPR_X(n)   (*(unsigned char*)(0xD000 + (n)*2))
#define VIC_SPR_Y(n)   (*(unsigned char*)(0xD001 + (n)*2))
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_MC     (*(unsigned char*)0xD01C)
#define VIC_SPR_PRIO   (*(unsigned char*)0xD01B)   /* 1 = behind background */
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

/* --- engine hooks --- */
extern volatile unsigned char fine_next, d018_next, vsync_flag;
void irq_init(void);
extern unsigned char *scr_src, *scr_dst;
extern unsigned char scr_rows;
void scroll_rows(void);
extern unsigned char mux_y[], mux_xlo[], mux_slot[], mux_slot2[];
extern unsigned char mux_ptr[], mux_col[], mux_xand[], mux_xor[];
extern unsigned char mux_count;

/* --- geometry: 2 HUD rows, 23 rows of graveyard --- */
#define PF_TOP_ROW 2
#define PF_ROWS 23
#define PF_OFFSET (PF_TOP_ROW * 40)

/* Sprite Y of the top of playfield row r */
#define ROW_Y(r) (50 + ((PF_TOP_ROW + (r)) << 3))

const unsigned int row_ofs[PF_ROWS] = {
      0,  40,  80, 120, 160, 200, 240, 280, 320, 360, 400, 440,
    480, 520, 560, 600, 640, 680, 720, 760, 800, 840, 880
};

/* --- agent I/O, same convention as the rest of the toolchain --- */
#define AGENT_INPUT 0x033C
#define AGENT_HOLD  0x033E
#define AGENT_TELE  0x0340
#define MISS_COUNTER 0x033D

unsigned char hold_last = 0, hold_ttl = 0;

unsigned char read_input(void) {
    unsigned char h = PEEK(AGENT_HOLD);
    unsigned char v = joy_read(JOY_2) | PEEK(AGENT_INPUT) | (h & 0x1F);
    POKE(AGENT_INPUT, 0);
    if (h != hold_last) { hold_last = h; hold_ttl = 25; }
    else if (hold_ttl)  hold_ttl--;
    else                POKE(AGENT_HOLD, 0);
    return v;
}

/* --- scroll state --- */
unsigned char fine_pos = 7;
unsigned char front = 0;
unsigned char head = 0;          /* level column shown at screen column 0 */
unsigned char char_cache[PF_ROWS];

unsigned char *front_scr(void) { return front ? SCREEN_B : SCREEN_A; }
unsigned char *back_scr(void)  { return front ? SCREEN_A : SCREEN_B; }

/* --- parallax far plane ---
   The row copier already drags these one character left with the
   graveyard, so an item only has to be pushed back to its old column
   on the cycles it should NOT move: that is what makes the horizon
   travel slower than the ground the player runs on. */
#define NUM_BG 14
unsigned char bg_col[NUM_BG], bg_row[NUM_BG], bg_ch[NUM_BG];
unsigned char bg_div[NUM_BG], bg_cnt[NUM_BG];

/* --- player --- */
#define ART_SPR 0
#define LANCE_SPR 1
unsigned char art_col = 6;       /* screen column Arthur stands on */
unsigned char art_sub = 4;       /* pixel offset inside that column */
unsigned char art_row;           /* playfield row his feet rest on */
unsigned char art_y;             /* sprite Y, driven by the jump arc */
unsigned char art_frm = SF_ART_STAND;
unsigned char jump_t = 0;        /* position along the arc while rising */
unsigned char airborne = 0;      /* 1 while the turf is not under him */
unsigned char jump_base;         /* foot level captured at take-off */
unsigned char face_right = 1;
unsigned char armour = 1;        /* 1 = knight, 0 = underwear */
unsigned char lives = 3;
unsigned char invuln = 0;
unsigned char anim = 0;
unsigned int  score = 0;
unsigned char timer = 99;
unsigned char sec_frames = 50;
unsigned char frame = 0;
unsigned char hud_dirty = 1;
unsigned char dead_timer = 0;
unsigned char game_over = 0;
unsigned char demo_mode = 0;

/* The arcade jump is a fixed parabola with no air control: once you
   leave the turf your fate is written. Height offsets in pixels above
   the standing position, one entry per frame. */
const unsigned char jump_arc[28] = {
     6, 12, 17, 21, 25, 28, 31, 33, 35, 36, 37, 37, 37, 36,
    35, 33, 31, 28, 25, 21, 17, 12,  6,  0,  0,  0,  0,  0
};
#define JUMP_LEN 24

struct { unsigned int x; unsigned char y, active, right; } lance;

/* --- enemies (multiplexed) --- */
#define MAX_EN 6
#define MUX_SLOTS 5
unsigned char en_x[MAX_EN];      /* screen pixel / 2 */
unsigned char en_y[MAX_EN];
unsigned char en_type[MAX_EN];   /* 0 zombie, 1 crow */
unsigned char en_act[MAX_EN];
unsigned char en_st[MAX_EN];     /* zombie: rise countdown; crow: phase */
unsigned char en_frm[MAX_EN];
unsigned char en_col[MAX_EN];
unsigned char spawn_timer = 60;
unsigned char scrolled_px = 0;   /* world pixels owed to the enemies */

/* ================= sound ================= */

void sound_init(void) {
    unsigned char i;
    for (i = 0; i < 25; i++) POKE(SID_V1F + i, 0);
    POKE(SID_VOL, 15);
    POKE(SID_V1AD, 0x19); POKE(SID_V1SR, 0x00);
    POKE(SID_V1PW, 0x00); POKE(SID_V1PW + 1, 0x04);
    POKE(SID_V2AD, 0x27); POKE(SID_V2SR, 0x00);
    POKE(SID_V3AD, 0x08); POKE(SID_V3SR, 0x00);
}

/* The graveyard march: a minor-key trudge under a jaunty melody, which
   is exactly the joke the original tune plays on you. */
const unsigned int FREQ[25] = {
    2228, 2360, 2500, 2649, 2806, 2973, 3150, 3338, 3536, 3747, 3970,
    4206, 4455, 4720, 5001, 5298, 5613, 5947, 6300, 6676, 7072, 7493,
    7939, 8412, 8910
};
#define REST 255
const unsigned char bass_pat[16] = {
    5, 5, 12, 5, 5, 5, 12, 5, 3, 3, 10, 3, 7, 7, 14, 7
};
const unsigned char lead_pat[32] = {
    17, 17, 19, 20, 19, 17, 15, REST, 12, 15, 17, REST, 15, 12, 10, REST,
    15, 15, 17, 19, 17, 15, 12, REST, 10, 12, 15, REST, 12, 10,  8, REST
};
unsigned char mframe = 0, bstep = 0, lstep = 0, sfx_timer = 0;

void music_tick(void) {
    unsigned char n;
    unsigned int f;
    mframe++;
    if (mframe == 4) { POKE(SID_V1CR, 0x40); POKE(SID_V2CR, 0x20); }
    if (mframe < 7) return;
    mframe = 0;
    n = bass_pat[bstep]; bstep = (bstep + 1) & 15;
    if (n != REST) {
        f = FREQ[n] >> 1;
        POKE(SID_V1F, f & 0xFF); POKE(SID_V1F + 1, f >> 8);
        POKE(SID_V1CR, 0x41);
    }
    n = lead_pat[lstep]; lstep = (lstep + 1) & 31;
    if (n != REST) {
        f = FREQ[n];
        POKE(SID_V2F, f & 0xFF); POKE(SID_V2F + 1, f >> 8);
        POKE(SID_V2CR, 0x21);
    }
    if (sfx_timer && !--sfx_timer) POKE(SID_V3CR, 0x80);
}

void sfx(unsigned char hi, unsigned char wave, unsigned char len) {
    POKE(SID_V3CR, 0x80);
    POKE(SID_V3F, 0x00); POKE(SID_V3F + 1, hi);
    POKE(SID_V3CR, wave);
    sfx_timer = len;
}

#define sfx_throw()  sfx(0x30, 0x21, 2)
#define sfx_jump()   sfx(0x18, 0x11, 3)
#define sfx_kill()   sfx(0x0A, 0x81, 4)
#define sfx_hurt()   sfx(0x05, 0x81, 8)

/* ================= video ================= */

void install_video(void) {
    unsigned int i;

    CIA2_PRA = (CIA2_PRA & 0xFC) | 0x02;         /* VIC bank 1 */

    __asm__("sei");
    POKE(1, PEEK(1) & 0xFB);
    memcpy((void*)CHARSET, (void*)0xD800, 96 * 8);   /* text glyphs */
    POKE(1, PEEK(1) | 0x04);
    __asm__("cli");
    memset((void*)(CHARSET + 96 * 8), 0, 32 * 8);
    memcpy((void*)(CHARSET + 128 * 8), tile_gfx, sizeof(tile_gfx));
    memcpy((void*)SPRDATA, sprite_gfx, sizeof(sprite_gfx));

    memset(SCREEN_A, 32, 1000);
    memset(SCREEN_B, 32, 1000);

    VIC_BG = 0;                  /* night */
    VIC_BORDER = 0;
    VIC_MC1 = 11;                /* shared 01: stone grey (sprites pass in front) */
    VIC_MC2 = 9;                 /* shared 10: earth brown (buries sprites)     */

    VIC_SPR_MC = 0xFF;
    /* Enemies live BEHIND the scenery: that is what lets a zombie claw
       its way out of the soil instead of sliding up in front of it.
       Arthur and his lance keep priority over the ground. */
    VIC_SPR_PRIO = 0xF8;
    VIC_SPR_MC0 = 2;             /* shared: dark red detail */
    VIC_SPR_MC1 = 1;             /* shared: white highlight */
    VIC_SPR_COL(ART_SPR) = 14;   /* armour: light blue steel */
    VIC_SPR_COL(LANCE_SPR) = 7;
    for (i = 3; i < 8; i++) VIC_SPR_COL(i) = 5;
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

void put_text(unsigned char *scr, unsigned char x, unsigned char y,
              const char *s) {
    unsigned int o = y * 40 + x;
    unsigned char c;
    while ((c = (unsigned char)*s++) != 0) {
        if (c >= 0xC1 && c <= 0xDA) c -= 0x80;   /* PETSCII -> screen */
        scr[o++] = c;
    }
}

void put_text2(unsigned char x, unsigned char y, const char *s) {
    put_text(SCREEN_A, x, y, s);
    put_text(SCREEN_B, x, y, s);
}

void put_num2(unsigned char x, unsigned char y, unsigned int v,
              unsigned char w) {
    char buf[7];
    signed char i;
    for (i = w - 1; i >= 0; i--) { buf[i] = '0' + (v % 10); v /= 10; }
    buf[w] = 0;
    put_text2(x, y, buf);
}

/* ================= the graveyard ================= */

/* Character for playfield row r of level column c */
unsigned char column_char(unsigned char c, unsigned char r) {
    unsigned char g, d;
    if (c >= LEVEL_LEN) return 32;
    g = level_ground[c];
    if (r > g) return (r > g + 2 && ((c + r) & 3) == 0) ? T_STONE : T_DIRT;
    if (r == g) return T_GRASS;

    d = level_deco[c];
    if (d == D_TOMB) {
        if (r == g - 1) return T_TOMB_BOT;
        if (r == g - 2) return T_TOMB_TOP;
    } else if (d == D_CROSS) {
        if (r == g - 1 || r == g - 2) return T_CROSS;
    } else if (d == D_TREE) {
        if (r >= g - 3 && r <= g - 1) return T_TRUNK;
        if (r == g - 4 || r == g - 5) return T_BRANCH;
    } else if (d == D_FENCE) {
        if (r == g - 1) return T_FENCE;
    }
    if (r == 3 && c == 44) return T_MOON;        /* a moon, once */
    return 32;
}

void write_column_chars(unsigned char *scr, unsigned char scr_col,
                        unsigned char c) {
    unsigned char r;
    unsigned int o = PF_OFFSET + scr_col;
    for (r = 0; r < PF_ROWS; r++) {
        scr[o] = column_char(c, r);
        o += 40;
    }
}

/* Colour RAM never scrolls, so it is painted in horizontal bands:
   night sky above, turf and earth below. Detail comes from the two
   shared multicolour registers, not from per-cell colour. */
/* Multicolor text mode allows only colours 0-7 in the per-character
   slot, so stone grey and earth brown have to come from the shared
   registers ($D022/$D023) and the per-char slot carries just the turf
   green. That makes the whole playfield a single colour value, which
   is lucky: colour RAM cannot be scrolled cheaply anyway. */
#define PF_COLOR 13              /* 8 = multicolor, 5 = green '11' pixels */
#define TEXT_COLOR 1             /* below 8 the VIC renders the char hires */

#define DEEP_COLOR 8             /* 8 = multicolor, 0 = black '11' pixels */

void init_colors(void) {
    unsigned char r;
    /* Two bands only, because colour RAM cannot be scrolled: turf green
       down to the lowest ground line, then black grit for the deep soil.
       Black still counts as foreground to the VIC, so buried sprites
       stay buried. */
    for (r = 0; r < PF_ROWS; r++)
        memset(COLRAM + (PF_TOP_ROW + r) * 40,
               (r <= 18) ? PF_COLOR : DEEP_COLOR, 40);
    memset(COLRAM, TEXT_COLOR, 80);              /* HUD stays hires */
}

/* Text inside the playfield needs hires cells or it comes out as
   multicolor mush */
void text_band(unsigned char lo, unsigned char hi) {
    unsigned char r;
    for (r = lo; r <= hi; r++)
        memset(COLRAM + r * 40, TEXT_COLOR, 40);
}

/* A rolling profile for the far land, four columns to a step. The
   ridge is repainted from this model every coarse scroll using half
   the foreground column index, which is the parallax. */
const unsigned char ridge_prof[16] = {
    13, 13, 12, 12, 13, 13, 13, 12, 12, 12, 13, 13, 12, 13, 13, 12
};
#define RIDGE_LO 11
#define RIDGE_HI 14              /* exclusive: a thin distant horizon */

/* Highest playfield row the graveyard itself occupies in column c */
unsigned char fg_top(unsigned char c) {
    unsigned char g = level_ground[c], d = level_deco[c];
    if (d == D_TOMB || d == D_CROSS) return g - 2;
    if (d == D_TREE) return g - 5;
    if (d == D_FENCE) return g - 1;
    return g;
}

/* The far plane: a thin band of distant land repainted from its own
   model once per coarse scroll, sampled at half the foreground column
   index. EVERY cell in the band is written - leaving one alone would
   preserve what the row copier dragged in, which moves at foreground
   speed and smears the horizon. */
void update_ridge(void) {
    unsigned char c, r, t, top, w, half;
    unsigned char *scr = back_scr() + PF_OFFSET;
    unsigned char nh = head + 1;
    unsigned int ro;

    for (c = 0; c < 40; c++) {
        w = nh + c;
        top = fg_top(w);
        half = w >> 1;                             /* half speed */
        t = ridge_prof[(half >> 2) & 15];
        for (r = RIDGE_LO; r < RIDGE_HI; r++) {
            ro = row_ofs[r];
            if (r >= top) {                        /* graveyard wins here */
                scr[ro + c] = column_char(w, r);
            } else if (r > t) {
                scr[ro + c] = T_RIDGE;
            } else if (r == t) {
                scr[ro + c] = T_RIDGE_TOP;
            } else if (r == t - 1 && (half & 7) == 3) {
                scr[ro + c] = T_FAR_CROSS;         /* a cross on the crest */
            } else {
                scr[ro + c] = 32;
            }
        }
    }
}

void init_bg(void) {
    unsigned char i;
    for (i = 0; i < NUM_BG; i++) {
        bg_col[i] = rand() % 39;
        bg_row[i] = 3 + (rand() % 8);              /* high, cold sky */
        bg_ch[i] = T_STAR;
        bg_div[i] = 3 + (i & 1);                   /* 1/3 and 1/4 speed */
        bg_cnt[i] = rand() & 3;
    }
}

/* Repaint one band of the far plane into the back buffer, right after
   the row copier has handled those rows. Nothing is drawn where the
   graveyard already occupies the cell, so the foreground occludes the
   horizon for free. */
void update_bg(unsigned char lo, unsigned char hi) {
    unsigned char i, oldc, r;
    unsigned char nh = head + 1;
    unsigned char *scr = back_scr() + PF_OFFSET;
    unsigned int ro;

    for (i = 0; i < NUM_BG; i++) {
        r = bg_row[i];
        if (r < lo || r >= hi) continue;
        ro = row_ofs[r];
        if (bg_col[i] == 0) {
            bg_col[i] = 38;                  /* 39 belongs to the new column */
        } else {
            oldc = bg_col[i] - 1;
            scr[ro + oldc] = column_char(nh + oldc, r);
            if (++bg_cnt[i] >= bg_div[i]) { bg_cnt[i] = 0; bg_col[i] = oldc; }
        }
        if (column_char(nh + bg_col[i], r) == 32)
            scr[ro + bg_col[i]] = bg_ch[i];
    }
}

void draw_all(void) {
    unsigned char c;
    for (c = 0; c < 40; c++) {
        write_column_chars(SCREEN_A, c, head + c);
        write_column_chars(SCREEN_B, c, head + c);
    }
    init_colors();
    init_bg();
}

/* ================= scrolling ================= */

void copy_row_chunk(unsigned char first, unsigned char count) {
    unsigned int o = PF_OFFSET + first * 40;
    scr_src = front_scr() + o + 1;
    scr_dst = back_scr() + o;
    scr_rows = count;
    scroll_rows();
}

/* One pixel of scroll per call; the heavy work is spread across the
   eight frames of the coarse cycle. Returns 1 on the flip frame. */
unsigned char scroll_step(void) {
    unsigned char r;
    unsigned int o;

    scrolled_px++;               /* the world moved under everything */
    if (fine_pos == 0) {
        fine_pos = 7;
        front ^= 1;
        d018_next = front ? D018_B : D018_A;
        fine_next = 7;
        head++;
        return 1;
    }
    fine_pos--;
    fine_next = fine_pos;
    switch (fine_pos) {
    case 6:
        for (r = 0; r < PF_ROWS; r++)
            char_cache[r] = column_char(head + 40, r);
        break;
    case 5: copy_row_chunk(0, 6);  update_bg(0, 6);   break;
    case 4: copy_row_chunk(6, 6);  update_bg(6, 12);  break;
    case 3: copy_row_chunk(12, 6); break;
    case 2: copy_row_chunk(18, 5); update_bg(18, 23); break;
    case 1: {
        unsigned char *bs = back_scr();
        update_ridge();
        o = PF_OFFSET + 39;
        for (r = 0; r < PF_ROWS; r++) { bs[o] = char_cache[r]; o += 40; }
        break;
    }
    }
    return 0;
}

/* ================= HUD ================= */

void draw_hud(void) {
    unsigned char c;
    put_text2(1, 0, "SCORE 000000   TIME 99   ARTHUR 3");
    for (c = 0; c < 40; c++) {
        SCREEN_A[40 + c] = T_HUDBAR;
        SCREEN_B[40 + c] = T_HUDBAR;
    }
}

void update_hud(void) {
    put_num2(7, 0, score, 6);
    put_num2(21, 0, timer, 2);
    put_num2(33, 0, lives, 1);
}

/* ================= enemies ================= */

void hide_enemy(unsigned char i) {
    en_act[i] = 0;
    en_y[i] = 0xFF;
}

void spawn_enemy(void) {
    unsigned char i, c, g;
    for (i = 0; i < MAX_EN; i++) {
        if (en_act[i]) continue;
        if (rand() & 3) {
            /* a zombie claws out of the turf ahead of Arthur */
            c = 30 + (rand() & 7);               /* screen column */
            g = level_ground[head + c];
            en_type[i] = 0;
            en_st[i] = 20;                       /* frames spent climbing */
            en_y[i] = ROW_Y(g) + 8;              /* under the turf, unseen */
            en_frm[i] = SF_ZOM_RISE;
            en_col[i] = 5;
        } else {
            en_type[i] = 1;                      /* a crow from the night */
            c = 36 + (rand() & 3);
            en_st[i] = 0;
            en_y[i] = 70 + (rand() % 60);
            en_frm[i] = SF_CROW1;
            en_col[i] = 11;
        }
        en_x[i] = (unsigned char)((24 + c * 8) >> 1);
        en_act[i] = 1;
        return;
    }
}

void update_enemies(void) {
    unsigned char i, au, drift = 0;
    au = (unsigned char)((24 + art_col * 8 + art_sub) >> 1);

    /* enemy X lives in 2-pixel units, so the world owes them one unit
       for every two pixels it scrolled */
    while (scrolled_px >= 2) { scrolled_px -= 2; drift++; }

    for (i = 0; i < MAX_EN; i++) {
        if (!en_act[i]) continue;

        if (en_act[i] == 2) {
            /* Dying: hold the puff and fade. This used to fall through
               into the walking logic, which overwrote the puff frame,
               dragged the corpse along the ground and let it keep
               killing Arthur. */
            en_frm[i] = SF_PUFF;
            if (en_x[i] > drift + 8) en_x[i] -= drift;
            if (++en_st[i] > 10) hide_enemy(i);
            continue;
        }

        if (en_type[i] == 0) {                   /* zombie */
            if (en_st[i]) {
                /* one pixel of daylight per frame; the soil in front of
                   the sprite does the rest of the work */
                en_st[i]--;
                en_y[i] -= 1;
                en_frm[i] = (en_st[i] > 10) ? SF_ZOM_RISE
                          : ((frame & 8) ? SF_ZOM_WALK1 : SF_ZOM_WALK2);
            } else {
                unsigned char c, g;
                en_frm[i] = (frame & 8) ? SF_ZOM_WALK1 : SF_ZOM_WALK2;
                /* every other frame only: at full speed a zombie ahead
                   of Arthur matches his pace once the scroll is added in,
                   and the pair looks glued together */
                if (frame & 1) {
                    if (en_x[i] > au + 1) en_x[i] -= 1;
                    else if (en_x[i] + 1 < au) en_x[i] += 1;
                }
                /* stay on the turf: the graveyard steps up and down and a
                   zombie that ignored it would wade through the soil or
                   walk on air over the plateau */
                c = (en_x[i] - 12) >> 2;              /* screen column */
                if (c < 40) {
                    g = level_ground[head + c];
                    en_y[i] = ROW_Y(g) - 12;
                }
            }
        } else {                                 /* crow */
            en_frm[i] = (frame & 4) ? SF_CROW1 : SF_CROW2;
            if (en_x[i] > 6) en_x[i] -= 2; else { hide_enemy(i); continue; }
            en_y[i] += (frame & 16) ? 1 : 0xFF;  /* lazy sine-less bob */
        }

        /* carried along by the scrolling graveyard */
        if (en_x[i] > drift + 8) en_x[i] -= drift;
        else { hide_enemy(i); continue; }

        /* lance vs enemy */
        if (lance.active) {
            unsigned char lu = (unsigned char)(lance.x >> 1);
            if (lu + 10 >= en_x[i] && lu <= en_x[i] + 8 &&
                lance.y + 8 >= en_y[i] && lance.y <= en_y[i] + 16) {
                en_frm[i] = SF_PUFF;
                en_col[i] = 1;
                en_st[i] = 0;
                en_act[i] = 2;                   /* dying puff */
                lance.active = 0;
                set_sprite_pos(LANCE_SPR, 0, 0);
                score += 200;
                hud_dirty = 1;
                sfx_kill();
                continue;
            }
        }

        /* enemy vs Arthur */
        if (!invuln && !dead_timer &&
            au + 8 >= en_x[i] && au <= en_x[i] + 8 &&
            art_y + 18 >= en_y[i] && art_y <= en_y[i] + 16) {
            sfx_hurt();
            invuln = 80;
            if (armour) {
                armour = 0;                      /* stripped to the shorts */
                VIC_SPR_COL(ART_SPR) = 1;
            } else {
                dead_timer = 40;
            }
        }
    }
}

/* Sort the live enemies by Y and hand them to the multiplexer */
void build_mux(void) {
    unsigned char n = 0, i, j, k, t, y, u, slot, mask;
    unsigned char ord[MAX_EN], ys[MAX_EN];

    for (i = 0; i < MAX_EN; i++) {
        if (!en_act[i]) continue;
        y = en_y[i];
        if (y >= 240) continue;
        ord[n] = i; ys[n] = y; n++;
    }
    if (!n) { mux_y[0] = 0xFF; mux_count = 0; return; }
    for (i = 1; i < n; i++) {
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
    mux_y[k] = 0xFF;
    mux_count = k;
}

/* ================= Arthur ================= */

unsigned char ground_row_at(unsigned char scr_col) {
    return level_ground[head + scr_col];
}

void land_on_ground(void) {
    art_row = ground_row_at(art_col);
    art_y = ROW_Y(art_row) - 15;      /* his boots on the turf */
}

void update_player(unsigned char joy) {
    unsigned char want_scroll = 0;
    unsigned char g, gy;

    if (dead_timer) {
        /* the classic ragdoll tumble - this used to return before the
           sprite was ever moved, so nobody saw it */
        art_frm = SF_ART_JUMP;
        if (art_y > 60) art_y -= 2;
        set_sprite_frame(ART_SPR, art_frm);
        VIC_SPR_ENA |= 0x01;
        set_sprite_pos(ART_SPR, 24 + art_col * 8 + art_sub, art_y);
        return;
    }

    /* horizontal: Arthur holds station near the left third and the
       world comes to him, exactly like the arcade */
    if (JOY_RIGHT(joy)) {
        face_right = 1;
        want_scroll = 1;
        if (!airborne) art_frm = (anim & 8) ? SF_ART_RUN1 : SF_ART_RUN2;
    } else if (JOY_LEFT(joy)) {
        face_right = 0;
        if (art_col > 2) {
            if (art_sub) art_sub -= 2; else { art_sub = 6; art_col--; }
        }
        if (!airborne) art_frm = (anim & 8) ? SF_ART_RUN1 : SF_ART_RUN2;
    } else if (!airborne) {
        art_frm = SF_ART_STAND;
    }

    /* Vertical state. The arc stays the arcade's rigid parabola - no air
       control - but it is measured from the ground Arthur LEFT, not from
       whatever column happens to be under him. Anchoring it to the
       current column made him teleport 32 pixels the moment the plateau
       scrolled underneath. Past the end of the arc, and whenever the
       turf falls away, gravity takes over: that is the falling the
       old snap-to-ground model simply did not have. */
    g = ground_row_at(art_col);
    gy = ROW_Y(g) - 15;

    if (airborne) {
        art_frm = SF_ART_JUMP;
        if (jump_t <= JUMP_LEN) {
            art_y = jump_base - jump_arc[jump_t - 1];
            jump_t++;
        } else {
            art_y += 3;                          /* free fall */
        }
        if (jump_t > 13 && art_y >= gy) {        /* descending onto turf */
            art_y = gy;
            airborne = 0;
            jump_t = 0;
        }
    } else if (art_y < gy) {
        airborne = 1;                            /* walked off an edge */
        jump_t = JUMP_LEN + 1;                   /* no arc, straight down */
    } else if (JOY_UP(joy)) {
        airborne = 1;
        jump_t = 1;
        jump_base = gy;
        sfx_jump();
    } else {
        art_y = gy;                              /* follow the turf up */
    }

    if (JOY_BTN_1(joy) && !lance.active) {
        lance.active = 1;
        lance.x = 24 + art_col * 8 + (face_right ? 16 : 0);
        lance.y = art_y + 6;
        lance.right = face_right;
        art_frm = SF_ART_THROW;
        sfx_throw();
    }

    set_sprite_frame(ART_SPR, art_frm);
    if (invuln && (invuln & 2)) VIC_SPR_ENA &= 0xFE;
    else                        VIC_SPR_ENA |= 0x01;
    set_sprite_pos(ART_SPR, 24 + art_col * 8 + art_sub, art_y);

    if (want_scroll && head + 41 < LEVEL_LEN) {
        scroll_step();
        if (lance.active && lance.x > 1) lance.x--;   /* carried along */
    }
}

/* ================= game ================= */

unsigned char wait_frame(void) {
    unsigned char n;
    while (!vsync_flag) ;
    n = vsync_flag;
    vsync_flag = 0;
    if (n > 1) POKE(MISS_COUNTER, PEEK(MISS_COUNTER) + n - 1);
    return n;
}

void reset_level(void) {
    unsigned char i;
    head = 0;
    fine_pos = 7;
    art_col = 6; art_sub = 4;
    jump_t = 0;
    armour = 1;
    invuln = 0;
    dead_timer = 0;
    timer = 99;
    sec_frames = 50;
    lance.active = 0;
    airborne = 0;
    spawn_timer = 60;
    for (i = 0; i < MAX_EN; i++) hide_enemy(i);
    mux_count = 0; mux_y[0] = 0xFF;
    VIC_SPR_COL(ART_SPR) = 14;
    airborne = 0;
    land_on_ground();
    draw_all();                  /* also repaints the colour bands */
    draw_hud();
    update_hud();
    set_sprite_pos(LANCE_SPR, 0, 0);
    VIC_SPR_ENA = 0xFF;
}

void play(void) {
    unsigned char joy;
    unsigned int demo_t = 0;

    reset_level();

    while (!game_over) {
        wait_frame();
        frame++;
        anim++;
        music_tick();

        joy = read_input();
        if (demo_mode) {
            /* a plain runner: keep going right, jump at obstacles and
               anything ahead, throw lances on a rhythm */
            unsigned char g0 = ground_row_at(art_col);
            unsigned char g1 = ground_row_at(art_col + 3);
            joy = 0x08;                                   /* right */
            if (!jump_t && (g1 < g0 || (frame & 63) == 0)) joy |= 0x01;
            if ((frame & 15) == 0) joy |= 0x10;
            if (++demo_t > 1800) { game_over = 1; }
        }

        update_player(joy);

        if (lance.active) {
            /* a left-thrown lance used to advance by zero pixels and so
               never left the screen, which disarmed Arthur for good */
            if (lance.right) lance.x += 6;
            else if (lance.x >= 30) lance.x -= 6;
            else lance.x = 0;
            if (lance.x > 336 || lance.x < 26) {
                lance.active = 0;
                set_sprite_pos(LANCE_SPR, 0, 0);
            } else {
                set_sprite_frame(LANCE_SPR, SF_LANCE);
                set_sprite_pos(LANCE_SPR, lance.x, lance.y);
            }
        }

        if (--spawn_timer == 0) {
            spawn_timer = 70 + (rand() & 31);
            spawn_enemy();
        }
        update_enemies();
        build_mux();

        if (--sec_frames == 0) {
            sec_frames = 50;
            if (timer) {
                timer--;
                hud_dirty = 1;
            } else if (!dead_timer) {
                dead_timer = 40;     /* the clock is the other enemy */
                sfx_hurt();
            }
        }

        if (dead_timer && !--dead_timer) {
            lives--;
            hud_dirty = 1;
            if (!lives) game_over = 1;
            else { armour = 1; VIC_SPR_COL(ART_SPR) = 14;
                   invuln = 80; airborne = 0; jump_t = 0;
                   land_on_ground(); }
        }
        if (invuln) invuln--;

        if (hud_dirty) { update_hud(); hud_dirty = 0; }
        if (head + 41 >= LEVEL_LEN) game_over = 1;   /* end of the map */
    }
}

void title_screen(void) {
    unsigned int t = 0;
    unsigned char joy;

    VIC_SPR_ENA = 0;
    mux_count = 0; mux_y[0] = 0xFF;
    head = 0;
    draw_all();
    draw_hud();
    update_hud();

    text_band(4, 17);
    put_text2(9,  6, "G H O S T   K E E P");
    put_text2(6,  8, "A GHOSTS AND GOBLINS TRIBUTE");
    put_text2(10, 10, "LEVEL 1 - GRAVEYARD");
    put_text2(7, 12, "JOYSTICK 2   FIRE TO THROW");
    put_text2(9, 16, "(C) 2026 AI TOOLCHAIN");

    while (1) {
        wait_frame();
        frame++;
        music_tick();
        t++;
        if ((t & 0x1F) == 0) {
            if (t & 0x20) put_text2(10, 14, "PRESS FIRE TO START");
            else          put_text2(10, 14, "                   ");
        }
        joy = read_input();
        if (JOY_BTN_1(joy)) { demo_mode = 0; break; }
        if (t > 600) { demo_mode = 1; break; }
    }
}

void over_screen(void) {
    unsigned int t = 0;
    VIC_SPR_ENA = 0;
    mux_count = 0; mux_y[0] = 0xFF;
    text_band(10, 14);
    put_text2(12, 11, "G A M E   O V E R");
    put_text2(13, 13, "SCORE ");
    put_num2(19, 13, score, 6);
    while (1) {
        wait_frame();
        frame++;
        music_tick();
        if (++t > 400 || JOY_BTN_1(read_input())) break;
    }
}

int main(void) {
    srand(0x600D);
    joy_install(joy_static_stddrv);
    POKE(AGENT_INPUT, 0);
    POKE(AGENT_HOLD, 0);

    install_video();
    sound_init();
    irq_init();

    while (1) {
        game_over = 0;
        lives = 3;
        score = 0;
        title_screen();
        play();
        if (!demo_mode) over_screen();
    }
    return 0;
}

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
extern volatile unsigned char fine_x;
extern volatile unsigned char d018_next;
extern volatile unsigned char vsync_flag;
void irq_init(void);

/* --- Row copier (scroll.s) --- */
extern unsigned char *scr_src;
extern unsigned char *scr_dst;
void scroll_rows(void);

/* --- Playfield geometry --- */
#define PF_TOP_ROW 3             /* first playfield char row */
#define PF_ROWS 22
#define PF_OFFSET (PF_TOP_ROW * 40)

/* --- Agent input ($033C, edge-triggered toolchain convention) --- */
#define AGENT_INPUT 0x033C
unsigned char read_input(void) {
    unsigned char v = joy_read(JOY_2) | PEEK(AGENT_INPUT);
    POKE(AGENT_INPUT, 0);
    return v;
}

/* --- Terrain model: ring buffer of column descriptors --- */
#define RING 64
#define RMASK 63
unsigned char ceil_h[RING];      /* ceiling thickness in rows (1..5) */
unsigned char floor_h[RING];     /* floor thickness in rows (1..5)  */
unsigned char tow_h[RING];       /* tower height above floor (0..8) */
unsigned char star_r[RING];      /* star row in the sky gap, 0xFF none */
unsigned char star_c[RING];      /* star char */
unsigned char head = 0;          /* ring index of screen column 0 */

unsigned char cur_ceil = 2, cur_floor = 2;
unsigned int  world_col = 0;

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

struct { unsigned int x; unsigned char y; unsigned char active; } bolt;
struct { unsigned int x; unsigned char y; unsigned char active; } orb;

#define MAX_EN 5                 /* sprites 3..7 */
struct {
    unsigned int x;
    unsigned char y, base_y;
    unsigned char type;          /* 0 drone (sine), 1 dart (fast) */
    unsigned char active;
    unsigned char dying;         /* explosion timer */
} en[MAX_EN];

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

    /* Sprites: all multicolor */
    VIC_SPR_MC = 0xFF;
    VIC_SPR_MC0 = 9;             /* shared: brown/orange glow */
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

    /* sparse stars in the sky gap */
    if ((rand() & 7) < 3) {
        star_r[idx] = cur_ceil + 1 + (rand() % (gap - 2));
        star_c[idx] = (rand() & 1) ? T_STAR1 : T_STAR2;
    } else star_r[idx] = 0xFF;

    world_col++;
}

/* Char + colour for playfield row r (0..21) of ring column idx */
void column_cell(unsigned char idx, unsigned char r,
                 unsigned char *ch, unsigned char *col) {
    unsigned char ch_ = 32, col_ = 11;
    unsigned char fl_top = PF_ROWS - floor_h[idx];

    if (r < ceil_h[idx]) {
        ch_ = (r == ceil_h[idx] - 1) ? T_CEIL_BOT : T_CEIL_FILL;
        col_ = 11;                          /* cyan MCM */
    } else if (r >= fl_top) {
        ch_ = (r == fl_top) ? T_FLOOR_TOP : T_FLOOR_FILL;
        col_ = 10;                          /* red MCM */
    } else if (tow_h[idx] && r >= fl_top - tow_h[idx]) {
        ch_ = (r == fl_top - tow_h[idx]) ? T_TOWER_TOP : T_TOWER;
        col_ = 14;                          /* blue MCM */
    } else if (r == star_r[idx]) {
        ch_ = star_c[idx];
        col_ = 1;                           /* white hires star */
    }
    *ch = ch_;
    *col = col_;
}

void write_column_chars(unsigned char *scr, unsigned char scr_col,
                        unsigned char idx) {
    unsigned char r, ch, col;
    unsigned int o = PF_OFFSET + scr_col;
    for (r = 0; r < PF_ROWS; r++) {
        column_cell(idx, r, &ch, &col);
        scr[o] = ch;
        o += 40;
    }
}

void write_column_colors(unsigned char scr_col, unsigned char idx) {
    unsigned char r, ch, col;
    unsigned int o = PF_OFFSET + scr_col;
    for (r = 0; r < PF_ROWS; r++) {
        column_cell(idx, r, &ch, &col);
        COLRAM[o] = col;
        o += 40;
    }
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
        write_column_colors(c, c);
    }
}

/* ================= Scroll engine ================= */

unsigned char *front_scr(void) { return front ? SCREEN_B : SCREEN_A; }
unsigned char *back_scr(void)  { return front ? SCREEN_A : SCREEN_B; }

/* Work is spread over the fine-scroll cycle so no single frame
   carries more than one heavy job:
     fine 5: generate the next column, cache its chars and colours
     fine 4: assembly row copy front -> back
     fine 3: write the cached column into the back buffer
     fine 0: flip, shift colour RAM, poke the cached colours
   Returns 1 on the flip frame. */
unsigned char char_cache[PF_ROWS], col_cache[PF_ROWS];

unsigned char scroll_step(void) {
    unsigned char r, ch, co;
    unsigned int o;

    if (fine_x == 0) {
        fine_x = 7;
        front ^= 1;
        d018_next = front ? D018_B : D018_A;
        scr_src = COLRAM + PF_OFFSET + 1;
        scr_dst = COLRAM + PF_OFFSET;
        scroll_rows();
        head = (head + 1) & RMASK;
        o = PF_OFFSET + 39;
        for (r = 0; r < PF_ROWS; r++) { COLRAM[o] = col_cache[r]; o += 40; }
        return 1;
    }
    fine_x--;
    if (fine_x == 5) {
        gen_column((head + 40) & RMASK);
        for (r = 0; r < PF_ROWS; r++) {
            column_cell((head + 40) & RMASK, r, &ch, &co);
            char_cache[r] = ch;
            col_cache[r] = co;
        }
    } else if (fine_x == 4) {
        scr_src = front_scr() + PF_OFFSET + 1;
        scr_dst = back_scr() + PF_OFFSET;
        scroll_rows();
    } else if (fine_x == 3) {
        o = PF_OFFSET + 39;
        for (r = 0; r < PF_ROWS; r++) {
            back_scr()[o] = char_cache[r];
            o += 40;
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
        write_column_colors(c, idx);
    }
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

/* ================= Entities ================= */

void hide_enemy(unsigned char i) {
    en[i].active = 0;
    en[i].dying = 0;
    set_sprite_pos(3 + i, 0, 0);
}

void spawn_enemy(void) {
    unsigned char i;
    for (i = 0; i < MAX_EN; i++) {
        if (!en[i].active && !en[i].dying) {
            en[i].active = 1;
            en[i].type = (rand() & 3) == 0 ? 1 : 0;
            en[i].x = 344;
            en[i].base_y = 90 + (rand() % 110);
            en[i].y = en[i].base_y;
            set_sprite_frame(3 + i, en[i].type ? SF_DART : SF_DRONE0);
            VIC_SPR_COL(3 + i) = en[i].type ? 7 : 4;
            break;
        }
    }
}

void kill_enemy(unsigned char i) {
    en[i].active = 0;
    en[i].dying = 16;
    set_sprite_frame(3 + i, SF_EXPL0);
    VIC_SPR_COL(3 + i) = 7;
    score += en[i].type ? 50 : 25;
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
    if (shields == 0) game_over_flag = 1;
    ship_y = 140;
    ship_x = 60;
}

void update_enemies(void) {
    unsigned char i, sy;
    for (i = 0; i < MAX_EN; i++) {
        if (en[i].dying) {
            en[i].dying--;
            set_sprite_frame(3 + i, (en[i].dying & 4) ? SF_EXPL0 : SF_EXPL1);
            if (!en[i].dying) hide_enemy(i);
            continue;
        }
        if (!en[i].active) continue;
        en[i].x -= en[i].type ? 4 : 2;
        if (en[i].x < 12 || en[i].x > 400) { hide_enemy(i); continue; }
        if (en[i].type == 0) {
            sy = en[i].base_y + (signed char)sine[((frame >> 2) + (i << 2)) & 31];
            en[i].y = sy;
            set_sprite_frame(3 + i, (frame & 8) ? SF_DRONE0 : SF_DRONE1);
        }
        set_sprite_pos(3 + i, en[i].x, en[i].y);

        /* enemy fires the shared orb */
        if (!orb.active && en[i].x > ship_x + 40 && (rand() & 63) == 0) {
            orb.active = 1;
            orb.x = en[i].x;
            orb.y = en[i].y + 4;
        }

        /* bolt vs enemy */
        if (bolt.active &&
            bolt.x + 20 >= en[i].x && bolt.x <= en[i].x + 16 &&
            bolt.y + 6 >= en[i].y && bolt.y <= en[i].y + 12) {
            kill_enemy(i);
            bolt.active = 0;
            set_sprite_pos(1, 0, 0);
            continue;
        }

        /* enemy vs ship */
        if (!invuln &&
            ship_x + 20 >= en[i].x && ship_x <= en[i].x + 16 &&
            ship_y + 10 >= en[i].y && ship_y <= en[i].y + 12) {
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
    return n > 3 ? 3 : n;
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
    spawn_timer = 90;
    speed_lvl = 0;
    bolt.active = 0;
    orb.active = 0;
    for (i = 0; i < MAX_EN; i++) hide_enemy(i);
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
            VIC_SPR_ENA = (invuln & 2) ? 0xFE : 0xFF;
            if (!invuln) { VIC_BORDER = 0; VIC_SPR_ENA = 0xFF; }
        }

        joy = read_input();
        if (demo_mode && joy) { demo_mode = 0; }

        if (demo_mode) {
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

        if (JOY_BTN_1(joy) && !bolt.active) {
            bolt.active = 1;
            bolt.x = ship_x + 18;
            bolt.y = ship_y + 6;
            sfx_shoot();
        }

        /* ship */
        set_sprite_frame(0, (frame & 4) ? SF_SHIP0 : SF_SHIP1);
        set_sprite_pos(0, ship_x, ship_y);
        if (!invuln && terrain_hit(ship_x + 10, ship_y + 10)) {
            sfx_boom();
            ship_hit();
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

        /* spawning, faster over time */
        t = 90 - (speed_lvl << 4);
        if (--spawn_timer == 0) {
            spawn_timer = t < 30 ? 30 : t;
            spawn_enemy();
        }
        if (score >= 500 && speed_lvl < 1) { speed_lvl = 1; hud_dirty = 1; }
        if (score >= 1500 && speed_lvl < 2) { speed_lvl = 2; hud_dirty = 1; }
        if (score >= 3000 && speed_lvl < 3) { speed_lvl = 3; hud_dirty = 1; }

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

void title_screen(void) {
    unsigned int timer = 0;
    unsigned char joy, n;

    VIC_SPR_ENA = 0x00;
    draw_hud_static();
    update_hud();
    stamp_title(SCREEN_A, 1);
    stamp_title(SCREEN_B, 1);
    panel_colors();

    while (1) {
        n = wait_frames();
        while (n--) {
            frame++;
            music_tick();
            if (scroll_step()) panel_colors();
            if (fine_x == 3)
                stamp_title(back_scr(), (timer & 0x20) ? 1 : 0);
        }
        timer++;
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
    unsigned char n;

    VIC_SPR_ENA = 0x00;
    stamp_gameover(SCREEN_A, 1);
    stamp_gameover(SCREEN_B, 1);
    panel_colors();

    while (1) {
        n = wait_frames();
        while (n--) {
            frame++;
            music_tick();
            if (scroll_step()) panel_colors();
            if (fine_x == 3)
                stamp_gameover(back_scr(), (timer & 0x20) ? 1 : 0);
        }
        timer++;
        if (JOY_BTN_1(read_input())) break;
        if (timer > 800) break;
    }
}

int main(void) {
    srand(0xC64);
    joy_install(joy_static_stddrv);
    POKE(AGENT_INPUT, 0);

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

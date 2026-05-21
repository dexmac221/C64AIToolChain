/*
 * FROGGER for Commodore 64 — cc65
 * Multicolor Bitmap Mode (MCM) for improved visual quality
 * VIC bank 0: screen@$0400 (colour data), bitmap@$2000, sprite@$3F40
 * AI Toolchain Project 2026
 */

#include <c64.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════
 *  HARDWARE
 * ═══════════════════════════════════════════════════════════ */
#define SCR_W   40
#define SCR_H   25

/* In MCM bitmap mode SCREEN holds colour nibbles, not char codes */
#define SCREEN      ((unsigned char*)0x0400)
#define BITMAP      ((unsigned char*)0x2000)   /* 8000 bytes, $2000-$3F3F */
#define COLRAM      ((unsigned char*)0xD800)

/* Sprite pointers: last 8 bytes of $0400 page */
#define SPRITE_PTRS ((unsigned char*)0x07F8)

/* Sprite data just after bitmap ($3F40-$3F7F); block 253 = $3F40/64 */
#define SPRITE_DATA  0x3F40
#define SPR_FROG     0
#define BLK_FROG     253

#define VIC_D011    (*(unsigned char*)0xD011)
#define VIC_D016    (*(unsigned char*)0xD016)
#define VIC_D018    (*(unsigned char*)0xD018)
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X  (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y  (*(unsigned char*)0xD017)
#define VIC_BORDER     (*(unsigned char*)0xD020)
#define VIC_BG         (*(unsigned char*)0xD021)
#define VIC_MC1        (*(unsigned char*)0xD022)
#define VIC_MC2        (*(unsigned char*)0xD023)

#define SPR_X(n)   (*(unsigned char*)(0xD000+(n)*2))
#define SPR_Y(n)   (*(unsigned char*)(0xD001+(n)*2))
#define SPR_COL(n) (*(unsigned char*)(0xD027+(n)))

/* SID */
#define SID_V1_FLO (*(unsigned char*)0xD400)
#define SID_V1_FHI (*(unsigned char*)0xD401)
#define SID_V1_CTL (*(unsigned char*)0xD404)
#define SID_V1_AD  (*(unsigned char*)0xD405)
#define SID_V1_SR  (*(unsigned char*)0xD406)
#define SID_V2_FLO (*(unsigned char*)0xD407)
#define SID_V2_FHI (*(unsigned char*)0xD408)
#define SID_V2_CTL (*(unsigned char*)0xD40B)
#define SID_V2_AD  (*(unsigned char*)0xD40C)
#define SID_V2_SR  (*(unsigned char*)0xD40D)
#define SID_V3_FLO (*(unsigned char*)0xD40E)
#define SID_V3_FHI (*(unsigned char*)0xD40F)
#define SID_V3_CTL (*(unsigned char*)0xD412)
#define SID_V3_AD  (*(unsigned char*)0xD413)
#define SID_V3_SR  (*(unsigned char*)0xD414)
#define SID_VOL    (*(unsigned char*)0xD418)

/* C64 palette */
#define BLACK    0
#define WHITE    1
#define RED      2
#define CYAN     3
#define PURPLE   4
#define GREEN    5
#define BLUE     6
#define YELLOW   7
#define ORANGE   8
#define BROWN    9
#define LTRED   10
#define GREY1   11
#define GREY2   12
#define LTGREEN 13
#define LTBLUE  14
#define GREY3   15

/* ═══════════════════════════════════════════════════════════
 *  SPRITE → SCREEN COORDINATES
 * ═══════════════════════════════════════════════════════════ */
#define SPR_XOFS  24
#define SPR_YOFS  50
#define C2SX(c)  ((unsigned int)((c)*8 + SPR_XOFS))
#define C2SY(r)  ((unsigned char)((r)*8 + SPR_YOFS))

/* ═══════════════════════════════════════════════════════════
 *  LAYOUT
 *  0      HUD
 *  1      top green bank
 *  2      home pads
 *  3-7    river (5 lanes)
 *  8      median
 *  9-13   road  (5 lanes)
 *  14     start zone
 *  15-24  lower safe zone
 * ═══════════════════════════════════════════════════════════ */
#define ROW_HUD        0
#define ROW_HOME       1
#define ROW_PADS       2
#define ROW_RIVER_TOP  3
#define ROW_RIVER_BOT  7
#define ROW_MEDIAN     8
#define ROW_ROAD_TOP   9
#define ROW_ROAD_BOT  13
#define ROW_START     14

#define FROG_START_CX  19
#define FROG_START_CY  14

/* ═══════════════════════════════════════════════════════════
 *  GAME CONSTANTS
 * ═══════════════════════════════════════════════════════════ */
#define NUM_HOME_PADS   5
#define NUM_LIVES_START 3
#define MAX_OBJECTS     24
#define TIME_START      60

#define OBJ_NONE   0
#define OBJ_CAR    1
#define OBJ_TRUCK  2
#define OBJ_LOG    3
#define OBJ_TURTLE 4

#define GS_TITLE  0
#define GS_PLAY   1
#define GS_DYING  2
#define GS_HOME   3
#define GS_LEVEL  4
#define GS_OVER   5

/* ═══════════════════════════════════════════════════════════
 *  FROG SPRITE DATA  (24×21, multicolour)
 *  00 transparent, 01 shared MC1, 10 sprite colour, 11 shared MC2
 * ═══════════════════════════════════════════════════════════ */
static const unsigned char frog_sprite[63] = {
    0x00,0x00,0x00,
    0x01,0xA4,0x00,
    0x06,0xAA,0x40,
    0x1A,0xAA,0x90,
    0x6A,0xFA,0xA4,
    0x6B,0xFE,0xA4,
    0x6A,0xAA,0xA4,
    0x69,0xAA,0x64,
    0x6A,0xAA,0xA4,
    0x1A,0xAA,0xA4,
    0x06,0xAA,0x90,
    0x01,0xAA,0x40,
    0x05,0x96,0x50,
    0x16,0x96,0x94,
    0x5A,0x96,0xA5,
    0x1A,0x50,0x59,
    0x05,0x00,0x50,
    0x05,0x00,0x50,
    0x14,0x00,0x14,
    0x50,0x00,0x05,
    0x00,0x00,0x00
};

/* ═══════════════════════════════════════════════════════════
 *  MCM BITMAP FONT  (18 glyphs × 8 bytes)
 *  2 bits/pixel: "01"=fg(scr_hi nibble), "00"=black($D021)
 *  Glyphs: 0-9, S(10),C(11),H(12),I(13),L(14),V(15),:(16),space(17)
 * ═══════════════════════════════════════════════════════════ */
static const unsigned char hud_font[18 * 8] = {
    /* 0 */ 0x14,0x41,0x41,0x41,0x41,0x41,0x14,0x00,
    /* 1 */ 0x10,0x10,0x10,0x10,0x10,0x10,0x15,0x00,
    /* 2 */ 0x14,0x41,0x01,0x04,0x10,0x40,0x55,0x00,
    /* 3 */ 0x14,0x41,0x01,0x14,0x01,0x41,0x14,0x00,
    /* 4 */ 0x41,0x41,0x55,0x01,0x01,0x01,0x01,0x00,
    /* 5 */ 0x55,0x40,0x54,0x01,0x01,0x41,0x14,0x00,
    /* 6 */ 0x15,0x40,0x54,0x41,0x41,0x41,0x14,0x00,
    /* 7 */ 0x55,0x01,0x01,0x04,0x04,0x04,0x04,0x00,
    /* 8 */ 0x14,0x41,0x41,0x14,0x41,0x41,0x14,0x00,
    /* 9 */ 0x14,0x41,0x41,0x15,0x01,0x01,0x14,0x00,
    /* S */ 0x14,0x41,0x40,0x14,0x01,0x41,0x14,0x00,
    /* C */ 0x15,0x40,0x40,0x40,0x40,0x40,0x15,0x00,
    /* H */ 0x41,0x41,0x41,0x55,0x41,0x41,0x41,0x00,
    /* I */ 0x55,0x04,0x04,0x04,0x04,0x04,0x55,0x00,
    /* L */ 0x40,0x40,0x40,0x40,0x40,0x40,0x55,0x00,
    /* V */ 0x41,0x41,0x41,0x41,0x14,0x14,0x10,0x00,
    /* : */ 0x00,0x14,0x14,0x00,0x14,0x14,0x00,0x00,
    /* ' '*/ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* Water wave pattern (alternating rows) */
static const unsigned char water_pat[8] = {
    0x69,0x96,0x69,0x96,0x69,0x96,0x69,0x96
};
/* $69=01 10 10 01 (LTBLUE/BLUE), $96=10 01 01 10 */

static const unsigned char grass_pat[8] = {
    0x00,0x04,0x11,0x40,0x05,0x10,0x41,0x00
};

static const unsigned char bank_pat[8] = {
    0x10,0x00,0x41,0x04,0x10,0x01,0x40,0x04
};

static const unsigned char road_pat[8] = {
    0xAA,0x80,0x2A,0x08,0xA0,0x82,0x28,0xAA
};

/* Hollow pad pattern for empty home spots */
static const unsigned char pad_pat[8] = {
    0x55,0x41,0x41,0x41,0x41,0x41,0x41,0x55
};
/* top+bottom full, sides only */

static const unsigned char car_pat_end[8] = {
    0x00,0x14,0x3C,0xFF,0xFF,0xBE,0x82,0x00
};

static const unsigned char car_pat_mid[8] = {
    0x00,0x14,0x7E,0xFF,0xFF,0xBE,0x82,0x00
};

static const unsigned char truck_pat_cab[8] = {
    0x00,0x14,0x3D,0xFF,0xFE,0xBE,0x82,0x00
};

static const unsigned char truck_pat_mid[8] = {
    0x00,0x3C,0xFF,0xFF,0xFF,0xBE,0x82,0x00
};

static const unsigned char log_pat[8] = {
    0xA5,0x6A,0x95,0x5A,0xA5,0x6A,0x95,0x5A
};

static const unsigned char turtle_pat[8] = {
    0x00,0x18,0x7E,0xFF,0x7E,0x3C,0x18,0x00
};

/* ═══════════════════════════════════════════════════════════
 *  OBJECT TABLE
 * ═══════════════════════════════════════════════════════════ */
static unsigned char obj_type [MAX_OBJECTS];
static signed char   obj_cx   [MAX_OBJECTS];
static unsigned char obj_cy   [MAX_OBJECTS];
static unsigned char obj_len  [MAX_OBJECTS];
static unsigned char obj_speed[MAX_OBJECTS];
static unsigned char obj_dir  [MAX_OBJECTS];
static unsigned char obj_anim [MAX_OBJECTS];
static unsigned char obj_tick [MAX_OBJECTS];
static unsigned char num_objs;

/* ═══════════════════════════════════════════════════════════
 *  FROG + GAME STATE
 * ═══════════════════════════════════════════════════════════ */
static unsigned char frog_cx, frog_cy;
static unsigned char frog_on_log;

static unsigned int  score;
static unsigned int  high_score;
static unsigned char lives;
static unsigned char level;
static unsigned char game_state;
static unsigned char frame_count;
static unsigned char time_left;
static unsigned char timer_frames;
static unsigned char homes_filled;
static unsigned char die_timer;
static unsigned char home_timer;
static unsigned char level_timer;
static unsigned char snd_hop_timer;
static unsigned char snd_sfx_timer;
static unsigned char prev_joy;
static unsigned char hop_lock;

/* ═══════════════════════════════════════════════════════════
 *  BITMAP DRAWING PRIMITIVES
 *  In MCM bitmap mode:
 *   pixel "00" → $D021 (BLACK, global bg)
 *   pixel "01" → scr_byte bits 7-4
 *   pixel "10" → scr_byte bits 3-0
 *   pixel "11" → colram bits 3-0
 * ═══════════════════════════════════════════════════════════ */

/* Solid colour fill (all "11" pixels) */
static void bmp_solid(unsigned char cx, unsigned char cy, unsigned char col) {
    unsigned int bofs = ((unsigned int)cy * 40 + cx) * 8;
    unsigned char p;
    for (p = 0; p < 8; ++p) BITMAP[bofs + p] = 0xFF;
    SCREEN[cy * 40 + cx] = 0;
    COLRAM[cy * 40 + cx] = col;
}

/* Water wave cell */
static void bmp_water(unsigned char cx, unsigned char cy) {
    unsigned int bofs = ((unsigned int)cy * 40 + cx) * 8;
    unsigned char p;
    for (p = 0; p < 8; ++p) BITMAP[bofs + p] = water_pat[p];
    /* "01"=LTBLUE(14)  "10"=BLUE(6) */
    SCREEN[cy * 40 + cx] = (14 << 4) | 6;
    COLRAM[cy * 40 + cx] = 0;
}

/* Custom 8-byte pattern cell — pattern uses only "01"/"00" pixels */
static void bmp_custom(unsigned char cx, unsigned char cy,
                        const unsigned char *pat8,
                        unsigned char fg_col) {
    unsigned int bofs = ((unsigned int)cy * 40 + cx) * 8;
    unsigned char p;
    for (p = 0; p < 8; ++p) BITMAP[bofs + p] = pat8[p];
    SCREEN[cy * 40 + cx] = (unsigned char)(fg_col << 4);
    COLRAM[cy * 40 + cx] = 0;
}

static void bmp_pattern(unsigned char cx, unsigned char cy,
                        const unsigned char *pat8,
                        unsigned char col01,
                        unsigned char col10,
                        unsigned char col11) {
    unsigned int bofs = ((unsigned int)cy * 40 + cx) * 8;
    unsigned char p;
    for (p = 0; p < 8; ++p) BITMAP[bofs + p] = pat8[p];
    SCREEN[cy * 40 + cx] = (unsigned char)((col01 << 4) | col10);
    COLRAM[cy * 40 + cx] = col11;
}

/* HUD font cell */
static void bmp_glyph(unsigned char cx, unsigned char cy,
                       unsigned char fi, unsigned char fg_col) {
    bmp_custom(cx, cy, hud_font + (unsigned int)fi * 8, fg_col);
}

/* Fill entire row with solid colour */
static void bmp_fill_solid(unsigned char cy, unsigned char col) {
    unsigned char cx;
    for (cx = 0; cx < SCR_W; ++cx) bmp_solid(cx, cy, col);
}

/* Fill entire row with water pattern */
static void bmp_fill_water(unsigned char cy) {
    unsigned char cx;
    for (cx = 0; cx < SCR_W; ++cx) bmp_water(cx, cy);
}

static void bmp_fill_pattern(unsigned char cy, const unsigned char *pat8,
                             unsigned char col01, unsigned char col10,
                             unsigned char col11) {
    unsigned char cx;
    for (cx = 0; cx < SCR_W; ++cx)
        bmp_pattern(cx, cy, pat8, col01, col10, col11);
}

/* Map ASCII → font index */
static unsigned char font_idx(char c) {
    if (c >= '0' && c <= '9') return (unsigned char)(c - '0');
    switch (c) {
        case 'S': return 10; case 'C': return 11;
        case 'H': return 12; case 'I': return 13;
        case 'L': return 14; case 'V': return 15;
        case ':': return 16;
        default:  return 17;
    }
}

static void bmp_puts(unsigned char cx, unsigned char cy,
                      const char *s, unsigned char fg) {
    while (*s && cx < SCR_W) {
        bmp_glyph(cx, cy, font_idx(*s), fg);
        ++cx; ++s;
    }
}

/* Write unsigned value, right-justified to 'width' cells */
static void bmp_putuint(unsigned char cx, unsigned char cy,
                         unsigned int v, unsigned char width,
                         unsigned char fg) {
    /* build string right-to-left */
    char buf[6];
    unsigned char i = 5;
    buf[5] = '\0';
    do {
        buf[--i] = '0' + (char)(v % 10);
        v /= 10;
    } while (v && i > 0);
    /* pad with zeros */
    while (i > (unsigned char)(5 - width)) buf[--i] = '0';
    bmp_puts(cx, cy, buf + i, fg);
}

/* ═══════════════════════════════════════════════════════════
 *  BITMAP MODE ENTER / EXIT
 * ═══════════════════════════════════════════════════════════ */
static void setup_bitmap_mode(void) {
    /* $D021 = BLACK ("00" pixels), $D022/$D023 = shared colours (unused here) */
    VIC_BG  = BLACK;
    VIC_MC1 = LTBLUE;
    VIC_MC2 = GREEN;
    /* Screen at $0400 (bank offset $0400), bitmap at $2000 (bank offset $2000) */
    VIC_D018 = 0x18;
    /* Enable bitmap + multicolour */
    VIC_D011 |= 0x20;
    VIC_D016 |= 0x10;
}

static void exit_bitmap_mode(void) {
    VIC_D011 &= ~0x20;
    VIC_D016 &= ~0x10;
    /* Restore default: screen at $0400, charset at $1000 ($D018=$17) */
    VIC_D018 = 0x17;
    VIC_BG   = BLACK;
}

/* ═══════════════════════════════════════════════════════════
 *  JOYSTICK
 * ═══════════════════════════════════════════════════════════ */
static unsigned char read_joy(void) {
    return ~(*(unsigned char*)0xDC00) & 0x1F;   /* port 2 = $DC00, matches vicerc JoyDevice2=3 */
}
#define FJOY_UP    0x01
#define FJOY_DOWN  0x02
#define FJOY_LEFT  0x04
#define FJOY_RIGHT 0x08
#define FJOY_FIRE  0x10

/* ═══════════════════════════════════════════════════════════
 *  SOUND
 * ═══════════════════════════════════════════════════════════ */
static void snd_init(void) {
    unsigned char i;
    for (i = 0; i < 25; ++i) *(unsigned char*)(0xD400+i) = 0;
    SID_VOL = 15;
}
static void snd_hop(void) {
    SID_V1_AD=0x01; SID_V1_SR=0x00;
    SID_V1_FHI=0x10; SID_V1_FLO=0x00;
    SID_V1_CTL=0x21; snd_hop_timer=4;
}
static void snd_squash(void) {
    SID_V2_AD=0x08; SID_V2_SR=0x00;
    SID_V2_FHI=0x03; SID_V2_FLO=0x00;
    SID_V2_CTL=0x81; snd_sfx_timer=12;
}
static void snd_splash(void) {
    SID_V2_AD=0x04; SID_V2_SR=0x00;
    SID_V2_FHI=0x06; SID_V2_FLO=0x00;
    SID_V2_CTL=0x81; snd_sfx_timer=10;
}
static void snd_home(void) {
    SID_V3_AD=0x01; SID_V3_SR=0x88;
    SID_V3_FHI=0x1A; SID_V3_FLO=0x00;
    SID_V3_CTL=0x11; snd_sfx_timer=16;
}
static void snd_update(void) {
    if (snd_hop_timer && !--snd_hop_timer) SID_V1_CTL=0x20;
    if (snd_sfx_timer && !--snd_sfx_timer) { SID_V2_CTL=0x80; SID_V3_CTL=0x10; }
}

/* ═══════════════════════════════════════════════════════════
 *  SPRITE SETUP
 * ═══════════════════════════════════════════════════════════ */
static void setup_sprite(void) {
    unsigned char i;
    unsigned char *dst = (unsigned char*)SPRITE_DATA;
    for (i = 0; i < 63; ++i) dst[i] = frog_sprite[i];
    SPRITE_PTRS[SPR_FROG] = BLK_FROG;
    SPR_COL(SPR_FROG)  = GREEN;
    VIC_SPR_ENA    = 0x01;
    VIC_SPR_HI_X   = 0x00;
    VIC_MC1        = YELLOW;
    VIC_MC2        = LTGREEN;
    VIC_SPR_MCOLOR = 0x01;
    VIC_SPR_DBL_X  = 0x00;
    VIC_SPR_DBL_Y  = 0x00;
}

static void draw_bg_cell(unsigned char cx, unsigned char cy) {
    if (cy == ROW_HUD) {
        bmp_solid(cx, cy, BLACK);
    } else if (cy == ROW_HOME) {
        bmp_pattern(cx, cy, bank_pat, GREEN, LTGREEN, YELLOW);
    } else if (cy == ROW_PADS) {
        bmp_pattern(cx, cy, bank_pat, GREEN, LTGREEN, GREEN);
    } else if (cy >= ROW_RIVER_TOP && cy <= ROW_RIVER_BOT) {
        bmp_water(cx, cy);
    } else if (cy == ROW_MEDIAN) {
        bmp_pattern(cx, cy, grass_pat, GREEN, LTGREEN, YELLOW);
    } else if (cy >= ROW_ROAD_TOP && cy <= ROW_ROAD_BOT) {
        if (cy == 11 && (cx & 1))
            bmp_solid(cx, cy, YELLOW);
        else
            bmp_pattern(cx, cy, road_pat, GREY2, GREY1, GREY3);
    } else if (cy == ROW_START) {
        bmp_pattern(cx, cy, grass_pat, GREEN, LTGREEN, YELLOW);
    } else {
        bmp_pattern(cx, cy, grass_pat, LTGREEN, GREEN, YELLOW);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  BACKGROUND DRAWING (bitmap mode)
 * ═══════════════════════════════════════════════════════════ */
static void draw_fixed_bg(void) {
    unsigned char cy, cx;

    /* Row 0: HUD — black bar, text drawn by draw_hud() */
    bmp_fill_solid(ROW_HUD, BLACK);

    /* Row 1: top green bank */
    bmp_fill_pattern(ROW_HOME, bank_pat, GREEN, LTGREEN, YELLOW);

    /* Row 2: home pad row — green + 5 empty pads drawn by draw_home_pads() */
    bmp_fill_pattern(ROW_PADS, bank_pat, GREEN, LTGREEN, GREEN);

    /* Rows 3-7: river (water wave pattern) */
    for (cy = ROW_RIVER_TOP; cy <= ROW_RIVER_BOT; ++cy)
        bmp_fill_water(cy);

    /* Row 8: median strip — lighter green */
    bmp_fill_pattern(ROW_MEDIAN, grass_pat, GREEN, LTGREEN, YELLOW);

    /* Rows 9-13: road — dark grey */
    for (cy = ROW_ROAD_TOP; cy <= ROW_ROAD_BOT; ++cy)
        bmp_fill_pattern(cy, road_pat, GREY2, GREY1, GREY3);

    /* Row 11: centre dashes — alternating YELLOW cells */
    for (cx = 1; cx < SCR_W; cx += 2)
        bmp_solid(cx, 11, YELLOW);

    /* Row 14: start safe zone */
    bmp_fill_pattern(ROW_START, grass_pat, GREEN, LTGREEN, YELLOW);

    /* Rows 15-24: lower safe zone (slightly different shade) */
    for (cy = ROW_START + 1; cy < SCR_H; ++cy)
        bmp_fill_pattern(cy, grass_pat, LTGREEN, GREEN, YELLOW);
}

static void draw_home_pads(void) {
    unsigned char p, px;
    for (p = 0; p < NUM_HOME_PADS; ++p) {
        px = 2 + p * 7;
        if (homes_filled & (1 << p)) {
            /* Filled pad: solid bright green */
            bmp_solid(px,   ROW_PADS, LTGREEN);
            bmp_solid(px+1, ROW_PADS, WHITE);
            bmp_solid(px+2, ROW_PADS, LTGREEN);
        } else {
            /* Empty pad: hollow cyan frame */
            bmp_custom(px,   ROW_PADS, pad_pat, CYAN);
            bmp_custom(px+1, ROW_PADS, pad_pat, LTBLUE);
            bmp_custom(px+2, ROW_PADS, pad_pat, CYAN);
        }
    }
}

static void draw_hud(void) {
    /* HUD background already black from draw_fixed_bg */
    bmp_puts(1,  ROW_HUD, "SC:", WHITE);
    bmp_putuint(4,  ROW_HUD, score,      5, YELLOW);
    bmp_puts(12, ROW_HUD, "HI:", WHITE);
    bmp_putuint(15, ROW_HUD, high_score, 5, YELLOW);
    bmp_puts(27, ROW_HUD, "LIV:", WHITE);
    bmp_putuint(31, ROW_HUD, (unsigned int)lives, 1, LTGREEN);
    /* Timer colour: green→yellow→red */
    {
        unsigned char tc = (time_left > 30) ? GREEN :
                           (time_left > 10) ? YELLOW : RED;
        bmp_putuint(36, ROW_HUD, (unsigned int)time_left, 2, tc);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  OBJECT DRAW / ERASE
 * ═══════════════════════════════════════════════════════════ */

/* Restore background under object */
static void erase_obj(unsigned char i) {
    unsigned char cy = obj_cy[i];
    signed char   sx;
    unsigned char k;
    for (k = 0; k < obj_len[i]; ++k) {
        sx = obj_cx[i] + (signed char)k;
        if (sx < 0 || sx >= SCR_W) continue;
        draw_bg_cell((unsigned char)sx, cy);
    }
}

static void draw_obj(unsigned char i) {
    unsigned char cy  = obj_cy[i];
    unsigned char len = obj_len[i];
    signed char   sx;
    unsigned char k, col;

    for (k = 0; k < len; ++k) {
        sx = obj_cx[i] + (signed char)k;
        if (sx < 0 || sx >= SCR_W) continue;
        switch (obj_type[i]) {
        case OBJ_CAR:
            col = (cy & 1) ? RED : PURPLE;
            bmp_pattern((unsigned char)sx, cy,
                        (k == 0 || k == len-1) ? car_pat_end : car_pat_mid,
                        WHITE, BLACK, col);
            break;
        case OBJ_TRUCK:
            bmp_pattern((unsigned char)sx, cy,
                        (k == 0) ? truck_pat_cab : truck_pat_mid,
                        CYAN, BLACK, (k == 0) ? ORANGE : BROWN);
            break;
        case OBJ_LOG:
            bmp_pattern((unsigned char)sx, cy, log_pat, BROWN, ORANGE, BLACK);
            break;
        case OBJ_TURTLE:
            if (obj_anim[i] < 6)
                bmp_pattern((unsigned char)sx, cy, turtle_pat,
                            LTGREEN, GREEN, BLACK);
            else
                bmp_water((unsigned char)sx, cy);
            break;
        default:
            break;
        }
    }
}

/* Lane parameters */
static const unsigned char lane_speed[5] = {2,1,2,1,2};
static const unsigned char road_dir[5]   = {1,0,1,0,1};
static const unsigned char river_dir[5]  = {0,1,0,1,0};

static void init_objects(void) {
    unsigned char lane, row, oi, d, sp;
    oi = 0;
    for (lane = 0; lane < 5; ++lane) {
        row = ROW_ROAD_TOP + lane;
        d   = road_dir[lane];
        sp  = lane_speed[lane];
        if (level >= 3 && sp > 1) sp = 1;
        if (oi < MAX_OBJECTS) {
            obj_type[oi]=OBJ_CAR;   obj_cy[oi]=row; obj_cx[oi]=0;
            obj_len[oi]=2; obj_dir[oi]=d; obj_speed[oi]=sp;
            obj_anim[oi]=0; obj_tick[oi]=0; ++oi;
        }
        if (oi < MAX_OBJECTS) {
            obj_type[oi]=(lane&1)?OBJ_TRUCK:OBJ_CAR; obj_cy[oi]=row;
            obj_cx[oi]=14; obj_len[oi]=(lane&1)?3:2;
            obj_dir[oi]=d; obj_speed[oi]=sp;
            obj_anim[oi]=0; obj_tick[oi]=0; ++oi;
        }
        if (oi < MAX_OBJECTS) {
            obj_type[oi]=OBJ_CAR; obj_cy[oi]=row; obj_cx[oi]=28;
            obj_len[oi]=2; obj_dir[oi]=d; obj_speed[oi]=sp;
            obj_anim[oi]=0; obj_tick[oi]=0; ++oi;
        }
    }
    for (lane = 0; lane < 5; ++lane) {
        row = ROW_RIVER_TOP + lane;
        d   = river_dir[lane];
        sp  = lane_speed[lane];
        if (level >= 2 && sp > 1) sp = 1;
        if (oi < MAX_OBJECTS) {
            obj_type[oi]=(lane&1)?OBJ_TURTLE:OBJ_LOG; obj_cy[oi]=row;
            obj_cx[oi]=0; obj_len[oi]=(lane&1)?2:4;
            obj_dir[oi]=d; obj_speed[oi]=sp;
            obj_anim[oi]=0; obj_tick[oi]=0; ++oi;
        }
        if (oi < MAX_OBJECTS) {
            obj_type[oi]=(lane&1)?OBJ_TURTLE:OBJ_LOG; obj_cy[oi]=row;
            obj_cx[oi]=20; obj_len[oi]=(lane&1)?2:5;
            obj_dir[oi]=d; obj_speed[oi]=sp;
            obj_anim[oi]=0; obj_tick[oi]=0; ++oi;
        }
    }
    num_objs = oi;
}

static void update_objects(void) {
    unsigned char i;
    for (i = 0; i < num_objs; ++i) {
        if (obj_type[i] == OBJ_TURTLE) {
            ++obj_anim[i]; if (obj_anim[i] >= 12) obj_anim[i] = 0;
        }
        if (++obj_tick[i] < obj_speed[i]) continue;
        obj_tick[i] = 0;
        erase_obj(i);
        if (obj_dir[i]) {
            ++obj_cx[i];
            if (obj_cx[i] >= SCR_W) obj_cx[i] = -(signed char)obj_len[i];
        } else {
            --obj_cx[i];
            if (obj_cx[i] < -(signed char)obj_len[i]) obj_cx[i] = SCR_W-1;
        }
        draw_obj(i);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  FROG PLACEMENT
 * ═══════════════════════════════════════════════════════════ */
static void place_frog_sprite(void) {
    unsigned int sx = C2SX(frog_cx);
    if (sx >= 256) { VIC_SPR_HI_X |= 0x01; SPR_X(SPR_FROG) = (unsigned char)(sx-256); }
    else           { VIC_SPR_HI_X &= 0xFE; SPR_X(SPR_FROG) = (unsigned char)sx; }
    SPR_Y(SPR_FROG) = C2SY(frog_cy);
}

/* ═══════════════════════════════════════════════════════════
 *  COLLISION DETECTION
 * ═══════════════════════════════════════════════════════════ */
static unsigned char obj_contains_x(unsigned char i, unsigned char frog_x) {
    signed char left  = obj_cx[i];
    signed char right = left + (signed char)obj_len[i];
    signed char fx    = (signed char)frog_x;

    return (fx >= left && fx < right) ? 1 : 0;
}

static unsigned char check_river(void) {
    unsigned char i, cy = frog_cy, cx = frog_cx;
    if (cy < ROW_RIVER_TOP || cy > ROW_RIVER_BOT) return 0;
    for (i = 0; i < num_objs; ++i) {
        if (obj_cy[i] != cy) continue;
        if (obj_type[i] != OBJ_LOG && obj_type[i] != OBJ_TURTLE) continue;
        if (obj_type[i] == OBJ_TURTLE && obj_anim[i] >= 6) continue;
        if (obj_contains_x(i, cx)) {
            frog_on_log = i+1; return 1;
        }
    }
    frog_on_log = 0; return 0;
}

static unsigned char check_road(void) {
    unsigned char i, cy = frog_cy, cx = frog_cx;
    if (cy < ROW_ROAD_TOP || cy > ROW_ROAD_BOT) return 0;
    for (i = 0; i < num_objs; ++i) {
        if (obj_cy[i] != cy) continue;
        if (obj_type[i] != OBJ_CAR && obj_type[i] != OBJ_TRUCK) continue;
        if (obj_contains_x(i, cx))
            return 1;
    }
    return 0;
}

static unsigned char check_home(void) {
    unsigned char p, pcx;
    if (frog_cy != ROW_PADS) return 0;
    for (p = 0; p < NUM_HOME_PADS; ++p) {
        pcx = 2 + p * 7;
        if (frog_cx >= pcx && frog_cx <= pcx+2) {
            if (homes_filled & (1<<p)) return 2;
            homes_filled |= (1<<p); return 1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  DEATH / RESET
 * ═══════════════════════════════════════════════════════════ */
static void show_death(unsigned char is_splash) {
    if (is_splash) {
        bmp_solid(frog_cx, frog_cy, CYAN);
        snd_splash();
    } else {
        bmp_solid(frog_cx, frog_cy, RED);
        snd_squash();
    }
    die_timer = 30; game_state = GS_DYING;
    VIC_SPR_ENA &= ~0x01;
}

static void reset_frog(void) {
    frog_cx = FROG_START_CX; frog_cy = FROG_START_CY;
    frog_on_log = 0; time_left = TIME_START;
    timer_frames = 0; prev_joy = 0; hop_lock = 0;
    place_frog_sprite();
    VIC_SPR_ENA |= 0x01;
}

/* ═══════════════════════════════════════════════════════════
 *  INPUT
 * ═══════════════════════════════════════════════════════════ */
static void handle_input(void) {
    unsigned char joy = read_joy();
    signed char dx = 0, dy = 0;
    if (hop_lock) { --hop_lock; return; }
    if (!(joy & (FJOY_UP|FJOY_DOWN|FJOY_LEFT|FJOY_RIGHT))) { prev_joy=0; return; }
    if (joy == prev_joy) return;
    prev_joy = joy;
    if      (joy & FJOY_UP)    dy = -1;
    else if (joy & FJOY_DOWN)  dy =  1;
    else if (joy & FJOY_LEFT)  dx = -1;
    else if (joy & FJOY_RIGHT) dx =  1;
    else return;
    {
        signed char nx = (signed char)frog_cx + dx;
        signed char ny = (signed char)frog_cy + dy;
        if (nx < 0)       nx = 0;
        if (nx >= SCR_W)  nx = SCR_W-1;
        if (ny < ROW_PADS)   ny = ROW_PADS;
        if (ny > ROW_START)  ny = ROW_START;
        frog_cx = (unsigned char)nx;
        frog_cy = (unsigned char)ny;
    }
    place_frog_sprite();
    snd_hop(); hop_lock = 6;
}

/* ═══════════════════════════════════════════════════════════
 *  LEVEL INIT
 * ═══════════════════════════════════════════════════════════ */
static void init_level(void) {
    unsigned char i;
    for (i = 0; i < MAX_OBJECTS; ++i) obj_tick[i] = 0;
    homes_filled = 0;
    draw_fixed_bg();
    draw_home_pads();
    init_objects();
    for (i = 0; i < num_objs; ++i) draw_obj(i);
    reset_frog();
    draw_hud();
}

/* ═══════════════════════════════════════════════════════════
 *  TITLE SCREEN  (text mode)
 * ═══════════════════════════════════════════════════════════ */
static void show_title(void) {
    clrscr();
    textcolor(GREEN);  gotoxy(12,5);  cputs("* FROGGER *");
    textcolor(LTGREEN); gotoxy(5,7);  cputs("C64 AI TOOLCHAIN 2026");
    textcolor(YELLOW); gotoxy(4,11);  cputs("CROSS ROAD AND RIVER");
                       gotoxy(5,12);  cputs("REACH ALL 5 HOME PADS");
    textcolor(CYAN);   gotoxy(6,15);  cputs("JOYSTICK PORT 2");
    textcolor(WHITE);  gotoxy(8,18);  cputs("FIRE  TO  START");
    if (high_score) {
        textcolor(ORANGE); gotoxy(8,21);
        cprintf("HIGH SCORE: %05u", high_score);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════ */
int main(void) {
    clrscr();
    VIC_BG = BLACK; VIC_BORDER = BLACK;
    snd_init();
    setup_sprite();

    score = 0; high_score = 0;
    lives = NUM_LIVES_START; level = 1;
    game_state = GS_TITLE; frame_count = 0;

    show_title();

    while (1) {
        waitvsync();
        ++frame_count;
        snd_update();

        /* ── TITLE (text mode) ──────────────────────── */
        if (game_state == GS_TITLE) {
            /* Start on fire OR auto-start after ~5s for AI toolchain */
            if ((read_joy() & FJOY_FIRE) || frame_count >= 250) {
                score = 0; lives = NUM_LIVES_START; level = 1;
                frame_count = 0;
                setup_bitmap_mode();
                init_level();
                game_state = GS_PLAY;
            }
            continue;
        }

        /* ── PLAY ───────────────────────────────────── */
        if (game_state == GS_PLAY) {
            ++timer_frames;
            if (timer_frames >= 50) {
                timer_frames = 0;
                if (time_left > 0) {
                    --time_left; draw_hud();
                    if (time_left == 0) { show_death(0); continue; }
                }
            }
            handle_input();
            if (frame_count & 1) {
                update_objects();
                if (frog_on_log) {
                    unsigned char li = frog_on_log - 1;
                    if (obj_tick[li] == 0) {
                        signed char nx;
                        if (obj_dir[li]) nx = (signed char)frog_cx + 1;
                        else             nx = (signed char)frog_cx - 1;
                        if (nx < 0 || nx >= SCR_W) { show_death(1); continue; }
                        frog_cx = (unsigned char)nx;
                        place_frog_sprite();
                    }
                }
            }
            {
                unsigned char cy = frog_cy;
                if (cy >= ROW_RIVER_TOP && cy <= ROW_RIVER_BOT) {
                    if (!check_river()) { show_death(1); continue; }
                } else { frog_on_log = 0; }
                if (cy >= ROW_ROAD_TOP && cy <= ROW_ROAD_BOT)
                    if (check_road()) { show_death(0); continue; }
                if (cy == ROW_PADS) {
                    unsigned char hres = check_home();
                    if (hres == 1) {
                        score += 50 + (unsigned int)time_left;
                        if (score > high_score) high_score = score;
                        draw_home_pads(); draw_hud();
                        snd_home(); home_timer = 30;
                        game_state = GS_HOME;
                        if (homes_filled == 0x1F) {
                            level_timer = 80; game_state = GS_LEVEL;
                        }
                    } else if (hres != 1) { show_death(0); }
                }
            }
            continue;
        }

        /* ── DYING ──────────────────────────────────── */
        if (game_state == GS_DYING) {
            if (die_timer) { --die_timer; continue; }
            {
                unsigned char cy = frog_cy, col;
                if      (cy >= ROW_RIVER_TOP && cy <= ROW_RIVER_BOT)  col = BLUE;
                else if (cy >= ROW_ROAD_TOP  && cy <= ROW_ROAD_BOT)   col = GREY1;
                else                                                    col = GREEN;
                if (cy >= ROW_RIVER_TOP && cy <= ROW_RIVER_BOT)
                    bmp_water(frog_cx, cy);
                else
                    draw_bg_cell(frog_cx, cy);
                /* restore dash if needed */
                if (cy == 11 && (frog_cx & 1) == 1)
                    bmp_solid(frog_cx, 11, YELLOW);
            }
            --lives; draw_hud();
            if (lives == 0) { game_state = GS_OVER; VIC_SPR_ENA &= ~0x01; }
            else            { reset_frog(); game_state = GS_PLAY; }
            continue;
        }

        /* ── HOME ───────────────────────────────────── */
        if (game_state == GS_HOME) {
            if (home_timer) { --home_timer; continue; }
            reset_frog(); game_state = GS_PLAY;
            continue;
        }

        /* ── LEVEL CLEAR ────────────────────────────── */
        if (game_state == GS_LEVEL) {
            if (level_timer == 80) {
                /* Draw "LEVEL X CLEAR" in HUD area via bitmap font */
                bmp_fill_solid(12, PURPLE);
                bmp_puts(11, 12, "LV", YELLOW);
                bmp_putuint(13, 12, (unsigned int)level, 1, WHITE);
                bmp_puts(14, 12, "CLEAR", LTGREEN);
                score += (unsigned int)level * 200;
                if (score > high_score) high_score = score;
                draw_hud();
            }
            if (level_timer) { --level_timer; continue; }
            ++level;
            /* Clear level clear row */
            bmp_fill_solid(12, GREY1);
            /* Restore centre dashes (row 11 already done in draw_fixed_bg) */
            init_level();
            game_state = GS_PLAY;
            continue;
        }

        /* ── GAME OVER ──────────────────────────────── */
        if (game_state == GS_OVER) {
            exit_bitmap_mode();
            clrscr();
            textcolor(RED);    gotoxy(13,11); cputs("GAME OVER");
            textcolor(WHITE);  gotoxy(10,13); cprintf("SCORE: %05u", score);
            textcolor(ORANGE); gotoxy(10,15); cprintf("LEVEL: %u", (unsigned int)level);
            {
                unsigned char w;
                for (w = 0; w < 200; ++w) waitvsync();
            }
            clrscr();
            frame_count = 0;
            show_title();
            game_state = GS_TITLE;
            continue;
        }
    }
    return 0;
}

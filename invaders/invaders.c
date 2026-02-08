/*
 * Space Invaders for Commodore 64 — Written in C using cc65
 *
 * Features:
 *  - 55 aliens (5 rows × 11 columns) drawn as characters
 *  - Player ship as hardware sprite
 *  - Player bullet & alien bombs as sprites
 *  - SID sound effects (shoot, explode, march, death)
 *  - Increasing march speed as aliens are destroyed
 *  - Shield bunkers that degrade when hit
 *  - Mystery UFO across the top
 *  - Demo AI mode (auto-play)
 *  - Multiple waves with increasing difficulty
 *
 * AI Toolchain Project 2026
 */

#include <c64.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <joystick.h>

/* ═══════════════════════════════════════════════════════════
 *  HARDWARE DEFINES
 * ═══════════════════════════════════════════════════════════ */

/* ── Screen ─────────────────────────────────────────────── */
#define SCR_W   40
#define SCR_H   25
#define SCREEN  ((unsigned char*)0x0400)
#define COLRAM  ((unsigned char*)0xD800)

/* ── Sprite pointers & data ─────────────────────────────── */
#define SPRITE_PTRS  ((unsigned char*)0x07F8)
#define SPRITE_DATA  0x3000            /* block 192 */

#define SPR_SHIP      0   /* sprite index: player ship */
#define SPR_BULLET    1   /* player bullet */
#define SPR_BOMB0     2   /* alien bomb 0 */
#define SPR_BOMB1     3   /* alien bomb 1 */
#define SPR_BOMB2     4   /* alien bomb 2 */
#define SPR_UFO       5   /* mystery UFO */

#define BLK_SHIP   192    /* $3000 / 64 */
#define BLK_BULLET 193
#define BLK_BOMB   194
#define BLK_UFO    195
#define BLK_ALIEN1 196    /* not used as sprite, just reserved */

/* Custom character screen codes for pixel-art aliens */
#define CHARSET_RAM     0x3800
#define CHAR_SQUID_L1   100
#define CHAR_SQUID_R1   101
#define CHAR_SQUID_L2   102
#define CHAR_SQUID_R2   103
#define CHAR_CRAB_L1    104
#define CHAR_CRAB_R1    105
#define CHAR_CRAB_L2    106
#define CHAR_CRAB_R2    107
#define CHAR_OCTO_L1    108
#define CHAR_OCTO_R1    109
#define CHAR_OCTO_L2    110
#define CHAR_OCTO_R2    111
#define CHAR_EXPLODE_L  112
#define CHAR_EXPLODE_R  113

/* ── VIC-II ─────────────────────────────────────────────── */
#define VIC_SPR_ENA     (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X    (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR  (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X   (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y   (*(unsigned char*)0xD017)
#define VIC_SPR_PRIO    (*(unsigned char*)0xD01B)

/* Sprite pos/color registers – addressed by index */
#define SPR_X(n) (*(unsigned char*)(0xD000 + (n)*2))
#define SPR_Y(n) (*(unsigned char*)(0xD001 + (n)*2))
#define SPR_COL(n) (*(unsigned char*)(0xD027 + (n)))

/* ── SID ────────────────────────────────────────────────── */
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

/* ── Colors ─────────────────────────────────────────────── */
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

/* ── Coord helpers ──────────────────────────────────────── */
#define SPR_XOFS  24
#define SPR_YOFS  50
#define C2SX(c) ((unsigned char)(SPR_XOFS + (c) * 8))
#define C2SY(r) ((unsigned char)(SPR_YOFS + (r) * 8))

/* ═══════════════════════════════════════════════════════════
 *  GAME CONSTANTS
 * ═══════════════════════════════════════════════════════════ */

#define ALIEN_COLS     11
#define ALIEN_ROWS      5
#define TOTAL_ALIENS   (ALIEN_COLS * ALIEN_ROWS)  /* 55 */

/* Alien grid position (character coords) */
#define ALIEN_START_X   3
#define ALIEN_START_Y   4
#define ALIEN_SPACING_X 3   /* chars between alien centres */
#define ALIEN_SPACING_Y 2   /* rows between alien rows */

#define ALIEN_DEAD     32  /* space */

/* Player ship */
#define SHIP_Y_CHAR    22
#define SHIP_Y_SPR     C2SY(SHIP_Y_CHAR)
#define SHIP_MIN_X     C2SX(1)
#define SHIP_MAX_X     C2SX(37)
#define SHIP_SPEED      2

/* Shields */
#define SHIELD_COUNT    4
#define SHIELD_Y       19   /* char row */
#define SHIELD_W        4   /* chars wide each */

/* Screen bounds for bullet/bomb */
#define BULLET_TOP     C2SY(1)
#define BOMB_BOTTOM    C2SY(23)

/* Game states */
#define GS_TITLE   0
#define GS_PLAY    1
#define GS_DYING   2
#define GS_WON     3
#define GS_LOST    4
#define GS_NEXT    5

/* ═══════════════════════════════════════════════════════════
 *  GAME VARIABLES
 * ═══════════════════════════════════════════════════════════ */

/* Alien grid: 0 = dead, 1-3 = type (row dependent) */
static unsigned char aliens[ALIEN_ROWS][ALIEN_COLS];
static unsigned char aliens_left;

/* Alien swarm position (top-left in char coords) */
static unsigned char swarm_x;       /* leftmost column offset */
static unsigned char swarm_y;       /* topmost row */
static signed char   swarm_dx;      /* +1 or -1 */
static unsigned char swarm_step;    /* march counter */
static unsigned char march_speed;   /* frames between steps (lower=faster) */
static unsigned char march_timer;

/* Find the actual column bounds of living aliens for wall detection */
static unsigned char alive_col_min, alive_col_max;
static unsigned char alive_row_max;

/* Player */
static unsigned char ship_x;       /* sprite X */
static unsigned char bullet_active;
static unsigned char bullet_x, bullet_y;

/* Alien bombs */
#define MAX_BOMBS 3
static unsigned char bomb_active[MAX_BOMBS];
static unsigned char bomb_x[MAX_BOMBS];
static unsigned char bomb_y[MAX_BOMBS];
static unsigned char bomb_timer;

/* UFO */
static unsigned char ufo_active;
static unsigned char ufo_x;
static signed char   ufo_dx;
static unsigned int  ufo_timer;

/* Score, lives, wave */
static unsigned int  score;
static unsigned char lives;
static unsigned char wave;
static unsigned char game_state;
static unsigned char demo_mode;
static unsigned char frame_count;
static unsigned char snd_timer1, snd_timer2;

/* Shields: 4 bunkers, each 4 chars wide × 3 rows (stored as screen data) */
/* We just draw them and let bullets/bombs erase chars on contact */

/* March sound notes (classic 4-note descending pattern) */
static const unsigned char march_notes[4] = { 0x1C, 0x18, 0x14, 0x10 };
static unsigned char march_note_idx;

/* Alien type per row */
static const unsigned char row_type[ALIEN_ROWS]  = { 3, 2, 2, 1, 1 };
static const unsigned char row_color[ALIEN_ROWS] = { WHITE, CYAN, CYAN, GREEN, GREEN };
static const unsigned char row_char_l1[ALIEN_ROWS] = {
    CHAR_SQUID_L1, CHAR_CRAB_L1, CHAR_CRAB_L1, CHAR_OCTO_L1, CHAR_OCTO_L1
};
static const unsigned char row_char_r1[ALIEN_ROWS] = {
    CHAR_SQUID_R1, CHAR_CRAB_R1, CHAR_CRAB_R1, CHAR_OCTO_R1, CHAR_OCTO_R1
};
static const unsigned char row_char_l2[ALIEN_ROWS] = {
    CHAR_SQUID_L2, CHAR_CRAB_L2, CHAR_CRAB_L2, CHAR_OCTO_L2, CHAR_OCTO_L2
};
static const unsigned char row_char_r2[ALIEN_ROWS] = {
    CHAR_SQUID_R2, CHAR_CRAB_R2, CHAR_CRAB_R2, CHAR_OCTO_R2, CHAR_OCTO_R2
};
static const unsigned char row_score[ALIEN_ROWS] = { 30, 20, 20, 10, 10 };

/* ═══════════════════════════════════════════════════════════
 *  SPRITE DATA INITIALISATION
 * ═══════════════════════════════════════════════════════════ */

static void init_sprite_data(void) {
    unsigned char *d;
    unsigned char i;

    /* ── Ship: classic "cannon" shape 16px wide ───────── */
    d = (unsigned char*)SPRITE_DATA;       /* block 192 */
    for (i = 0; i < 63; ++i) d[i] = 0;
    /*  Row 0:     ..#.....  = centered turret */
    d[0]  = 0x04; d[1]  = 0x00;
    /*  Row 1:     .###....  */
    d[3]  = 0x0E; d[4]  = 0x00;
    /*  Row 2:     .###....  */
    d[6]  = 0x0E; d[7]  = 0x00;
    /*  Row 3:    #######.. body */
    d[9]  = 0x1F; d[10] = 0xC0;
    /*  Row 4:   #########. */
    d[12] = 0x3F; d[13] = 0xE0;
    /*  Row 5:   #########. */
    d[15] = 0x3F; d[16] = 0xE0;
    /*  Row 6:   #########. */
    d[18] = 0x3F; d[19] = 0xE0;

    /* ── Bullet: thin 2×4 vertical line ───────────────── */
    d = (unsigned char*)(SPRITE_DATA + 64);  /* block 193 */
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0] = 0xC0;
    d[3] = 0xC0;
    d[6] = 0xC0;
    d[9] = 0xC0;

    /* ── Bomb: small 2×4 dot ──────────────────────────── */
    d = (unsigned char*)(SPRITE_DATA + 128);  /* block 194 */
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0] = 0xC0;
    d[3] = 0x40;
    d[6] = 0xC0;
    d[9] = 0x40;

    /* ── UFO: small saucer shape ──────────────────────── */
    d = (unsigned char*)(SPRITE_DATA + 192);  /* block 195 */
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0]  = 0x0F; d[1]  = 0x00;   /* ..XXXX...... */
    d[3]  = 0x3F; d[4]  = 0xC0;   /* .XXXXXXXX... */
    d[6]  = 0x7F; d[7]  = 0xE0;   /* XXXXXXXXXX.. */
    d[9]  = 0xFF; d[10] = 0xF0;   /* XXXXXXXXXXXX */
    d[12] = 0x3F; d[13] = 0xC0;   /* .XXXXXXXX... */
}

/* ═══════════════════════════════════════════════════════════
 *  CUSTOM CHARACTER SET FOR ALIEN PIXEL ART
 * ═══════════════════════════════════════════════════════════ */

/* 14 custom characters × 8 bytes each:
 * 3 alien types × (left + right) × 2 animation frames = 12 chars
 * + explosion left/right = 2 chars
 *
 *  Squid (30pts):  Narrow tentacled alien
 *  Crab  (20pts):  Wide-armed antenna alien
 *  Octopus (10pts): Round bodied with legs
 */
static const unsigned char alien_chardata[] = {
    /* ── Squid left, frame 1 ───── */
    /*  ....##..  */  0x0C,
    /*  ...####.  */  0x1E,
    /*  ..######  */  0x3F,
    /*  .##.##.#  */  0x6D,
    /*  .#######  */  0x7F,
    /*  ...#..#.  */  0x12,
    /*  ..#.....  */  0x20,
    /*  .#.#....  */  0x50,
    /* ── Squid right, frame 1 ──── */
    /*  ..##....  */  0x30,
    /*  .####...  */  0x78,
    /*  ######..  */  0xFC,
    /*  #.##.##.  */  0xB6,
    /*  #######.  */  0xFE,
    /*  .#..#...  */  0x48,
    /*  .....#..  */  0x04,
    /*  ....#.#.  */  0x0A,
    /* ── Squid left, frame 2 ───── */
    0x0C, 0x1E, 0x3F, 0x6D, 0x7F, 0x12, 0x24, 0x05,
    /* ── Squid right, frame 2 ──── */
    0x30, 0x78, 0xFC, 0xB6, 0xFE, 0x48, 0x24, 0xA0,
    /* ── Crab left, frame 1 ────── */
    /*  ..#.....  */  0x20,
    /*  ...#....  */  0x10,
    /*  ..######  */  0x3F,
    /*  .##.##.#  */  0x6D,
    /*  ########  */  0xFF,
    /*  #.####.#  */  0xBD,
    /*  #.#.....  */  0xA0,
    /*  ....##..  */  0x0C,
    /* ── Crab right, frame 1 ───── */
    /*  .....#..  */  0x04,
    /*  ....#...  */  0x08,
    /*  ######..  */  0xFC,
    /*  #.##.##.  */  0xB6,
    /*  ########  */  0xFF,
    /*  #.####.#  */  0xBD,
    /*  .....#.#  */  0x05,
    /*  ..##....  */  0x30,
    /* ── Crab left, frame 2 ────── */
    0x20, 0x90, 0xBF, 0xED, 0xFF, 0x7D, 0x20, 0x40,
    /* ── Crab right, frame 2 ───── */
    0x04, 0x09, 0xFD, 0xB7, 0xFF, 0xBE, 0x04, 0x02,
    /* ── Octopus left, frame 1 ─── */
    /*  ....####  */  0x0F,
    /*  ..######  */  0x3F,
    /*  .##.##.#  */  0x6D,
    /*  .#######  */  0x7F,
    /*  ...##.##  */  0x1B,
    /*  ..#.#.#.  */  0x2A,
    /*  .#.#....  */  0x50,
    /*  #.......  */  0x80,
    /* ── Octopus right, frame 1 ── */
    /*  ####....  */  0xF0,
    /*  ######..  */  0xFC,
    /*  #.##.##.  */  0xB6,
    /*  #######.  */  0xFE,
    /*  ##.##...  */  0xD8,
    /*  .#.#.#..  */  0x54,
    /*  ....#.#.  */  0x0A,
    /*  .......#  */  0x01,
    /* ── Octopus left, frame 2 ─── */
    0x0F, 0x3F, 0x6D, 0x7F, 0x1B, 0x26, 0x52, 0x20,
    /* ── Octopus right, frame 2 ── */
    0xF0, 0xFC, 0xB6, 0xFE, 0xD8, 0x64, 0x4A, 0x04,
    /* ── Explosion left ────────── */
    /*  .#...#..  */  0x44,
    /*  ..#.....  */  0x20,
    /*  ....#...  */  0x08,
    /*  ##....#.  */  0xC2,
    /*  ....#...  */  0x08,
    /*  .#......  */  0x40,
    /*  ..#.#...  */  0x28,
    /*  #...#...  */  0x88,
    /* ── Explosion right ───────── */
    /*  ..#...#.  */  0x22,
    /*  .....#..  */  0x04,
    /*  ...#....  */  0x10,
    /*  .#....##  */  0x43,
    /*  ...#....  */  0x10,
    /*  ......#.  */  0x02,
    /*  ...#.#..  */  0x14,
    /*  ...#...#  */  0x11
};

#define NUM_CUSTOM_CHARS  14
#define FIRST_CUSTOM_CHAR 100

static void init_custom_charset(void) {
    unsigned char *dst;
    unsigned int i;
    unsigned char old_port;

    /* ── Copy ROM charset (lowercase set) to RAM at $3800 */
    __asm__("sei");
    old_port = *(unsigned char*)0x01;
    *(unsigned char*)0x01 = old_port & 0xFB;  /* reveal char ROM at $D000 */

    /* cc65 runtime uses lowercase/uppercase charset (ROM at $D800),
       NOT the uppercase/graphics set at $D000 */
    for (i = 0; i < 2048; ++i) {
        ((unsigned char*)CHARSET_RAM)[i] = ((unsigned char*)0xD800)[i];
    }

    *(unsigned char*)0x01 = old_port;         /* restore I/O at $D000 */
    __asm__("cli");

    /* ── Write custom alien characters into charset ──── */
    dst = (unsigned char*)(CHARSET_RAM + FIRST_CUSTOM_CHAR * 8);
    for (i = 0; i < NUM_CUSTOM_CHARS * 8; ++i) {
        dst[i] = alien_chardata[i];
    }

    /* ── Point VIC-II to custom charset at $3800 ─────── */
    /* $D018: bits7-4=screen@$0400(0001), bits3-1=chars@$3800(111) */
    *(unsigned char*)0xD018 = 0x1E;
}

static void setup_sprites(void) {
    SPRITE_PTRS[SPR_SHIP]   = BLK_SHIP;
    SPRITE_PTRS[SPR_BULLET] = BLK_BULLET;
    SPRITE_PTRS[SPR_BOMB0]  = BLK_BOMB;
    SPRITE_PTRS[SPR_BOMB1]  = BLK_BOMB;
    SPRITE_PTRS[SPR_BOMB2]  = BLK_BOMB;
    SPRITE_PTRS[SPR_UFO]    = BLK_UFO;

    VIC_SPR_ENA    = 0;    /* enable individually later */
    VIC_SPR_DBL_X  = 0;
    VIC_SPR_DBL_Y  = 0;
    VIC_SPR_MCOLOR = 0;
    VIC_SPR_HI_X   = 0;
    VIC_SPR_PRIO   = 0;

    SPR_COL(SPR_SHIP)   = GREEN;
    SPR_COL(SPR_BULLET) = WHITE;
    SPR_COL(SPR_BOMB0)  = YELLOW;
    SPR_COL(SPR_BOMB1)  = YELLOW;
    SPR_COL(SPR_BOMB2)  = YELLOW;
    SPR_COL(SPR_UFO)    = RED;
}

/* ═══════════════════════════════════════════════════════════
 *  SOUND
 * ═══════════════════════════════════════════════════════════ */

static void snd_init(void) {
    SID_VOL    = 15;
    SID_V1_AD  = 0x00;
    SID_V1_SR  = 0xA0;
    SID_V2_AD  = 0x08;
    SID_V2_SR  = 0x00;
    SID_V3_AD  = 0x00;
    SID_V3_SR  = 0x90;
}

/* Shoot: short high noise burst */
static void snd_shoot(void) {
    SID_V1_FHI = 0x28;
    SID_V1_CTL = 0x81;  /* noise gate on */
    snd_timer1 = 4;
}

/* Alien explode: medium crash */
static void snd_explode(void) {
    SID_V2_FHI = 0x20;
    SID_V2_CTL = 0x81;  /* noise gate on */
    snd_timer2 = 6;
}

/* March: bass thump (4-note cycle) */
static void snd_march(void) {
    SID_V3_FHI = march_notes[march_note_idx];
    SID_V3_FLO = 0x00;
    SID_V3_CTL = 0x21;  /* sawtooth gate on */
    march_note_idx = (march_note_idx + 1) & 3;
}

/* UFO whine */
static void snd_ufo(void) {
    SID_V2_FHI = 0x30 + (frame_count & 7);
    SID_V2_CTL = 0x41;  /* pulse gate on */
}

/* Player death: descending wail */
static void snd_death(void) {
    unsigned char i;
    for (i = 0x40; i > 0x05; i -= 2) {
        SID_V1_FHI = i;
        SID_V1_CTL = 0x21;
        waitvsync();
    }
    SID_V1_CTL = 0x20;
}

static void snd_off(void) {
    SID_V1_CTL = 0x00;
    SID_V2_CTL = 0x00;
    SID_V3_CTL = 0x00;
}

static void snd_tick(void) {
    if (snd_timer1 > 0) { --snd_timer1; if (!snd_timer1) SID_V1_CTL = 0x00; }
    if (snd_timer2 > 0) { --snd_timer2; if (!snd_timer2) SID_V2_CTL = 0x00; }
}

/* ═══════════════════════════════════════════════════════════
 *  DRAWING
 * ═══════════════════════════════════════════════════════════ */

static void draw_char(unsigned char x, unsigned char y,
                      unsigned char ch, unsigned char col) {
    unsigned int pos = (unsigned int)y * 40 + x;
    SCREEN[pos] = ch;
    COLRAM[pos] = col;
}

static unsigned char read_char(unsigned char x, unsigned char y) {
    return SCREEN[(unsigned int)y * 40 + x];
}

/* ── Alien Grid Drawing ─────────────────────────────────── */

static void draw_aliens(void) {
    unsigned char r, c;
    unsigned char ax, ay;

    for (r = 0; r < ALIEN_ROWS; ++r) {
        ay = swarm_y + r * ALIEN_SPACING_Y;
        if (ay >= SCR_H) continue;
        for (c = 0; c < ALIEN_COLS; ++c) {
            ax = swarm_x + c * ALIEN_SPACING_X;
            if (ax >= SCR_W - 1) continue;

            if (aliens[r][c]) {
                /* Alternate between two animation frames */
                if (swarm_step & 1) {
                    draw_char(ax,     ay, row_char_l2[r], row_color[r]);
                    draw_char(ax + 1, ay, row_char_r2[r], row_color[r]);
                } else {
                    draw_char(ax,     ay, row_char_l1[r], row_color[r]);
                    draw_char(ax + 1, ay, row_char_r1[r], row_color[r]);
                }
            } else {
                draw_char(ax,     ay, 32, BLACK);
                draw_char(ax + 1, ay, 32, BLACK);
            }
        }
    }
}

/* Erase old alien positions before moving */
static void erase_aliens(void) {
    unsigned char r, c, ax, ay;
    for (r = 0; r < ALIEN_ROWS; ++r) {
        ay = swarm_y + r * ALIEN_SPACING_Y;
        if (ay >= SCR_H) continue;
        for (c = 0; c < ALIEN_COLS; ++c) {
            ax = swarm_x + c * ALIEN_SPACING_X;
            if (ax >= SCR_W - 1) continue;
            draw_char(ax,     ay, 32, BLACK);
            draw_char(ax + 1, ay, 32, BLACK);
        }
    }
}

/* ── Shields ──────────────────────────────────────────── */

static void draw_shields(void) {
    unsigned char s, x, bx;
    for (s = 0; s < SHIELD_COUNT; ++s) {
        bx = 4 + s * 9;  /* evenly spaced */
        /* 3 rows of shield blocks */
        for (x = 0; x < SHIELD_W; ++x) {
            draw_char(bx + x, SHIELD_Y,     0xA0, GREEN);
            draw_char(bx + x, SHIELD_Y + 1, 0xA0, GREEN);
            draw_char(bx + x, SHIELD_Y + 2, 0xA0, GREEN);
        }
        /* Notch in bottom centre */
        draw_char(bx + 1, SHIELD_Y + 2, 32, BLACK);
        draw_char(bx + 2, SHIELD_Y + 2, 32, BLACK);
    }
}

/* ── HUD ──────────────────────────────────────────────── */

static void draw_hud(void) {
    gotoxy(0, 0);
    textcolor(WHITE);
    cprintf("SCORE:%05u", score);

    if (demo_mode) {
        gotoxy(15, 0);
        textcolor(GREEN);
        cprintf("DEMO");
    } else {
        gotoxy(15, 0);
        textcolor(LTBLUE);
        cprintf("WAVE:%u", wave);
    }

    gotoxy(33, 0);
    textcolor(YELLOW);
    cprintf("x%u", lives);

    /* Ground line row 23 */
    {
        unsigned char i;
        for (i = 0; i < 40; ++i)
            draw_char(i, 23, 0xC0, GREEN); /* horizontal line char */
    }
}

/* ═══════════════════════════════════════════════════════════
 *  GAME LOGIC – ALIEN SWARM
 * ═══════════════════════════════════════════════════════════ */

static void find_alive_bounds(void) {
    unsigned char r, c;
    alive_col_min = ALIEN_COLS;
    alive_col_max = 0;
    alive_row_max = 0;

    for (r = 0; r < ALIEN_ROWS; ++r) {
        for (c = 0; c < ALIEN_COLS; ++c) {
            if (aliens[r][c]) {
                if (c < alive_col_min) alive_col_min = c;
                if (c > alive_col_max) alive_col_max = c;
                if (r > alive_row_max) alive_row_max = r;
            }
        }
    }
}

static void move_swarm(void) {
    unsigned char right_edge, left_edge;
    unsigned char drop;

    ++march_timer;
    if (march_timer < march_speed) return;
    march_timer = 0;

    find_alive_bounds();
    if (alive_col_min >= ALIEN_COLS) return; /* all dead */

    /* Calculate screen positions of outermost living aliens */
    right_edge = swarm_x + alive_col_max * ALIEN_SPACING_X + 2;
    left_edge  = swarm_x + alive_col_min * ALIEN_SPACING_X;

    drop = 0;

    /* Check if we need to reverse direction */
    if (swarm_dx > 0 && right_edge >= SCR_W - 2) {
        swarm_dx = -1;
        drop = 1;
    } else if (swarm_dx < 0 && left_edge <= 1) {
        swarm_dx = 1;
        drop = 1;
    }

    erase_aliens();

    if (drop) {
        ++swarm_y;
        /* Check invasion – aliens reached the ship row */
        if (swarm_y + alive_row_max * ALIEN_SPACING_Y >= SHIP_Y_CHAR - 1) {
            game_state = GS_LOST;
            return;
        }
    } else {
        swarm_x = (unsigned char)((signed char)swarm_x + swarm_dx);
    }

    ++swarm_step;
    snd_march();

    draw_aliens();
}

/* ═══════════════════════════════════════════════════════════
 *  BULLETS & BOMBS
 * ═══════════════════════════════════════════════════════════ */

static void fire_bullet(void) {
    if (bullet_active) return;
    bullet_active = 1;
    bullet_x = ship_x + 4;  /* centre of ship sprite */
    bullet_y = SHIP_Y_SPR - 8;
    snd_shoot();
}

static void move_bullet(void) {
    unsigned char cx, cy, ch;
    unsigned char r, c, ax, ay;

    if (!bullet_active) return;

    /* Move up */
    if (bullet_y <= BULLET_TOP + 4) {
        bullet_active = 0;
        VIC_SPR_ENA &= ~(1 << SPR_BULLET);
        return;
    }
    bullet_y -= 4;  /* fast bullet */

    /* Update sprite */
    SPR_X(SPR_BULLET) = bullet_x;
    SPR_Y(SPR_BULLET) = bullet_y;
    VIC_SPR_ENA |= (1 << SPR_BULLET);

    /* Convert to char coords */
    cx = (bullet_x - SPR_XOFS) >> 3;
    cy = (bullet_y - SPR_YOFS) >> 3;

    /* ── Hit UFO? ─────────────────────────────────────── */
    if (ufo_active && cy <= 2) {
        if (bullet_x >= ufo_x - 8 && bullet_x <= ufo_x + 16) {
            /* UFO destroyed */
            ufo_active = 0;
            VIC_SPR_ENA &= ~(1 << SPR_UFO);
            score += 100 + (rand() & 0x7F); /* 100-227 points */
            snd_explode();
            bullet_active = 0;
            VIC_SPR_ENA &= ~(1 << SPR_BULLET);
            return;
        }
    }

    /* ── Hit shield? ──────────────────────────────────── */
    ch = read_char(cx, cy);
    if (ch == 0xA0) { /* shield block */
        draw_char(cx, cy, 32, BLACK);
        bullet_active = 0;
        VIC_SPR_ENA &= ~(1 << SPR_BULLET);
        return;
    }

    /* ── Hit alien? ───────────────────────────────────── */
    for (r = 0; r < ALIEN_ROWS; ++r) {
        ay = swarm_y + r * ALIEN_SPACING_Y;
        if (cy != ay) continue;
        for (c = 0; c < ALIEN_COLS; ++c) {
            if (!aliens[r][c]) continue;
            ax = swarm_x + c * ALIEN_SPACING_X;
            if (cx >= ax && cx <= ax + 1) {
                /* Kill alien! */
                aliens[r][c] = 0;
                --aliens_left;
                score += row_score[r] * wave;
                draw_char(ax,     ay, CHAR_EXPLODE_L, YELLOW);
                draw_char(ax + 1, ay, CHAR_EXPLODE_R, YELLOW);
                snd_explode();

                /* Speed up march */
                if (march_speed > 3) {
                    if (aliens_left < 10)      march_speed = 2;
                    else if (aliens_left < 20) march_speed = 4;
                    else if (aliens_left < 35) march_speed = 6;
                }

                bullet_active = 0;
                VIC_SPR_ENA &= ~(1 << SPR_BULLET);

                if (aliens_left == 0) game_state = GS_WON;
                return;
            }
        }
    }
}

/* ── Alien Bombs ──────────────────────────────────────── */

static void drop_bomb(void) {
    unsigned char i, c, r;
    unsigned char ax, ay;

    /* Find a free bomb slot */
    for (i = 0; i < MAX_BOMBS; ++i) {
        if (!bomb_active[i]) break;
    }
    if (i >= MAX_BOMBS) return;

    /* Pick a random living alien from the bottom row of each column */
    c = rand() % ALIEN_COLS;
    for (r = ALIEN_ROWS; r > 0; --r) {
        if (aliens[r - 1][c]) {
            ax = swarm_x + c * ALIEN_SPACING_X;
            ay = swarm_y + (r - 1) * ALIEN_SPACING_Y;

            bomb_active[i] = 1;
            bomb_x[i] = C2SX(ax + 1);
            bomb_y[i] = C2SY(ay + 1);
            return;
        }
    }
}

static void move_bombs(void) {
    unsigned char i, cx, cy, ch;

    for (i = 0; i < MAX_BOMBS; ++i) {
        if (!bomb_active[i]) continue;

        bomb_y[i] += 2;  /* fall speed */

        /* Off bottom? */
        if (bomb_y[i] >= BOMB_BOTTOM) {
            bomb_active[i] = 0;
            VIC_SPR_ENA &= ~(1 << (SPR_BOMB0 + i));
            continue;
        }

        /* Update sprite */
        SPR_X(SPR_BOMB0 + i) = bomb_x[i];
        SPR_Y(SPR_BOMB0 + i) = bomb_y[i];
        VIC_SPR_ENA |= (1 << (SPR_BOMB0 + i));

        /* Convert to char */
        cx = (bomb_x[i] - SPR_XOFS) >> 3;
        cy = (bomb_y[i] - SPR_YOFS) >> 3;

        /* Hit shield? */
        ch = read_char(cx, cy);
        if (ch == 0xA0) {
            draw_char(cx, cy, 32, BLACK);
            bomb_active[i] = 0;
            VIC_SPR_ENA &= ~(1 << (SPR_BOMB0 + i));
            continue;
        }

        /* Hit player? */
        if (bomb_y[i] >= SHIP_Y_SPR - 2 && bomb_y[i] <= SHIP_Y_SPR + 8) {
            if (bomb_x[i] >= ship_x - 4 && bomb_x[i] <= ship_x + 12) {
                bomb_active[i] = 0;
                VIC_SPR_ENA &= ~(1 << (SPR_BOMB0 + i));
                game_state = GS_DYING;
                return;
            }
        }
    }
}

/* ── UFO ──────────────────────────────────────────────── */

static void update_ufo(void) {
    if (!ufo_active) {
        ++ufo_timer;
        if (ufo_timer > 600) {  /* appear roughly every 10 sec */
            ufo_active = 1;
            ufo_timer = 0;
            if (rand() & 1) {
                ufo_x = C2SX(1);
                ufo_dx = 1;
            } else {
                ufo_x = C2SX(37);
                ufo_dx = -1;
            }
            SPR_Y(SPR_UFO) = C2SY(1);
        }
        return;
    }

    /* Move UFO */
    ufo_x = (unsigned char)((signed char)ufo_x + ufo_dx);
    SPR_X(SPR_UFO) = ufo_x;
    VIC_SPR_ENA |= (1 << SPR_UFO);
    snd_ufo();

    /* Off screen? */
    if (ufo_x <= C2SX(0) || ufo_x >= C2SX(38)) {
        ufo_active = 0;
        VIC_SPR_ENA &= ~(1 << SPR_UFO);
        SID_V2_CTL = 0x00;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  INPUT
 * ═══════════════════════════════════════════════════════════ */

static unsigned char joy_dir(void) {
    unsigned char j = joy_read(JOY_2);
    if (JOY_LEFT(j))  return 1;
    if (JOY_RIGHT(j)) return 2;
    return 0;
}

static unsigned char joy_fire(void) {
    return JOY_FIRE(joy_read(JOY_2));
}

/* Demo AI: track lowest alien column and shoot */
static void demo_ai(void) {
    unsigned char c, r, target_col;
    unsigned char target_x;

    /* Find a random living alien to aim at */
    target_col = 255;
    for (c = 0; c < ALIEN_COLS; ++c) {
        for (r = ALIEN_ROWS; r > 0; --r) {
            if (aliens[r - 1][c]) {
                target_col = c;
                goto found;
            }
        }
    }
found:
    if (target_col == 255) return;

    target_x = C2SX(swarm_x + target_col * ALIEN_SPACING_X + 1);

    /* Move toward target */
    if (ship_x < target_x - 2 && ship_x < SHIP_MAX_X)
        ship_x += SHIP_SPEED;
    else if (ship_x > target_x + 2 && ship_x > SHIP_MIN_X)
        ship_x -= SHIP_SPEED;

    /* Shoot when aligned */
    if (ship_x >= target_x - 4 && ship_x <= target_x + 4) {
        if ((frame_count & 7) == 0) fire_bullet();
    }
}

/* ═══════════════════════════════════════════════════════════
 *  INIT & SCREEN
 * ═══════════════════════════════════════════════════════════ */

static void init_wave(void) {
    unsigned char r, c, i;

    /* Fill alien grid */
    aliens_left = 0;
    for (r = 0; r < ALIEN_ROWS; ++r) {
        for (c = 0; c < ALIEN_COLS; ++c) {
            aliens[r][c] = row_type[r];
            ++aliens_left;
        }
    }

    swarm_x  = ALIEN_START_X;
    swarm_y  = ALIEN_START_Y;
    swarm_dx = 1;
    swarm_step = 0;
    march_timer = 0;
    march_note_idx = 0;

    /* March speed decreases with wave */
    march_speed = 12;
    if (wave >= 2) march_speed = 10;
    if (wave >= 4) march_speed = 8;

    /* Ship */
    ship_x = C2SX(19);  /* centre */
    bullet_active = 0;

    /* Bombs */
    for (i = 0; i < MAX_BOMBS; ++i) bomb_active[i] = 0;
    bomb_timer = 0;

    /* UFO */
    ufo_active = 0;
    ufo_timer = 0;

    /* Draw */
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);
    draw_hud();
    draw_aliens();
    draw_shields();

    /* Position ship sprite */
    SPR_Y(SPR_SHIP) = SHIP_Y_SPR;
    SPR_X(SPR_SHIP) = ship_x;
    VIC_SPR_ENA = (1 << SPR_SHIP);  /* only ship visible initially */
}

/* ═══════════════════════════════════════════════════════════
 *  TITLE SCREEN
 * ═══════════════════════════════════════════════════════════ */

static void draw_title(void) {
    unsigned char i;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    gotoxy(8, 3);
    textcolor(WHITE);
    cprintf("S P A C E");

    gotoxy(6, 5);
    textcolor(GREEN);
    cprintf("I N V A D E R S");

    /* Draw pixel-art alien chars as decoration */
    for (i = 0; i < 11; ++i) {
        draw_char(4 + i * 3, 8,  row_char_l1[0], WHITE);
        draw_char(5 + i * 3, 8,  row_char_r1[0], WHITE);
        draw_char(4 + i * 3, 10, row_char_l1[2], CYAN);
        draw_char(5 + i * 3, 10, row_char_r1[2], CYAN);
        draw_char(4 + i * 3, 12, row_char_l1[4], GREEN);
        draw_char(5 + i * 3, 12, row_char_r1[4], GREEN);
    }

    gotoxy(5, 9);
    textcolor(WHITE);
    /* score table */
    gotoxy(11, 15);
    textcolor(WHITE);
    cprintf("= 30 POINTS");
    draw_char(8, 15, row_char_l1[0], WHITE);
    draw_char(9, 15, row_char_r1[0], WHITE);

    gotoxy(11, 16);
    textcolor(CYAN);
    cprintf("= 20 POINTS");
    draw_char(8, 16, row_char_l1[2], CYAN);
    draw_char(9, 16, row_char_r1[2], CYAN);

    gotoxy(11, 17);
    textcolor(GREEN);
    cprintf("= 10 POINTS");
    draw_char(8, 17, row_char_l1[4], GREEN);
    draw_char(9, 17, row_char_r1[4], GREEN);

    gotoxy(10, 18);
    textcolor(RED);
    cprintf("= ??? MYSTERY");
    /* small "UFO" text */
    draw_char(8, 18, 0x55, RED); /* 'U' */

    gotoxy(7, 21);
    textcolor(CYAN);
    cprintf("PRESS FIRE TO START");

    gotoxy(8, 22);
    textcolor(GREEN);
    cprintf("OR WAIT FOR DEMO");

    gotoxy(5, 24);
    textcolor(GREY1);
    cprintf("AI TOOLCHAIN PROJECT 2026");
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN GAME LOOP
 * ═══════════════════════════════════════════════════════════ */

static void game_loop(void) {
    unsigned char dir;

    while (1) {
        waitvsync();
        ++frame_count;
        snd_tick();

        /* ── PLAY ────────────────────────────────────── */
        if (game_state == GS_PLAY) {

            /* Input / AI */
            if (demo_mode) {
                demo_ai();
            } else {
                dir = joy_dir();
                if (dir == 1 && ship_x > SHIP_MIN_X)
                    ship_x -= SHIP_SPEED;
                else if (dir == 2 && ship_x < SHIP_MAX_X)
                    ship_x += SHIP_SPEED;
                if (joy_fire()) fire_bullet();
            }

            /* Update ship sprite */
            SPR_X(SPR_SHIP) = ship_x;

            /* Alien march */
            move_swarm();

            /* Bullet */
            move_bullet();

            /* Bombs */
            ++bomb_timer;
            if (bomb_timer >= 30) {
                bomb_timer = 0;
                drop_bomb();
            }
            move_bombs();

            /* UFO */
            update_ufo();

            /* HUD */
            draw_hud();

            /* Demo exit on fire */
            if (demo_mode && joy_fire()) return;

            continue;
        }

        /* ── DYING ───────────────────────────────────── */
        if (game_state == GS_DYING) {
            snd_death();
            --lives;
            if (lives == 0) {
                game_state = GS_LOST;
            } else {
                /* Reset player position, keep aliens */
                ship_x = C2SX(19);
                SPR_X(SPR_SHIP) = ship_x;
                bullet_active = 0;
                {
                    unsigned char i;
                    for (i = 0; i < MAX_BOMBS; ++i) {
                        bomb_active[i] = 0;
                        VIC_SPR_ENA &= ~(1 << (SPR_BOMB0 + i));
                    }
                }
                VIC_SPR_ENA &= ~(1 << SPR_BULLET);
                game_state = GS_PLAY;
            }
            continue;
        }

        /* ── WON ─────────────────────────────────────── */
        if (game_state == GS_WON) {
            gotoxy(12, 12);
            textcolor(YELLOW);
            cprintf("WAVE CLEAR!");
            {
                unsigned char w;
                for (w = 0; w < 180; ++w) waitvsync();
            }
            if (demo_mode) return;
            ++wave;
            init_wave();
            game_state = GS_PLAY;
            continue;
        }

        /* ── LOST ────────────────────────────────────── */
        if (game_state == GS_LOST) {
            snd_off();
            gotoxy(14, 12);
            textcolor(RED);
            cprintf("GAME OVER");
            gotoxy(12, 14);
            textcolor(WHITE);
            cprintf("SCORE: %05u", score);
            {
                unsigned char w;
                for (w = 0; w < 240; ++w) waitvsync();
            }
            return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════ */

int main(void) {
    unsigned int title_timer;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    snd_init();
    init_sprite_data();
    init_custom_charset();
    setup_sprites();
    joy_install(joy_static_stddrv);

title_screen:
    draw_title();
    snd_off();

    title_timer = 0;
    while (!joy_fire() && title_timer < 350) {
        waitvsync();
        ++title_timer;
    }

    /* Setup game */
    score = 0;
    lives = 3;
    wave  = 1;
    frame_count = 0;
    snd_timer1 = 0;
    snd_timer2 = 0;

    demo_mode = joy_fire() ? 0 : 1;

    init_wave();
    game_state = GS_PLAY;

    game_loop();

    goto title_screen;

    return 0;
}

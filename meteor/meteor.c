/*
 * METEOR STORM for Commodore 64 — Written in C using cc65
 *
 * A mashup of Asteroids + Space Invaders + Arkanoid:
 *  - Large meteors split into 2 small ones when hit (Asteroids)
 *  - 4 destructible shield bunkers protect the player (Invaders)
 *  - Power-ups drop from destroyed meteors (Arkanoid)
 *  - Scrolling starfield background
 *  - UFO mystery ship bonus
 *  - SID sound effects (3 voices)
 *  - Demo AI auto-play mode
 *  - Progressive wave difficulty
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

#define SCR_W   40
#define SCR_H   25
#define SCREEN  ((unsigned char*)0x0400)
#define COLRAM  ((unsigned char*)0xD800)

/* ── Sprite pointers & data ─────────────────────────────── */
#define SPRITE_PTRS  ((unsigned char*)0x07F8)
#define SPRITE_DATA  0x3000

#define SPR_SHIP      0
#define SPR_BULLET    1
#define SPR_PWRUP     2
#define SPR_UFO       3

#define BLK_SHIP    192   /* $3000 */
#define BLK_BULLET  193   /* $3040 */
#define BLK_PWRUP   194   /* $3080 */
#define BLK_UFO     195   /* $30C0 */

/* Custom charset for meteors + stars at $3800 */
#define CHARSET_RAM     0x3800
#define CHAR_METEOR_L1  100   /* large meteor left,  frame 1 */
#define CHAR_METEOR_R1  101   /* large meteor right, frame 1 */
#define CHAR_METEOR_L2  102   /* large meteor left,  frame 2 */
#define CHAR_METEOR_R2  103   /* large meteor right, frame 2 */
#define CHAR_SMALL_1    104   /* small meteor frame 1 */
#define CHAR_SMALL_2    105   /* small meteor frame 2 */
#define CHAR_EXPLODE1   106   /* explosion frame 1 */
#define CHAR_EXPLODE2   107   /* explosion frame 2 */
#define CHAR_STAR1      108   /* dim star */
#define CHAR_STAR2      109   /* bright star */
#define CHAR_SHIELD     110   /* shield block (rounded) */
#define CHAR_PWRUP_S    111   /* power-up icon: shield */
#define CHAR_PWRUP_D    112   /* power-up icon: double */
#define CHAR_PWRUP_B    113   /* power-up icon: bomb */

#define NUM_CUSTOM_CHARS 14
#define FIRST_CUSTOM_CHAR 100

/* ── VIC-II ─────────────────────────────────────────────── */
#define VIC_SPR_ENA     (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X    (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR  (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X   (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y   (*(unsigned char*)0xD017)
#define VIC_SPR_PRIO    (*(unsigned char*)0xD01B)

#define SPR_X(n) (*(unsigned char*)(0xD000 + (n)*2))
#define SPR_Y(n) (*(unsigned char*)(0xD001 + (n)*2))
#define SPR_COL(n) (*(unsigned char*)(0xD027 + (n)))

/* ── SID ────────────────────────────────────────────────── */
#define SID_V1_FLO (*(unsigned char*)0xD400)
#define SID_V1_FHI (*(unsigned char*)0xD401)
#define SID_V1_PWL (*(unsigned char*)0xD402)
#define SID_V1_PWH (*(unsigned char*)0xD403)
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

/* ── Coordinate helpers ─────────────────────────────────── */
#define SPR_XOFS  24
#define SPR_YOFS  50
#define C2SX(c) ((unsigned char)(SPR_XOFS + (c) * 8))
#define C2SY(r) ((unsigned char)(SPR_YOFS + (r) * 8))

/* ═══════════════════════════════════════════════════════════
 *  GAME CONSTANTS
 * ═══════════════════════════════════════════════════════════ */

/* Ship */
#define SHIP_Y_CHAR  22
#define SHIP_Y_SPR   C2SY(SHIP_Y_CHAR)
#define SHIP_MIN_X   C2SX(1)
#define SHIP_MAX_X   C2SX(37)
#define SHIP_SPEED   2

/* Shields */
#define SHIELD_COUNT   4
#define SHIELD_Y      20
#define SHIELD_W       4

/* Bullet */
#define BULLET_TOP    C2SY(1)
#define BULLET_SPEED  5

/* Meteors */
#define MAX_METEORS   16
#define METEOR_LARGE   2    /* takes 2 hits, splits */
#define METEOR_SMALL   1    /* takes 1 hit */

/* Power-ups */
#define PWRUP_NONE     0
#define PWRUP_SHIELD   1    /* repair shields */
#define PWRUP_DOUBLE   2    /* double shot for 10 sec */
#define PWRUP_BOMB     3    /* clear all meteors on screen */

/* Starfield */
#define MAX_STARS     20

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

/* Meteor data */
static unsigned char met_active[MAX_METEORS];
static unsigned char met_x[MAX_METEORS];     /* char X */
static unsigned char met_y[MAX_METEORS];     /* char Y (8.0 – integer) */
static signed char   met_dx[MAX_METEORS];    /* -1, 0, +1 */
static unsigned char met_dy[MAX_METEORS];    /* fall speed (1-3) */
static unsigned char met_size[MAX_METEORS];  /* LARGE=2 or SMALL=1 */
static unsigned char met_hp[MAX_METEORS];    /* hitpoints */
static unsigned char meteors_alive;
static unsigned char meteors_spawned;
static unsigned char meteors_this_wave;

/* Player */
static unsigned char ship_x;
static unsigned char bullet_active;
static unsigned char bullet_x, bullet_y;
static unsigned char bullet2_active;         /* double-shot second bullet */
static unsigned char bullet2_x, bullet2_y;

/* Power-up falling */
static unsigned char pwrup_active;
static unsigned char pwrup_type;
static unsigned char pwrup_x, pwrup_y;       /* sprite coords */

/* Active power-up effect */
static unsigned char double_shot;
static unsigned int  double_timer;

/* UFO */
static unsigned char ufo_active;
static unsigned char ufo_x;
static signed char   ufo_dx;
static unsigned int  ufo_timer;

/* Stars */
static unsigned char star_x[MAX_STARS];
static unsigned char star_y[MAX_STARS];
static unsigned char star_speed[MAX_STARS];  /* 1=slow, 2=med, 3=fast */
static unsigned char star_char[MAX_STARS];

/* Game state */
static unsigned int  score;
static unsigned char lives;
static unsigned char wave;
static unsigned char game_state;
static unsigned char demo_mode;
static unsigned char frame_count;
static unsigned char spawn_timer;
static unsigned char snd_timer1, snd_timer2, snd_timer3;
static unsigned char anim_frame;

/* Combo system */
static unsigned char combo_count;
static unsigned char combo_timer;

/* ═══════════════════════════════════════════════════════════
 *  CUSTOM CHARACTER DATA (14 chars × 8 bytes)
 * ═══════════════════════════════════════════════════════════ */

static const unsigned char custom_chardata[] = {
    /* ── CHAR_METEOR_L1: large meteor left half, frame 1 ── */
    0x07,  /*  .....###  */
    0x1F,  /*  ...#####  */
    0x3F,  /*  ..######  */
    0x7E,  /*  .######.  */
    0x7F,  /*  .#######  */
    0x3F,  /*  ..######  */
    0x1E,  /*  ...####.  */
    0x07,  /*  .....###  */

    /* ── CHAR_METEOR_R1: large meteor right half, frame 1 ─ */
    0xC0,  /*  ##......  */
    0xF0,  /*  ####....  */
    0xF8,  /*  #####...  */
    0xFC,  /*  ######..  */
    0xF8,  /*  #####...  */
    0xFC,  /*  ######..  */
    0xF0,  /*  ####....  */
    0xE0,  /*  ###.....  */

    /* ── CHAR_METEOR_L2: large meteor left half, frame 2 ── */
    0x03,  /*  ......##  */
    0x0F,  /*  ....####  */
    0x3F,  /*  ..######  */
    0x7F,  /*  .#######  */
    0x7E,  /*  .######.  */
    0x3F,  /*  ..######  */
    0x0F,  /*  ....####  */
    0x06,  /*  .....##.  */

    /* ── CHAR_METEOR_R2: large meteor right half, frame 2 ─ */
    0xE0,  /*  ###.....  */
    0xF8,  /*  #####...  */
    0xFC,  /*  ######..  */
    0xF8,  /*  #####...  */
    0xFC,  /*  ######..  */
    0xF0,  /*  ####....  */
    0xF8,  /*  #####...  */
    0xC0,  /*  ##......  */

    /* ── CHAR_SMALL_1: small meteor, frame 1 ───────────── */
    0x18,  /*  ...##...  */
    0x3C,  /*  ..####..  */
    0x7E,  /*  .######.  */
    0x7E,  /*  .######.  */
    0x7F,  /*  .#######  */
    0x3E,  /*  ..#####.  */
    0x1C,  /*  ...###..  */
    0x08,  /*  ....#...  */

    /* ── CHAR_SMALL_2: small meteor, frame 2 ───────────── */
    0x10,  /*  ...#....  */
    0x38,  /*  ..###...  */
    0x7C,  /*  .#####..  */
    0xFE,  /*  #######.  */
    0x7E,  /*  .######.  */
    0x3C,  /*  ..####..  */
    0x3C,  /*  ..####..  */
    0x18,  /*  ...##...  */

    /* ── CHAR_EXPLODE1: explosion frame 1 ──────────────── */
    0x42,  /*  .#....#.  */
    0x24,  /*  ..#..#..  */
    0x08,  /*  ....#...  */
    0xC3,  /*  ##....##  */
    0x10,  /*  ...#....  */
    0x24,  /*  ..#..#..  */
    0x42,  /*  .#....#.  */
    0x81,  /*  #......#  */

    /* ── CHAR_EXPLODE2: explosion frame 2 ──────────────── */
    0x81,  /*  #......#  */
    0x00,  /*  ........  */
    0x24,  /*  ..#..#..  */
    0x00,  /*  ........  */
    0x24,  /*  ..#..#..  */
    0x00,  /*  ........  */
    0x81,  /*  #......#  */
    0x00,  /*  ........  */

    /* ── CHAR_STAR1: dim star (single pixel) ───────────── */
    0x00,  /*  ........  */
    0x00,  /*  ........  */
    0x00,  /*  ........  */
    0x08,  /*  ....#...  */
    0x00,  /*  ........  */
    0x00,  /*  ........  */
    0x00,  /*  ........  */
    0x00,  /*  ........  */

    /* ── CHAR_STAR2: bright star (cross) ───────────────── */
    0x00,  /*  ........  */
    0x08,  /*  ....#...  */
    0x08,  /*  ....#...  */
    0x3E,  /*  ..#####.  */
    0x08,  /*  ....#...  */
    0x08,  /*  ....#...  */
    0x00,  /*  ........  */
    0x00,  /*  ........  */

    /* ── CHAR_SHIELD: shield block ─────────────────────── */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */
    0xFF,  /*  ########  */

    /* ── CHAR_PWRUP_S: shield repair icon ──────────────── */
    0x3C,  /*  ..####..  */
    0x42,  /*  .#....#.  */
    0x99,  /*  #..##..#  */
    0xA5,  /*  #.#..#.#  */
    0xA5,  /*  #.#..#.#  */
    0x99,  /*  #..##..#  */
    0x42,  /*  .#....#.  */
    0x3C,  /*  ..####..  */

    /* ── CHAR_PWRUP_D: double shot icon ────────────────── */
    0x24,  /*  ..#..#..  */
    0x24,  /*  ..#..#..  */
    0x24,  /*  ..#..#..  */
    0x24,  /*  ..#..#..  */
    0x24,  /*  ..#..#..  */
    0x24,  /*  ..#..#..  */
    0x7E,  /*  .######.  */
    0x3C,  /*  ..####..  */

    /* ── CHAR_PWRUP_B: bomb icon ───────────────────────── */
    0x0C,  /*  ....##..  */
    0x18,  /*  ...##...  */
    0x3C,  /*  ..####..  */
    0x7E,  /*  .######.  */
    0x7E,  /*  .######.  */
    0x7E,  /*  .######.  */
    0x3C,  /*  ..####..  */
    0x18,  /*  ...##...  */
};

/* ═══════════════════════════════════════════════════════════
 *  CHARSET + SPRITE INITIALIZATION
 * ═══════════════════════════════════════════════════════════ */

static void init_custom_charset(void) {
    unsigned char *dst;
    unsigned int i;
    unsigned char old_port;

    /* Copy ROM charset to RAM at $3800 */
    /* cc65 runtime uses lowercase/uppercase charset (ROM at $D800),
       NOT the uppercase/graphics set at $D000 */
    __asm__("sei");
    old_port = *(unsigned char*)0x01;
    *(unsigned char*)0x01 = (old_port & 0xFB);  /* expose char ROM, hide I/O */

    for (i = 0; i < 2048; ++i) {
        ((unsigned char*)CHARSET_RAM)[i] = ((unsigned char*)0xD800)[i];
    }

    *(unsigned char*)0x01 = old_port;
    __asm__("cli");

    /* Write custom characters */
    dst = (unsigned char*)(CHARSET_RAM + FIRST_CUSTOM_CHAR * 8);
    for (i = 0; i < NUM_CUSTOM_CHARS * 8; ++i) {
        dst[i] = custom_chardata[i];
    }

    /* Point VIC-II to charset at $3800 (Bank 0 default) */
    /* D018: screen at $0400 (bits 7-4=0001), charset at $3800 (bits 3-1=111) */
    *(unsigned char*)0xD018 = 0x1E;
}

static void init_sprite_data(void) {
    unsigned char *d;
    unsigned char i;

    /* ── Ship: triangular fighter ─────────────────────── */
    d = (unsigned char*)SPRITE_DATA;
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0]  = 0x02; d[1]  = 0x00;  /*  ......#.  */
    d[3]  = 0x07; d[4]  = 0x00;  /*  .....###  */
    d[6]  = 0x07; d[7]  = 0x00;  /*  .....###  */
    d[9]  = 0x0F; d[10] = 0x80;  /*  ....####  # */
    d[12] = 0x0F; d[13] = 0x80;  /*  ....####  # */
    d[15] = 0x1F; d[16] = 0xC0;  /*  ...######  */
    d[18] = 0x3F; d[19] = 0xE0;  /*  ..#######  # */
    d[21] = 0xFF; d[22] = 0xF8;  /*  ##########  ## */
    d[24] = 0xFF; d[25] = 0xF8;  /*  ##########  ## */

    /* ── Bullet: thin 2×6 line ────────────────────────── */
    d = (unsigned char*)(SPRITE_DATA + 64);
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0] = 0xC0;
    d[3] = 0xC0;
    d[6] = 0xC0;
    d[9] = 0xC0;
    d[12] = 0xC0;
    d[15] = 0xC0;

    /* ── Power-up: pulsing diamond ────────────────────── */
    d = (unsigned char*)(SPRITE_DATA + 128);
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0]  = 0x04; d[1]  = 0x00;
    d[3]  = 0x0E; d[4]  = 0x00;
    d[6]  = 0x1F; d[7]  = 0x00;
    d[9]  = 0x3F; d[10] = 0x80;
    d[12] = 0x1F; d[13] = 0x00;
    d[15] = 0x0E; d[16] = 0x00;
    d[18] = 0x04; d[19] = 0x00;

    /* ── UFO: saucer ──────────────────────────────────── */
    d = (unsigned char*)(SPRITE_DATA + 192);
    for (i = 0; i < 63; ++i) d[i] = 0;
    d[0]  = 0x0F; d[1]  = 0x00;
    d[3]  = 0x3F; d[4]  = 0xC0;
    d[6]  = 0x7F; d[7]  = 0xE0;
    d[9]  = 0xFF; d[10] = 0xF0;
    d[12] = 0x3F; d[13] = 0xC0;
}

static void setup_sprites(void) {
    SPRITE_PTRS[SPR_SHIP]   = BLK_SHIP;
    SPRITE_PTRS[SPR_BULLET] = BLK_BULLET;
    SPRITE_PTRS[SPR_PWRUP]  = BLK_PWRUP;
    SPRITE_PTRS[SPR_UFO]    = BLK_UFO;

    VIC_SPR_ENA    = 0;
    VIC_SPR_DBL_X  = 0;
    VIC_SPR_DBL_Y  = 0;
    VIC_SPR_MCOLOR = 0;
    VIC_SPR_HI_X   = 0;
    VIC_SPR_PRIO   = 0;

    SPR_COL(SPR_SHIP)   = LTGREEN;
    SPR_COL(SPR_BULLET) = WHITE;
    SPR_COL(SPR_PWRUP)  = CYAN;
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

static void snd_shoot(void) {
    SID_V1_FHI = 0x28;
    SID_V1_FLO = 0x00;
    SID_V1_CTL = 0x81;   /* noise, gate on */
    snd_timer1 = 3;
}

static void snd_explode_small(void) {
    SID_V2_FHI = 0x15;
    SID_V2_FLO = 0x00;
    SID_V2_CTL = 0x81;   /* noise, gate on */
    snd_timer2 = 5;
}

static void snd_explode_large(void) {
    SID_V2_FHI = 0x0A;
    SID_V2_FLO = 0x00;
    SID_V2_CTL = 0x81;   /* deep noise */
    snd_timer2 = 8;
}

static void snd_split(void) {
    /* Quick high-low chirp for meteor split */
    SID_V3_FHI = 0x30;
    SID_V3_FLO = 0x00;
    SID_V3_CTL = 0x21;   /* sawtooth, gate on */
    snd_timer3 = 4;
}

static void snd_powerup(void) {
    SID_V3_FHI = 0x20;
    SID_V3_FLO = 0x00;
    SID_V3_CTL = 0x11;   /* triangle, gate on */
    snd_timer3 = 8;
}

static void snd_bomb(void) {
    /* Dramatic sweep */
    unsigned char i;
    for (i = 0x30; i > 0x02; i -= 3) {
        SID_V1_FHI = i;
        SID_V1_CTL = 0x81;
        SID_V2_FHI = 0x40 - i;
        SID_V2_CTL = 0x21;
        waitvsync();
    }
    SID_V1_CTL = 0x00;
    SID_V2_CTL = 0x00;
}

static void snd_ufo_tick(void) {
    SID_V3_FHI = 0x30 + (frame_count & 0x07);
    SID_V3_CTL = 0x41;   /* pulse, gate on */
}

static void snd_death(void) {
    unsigned char i;
    for (i = 0x40; i > 0x04; i -= 2) {
        SID_V1_FHI = i;
        SID_V1_CTL = 0x21;
        waitvsync();
    }
    SID_V1_CTL = 0x20;
}

static void snd_combo(void) {
    /* Rising arpeggio for combos */
    SID_V3_FHI = 0x18 + combo_count * 4;
    SID_V3_CTL = 0x11;
    snd_timer3 = 3;
}

static void snd_off(void) {
    SID_V1_CTL = 0x00;
    SID_V2_CTL = 0x00;
    SID_V3_CTL = 0x00;
}

static void snd_tick(void) {
    if (snd_timer1 > 0) { --snd_timer1; if (!snd_timer1) SID_V1_CTL = 0x00; }
    if (snd_timer2 > 0) { --snd_timer2; if (!snd_timer2) SID_V2_CTL = 0x00; }
    if (snd_timer3 > 0) { --snd_timer3; if (!snd_timer3) SID_V3_CTL = 0x00; }
}

/* ═══════════════════════════════════════════════════════════
 *  DRAWING HELPERS
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

/* ═══════════════════════════════════════════════════════════
 *  STARFIELD
 * ═══════════════════════════════════════════════════════════ */

static void init_stars(void) {
    unsigned char i;
    for (i = 0; i < MAX_STARS; ++i) {
        star_x[i]     = rand() % 40;
        star_y[i]     = 2 + (rand() % 17);  /* rows 2-18 */
        star_speed[i] = 1 + (rand() % 3);
        star_char[i]  = (rand() & 1) ? CHAR_STAR1 : CHAR_STAR2;
    }
}

static void update_stars(void) {
    unsigned char i;
    unsigned char ch;

    for (i = 0; i < MAX_STARS; ++i) {
        /* Only move if this frame aligns with star speed */
        if ((frame_count & 3) >= star_speed[i]) continue;

        /* Erase old position only if still a star char (never erase meteors/shields) */
        if (star_y[i] < SCR_H && star_x[i] < SCR_W) {
            ch = read_char(star_x[i], star_y[i]);
            if (ch == CHAR_STAR1 || ch == CHAR_STAR2) {
                draw_char(star_x[i], star_y[i], 32, BLACK);
            }
        }

        /* Move down */
        ++star_y[i];
        if (star_y[i] >= SHIELD_Y) {
            /* Respawn at top */
            star_y[i] = 2;
            star_x[i] = rand() % 40;
            star_speed[i] = 1 + (rand() % 3);
            star_char[i]  = (star_speed[i] == 3) ? CHAR_STAR2 : CHAR_STAR1;
        }

        /* Draw new position (only if cell is truly empty – not a meteor or other object) */
        ch = read_char(star_x[i], star_y[i]);
        if (ch == 32) {
            draw_char(star_x[i], star_y[i], star_char[i],
                      (star_speed[i] == 3) ? WHITE :
                      (star_speed[i] == 2) ? GREY3 : GREY1);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  SHIELDS
 * ═══════════════════════════════════════════════════════════ */

static void draw_shields(void) {
    unsigned char s, x, bx;
    for (s = 0; s < SHIELD_COUNT; ++s) {
        bx = 2 + s * 10;
        for (x = 0; x < SHIELD_W; ++x) {
            draw_char(bx + x, SHIELD_Y,     CHAR_SHIELD, GREEN);
            draw_char(bx + x, SHIELD_Y + 1, CHAR_SHIELD, GREEN);
        }
        /* Notch */
        draw_char(bx + 1, SHIELD_Y + 1, 32, BLACK);
        draw_char(bx + 2, SHIELD_Y + 1, 32, BLACK);
    }
}

static void repair_shields(void) {
    /* Power-up: redraw shields fully */
    draw_shields();
    snd_powerup();
}

/* ═══════════════════════════════════════════════════════════
 *  METEORS
 * ═══════════════════════════════════════════════════════════ */

/* Helper: is this screen code one of our meteor chars? */
static unsigned char is_meteor_char(unsigned char ch) {
    return (ch >= CHAR_METEOR_L1 && ch <= CHAR_SMALL_2);
}

static void erase_meteor(unsigned char idx) {
    unsigned char x = met_x[idx];
    unsigned char y = met_y[idx];
    unsigned char ch;

    if (y >= SCR_H || y < 2 || x >= SCR_W) return;

    if (met_size[idx] == METEOR_LARGE) {
        /* 2 chars wide – only erase if still our chars */
        if (x < SCR_W - 1) {
            ch = read_char(x, y);
            if (is_meteor_char(ch)) draw_char(x, y, 32, BLACK);
            ch = read_char(x + 1, y);
            if (is_meteor_char(ch)) draw_char(x + 1, y, 32, BLACK);
        }
    } else {
        ch = read_char(x, y);
        if (is_meteor_char(ch)) draw_char(x, y, 32, BLACK);
    }
}

/* Can we safely draw a meteor at this cell? (empty, star, or already a meteor) */
static unsigned char cell_free_for_meteor(unsigned char x, unsigned char y) {
    unsigned char ch;
    if (x >= SCR_W || y >= SCR_H) return 0;
    ch = read_char(x, y);
    return (ch == 32 || ch == CHAR_STAR1 || ch == CHAR_STAR2 ||
            is_meteor_char(ch) || ch == CHAR_EXPLODE1 || ch == CHAR_EXPLODE2);
}

static void draw_meteor(unsigned char idx) {
    unsigned char x = met_x[idx];
    unsigned char y = met_y[idx];
    unsigned char color;

    if (y >= SCR_H - 1 || y < 2 || x >= SCR_W) return;

    if (met_size[idx] == METEOR_LARGE) {
        color = (met_hp[idx] > 1) ? ORANGE : LTRED;
        if (x < SCR_W - 1 && cell_free_for_meteor(x, y) && cell_free_for_meteor(x + 1, y)) {
            if (anim_frame & 1) {
                draw_char(x,     y, CHAR_METEOR_L1, color);
                draw_char(x + 1, y, CHAR_METEOR_R1, color);
            } else {
                draw_char(x,     y, CHAR_METEOR_L2, color);
                draw_char(x + 1, y, CHAR_METEOR_R2, color);
            }
        }
    } else {
        color = BROWN;
        if (cell_free_for_meteor(x, y)) {
            draw_char(x, y, (anim_frame & 1) ? CHAR_SMALL_1 : CHAR_SMALL_2, color);
        }
    }
}

static void spawn_meteor(void) {
    unsigned char i;
    unsigned char speed;

    /* Find free slot */
    for (i = 0; i < MAX_METEORS; ++i) {
        if (!met_active[i]) break;
    }
    if (i >= MAX_METEORS) return;

    met_active[i] = 1;
    met_size[i]   = METEOR_LARGE;
    met_hp[i]     = 2;
    met_x[i]      = 2 + (rand() % 34);  /* 2..35, so x+1 stays <=36 (safe for 2-char wide) */
    met_y[i]      = 2;
    met_dx[i]     = (signed char)((rand() % 3) - 1);  /* -1, 0, +1 */

    /* Speed increases with wave */
    speed = 1;
    if (wave >= 3) speed = 1 + (rand() & 1);
    if (wave >= 5) speed = 2;
    if (wave >= 7) speed = 2 + (rand() & 1);
    met_dy[i] = speed;

    ++meteors_alive;
    ++meteors_spawned;
}

/* Split a large meteor into 2 small ones */
static void split_meteor(unsigned char idx) {
    unsigned char i;
    unsigned char ox = met_x[idx];
    unsigned char oy = met_y[idx];
    unsigned char count = 0;

    snd_split();

    for (i = 0; i < MAX_METEORS && count < 2; ++i) {
        if (!met_active[i]) {
            met_active[i] = 1;
            met_size[i]   = METEOR_SMALL;
            met_hp[i]     = 1;
            met_y[i]      = oy;
            met_dy[i]     = met_dy[idx];  /* same fall speed */

            if (count == 0) {
                /* Left child */
                met_x[i]  = (ox > 1) ? ox - 1 : 1;
                met_dx[i] = -1;
            } else {
                /* Right child */
                met_x[i]  = (ox < 37) ? ox + 2 : 37;
                met_dx[i] = 1;
            }
            ++meteors_alive;
            ++count;
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  EXPLOSION TRACKING
 * ═══════════════════════════════════════════════════════════ */

/* Track explosion positions for efficient cleanup */
#define MAX_EXPLOSIONS 8
static unsigned char exp_x[MAX_EXPLOSIONS];
static unsigned char exp_y[MAX_EXPLOSIONS];
static unsigned char exp_timer[MAX_EXPLOSIONS];
static unsigned char exp_count;

static void register_explosion(unsigned char x, unsigned char y) {
    if (exp_count < MAX_EXPLOSIONS) {
        exp_x[exp_count] = x;
        exp_y[exp_count] = y;
        exp_timer[exp_count] = 8;  /* visible for 8 frames */
        ++exp_count;
    }
}

static void update_explosions(void) {
    unsigned char i, ch;
    for (i = 0; i < exp_count; ) {
        if (exp_timer[i] > 0) {
            --exp_timer[i];
            if (exp_timer[i] == 4) {
                /* Switch to second explosion frame */
                ch = read_char(exp_x[i], exp_y[i]);
                if (ch == CHAR_EXPLODE1) {
                    draw_char(exp_x[i], exp_y[i], CHAR_EXPLODE2, LTRED);
                }
            }
            ++i;
        } else {
            /* Time up – erase if still an explosion char */
            ch = read_char(exp_x[i], exp_y[i]);
            if (ch == CHAR_EXPLODE1 || ch == CHAR_EXPLODE2) {
                draw_char(exp_x[i], exp_y[i], 32, BLACK);
            }
            /* Remove from array (swap with last) */
            exp_x[i] = exp_x[exp_count - 1];
            exp_y[i] = exp_y[exp_count - 1];
            exp_timer[i] = exp_timer[exp_count - 1];
            --exp_count;
            /* Don't increment i, re-check swapped entry */
        }
    }
}

/* Show explosion at char position and register for timed cleanup */
static void show_explosion(unsigned char x, unsigned char y) {
    if (x < SCR_W && y < SCR_H && y >= 2) {
        draw_char(x, y, CHAR_EXPLODE1, YELLOW);
        register_explosion(x, y);
    }
}

/* Maybe drop a power-up */
static void maybe_drop_powerup(unsigned char x, unsigned char y) {
    if (pwrup_active) return;
    /* ~15% chance */
    if ((rand() & 7) > 1) return;

    pwrup_active = 1;
    pwrup_type = 1 + (rand() % 3);  /* 1=shield, 2=double, 3=bomb */
    pwrup_x = C2SX(x);
    pwrup_y = C2SY(y);

    /* Set power-up sprite color based on type */
    switch (pwrup_type) {
        case PWRUP_SHIELD: SPR_COL(SPR_PWRUP) = GREEN;  break;
        case PWRUP_DOUBLE: SPR_COL(SPR_PWRUP) = CYAN;   break;
        case PWRUP_BOMB:   SPR_COL(SPR_PWRUP) = RED;    break;
    }
}

static void move_meteors(void) {
    unsigned char i;
    unsigned char ch;
    unsigned char max_x;
    unsigned char met_sx;

    for (i = 0; i < MAX_METEORS; ++i) {
        if (!met_active[i]) continue;

        /* Only move every N frames based on speed */
        if ((frame_count % (4 - met_dy[i])) != 0) continue;

        erase_meteor(i);

        /* Horizontal drift – clamp for large meteors (need x+1 in bounds) */
        max_x = (met_size[i] == METEOR_LARGE) ? 37 : 38;
        if (met_dx[i] < 0 && met_x[i] > 1) {
            --met_x[i];
        } else if (met_dx[i] > 0 && met_x[i] < max_x) {
            ++met_x[i];
        }

        /* Fall */
        ++met_y[i];

        /* Bounce off walls horizontally */
        if (met_x[i] <= 1) met_dx[i] = 1;
        if (met_x[i] >= max_x) met_dx[i] = -1;

        /* Hit shield? Check both chars for large meteors */
        if (met_y[i] < SCR_H) {
            ch = read_char(met_x[i], met_y[i]);
            if (ch == CHAR_SHIELD) {
                draw_char(met_x[i], met_y[i], 32, BLACK);
                show_explosion(met_x[i], met_y[i]);
                snd_explode_small();
                met_active[i] = 0;
                --meteors_alive;
                continue;
            }
            /* Second char of large meteor */
            if (met_size[i] == METEOR_LARGE && met_x[i] < SCR_W - 1) {
                ch = read_char(met_x[i] + 1, met_y[i]);
                if (ch == CHAR_SHIELD) {
                    draw_char(met_x[i] + 1, met_y[i], 32, BLACK);
                    show_explosion(met_x[i] + 1, met_y[i]);
                    snd_explode_small();
                    met_active[i] = 0;
                    --meteors_alive;
                    continue;
                }
            }
        }

        /* Hit player row? */
        if (met_y[i] >= SHIP_Y_CHAR) {
            met_sx = C2SX(met_x[i]);
            /* Check proximity to ship sprite */
            if (met_sx >= ship_x - 8 && met_sx <= ship_x + 12) {
                game_state = GS_DYING;
                erase_meteor(i);
                met_active[i] = 0;
                --meteors_alive;
                return;
            }
        }

        /* Off bottom of play area? */
        if (met_y[i] >= SCR_H - 1) {
            met_active[i] = 0;
            --meteors_alive;
            continue;
        }

        draw_meteor(i);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  BULLET
 * ═══════════════════════════════════════════════════════════ */

static void fire_bullet(void) {
    if (bullet_active) return;
    bullet_active = 1;
    bullet_x = ship_x + 4;
    bullet_y = SHIP_Y_SPR - 10;
    snd_shoot();

    /* If double-shot active, fire second bullet */
    if (double_shot && !bullet2_active) {
        bullet2_active = 1;
        bullet2_x = ship_x + 10;
        bullet2_y = SHIP_Y_SPR - 10;
    }
}

static unsigned char check_bullet_hit(unsigned char bx, unsigned char by) {
    unsigned char cx, cy;
    unsigned char i, ch;
    unsigned char ax;

    /* Convert sprite to char coords */
    cx = (bx - SPR_XOFS) >> 3;
    cy = (by - SPR_YOFS) >> 3;

    /* Hit UFO? */
    if (ufo_active && cy <= 2) {
        if (bx >= ufo_x - 8 && bx <= ufo_x + 16) {
            ufo_active = 0;
            VIC_SPR_ENA &= ~(1 << SPR_UFO);
            score += 100 + (rand() & 0x7F);
            snd_explode_large();

            /* Combo */
            ++combo_count;
            combo_timer = 60;
            snd_combo();
            return 1;
        }
    }

    /* Hit shield? (friendly fire) */
    if (cx < SCR_W && cy < SCR_H) {
        ch = read_char(cx, cy);
        if (ch == CHAR_SHIELD) {
            draw_char(cx, cy, 32, BLACK);
            return 1;
        }
    }

    /* Hit meteor? */
    for (i = 0; i < MAX_METEORS; ++i) {
        if (!met_active[i]) continue;

        ax = met_x[i];
        if (met_size[i] == METEOR_LARGE) {
            if (cy == met_y[i] && (cx == ax || cx == ax + 1)) {
                /* Hit large meteor */
                --met_hp[i];
                if (met_hp[i] == 0) {
                    /* Split into 2 small */
                    erase_meteor(i);
                    met_active[i] = 0;
                    --meteors_alive;
                    split_meteor(i);
                    score += 25 * wave;
                    show_explosion(ax, met_y[i]);
                    snd_explode_large();

                    /* Maybe drop power-up */
                    maybe_drop_powerup(ax, met_y[i]);
                } else {
                    /* Damaged but alive */
                    snd_explode_small();
                    score += 10;
                    draw_meteor(i);  /* redraw with damage color */
                }

                /* Combo system */
                ++combo_count;
                combo_timer = 60;
                if (combo_count >= 3) {
                    score += combo_count * 5;
                    snd_combo();
                }
                return 1;
            }
        } else {
            /* Small meteor */
            if (cy == met_y[i] && cx == ax) {
                erase_meteor(i);
                met_active[i] = 0;
                --meteors_alive;
                score += 10 * wave;
                show_explosion(ax, met_y[i]);
                snd_explode_small();

                ++combo_count;
                combo_timer = 60;
                if (combo_count >= 3) {
                    score += combo_count * 5;
                    snd_combo();
                }
                return 1;
            }
        }
    }

    return 0;
}

static void move_bullet(void) {
    /* Primary bullet */
    if (bullet_active) {
        if (bullet_y <= BULLET_TOP + 4) {
            bullet_active = 0;
            VIC_SPR_ENA &= ~(1 << SPR_BULLET);
        } else {
            bullet_y -= BULLET_SPEED;
            SPR_X(SPR_BULLET) = bullet_x;
            SPR_Y(SPR_BULLET) = bullet_y;
            VIC_SPR_ENA |= (1 << SPR_BULLET);

            if (check_bullet_hit(bullet_x, bullet_y)) {
                bullet_active = 0;
                VIC_SPR_ENA &= ~(1 << SPR_BULLET);
            }
        }
    }

    /* Second bullet (double-shot) – rendered as PETSCII since we're out of sprites */
    if (bullet2_active) {
        unsigned char cx2, cy2;

        if (bullet2_y <= BULLET_TOP + 4) {
            bullet2_active = 0;
        } else {
            /* Erase old pos */
            cx2 = (bullet2_x - SPR_XOFS) >> 3;
            cy2 = (bullet2_y - SPR_YOFS) >> 3;
            if (cx2 < SCR_W && cy2 < SCR_H) {
                unsigned char och = read_char(cx2, cy2);
                if (och == '|' || och == 0x7D) {
                    draw_char(cx2, cy2, 32, BLACK);
                }
            }

            bullet2_y -= BULLET_SPEED;

            /* Draw new pos */
            cx2 = (bullet2_x - SPR_XOFS) >> 3;
            cy2 = (bullet2_y - SPR_YOFS) >> 3;

            if (check_bullet_hit(bullet2_x, bullet2_y)) {
                bullet2_active = 0;
            } else if (cx2 < SCR_W && cy2 < SCR_H) {
                draw_char(cx2, cy2, 0x7D, YELLOW);  /* vertical bar */
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  POWER-UPS
 * ═══════════════════════════════════════════════════════════ */

static void update_powerup(void) {
    if (!pwrup_active) return;

    pwrup_y += 2;

    /* Off screen? */
    if (pwrup_y >= C2SY(23)) {
        pwrup_active = 0;
        VIC_SPR_ENA &= ~(1 << SPR_PWRUP);
        return;
    }

    /* Update sprite */
    SPR_X(SPR_PWRUP) = pwrup_x;
    SPR_Y(SPR_PWRUP) = pwrup_y;
    VIC_SPR_ENA |= (1 << SPR_PWRUP);

    /* Player collision */
    if (pwrup_y >= SHIP_Y_SPR - 8 && pwrup_y <= SHIP_Y_SPR + 8) {
        if (pwrup_x >= ship_x - 8 && pwrup_x <= ship_x + 16) {
            /* Collected! */
            pwrup_active = 0;
            VIC_SPR_ENA &= ~(1 << SPR_PWRUP);

            switch (pwrup_type) {
                case PWRUP_SHIELD:
                    repair_shields();
                    break;
                case PWRUP_DOUBLE:
                    double_shot = 1;
                    double_timer = 600;  /* ~10 seconds */
                    snd_powerup();
                    break;
                case PWRUP_BOMB:
                    /* Clear all meteors! */
                    {
                        unsigned char i;
                        for (i = 0; i < MAX_METEORS; ++i) {
                            if (met_active[i]) {
                                erase_meteor(i);
                                show_explosion(met_x[i], met_y[i]);
                                met_active[i] = 0;
                            }
                        }
                        meteors_alive = 0;
                    }
                    score += 50;
                    snd_bomb();
                    break;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  UFO
 * ═══════════════════════════════════════════════════════════ */

static void update_ufo(void) {
    if (!ufo_active) {
        ++ufo_timer;
        if (ufo_timer > 800) {
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

    ufo_x = (unsigned char)((signed char)ufo_x + ufo_dx);
    SPR_X(SPR_UFO) = ufo_x;
    VIC_SPR_ENA |= (1 << SPR_UFO);
    snd_ufo_tick();

    if (ufo_x <= C2SX(0) || ufo_x >= C2SX(38)) {
        ufo_active = 0;
        VIC_SPR_ENA &= ~(1 << SPR_UFO);
        SID_V3_CTL = 0x00;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  HUD
 * ═══════════════════════════════════════════════════════════ */

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

    /* Ground line */
    {
        unsigned char i;
        for (i = 0; i < 40; ++i)
            draw_char(i, 23, 0xC0, LTBLUE);
    }

    /* Combo display */
    if (combo_count >= 3 && combo_timer > 0) {
        gotoxy(16, 24);
        textcolor(YELLOW);
        cprintf("COMBO x%u!", combo_count);
    } else {
        gotoxy(16, 24);
        cprintf("          ");
    }

    /* Double-shot indicator */
    if (double_shot) {
        gotoxy(0, 24);
        textcolor(CYAN);
        cprintf("DBL");
    } else {
        gotoxy(0, 24);
        cprintf("   ");
    }

    /* Wave progress: meteors spawned / total */
    gotoxy(33, 24);
    textcolor(GREY2);
    cprintf("%02u/%02u", meteors_spawned, meteors_this_wave);
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

/* ═══════════════════════════════════════════════════════════
 *  DEMO AI
 * ═══════════════════════════════════════════════════════════ */

static void demo_ai(void) {
    unsigned char i;
    unsigned char closest_idx = 0xFF;
    unsigned char closest_y = 0;
    unsigned char target_x;

    /* Find the lowest (most dangerous) active meteor */
    for (i = 0; i < MAX_METEORS; ++i) {
        if (!met_active[i]) continue;
        if (met_y[i] > closest_y) {
            closest_y = met_y[i];
            closest_idx = i;
        }
    }

    if (closest_idx == 0xFF) {
        /* No meteors – center the ship */
        if (ship_x < C2SX(19)) ship_x += SHIP_SPEED;
        else if (ship_x > C2SX(21)) ship_x -= SHIP_SPEED;
        return;
    }

    target_x = C2SX(met_x[closest_idx]);

    /* Move toward target */
    if (ship_x < target_x - 4 && ship_x < SHIP_MAX_X)
        ship_x += SHIP_SPEED;
    else if (ship_x > target_x + 4 && ship_x > SHIP_MIN_X)
        ship_x -= SHIP_SPEED;

    /* Shoot when aligned */
    if (ship_x >= target_x - 8 && ship_x <= target_x + 8) {
        if ((frame_count & 3) == 0) fire_bullet();
    }
}

/* ═══════════════════════════════════════════════════════════
 *  WAVE MANAGEMENT
 * ═══════════════════════════════════════════════════════════ */

static void init_wave_state(void) {
    unsigned char i;

    for (i = 0; i < MAX_METEORS; ++i) met_active[i] = 0;
    meteors_alive = 0;
    meteors_spawned = 0;

    /* Meteors per wave: 8, 12, 16, 20, 24, ... */
    meteors_this_wave = 8 + (wave - 1) * 4;
    if (meteors_this_wave > 40) meteors_this_wave = 40;

    spawn_timer = 0;
    bullet_active = 0;
    bullet2_active = 0;
    pwrup_active = 0;
    ufo_active = 0;
    ufo_timer = 0;
    combo_count = 0;
    combo_timer = 0;
    exp_count = 0;

    VIC_SPR_ENA = (1 << SPR_SHIP);
}

/* ═══════════════════════════════════════════════════════════
 *  TITLE SCREEN
 * ═══════════════════════════════════════════════════════════ */

static void draw_title(void) {
    unsigned char i;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    gotoxy(8, 2);
    textcolor(ORANGE);
    cprintf("M E T E O R");

    gotoxy(10, 4);
    textcolor(RED);
    cprintf("S T O R M");

    /* Decorative meteor chars */
    for (i = 0; i < 8; ++i) {
        draw_char(5 + i * 4, 7, CHAR_METEOR_L1, ORANGE);
        draw_char(6 + i * 4, 7, CHAR_METEOR_R1, ORANGE);
    }

    /* Small meteors */
    for (i = 0; i < 10; ++i) {
        draw_char(3 + i * 4, 9, CHAR_SMALL_1, BROWN);
    }

    /* Score table */
    gotoxy(6, 11);
    textcolor(WHITE);
    cprintf("LARGE METEOR = 25 PTS");
    draw_char(3, 11, CHAR_METEOR_L1, ORANGE);
    draw_char(4, 11, CHAR_METEOR_R1, ORANGE);

    gotoxy(6, 12);
    textcolor(BROWN);
    cprintf("SMALL METEOR = 10 PTS");
    draw_char(4, 12, CHAR_SMALL_1, BROWN);

    gotoxy(6, 13);
    textcolor(RED);
    cprintf("UFO = 100+ MYSTERY");

    /* Power-ups */
    gotoxy(4, 15);
    textcolor(GREEN);
    cprintf("POWER-UPS:");

    gotoxy(5, 16);
    textcolor(GREEN);
    draw_char(4, 16, CHAR_PWRUP_S, GREEN);
    cprintf(" SHIELD REPAIR");

    gotoxy(5, 17);
    textcolor(CYAN);
    draw_char(4, 17, CHAR_PWRUP_D, CYAN);
    cprintf(" DOUBLE SHOT");

    gotoxy(5, 18);
    textcolor(RED);
    draw_char(4, 18, CHAR_PWRUP_B, RED);
    cprintf(" SCREEN BOMB");

    gotoxy(7, 20);
    textcolor(YELLOW);
    cprintf("3+ HITS = COMBO BONUS!");

    gotoxy(7, 22);
    textcolor(CYAN);
    cprintf("PRESS FIRE TO START");

    gotoxy(8, 23);
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
    unsigned char spawn_interval;

    while (1) {
        waitvsync();
        ++frame_count;
        snd_tick();

        /* Animation frame toggle every 8 frames */
        if ((frame_count & 7) == 0) ++anim_frame;

        /* ── PLAY ──────────────────────────────────── */
        if (game_state == GS_PLAY) {

            /* Combo timer countdown */
            if (combo_timer > 0) {
                --combo_timer;
                if (combo_timer == 0) combo_count = 0;
            }

            /* Double-shot timer */
            if (double_shot) {
                if (double_timer > 0) {
                    --double_timer;
                } else {
                    double_shot = 0;
                }
            }

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

            /* Spawn meteors */
            spawn_interval = 40;
            if (wave >= 2) spawn_interval = 30;
            if (wave >= 4) spawn_interval = 22;
            if (wave >= 6) spawn_interval = 16;
            if (wave >= 8) spawn_interval = 12;

            ++spawn_timer;
            if (spawn_timer >= spawn_interval &&
                meteors_spawned < meteors_this_wave) {
                spawn_meteor();
                spawn_timer = 0;
            }

            /* Move meteors */
            move_meteors();

            /* Move bullet */
            move_bullet();

            /* Power-ups */
            update_powerup();

            /* UFO */
            update_ufo();

            /* Starfield */
            update_stars();

            /* Update explosion animations */
            update_explosions();

            /* HUD */
            draw_hud();

            /* Wave complete? All spawned and all destroyed */
            if (meteors_spawned >= meteors_this_wave &&
                meteors_alive == 0) {
                game_state = GS_WON;
            }

            /* Demo exit */
            if (demo_mode && joy_fire()) return;

            continue;
        }

        /* ── DYING ─────────────────────────────────── */
        if (game_state == GS_DYING) {
            snd_death();
            --lives;
            if (lives == 0) {
                game_state = GS_LOST;
            } else {
                /* Reset ship position */
                ship_x = C2SX(19);
                SPR_X(SPR_SHIP) = ship_x;
                bullet_active = 0;
                bullet2_active = 0;
                VIC_SPR_ENA &= ~(1 << SPR_BULLET);
                game_state = GS_PLAY;
            }
            continue;
        }

        /* ── WON ───────────────────────────────────── */
        if (game_state == GS_WON) {
            snd_off();

            gotoxy(12, 11);
            textcolor(YELLOW);
            cprintf("WAVE %u CLEAR!", wave);

            /* Bonus points */
            {
                unsigned int wave_bonus = wave * 100;
                score += wave_bonus;
                gotoxy(10, 13);
                textcolor(WHITE);
                cprintf("BONUS: %u PTS", wave_bonus);
            }

            {
                unsigned char w;
                for (w = 0; w < 180; ++w) waitvsync();
            }

            if (demo_mode) return;

            ++wave;

            /* Refresh screen */
            clrscr();
            bgcolor(BLACK);
            bordercolor(BLACK);
            init_stars();
            draw_shields();
            init_wave_state();
            game_state = GS_PLAY;
            continue;
        }

        /* ── LOST ──────────────────────────────────── */
        if (game_state == GS_LOST) {
            snd_off();
            gotoxy(13, 11);
            textcolor(RED);
            cprintf("GAME  OVER");
            gotoxy(11, 13);
            textcolor(WHITE);
            cprintf("FINAL SCORE: %05u", score);

            gotoxy(11, 15);
            textcolor(GREY3);
            cprintf("WAVES CLEARED: %u", wave - 1);

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

    /* Game setup */
    score = 0;
    lives = 3;
    wave  = 1;
    frame_count = 0;
    anim_frame = 0;
    snd_timer1 = 0;
    snd_timer2 = 0;
    snd_timer3 = 0;
    double_shot = 0;
    double_timer = 0;

    demo_mode = joy_fire() ? 0 : 1;

    /* Initial screen */
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    init_stars();
    draw_shields();
    init_wave_state();

    ship_x = C2SX(19);
    SPR_X(SPR_SHIP) = ship_x;
    SPR_Y(SPR_SHIP) = SHIP_Y_SPR;
    VIC_SPR_ENA = (1 << SPR_SHIP);

    game_state = GS_PLAY;

    game_loop();

    goto title_screen;

    return 0;
}

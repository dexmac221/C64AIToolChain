/*
 * FULGOR - an 8-way scrolling action game for the Commodore 64
 *
 * The frame is assembly (engine.s, game.s, sound.s). C keeps what
 * happens once or seldom: start-up, the title, the HUD's text, the
 * steps between a life and the next.
 *
 * Agent input, as in the rest of the toolchain:
 *   $033C  one press (cleared when read)     $033E  held
 *   bits: up 1, down 2, left 4, right 8, fire 16
 *   $0341  n: drop the hero at spot n (1..10), for tests and filming
 * Telemetry at $0350: frames, late frames, worst commit line, stalls,
 * flips (see defs.inc).
 *
 * AI Toolchain Project 2026
 */

#include <string.h>
#include "assets.h"

#define SCREEN_A ((unsigned char*)0x4400)
#define SCREEN_B ((unsigned char*)0x4800)
#define SCREEN_C ((unsigned char*)0x4C00)
#define COLRAM   ((unsigned char*)0xD800)
#define CHARSET  ((unsigned char*)0x5000)
#define PEEK(a)  (*(unsigned char*)(a))
#define POKE(a, v) (*(unsigned char*)(a) = (v))

/* HUD glyphs, put over three ROM characters nobody types */
#define CH_BAR   27
#define CH_GEM   28
#define CH_HEART 29

#define D_SCORE  1
#define D_ENERGY 2
#define D_LIVES  4
#define D_GEMS   8
#define D_POWER  16

void sys_init(void);
void video_on(void);
void snd_init(void);
void __fastcall__ snd_tune(unsigned char on);
void game_reset(void);
void game_frame(void);
void respawn(void);
void title_init(void);
unsigned char title_frame(void);
void idle_frame(void);
void __fastcall__ teleport(unsigned char spot);

extern unsigned char g_event, g_lives, g_energy, g_gems, g_power;
extern unsigned char g_dirty, g_spot;
extern unsigned char g_score[6];

static const unsigned char glyphs[24] = {
    0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00,
    0x18, 0x3C, 0x7E, 0xFF, 0x7E, 0x3C, 0x18, 0x00,
    0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00
};

/* One cell of the HUD, in all three screens. */
static void __fastcall__ put(unsigned char o, unsigned char c) {
    SCREEN_A[o] = c;
    SCREEN_B[o] = c;
    SCREEN_C[o] = c;
}

/* Text into the screens. The font is the ROM's upper-case set, where
   A..Z are screen codes 1..26; cc65 hands us PETSCII. */
static void put_text(unsigned char o, const char *s) {
    unsigned char c;
    while ((c = (unsigned char)*s++) != 0) {
        if (c >= 0xC1 && c <= 0xDA) c -= 0xC0;
        else if (c >= 0x41 && c <= 0x5A) c -= 0x40;
        put(o, c);
        ++o;
    }
}

static void hud_clear(void) {
    memset(SCREEN_A, 32, 120);
    memset(SCREEN_B, 32, 120);
    memset(SCREEN_C, 32, 120);
    memset(COLRAM, 1, 120);
}

static void hud_frame(void) {
    hud_clear();
    put_text(1, "SCORE");
    memset(COLRAM + 7, 7, 6);
    put(15, CH_GEM);
    COLRAM[15] = 3;
    put(20, CH_HEART);
    COLRAM[20] = 2;
    put_text(24, "POWER");
    COLRAM[30] = 7;
    put_text(41, "ENERGY");
    memset(COLRAM + 48, 5, 8);
    g_dirty = 0xFF;
}

static void hud_update(void) {
    unsigned char i, d = g_dirty;
    g_dirty = 0;
    if (d & D_SCORE)
        for (i = 0; i < 6; ++i) put(7 + i, 48 + g_score[i]);
    if (d & D_GEMS) {
        i = g_gems;
        while (i >= 100) i -= 100;
        put(16, 48 + i / 10);
        put(17, 48 + i % 10);
    }
    if (d & D_LIVES) put(21, 48 + g_lives);
    if (d & D_POWER) put(30, 49 + g_power);
    if (d & D_ENERGY)
        for (i = 0; i < 8; ++i) put(48 + i, i < g_energy ? CH_BAR : 32);
}

/* Frames of the running game with a line of text in the HUD. */
static void pause_with(const char *line0, const char *line1, unsigned char n) {
    hud_clear();
    memset(COLRAM, 7, 40);
    put_text(1, line0);
    put_text(41, line1);
    while (n--) {
        idle_frame();
        idle_frame();
    }
}

void main(void) {
    sys_init();
    memcpy(CHARSET + CH_BAR * 8, glyphs, 24);
    snd_init();
    hud_clear();
    teleport(0);
    video_on();

    for (;;) {
        /* ---- title */
        respawn();
        teleport(0);
        title_init();
        hud_clear();
        memset(COLRAM, 7, 40);
        put_text(1, "FULGOR    A DEMO IN EIGHT DIRECTIONS");
        put_text(41, "PRESS FIRE     JOYSTICK IN PORT 2");
        snd_tune(1);
        while (title_frame()) ;             /* let go of fire first */
        while (!title_frame()) ;

        /* ---- the game */
        game_reset();
        hud_frame();
        teleport(0);
        for (;;) {
            game_frame();
            if (g_dirty) hud_update();
            if (PEEK(0x0341)) {              /* teleport hook */
                unsigned char n = PEEK(0x0341) - 1;
                POKE(0x0341, 0);
                if (n < NUM_SPOTS) teleport(n);
            }
            if (g_event == 3) {              /* a life lost: back to the */
                respawn();                   /* last checkpoint touched */
                teleport(g_spot);
            } else if (g_event) {
                break;
            }
        }
        hud_update();
        if (g_event == 2)
            pause_with("STAGE CLEAR", "THE WAY ON IS OPEN   WELL DONE", 120);
        else
            pause_with("GAME OVER", "", 90);
    }
}

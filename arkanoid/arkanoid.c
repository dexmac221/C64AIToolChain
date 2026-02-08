/*
 * Arkanoid for C64 - Written in C using cc65
 * Features: Sprites (paddle+ball), SID sound, brick collision,
 *           power-ups, multiple levels, demo AI mode.
 *
 * AI Toolchain Project 2026
 */

#include <c64.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <joystick.h>

/* ── Screen Dimensions ────────────────────────────────── */
#define SCREEN_WIDTH   40
#define SCREEN_HEIGHT  25

/* ── Play-field (character coordinates) ───────────────── */
#define FIELD_LEFT      1   /* left wall column */
#define FIELD_RIGHT    27   /* right wall column (keeps sprite X<255) */
#define FIELD_TOP       2   /* top wall row */
#define FIELD_BOTTOM   24   /* bottom (death) row */
#define FIELD_W        (FIELD_RIGHT - FIELD_LEFT - 1)  /* 25 usable cols */
#define FIELD_H        (FIELD_BOTTOM - FIELD_TOP - 1)  /* 21 usable rows */

/* ── Brick layout ─────────────────────────────────────── */
#define BRICK_CHAR_W    3   /* each brick = 3 chars wide */
#define BRICK_COLS      8   /* 8 bricks per row (8*3=24 < 25) */
#define BRICK_ROWS      6   /* 6 rows of bricks */
#define BRICK_START_X  (FIELD_LEFT + 1 + 1)  /* centre bricks: 1 char padding */
#define BRICK_START_Y  (FIELD_TOP + 1)
#define MAX_BRICKS     (BRICK_COLS * BRICK_ROWS)  /* 48 */

/* ── Sprite settings ──────────────────────────────────── */
#define SPRITE_PTRS     ((unsigned char*)0x07F8)

/* Sprite data at $3000 (block 192) – safe for cc65 */
#define SPRITE_DATA     0x3000
#define SPR_BLOCK_PADDLE  192   /* $3000 */
#define SPR_BLOCK_BALL    193   /* $3040 */

/* ── VIC-II registers ─────────────────────────────────── */
#define VIC_SPR_ENA     (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X    (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR  (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X   (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y   (*(unsigned char*)0xD017)

/* Sprite 0 = paddle, Sprite 1 = ball */
#define SPR0_X  (*(unsigned char*)0xD000)
#define SPR0_Y  (*(unsigned char*)0xD001)
#define SPR1_X  (*(unsigned char*)0xD002)
#define SPR1_Y  (*(unsigned char*)0xD003)

#define SPR0_COL (*(unsigned char*)0xD027)
#define SPR1_COL (*(unsigned char*)0xD028)

/* Sprite-background collision register */
#define VIC_SPR_BG_COLL  (*(unsigned char*)0xD01F)
/* Sprite-sprite  collision register */
#define VIC_SPR_SPR_COLL (*(unsigned char*)0xD01E)

/* Coordinate helpers:  screen char → sprite pixel */
#define SPRITE_X_OFS  24   /* hardware X offset */
#define SPRITE_Y_OFS  50   /* hardware Y offset */
#define CHAR2SPR_X(c) (SPRITE_X_OFS + (c) * 8)
#define CHAR2SPR_Y(r) (SPRITE_Y_OFS + (r) * 8)

/* ── SID registers ────────────────────────────────────── */
#define SID_V1_FREQ_LO  (*(unsigned char*)0xD400)
#define SID_V1_FREQ_HI  (*(unsigned char*)0xD401)
#define SID_V1_PW_LO    (*(unsigned char*)0xD402)
#define SID_V1_PW_HI    (*(unsigned char*)0xD403)
#define SID_V1_CTRL     (*(unsigned char*)0xD404)
#define SID_V1_AD       (*(unsigned char*)0xD405)
#define SID_V1_SR       (*(unsigned char*)0xD406)

#define SID_V2_FREQ_LO  (*(unsigned char*)0xD407)
#define SID_V2_FREQ_HI  (*(unsigned char*)0xD408)
#define SID_V2_CTRL     (*(unsigned char*)0xD40B)
#define SID_V2_AD       (*(unsigned char*)0xD40C)
#define SID_V2_SR       (*(unsigned char*)0xD40D)

#define SID_VOLUME       (*(unsigned char*)0xD418)

/* ── Colors ───────────────────────────────────────────── */
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

/* ── Character codes (screen codes) ───────────────────── */
#define CHAR_WALL   160   /* reversed space – solid block */
#define CHAR_BRICK  160   /* same reversed space for bricks */
#define CHAR_SPACE   32

/* ── Game states ──────────────────────────────────────── */
#define STATE_TITLE     0
#define STATE_PLAY      1
#define STATE_LAUNCH    2   /* ball stuck to paddle, waiting */
#define STATE_DYING     3
#define STATE_WON       4
#define STATE_LOST      5
#define STATE_NEXT_LVL  6

/* ── Directions ───────────────────────────────────────── */
#define DIR_NONE  0
#define DIR_LEFT  1
#define DIR_RIGHT 2

/* ── Fixed-point ball position (8.8) ──────────────────── */
/* We use 16-bit values: hi byte = pixel, lo byte = fraction */
static unsigned int ball_x;  /* 8.8 fixed point */
static unsigned int ball_y;
static int ball_dx;           /* signed 8.8 velocity */
static int ball_dy;

/* ── Paddle ───────────────────────────────────────────── */
#define PADDLE_Y_CHAR   22            /* paddle row in char coords */
#define PADDLE_Y_SPR    CHAR2SPR_Y(PADDLE_Y_CHAR)
#define PADDLE_WIDTH     5            /* paddle width in chars (40px) */
#define PADDLE_SPEED     3            /* pixels per frame */

static unsigned char paddle_x;        /* sprite X of paddle left edge */

/* ── Ball limits (sprite coords) ──────────────────────── */
#define BALL_MIN_X  CHAR2SPR_X(FIELD_LEFT + 1)    /* 40  */
#define BALL_MAX_X  CHAR2SPR_X(FIELD_RIGHT - 1)   /* 232 */
#define BALL_MIN_Y  CHAR2SPR_Y(FIELD_TOP + 1)      /* 74  */
#define BALL_MAX_Y  CHAR2SPR_Y(FIELD_BOTTOM)        /* 242 */

/* ── Game variables ───────────────────────────────────── */
static unsigned int  score;
static unsigned char lives;
static unsigned char level;
static unsigned char bricks_left;
static unsigned char game_state;
static unsigned char demo_mode;
static unsigned char frame_count;
static unsigned char sound_timer;
static unsigned char bricks[BRICK_ROWS][BRICK_COLS]; /* 0=gone, 1+=hp */

/* Brick row colors per level (cycling palette) */
static const unsigned char brick_colors[5] = {
    RED, ORANGE, YELLOW, GREEN, CYAN
};

/* ── Brick colors for multi-hit bricks ────────────────── */
static const unsigned char hp_colors[4] = {
    GREY2,    /* 1 hp  – about to break */
    LTBLUE,   /* 2 hp */
    CYAN,     /* 3 hp */
    WHITE     /* 4 hp – silver/unbreakable look */
};

/* ================================================================
 *  SPRITE DATA – Paddle (24 px wide, 6 rows high) & Ball (6×6)
 * ================================================================ */

static void init_sprite_data(void) {
    unsigned char *dest;
    unsigned char i;

    /* ---- Paddle: 24 pixels wide × 6 rows (top of sprite) ---- */
    dest = (unsigned char*)SPRITE_DATA;             /* block 192 */
    for (i = 0; i < 63; ++i) dest[i] = 0;

    /* Row 0: top edge – rounded corners */
    dest[0]  = 0x1F; dest[1]  = 0xFF; dest[2]  = 0xF8;
    /* Row 1: slightly wider */
    dest[3]  = 0x7F; dest[4]  = 0xFF; dest[5]  = 0xFE;
    /* Row 2-3: full solid body */
    dest[6]  = 0xFF; dest[7]  = 0xFF; dest[8]  = 0xFF;
    dest[9]  = 0xFF; dest[10] = 0xFF; dest[11] = 0xFF;
    /* Row 4: slightly narrower */
    dest[12] = 0x7F; dest[13] = 0xFF; dest[14] = 0xFE;
    /* Row 5: bottom edge – rounded */
    dest[15] = 0x1F; dest[16] = 0xFF; dest[17] = 0xF8;

    /* ---- Ball: 6×6 dot (rounder shape) ----------------------- */
    dest = (unsigned char*)(SPRITE_DATA + 64);      /* block 193 */
    for (i = 0; i < 63; ++i) dest[i] = 0;

    dest[0]  = 0x70; /* .XXX.... */
    dest[3]  = 0xF8; /* XXXXX... */
    dest[6]  = 0xF8; /* XXXXX... */
    dest[9]  = 0xF8; /* XXXXX... */
    dest[12] = 0x70; /* .XXX.... */
}

/* ================================================================
 *  SOUND
 * ================================================================ */

static void init_sound(void) {
    SID_VOLUME = 15;
    SID_V1_AD  = 0x00;   /* instant attack, no decay */
    SID_V1_SR  = 0xA0;   /* sustain medium, fast release */
    SID_V2_AD  = 0x09;   /* short attack+decay for bounce */
    SID_V2_SR  = 0x00;
}

/* Short high-pitched "tink" for wall bounce */
static void sound_bounce(void) {
    SID_V1_FREQ_LO = 0x00;
    SID_V1_FREQ_HI = 0x30;
    SID_V1_CTRL    = 0x81; /* noise, gate on */
    sound_timer = 3;
}

/* Medium "thud" for paddle hit */
static void sound_paddle(void) {
    SID_V1_FREQ_LO = 0x00;
    SID_V1_FREQ_HI = 0x18;
    SID_V1_CTRL    = 0x21; /* sawtooth, gate on */
    sound_timer = 4;
}

/* Satisfying "pop" for brick break */
static void sound_brick(void) {
    SID_V2_FREQ_LO = 0x00;
    SID_V2_FREQ_HI = 0x28;
    SID_V2_CTRL    = 0x11; /* triangle, gate on */
    sound_timer = 5;
}

/* Descending death tone */
static void sound_die(void) {
    unsigned char i;
    for (i = 0x30; i > 0x05; i -= 2) {
        SID_V1_FREQ_HI = i;
        SID_V1_CTRL = 0x21;
        waitvsync();
    }
    SID_V1_CTRL = 0x20;
}

/* Ascending victory jingle */
static void sound_win(void) {
    unsigned char i;
    for (i = 0x10; i < 0x40; i += 3) {
        SID_V1_FREQ_HI = i;
        SID_V1_CTRL = 0x11;
        waitvsync();
    }
    SID_V1_CTRL = 0x10;
}

static void sound_off(void) {
    SID_V1_CTRL = 0x00;
    SID_V2_CTRL = 0x00;
}

/* ================================================================
 *  DRAWING
 * ================================================================ */

/* Draw the play-field border */
static void draw_border(void) {
    unsigned char *scr = (unsigned char*)0x0400;
    unsigned char *col = (unsigned char*)0xD800;
    unsigned char x, y;
    unsigned int pos;

    /* Top wall */
    for (x = FIELD_LEFT; x <= FIELD_RIGHT; ++x) {
        pos = (unsigned int)FIELD_TOP * 40 + x;
        scr[pos] = CHAR_WALL;
        col[pos] = GREY2;
    }
    /* Bottom wall (visual only – ball dies here) */
    /*  We skip bottom wall to keep the opening visible */

    /* Side walls */
    for (y = FIELD_TOP; y <= FIELD_BOTTOM; ++y) {
        pos = (unsigned int)y * 40 + FIELD_LEFT;
        scr[pos] = CHAR_WALL;
        col[pos] = GREY2;
        pos = (unsigned int)y * 40 + FIELD_RIGHT;
        scr[pos] = CHAR_WALL;
        col[pos] = GREY2;
    }
}

/* Draw bricks from the array */
static void draw_bricks(void) {
    unsigned char *scr = (unsigned char*)0x0400;
    unsigned char *col = (unsigned char*)0xD800;
    unsigned char r, c, k;
    unsigned int pos;
    unsigned char color;

    bricks_left = 0;

    for (r = 0; r < BRICK_ROWS; ++r) {
        for (c = 0; c < BRICK_COLS; ++c) {
            pos = (unsigned int)(BRICK_START_Y + r) * 40
                + (BRICK_START_X + c * BRICK_CHAR_W);

            if (bricks[r][c] > 0) {
                ++bricks_left;
                /* Color based on hit-points (multi-hit) or row color */
                if (bricks[r][c] > 1) {
                    color = hp_colors[bricks[r][c] - 1];
                } else {
                    color = brick_colors[r % 5];
                }
                for (k = 0; k < BRICK_CHAR_W; ++k) {
                    scr[pos + k] = CHAR_BRICK;
                    col[pos + k] = color;
                }
            } else {
                for (k = 0; k < BRICK_CHAR_W; ++k) {
                    scr[pos + k] = CHAR_SPACE;
                    col[pos + k] = BLACK;
                }
            }
        }
    }
}

/* Erase a single brick visually */
static void erase_brick(unsigned char r, unsigned char c) {
    unsigned char *scr = (unsigned char*)0x0400;
    unsigned char *col = (unsigned char*)0xD800;
    unsigned int pos;
    unsigned char k;

    pos = (unsigned int)(BRICK_START_Y + r) * 40
        + (BRICK_START_X + c * BRICK_CHAR_W);
    for (k = 0; k < BRICK_CHAR_W; ++k) {
        scr[pos + k] = CHAR_SPACE;
        col[pos + k] = BLACK;
    }
}

/* Redraw a single brick (when damaged but not destroyed) */
static void redraw_brick(unsigned char r, unsigned char c) {
    unsigned char *scr = (unsigned char*)0x0400;
    unsigned char *col = (unsigned char*)0xD800;
    unsigned int pos;
    unsigned char k, color;

    pos = (unsigned int)(BRICK_START_Y + r) * 40
        + (BRICK_START_X + c * BRICK_CHAR_W);

    if (bricks[r][c] > 1) {
        color = hp_colors[bricks[r][c] - 1];
    } else {
        color = brick_colors[r % 5];
    }
    for (k = 0; k < BRICK_CHAR_W; ++k) {
        scr[pos + k] = CHAR_BRICK;
        col[pos + k] = color;
    }
}

static void draw_hud(void) {
    gotoxy(0, 0);
    textcolor(WHITE);
    cprintf("SCORE:%05u", score);

    if (demo_mode) {
        gotoxy(16, 0);
        textcolor(GREEN);
        cprintf("DEMO");
    } else {
        gotoxy(15, 0);
        textcolor(LTBLUE);
        cprintf("LVL:%u", level);
    }

    gotoxy(33, 0);
    textcolor(YELLOW);
    cprintf("LIFE:%u", lives);

    /* Right panel info */
    gotoxy(29, 5);
    textcolor(GREY2);
    cprintf("ARKANOID");
    gotoxy(29, 7);
    textcolor(WHITE);
    cprintf("SCORE");
    gotoxy(29, 8);
    textcolor(YELLOW);
    cprintf("%05u", score);
    gotoxy(29, 10);
    textcolor(WHITE);
    cprintf("LEVEL");
    gotoxy(29, 11);
    textcolor(CYAN);
    cprintf("  %u", level);
    gotoxy(29, 13);
    textcolor(WHITE);
    cprintf("LIVES");
    gotoxy(29, 14);
    textcolor(LTRED);
    cprintf("  %u", lives);
    gotoxy(29, 16);
    textcolor(WHITE);
    cprintf("BRICKS");
    gotoxy(29, 17);
    textcolor(GREEN);
    cprintf("  %u ", bricks_left);
}

/* ================================================================
 *  LEVEL GENERATION
 * ================================================================ */

static void generate_level(void) {
    unsigned char r, c;

    for (r = 0; r < BRICK_ROWS; ++r) {
        for (c = 0; c < BRICK_COLS; ++c) {
            if (level == 1) {
                /* Level 1: all single-hit */
                bricks[r][c] = 1;
            } else if (level == 2) {
                /* Level 2: top row 2-hit */
                bricks[r][c] = (r == 0) ? 2 : 1;
            } else if (level == 3) {
                /* Level 3: checkerboard 2-hit */
                bricks[r][c] = ((r + c) & 1) ? 2 : 1;
            } else if (level == 4) {
                /* Level 4: gradient – top rows harder */
                bricks[r][c] = (unsigned char)(BRICK_ROWS - r);
                if (bricks[r][c] > 3) bricks[r][c] = 3;
            } else {
                /* Level 5+: all 2-hit with random 3-hit */
                bricks[r][c] = ((rand() & 3) == 0) ? 3 : 2;
            }
        }
    }
}

/* ================================================================
 *  SPRITE POSITIONING
 * ================================================================ */

static void init_sprites(void) {
    init_sprite_data();

    SPRITE_PTRS[0] = SPR_BLOCK_PADDLE;
    SPRITE_PTRS[1] = SPR_BLOCK_BALL;

    VIC_SPR_ENA    = 0x03;   /* enable sprite 0 & 1 */
    VIC_SPR_DBL_X  = 0x01;   /* double-width paddle (sprite 0) */
    VIC_SPR_DBL_Y  = 0x00;
    VIC_SPR_MCOLOR = 0x00;   /* hi-res sprites */
    VIC_SPR_HI_X   = 0x00;

    SPR0_COL = LTBLUE;       /* paddle */
    SPR1_COL = WHITE;        /* ball */
}

static void update_sprites(void) {
    SPR0_X = paddle_x;
    SPR0_Y = PADDLE_Y_SPR;
    SPR1_X = (unsigned char)(ball_x >> 8);
    SPR1_Y = (unsigned char)(ball_y >> 8);
}

/* ================================================================
 *  PADDLE MOVEMENT
 * ================================================================ */

/* Pixel limits for paddle sprite X (double-width = 48 real px) */
#define PADDLE_MIN_X  CHAR2SPR_X(FIELD_LEFT + 1)
#define PADDLE_MAX_X  (CHAR2SPR_X(FIELD_RIGHT) - 48)  /* paddle is 48px wide (double-width) */

static void move_paddle(unsigned char dir) {
    if (dir == DIR_LEFT) {
        if (paddle_x > PADDLE_MIN_X + PADDLE_SPEED)
            paddle_x -= PADDLE_SPEED;
        else
            paddle_x = PADDLE_MIN_X;
    } else if (dir == DIR_RIGHT) {
        if (paddle_x + PADDLE_SPEED < PADDLE_MAX_X)
            paddle_x += PADDLE_SPEED;
        else
            paddle_x = PADDLE_MAX_X;
    }
}

/* ================================================================
 *  BALL PHYSICS
 * ================================================================ */

static void launch_ball(void) {
    /* Place ball just above paddle, slight right bias */
    ball_x = ((unsigned int)paddle_x + 12) << 8;
    ball_y = ((unsigned int)(PADDLE_Y_SPR - 8)) << 8;
    ball_dx =  0x0140;   /* ~1.25 px/frame right */
    ball_dy = -0x0180;   /* ~1.5  px/frame up */
}

/* Convert ball sprite pos → char column & row for collision */
static void ball_to_char(unsigned char *cx, unsigned char *cy) {
    unsigned char bx = (unsigned char)(ball_x >> 8);
    unsigned char by = (unsigned char)(ball_y >> 8);
    /* add 2 for ball centre (4px ball) */
    *cx = (bx - SPRITE_X_OFS + 2) >> 3;
    *cy = (by - SPRITE_Y_OFS + 2) >> 3;
}

/* Check if a character cell is a brick, return 1 if hit */
static unsigned char check_brick_at(unsigned char cx, unsigned char cy) {
    unsigned char br, bc;

    /* Is it within the brick area? */
    if (cy < BRICK_START_Y || cy >= BRICK_START_Y + BRICK_ROWS) return 0;
    if (cx < BRICK_START_X || cx >= BRICK_START_X + BRICK_COLS * BRICK_CHAR_W) return 0;

    br = cy - BRICK_START_Y;
    bc = (cx - BRICK_START_X) / BRICK_CHAR_W;

    if (bc >= BRICK_COLS) return 0;
    if (bricks[br][bc] == 0) return 0;

    /* Hit! */
    --bricks[br][bc];
    if (bricks[br][bc] == 0) {
        erase_brick(br, bc);
        --bricks_left;
        score += 10 * level;
        sound_brick();
    } else {
        /* brick damaged but alive – change its color */
        redraw_brick(br, bc);
        score += 5;
        sound_bounce();
    }
    return 1;
}

static void move_ball(void) {
    unsigned char bx, by;
    unsigned char cx, cy;
    unsigned char hit;
    int next_x, next_y;
    unsigned char paddle_left, paddle_right;
    unsigned char ball_centre;
    signed char offset;

    /* Advance position */
    next_x = (int)ball_x + ball_dx;
    next_y = (int)ball_y + ball_dy;

    /* ---- Wall collisions (X axis) ---- */
    bx = (unsigned char)(next_x >> 8);
    if (bx <= BALL_MIN_X) {
        ball_dx = -ball_dx;
        next_x  = (int)((unsigned int)BALL_MIN_X << 8) + 0x0100;
        sound_bounce();
    } else if (bx >= BALL_MAX_X) {
        ball_dx = -ball_dx;
        next_x  = (int)((unsigned int)(BALL_MAX_X - 1) << 8);
        sound_bounce();
    }

    /* ---- Wall collision (top) ---- */
    by = (unsigned char)(next_y >> 8);
    if (by <= BALL_MIN_Y) {
        ball_dy = -ball_dy;
        next_y  = (int)((unsigned int)BALL_MIN_Y << 8) + 0x0100;
        sound_bounce();
    }

    /* ---- Death (bottom) ---- */
    if (by >= BALL_MAX_Y) {
        game_state = STATE_DYING;
        return;
    }

    /* Commit position */
    ball_x = (unsigned int)next_x;
    ball_y = (unsigned int)next_y;

    /* ---- Paddle collision ---- */
    by = (unsigned char)(ball_y >> 8);
    bx = (unsigned char)(ball_x >> 8);

    if (ball_dy > 0) {  /* ball moving down */
        if (by >= PADDLE_Y_SPR - 4 && by <= PADDLE_Y_SPR + 2) {
            paddle_left  = paddle_x;
            paddle_right = paddle_x + 48; /* double-width sprite */
            if (bx >= paddle_left && bx <= paddle_right) {
                /* Reflect Y */
                ball_dy = -ball_dy;
                ball_y  = ((unsigned int)(PADDLE_Y_SPR - 5)) << 8;

                /* Angle based on hit position */
                ball_centre = (unsigned char)(bx - paddle_left);  /* 0..47 */
                offset = (signed char)ball_centre - 24; /* -24..+23 */

                /* Horizontal speed influenced by hit position */
                ball_dx = (int)offset * 6;  /* wider angle at edges */

                /* Clamp minimum X speed so ball doesn't go straight up */
                if (ball_dx > -0x0060 && ball_dx < 0x0060) {
                    ball_dx = (ball_dx >= 0) ? 0x0060 : -0x0060;
                }
                /* Clamp max */
                if (ball_dx >  0x0250) ball_dx =  0x0250;
                if (ball_dx < -0x0250) ball_dx = -0x0250;

                /* Ensure ball always moves up fast enough */
                if (ball_dy > -0x0100) ball_dy = -0x0100;

                /* Slight speed-up per level */
                if (ball_dy > -0x0280) {
                    ball_dy -= 0x0008;  /* get slightly faster */
                }

                sound_paddle();
            }
        }
    }

    /* ---- Brick collision ---- */
    /* Check the 4 corners of the ball (approx 4×4 px) */
    hit = 0;

    /* Ball centre for brick test */
    ball_to_char(&cx, &cy);

    /* Test cell the ball centre is in */
    if (check_brick_at(cx, cy)) hit = 1;

    /* Also test one cell above/below based on ball direction */
    if (ball_dy < 0) {
        /* moving up – check cell above */
        if (cy > 0 && check_brick_at(cx, cy - 1)) hit = 1;
    } else {
        /* moving down – check cell below */
        if (check_brick_at(cx, cy + 1)) hit = 1;
    }

    /* If we hit a brick, reverse Y */
    if (hit) {
        ball_dy = -ball_dy;
    }

    /* Win condition */
    if (bricks_left == 0) {
        game_state = STATE_WON;
    }
}

/* ================================================================
 *  DEMO AI – simple paddle-tracks-ball
 * ================================================================ */

static void demo_ai(void) {
    unsigned char bx = (unsigned char)(ball_x >> 8);
    unsigned char paddle_mid = paddle_x + 24;

    if (bx < paddle_mid - 4)
        move_paddle(DIR_LEFT);
    else if (bx > paddle_mid + 4)
        move_paddle(DIR_RIGHT);
}

/* ================================================================
 *  INPUT
 * ================================================================ */

static unsigned char read_joy_dir(void) {
    unsigned char joy = joy_read(JOY_2);
    if (JOY_LEFT(joy))  return DIR_LEFT;
    if (JOY_RIGHT(joy)) return DIR_RIGHT;
    return DIR_NONE;
}

static unsigned char read_joy_fire(void) {
    unsigned char joy = joy_read(JOY_2);
    return JOY_FIRE(joy);
}

/* ================================================================
 *  TITLE SCREEN
 * ================================================================ */

static void draw_title(void) {
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    gotoxy(12, 3);
    textcolor(YELLOW);
    cprintf("A R K A N O I D");

    gotoxy(10, 6);
    textcolor(WHITE);
    cprintf("FOR COMMODORE 64");

    /* Draw a decorative brick row */
    {
        unsigned char i;
        unsigned char *scr = (unsigned char*)0x0400;
        unsigned char *col = (unsigned char*)0xD800;
        for (i = 4; i < 28; ++i) {
            scr[8 * 40 + i] = CHAR_BRICK;
            col[8 * 40 + i] = brick_colors[i % 5];
        }
    }

    gotoxy(8, 11);
    textcolor(CYAN);
    cprintf("PRESS FIRE TO START");

    gotoxy(10, 13);
    textcolor(GREEN);
    cprintf("OR WAIT FOR DEMO");

    gotoxy(6, 16);
    textcolor(GREY2);
    cprintf("USE JOYSTICK PORT 2");

    gotoxy(4, 18);
    textcolor(LTBLUE);
    cprintf("ANGLE BALL WITH PADDLE");

    gotoxy(4, 20);
    textcolor(GREY3);
    cprintf("EDGE = WIDE ANGLE");

    gotoxy(4, 21);
    textcolor(GREY3);
    cprintf("CENTER = STRAIGHT UP");

    gotoxy(5, 24);
    textcolor(GREY1);
    cprintf("AI TOOLCHAIN PROJECT 2026");
}

/* ================================================================
 *  GAME INIT PER ROUND
 * ================================================================ */

static void init_round(void) {
    /* Place paddle centred */
    paddle_x = CHAR2SPR_X(FIELD_LEFT + 1 + FIELD_W / 2 - PADDLE_WIDTH / 2);

    /* Ball stuck to paddle */
    ball_x = ((unsigned int)(paddle_x + 12)) << 8;
    ball_y = ((unsigned int)(PADDLE_Y_SPR - 8)) << 8;
    ball_dx = 0;
    ball_dy = 0;

    game_state = STATE_LAUNCH;
}

static void draw_field(void) {
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);
    draw_border();
    draw_bricks();
    draw_hud();
}

/* ================================================================
 *  MAIN GAME LOOP
 * ================================================================ */

static void game_loop(void) {
    unsigned char dir;

    while (1) {
        waitvsync();
        ++frame_count;

        /* Sound auto-off */
        if (sound_timer > 0) {
            --sound_timer;
            if (sound_timer == 0) sound_off();
        }

        /* ── STATE: LAUNCH (ball on paddle) ──────────── */
        if (game_state == STATE_LAUNCH) {
            if (demo_mode) {
                /* Demo auto-launches after short delay */
                if ((frame_count & 0x3F) == 0) {
                    launch_ball();
                    game_state = STATE_PLAY;
                }
                demo_ai();
            } else {
                dir = read_joy_dir();
                move_paddle(dir);
                /* Ball tracks paddle */
                ball_x = ((unsigned int)(paddle_x + 12)) << 8;
                ball_y = ((unsigned int)(PADDLE_Y_SPR - 8)) << 8;
                if (read_joy_fire()) {
                    launch_ball();
                    game_state = STATE_PLAY;
                }
            }
            update_sprites();
            draw_hud();
            continue;
        }

        /* ── STATE: PLAY ──────────────────────────────── */
        if (game_state == STATE_PLAY) {
            if (demo_mode) {
                demo_ai();
            } else {
                dir = read_joy_dir();
                move_paddle(dir);
            }

            move_ball();
            update_sprites();
            draw_hud();

            /* Demo: fire exits to title */
            if (demo_mode && read_joy_fire()) return;

            continue;
        }

        /* ── STATE: DYING ─────────────────────────────── */
        if (game_state == STATE_DYING) {
            sound_die();
            --lives;
            if (lives == 0) {
                game_state = STATE_LOST;
            } else {
                init_round();
                update_sprites();
            }
            continue;
        }

        /* ── STATE: WON ──────────────────────────────── */
        if (game_state == STATE_WON) {
            sound_win();
            gotoxy(13, 12);
            textcolor(YELLOW);
            cprintf("LEVEL CLEAR!");
            /* Wait ~3 seconds */
            {
                unsigned char w;
                for (w = 0; w < 180; ++w) waitvsync();
            }
            if (demo_mode) return;

            ++level;
            generate_level();
            draw_field();
            init_round();
            update_sprites();
            continue;
        }

        /* ── STATE: LOST ──────────────────────────────── */
        if (game_state == STATE_LOST) {
            gotoxy(14, 12);
            textcolor(RED);
            cprintf("GAME OVER");
            gotoxy(11, 14);
            textcolor(WHITE);
            cprintf("SCORE: %05u", score);
            /* Wait ~4 seconds */
            {
                unsigned char w;
                for (w = 0; w < 240; ++w) waitvsync();
            }
            return;
        }
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

int main(void) {
    unsigned int title_timer;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    init_sound();
    init_sprites();

    joy_install(joy_static_stddrv);

title_screen:
    draw_title();

    /* Wait for fire or timeout → demo */
    title_timer = 0;
    while (!read_joy_fire() && title_timer < 300) {
        waitvsync();
        ++title_timer;
    }

    /* Common setup */
    score = 0;
    lives = 3;
    level = 1;
    frame_count = 0;
    sound_timer = 0;

    if (read_joy_fire()) {
        demo_mode = 0;
    } else {
        demo_mode = 1;
    }

    generate_level();
    draw_field();
    init_round();
    update_sprites();

    game_loop();

    goto title_screen;

    return 0;
}

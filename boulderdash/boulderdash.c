/*
 * BOULDER RUSH — a Boulder Dash tribute for the Commodore 64 (cc65)
 *
 * Character-mode game with a custom charset at $3800.
 * Classic Boulder Dash cave physics: boulders and gems fall, roll off
 * rounded objects, and are deadly only while falling. Dig dirt, collect
 * gems, open the exit, beat the clock.
 *
 * AI Toolchain Project 2026
 */

#include <c64.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <joystick.h>
#include <peekpoke.h>

/* --- Hardware --- */
#define SCREEN  ((unsigned char*)0x0400)
#define COLRAM  ((unsigned char*)0xD800)
#define CHARSET 0x3800          /* charset slot 7 in VIC bank 0 */

#define MAP_W 40
#define MAP_H 24                /* playfield on screen rows 1-24, HUD row 0 */

/* --- Tiles (low nibble) --- */
#define T_SPACE    0
#define T_DIRT     1
#define T_BOULDER  2
#define T_DIAMOND  3
#define T_WALL     4
#define T_STEEL    5
#define T_EXIT     6
#define T_EXITOPEN 7
#define T_PLAYER   8
#define T_EXPL     9

/* --- Cell flags (high nibble) --- */
#define F_FALL 0x10             /* object is falling */
#define F_MOV0 0x40             /* moved on even physics tick */
#define F_MOV1 0x80             /* moved on odd physics tick */
#define TILE(c) ((c) & 0x0F)

/* Screen codes of the custom tiles (charset indexes 128+) */
const unsigned char tile_char[10]  = { 32, 128, 129, 130, 131, 132, 133, 134, 135, 136 };
const unsigned char tile_color[10] = {
    COLOR_BLACK,      /* space   */
    COLOR_ORANGE,     /* dirt    */
    COLOR_GRAY2,      /* boulder */
    COLOR_WHITE,      /* diamond */
    COLOR_LIGHTRED,   /* wall    */
    COLOR_GRAY3,      /* steel   */
    COLOR_GRAY1,      /* exit closed */
    COLOR_WHITE,      /* exit open   */
    COLOR_CYAN,       /* player  */
    COLOR_YELLOW      /* explosion   */
};

/* --- Custom tile glyphs, chars 128..136 --- */
const unsigned char tile_gfx[9 * 8] = {
    /* 128 DIRT: speckled soil */
    0x59, 0xA6, 0x95, 0x6A, 0x59, 0xA6, 0x95, 0x6A,
    /* 129 BOULDER: round rock with highlight */
    0x3C, 0x5E, 0xBF, 0xDF, 0xFF, 0xFF, 0x7E, 0x3C,
    /* 130 DIAMOND */
    0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x18,
    /* 131 WALL: bricks */
    0xEF, 0xEF, 0xEF, 0x00, 0xFE, 0xFE, 0xFE, 0x00,
    /* 132 STEEL: plate with diagonal shine */
    0xFF, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0xFF,
    /* 133 EXIT closed: X plate */
    0xFF, 0xC3, 0xA5, 0x99, 0x99, 0xA5, 0xC3, 0xFF,
    /* 134 EXIT open: hollow door */
    0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF,
    /* 135 PLAYER: little miner */
    0x18, 0x18, 0x3C, 0x5A, 0x99, 0x3C, 0x24, 0x66,
    /* 136 EXPLOSION: burst */
    0x99, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x99
};

/* --- SID --- */
#define SID_V1FLo 0xD400
#define SID_V1FHi 0xD401
#define SID_V1CR  0xD404
#define SID_V1AD  0xD405
#define SID_V1SR  0xD406
#define SID_V2FLo 0xD407
#define SID_V2FHi 0xD408
#define SID_V2CR  0xD40B
#define SID_V2AD  0xD40C
#define SID_V2SR  0xD40D
#define SID_VOL   0xD418

/* --- Game state --- */
unsigned char grid[MAP_H][MAP_W];
unsigned char px, py;               /* player cell */
unsigned char exit_x, exit_y;
unsigned char level = 0;
unsigned char lives = 3;
unsigned int  score = 0;
unsigned int  high_score = 0;
unsigned char gems = 0, needed = 0;
unsigned char time_left = 0;        /* seconds */
unsigned char sec_frames = 50;      /* PAL frames per second tick */
unsigned char dead = 0, won = 0;
unsigned char hud_dirty = 0;
unsigned char parity = 0;
unsigned char push_cnt = 0;
unsigned char move_delay = 0;
unsigned char flash = 0;            /* border flash frames */
unsigned char exit_open = 0;

const unsigned int cave_seeds[5] = { 1234, 5678, 9012, 3456, 7890 };

/* Agent input byte (async_agent_control.py convention, same bits as
   the cc65 joystick masks). ORed with the real joystick, then cleared:
   edge-triggered, so one monitor write is exactly one input event
   regardless of host/emulator timing */
#define AGENT_INPUT 0x033C
unsigned char read_input(void) {
    unsigned char v = joy_read(JOY_2) | PEEK(AGENT_INPUT);
    POKE(AGENT_INPUT, 0);
    return v;
}
const unsigned char spark_colors[3] = { COLOR_WHITE, COLOR_CYAN, COLOR_YELLOW };

/* --- Sound --- */
void init_sound(void) {
    unsigned char i;
    for (i = 0; i < 25; i++) POKE(SID_V1FLo + i, 0);
    POKE(SID_VOL, 15);
}

void sfx_dig(void) {
    POKE(SID_V1CR, 0x80);
    POKE(SID_V1AD, 0x08); POKE(SID_V1SR, 0x00);
    POKE(SID_V1FHi, 0x06); POKE(SID_V1FLo, 0x00);
    POKE(SID_V1CR, 0x81);           /* noise */
}

void sfx_gem(void) {
    POKE(SID_V1CR, 0x20);
    POKE(SID_V1AD, 0x0A); POKE(SID_V1SR, 0x00);
    POKE(SID_V1FHi, 0x30); POKE(SID_V1FLo, 0x00);
    POKE(SID_V1CR, 0x21);           /* triangle blip */
}

void sfx_thud(void) {
    POKE(SID_V2CR, 0x80);
    POKE(SID_V2AD, 0x09); POKE(SID_V2SR, 0x00);
    POKE(SID_V2FHi, 0x03); POKE(SID_V2FLo, 0x00);
    POKE(SID_V2CR, 0x81);           /* low noise */
}

void sfx_boom(void) {
    POKE(SID_V2CR, 0x80);
    POKE(SID_V2AD, 0x0C); POKE(SID_V2SR, 0x00);
    POKE(SID_V2FHi, 0x05); POKE(SID_V2FLo, 0x00);
    POKE(SID_V2CR, 0x81);
}

void sfx_exit(void) {
    POKE(SID_V1CR, 0x20);
    POKE(SID_V1AD, 0x09); POKE(SID_V1SR, 0x00);
    POKE(SID_V1FHi, 0x20); POKE(SID_V1FLo, 0x00);
    POKE(SID_V1CR, 0x21);
}

/* Short blocking jingle for title/game over */
void jingle(unsigned char up) {
    static const unsigned char hi_up[3]   = { 0x11, 0x15, 0x19 };
    static const unsigned char hi_down[3] = { 0x19, 0x15, 0x11 };
    const unsigned char *hi = up ? hi_up : hi_down;
    unsigned char n, d;
    POKE(SID_V1AD, 0x09); POKE(SID_V1SR, 0x00);
    for (n = 0; n < 3; n++) {
        POKE(SID_V1FHi, hi[n]); POKE(SID_V1FLo, 0x40);
        POKE(SID_V1CR, 0x21);
        for (d = 0; d < 9; d++) waitvsync();
        POKE(SID_V1CR, 0x20);
    }
}

/* --- Charset --- */
void install_charset(void) {
    __asm__("sei");
    POKE(1, PEEK(1) & 0xFB);        /* map char ROM at $D000 for CPU */
    /* Copy the lower/upper set ($D800): cc65's conio emits screen
       codes $41-$5A for uppercase text, which are letters only in
       this second charset, not in the uppercase/graphics one */
    memcpy((void*)CHARSET, (void*)0xD800, 0x0800);
    POKE(1, PEEK(1) | 0x04);        /* restore I/O */
    __asm__("cli");
    memcpy((void*)(CHARSET + 128 * 8), tile_gfx, sizeof(tile_gfx));
    POKE(0xD018, 0x1E);             /* screen $0400, charset $3800 */
}

/* --- Drawing --- */
void draw_cell(unsigned char x, unsigned char y) {
    unsigned int o = 40 + y * 40 + x;   /* +40: HUD occupies row 0 */
    unsigned char t = TILE(grid[y][x]);
    SCREEN[o] = tile_char[t];
    COLRAM[o] = tile_color[t];
}

void draw_cave(void) {
    unsigned char x, y;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            draw_cell(x, y);
}

void update_hud(void) {
    gotoxy(0, 0);
    textcolor(COLOR_WHITE);
    cprintf("CAVE %u  GEMS %02u/%02u  TIME %3u  SC %05u",
            (unsigned)(level + 1), (unsigned)gems, (unsigned)needed,
            (unsigned)time_left, score);
}

void draw_centered(unsigned char row, const char *text, unsigned char color) {
    unsigned char len = strlen(text);
    textcolor(color);
    gotoxy((40 - len) / 2, row);
    cputs(text);
}

/* --- Cave generation --- */
void gen_cave(unsigned char lvl) {
    unsigned char x, y, r;
    unsigned char count = 0;

    srand(cave_seeds[lvl % 5] + lvl);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) {
                grid[y][x] = T_STEEL;
                continue;
            }
            r = rand() % 100;
            if      (r < 58) grid[y][x] = T_DIRT;
            else if (r < 75) grid[y][x] = T_BOULDER;
            else if (r < 87) grid[y][x] = T_SPACE;
            else if (r < 93) grid[y][x] = T_WALL;
            else             grid[y][x] = T_DIAMOND;
        }
    }

    /* Safe pocket for the player start */
    for (y = 1; y <= 3; y++)
        for (x = 1; x <= 4; x++)
            grid[y][x] = T_DIRT;
    px = 2; py = 2;
    grid[py][px] = T_PLAYER;

    /* Exit door bottom-right, kept diggable from above */
    exit_x = MAP_W - 3; exit_y = MAP_H - 2;
    grid[exit_y][exit_x] = T_EXIT;
    grid[exit_y - 1][exit_x] = T_DIRT;

    /* Count gems actually present */
    for (y = 1; y < MAP_H - 1; y++)
        for (x = 1; x < MAP_W - 1; x++)
            if (TILE(grid[y][x]) == T_DIAMOND) count++;

    needed = (count * 3) / 5;       /* 60% of the spawned gems */
    if (needed < 5) needed = 5;
    gems = 0;
    time_left = 120;
    sec_frames = 50;
    dead = 0; won = 0;
    exit_open = 0;
    parity = 0;
    push_cnt = 0;
    move_delay = 0;
    flash = 0;
}

/* --- Explosion (player death) --- */
void explode(unsigned char cx, unsigned char cy) {
    signed char dx, dy;
    unsigned char x, y, i;

    sfx_boom();
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            x = cx + dx; y = cy + dy;
            if (TILE(grid[y][x]) != T_STEEL) {
                grid[y][x] = T_EXPL;
                draw_cell(x, y);
            }
        }
    }
    /* Flash animation */
    for (i = 0; i < 30; i++) {
        waitvsync();
        bordercolor((i & 2) ? COLOR_RED : COLOR_YELLOW);
    }
    bordercolor(COLOR_BLACK);
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            x = cx + dx; y = cy + dy;
            if (TILE(grid[y][x]) == T_EXPL) {
                grid[y][x] = T_SPACE;
                draw_cell(x, y);
            }
        }
    }
    dead = 1;
}

/* --- Physics: classic Boulder Dash cave scan --- */
/* Rounded objects let boulders/gems roll off them */
#define ROUNDED(t) ((t) == T_BOULDER || (t) == T_DIAMOND || (t) == T_WALL)

void physics_tick(void) {
    unsigned char x, t, b, cell;
    signed char y;
    unsigned char cur, oth;
    unsigned char *row, *rowb;

    parity ^= 1;
    cur = parity ? F_MOV1 : F_MOV0;
    oth = parity ? F_MOV0 : F_MOV1;

    /* Bottom-up scan: an object moving down lands in an already
       processed row, so it falls exactly one cell per tick */
    for (y = MAP_H - 2; y >= 1; y--) {
        row  = grid[y];
        rowb = grid[y + 1];
        for (x = 1; x < MAP_W - 1; x++) {
            cell = row[x];
            if (cell & oth) { cell &= ~oth; row[x] = cell; }
            if (cell & cur) continue;   /* already moved this tick */
            t = TILE(cell);
            if (t != T_BOULDER && t != T_DIAMOND) continue;

            b = TILE(rowb[x]);
            if (b == T_SPACE) {
                /* fall straight down */
                rowb[x] = t | F_FALL | cur;
                row[x] = T_SPACE;
                draw_cell(x, y);
                draw_cell(x, y + 1);
            } else if ((cell & F_FALL) && b == T_PLAYER) {
                explode(px, py);
                return;
            } else if (ROUNDED(b)) {
                /* roll off a rounded object if side+diagonal free */
                if (TILE(row[x - 1]) == T_SPACE && TILE(rowb[x - 1]) == T_SPACE) {
                    row[x - 1] = t | F_FALL | cur;
                    row[x] = T_SPACE;
                    draw_cell(x - 1, y);
                    draw_cell(x, y);
                } else if (TILE(row[x + 1]) == T_SPACE && TILE(rowb[x + 1]) == T_SPACE) {
                    row[x + 1] = t | F_FALL | cur;
                    row[x] = T_SPACE;
                    draw_cell(x + 1, y);
                    draw_cell(x, y);
                } else if (cell & F_FALL) {
                    row[x] = t;         /* stop */
                    sfx_thud();
                }
            } else if (cell & F_FALL) {
                row[x] = t;             /* land */
                sfx_thud();
            }
        }
    }
}

/* --- Exit --- */
void open_exit(void) {
    exit_open = 1;
    grid[exit_y][exit_x] = T_EXITOPEN;
    draw_cell(exit_x, exit_y);
    sfx_exit();
    flash = 16;
}

void collect_gem(void) {
    gems++;
    score += 10;
    sfx_gem();
    hud_dirty = 1;
    if (!exit_open && gems >= needed) open_exit();
}

/* --- Player movement --- */
void try_move(signed char dx, signed char dy, unsigned char grab) {
    unsigned char tx = px + dx, ty = py + dy;
    unsigned char t = TILE(grid[ty][tx]);
    unsigned char walk = 0;

    switch (t) {
    case T_SPACE:
        walk = !grab;
        break;
    case T_DIRT:
        sfx_dig();
        if (grab) {
            grid[ty][tx] = T_SPACE;
            draw_cell(tx, ty);
        } else walk = 1;
        break;
    case T_DIAMOND:
        collect_gem();
        if (grab) {
            grid[ty][tx] = T_SPACE;
            draw_cell(tx, ty);
        } else walk = 1;
        break;
    case T_EXITOPEN:
        won = 1;
        walk = 1;
        break;
    case T_BOULDER:
        /* push horizontally, needs empty cell beyond and insistence */
        if (dy == 0 && !grab && TILE(grid[ty][tx + dx]) == T_SPACE) {
            if (++push_cnt >= 3) {
                push_cnt = 0;
                grid[ty][tx + dx] = T_BOULDER;
                draw_cell(tx + dx, ty);
                sfx_thud();
                walk = 1;
            }
        }
        break;
    default:
        break;
    }

    if (walk) {
        grid[py][px] = T_SPACE;
        draw_cell(px, py);
        px = tx; py = ty;
        grid[py][px] = T_PLAYER;
        draw_cell(px, py);
    }
}

/* --- Diamond sparkle: random gems briefly change colour --- */
void sparkle(void) {
    unsigned char x = rand() % (MAP_W - 2) + 1;
    unsigned char y = rand() % (MAP_H - 2) + 1;
    if (TILE(grid[y][x]) == T_DIAMOND)
        COLRAM[40 + y * 40 + x] = spark_colors[rand() % 3];
}

/* --- One cave round: returns 1 on completion, 0 on death --- */
unsigned char play_cave(void) {
    unsigned char joy;
    unsigned char frame = 0;
    signed char dx, dy;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    clrscr();
    draw_cave();
    update_hud();

    while (1) {
        waitvsync();
        frame++;

        if (flash) {
            flash--;
            bordercolor(flash ? ((flash & 2) ? COLOR_WHITE : COLOR_GRAY2) : COLOR_BLACK);
        }

        sparkle();

        /* Input: polled only when the move cooldown expired, so a
           single agent tap can't be consumed without moving */
        if (move_delay) {
            move_delay--;
        } else {
            joy = read_input();
            dx = dy = 0;
            if      (JOY_UP(joy))    dy = -1;
            else if (JOY_DOWN(joy))  dy = 1;
            else if (JOY_LEFT(joy))  dx = -1;
            else if (JOY_RIGHT(joy)) dx = 1;
            else push_cnt = 0;
            if (dx || dy) {
                try_move(dx, dy, JOY_BTN_1(joy) ? 1 : 0);
                move_delay = 4;
            }
        }

        /* Cave physics at ~8 Hz */
        if ((frame % 6) == 0) physics_tick();

        /* Timer */
        if (--sec_frames == 0) {
            sec_frames = 50;
            if (time_left) {
                time_left--;
                hud_dirty = 1;
                if (time_left == 0 && !dead && !won) explode(px, py);
            }
        }

        if (hud_dirty) {
            update_hud();
            hud_dirty = 0;
        }

        if (dead) return 0;
        if (won)  return 1;
    }
}

/* --- Screens --- */
void title_screen(void) {
    unsigned int timer = 0;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    clrscr();

    draw_centered(3,  "* * * * * * * * * * * *", COLOR_BROWN);
    draw_centered(5,  "B O U L D E R   R U S H", COLOR_YELLOW);
    draw_centered(7,  "* * * * * * * * * * * *", COLOR_BROWN);
    draw_centered(9,  "A BOULDER DASH TRIBUTE", COLOR_GRAY3);

    draw_centered(12, "DIG DIRT - COLLECT GEMS", COLOR_LIGHTBLUE);
    draw_centered(13, "BEWARE OF FALLING ROCKS", COLOR_LIGHTRED);
    draw_centered(14, "OPEN THE EXIT AND ESCAPE", COLOR_LIGHTGREEN);

    draw_centered(17, "JOYSTICK PORT 2", COLOR_YELLOW);
    draw_centered(18, "FIRE+DIR GRABS WITHOUT MOVING", COLOR_YELLOW);

    if (high_score > 0) {
        gotoxy(11, 20);
        textcolor(COLOR_CYAN);
        cprintf("HIGH SCORE: %05u", high_score);
    }

    draw_centered(22, "PRESS FIRE TO START", COLOR_GREEN);
    draw_centered(24, "(C) 2026 AI TOOLCHAIN", COLOR_GRAY2);

    jingle(1);

    while (1) {
        waitvsync();
        timer++;
        if (timer & 0x20)
            draw_centered(22, "PRESS FIRE TO START", COLOR_GREEN);
        else
            draw_centered(22, "                   ", COLOR_BLACK);
        if (JOY_BTN_1(read_input())) break;
    }
    while (JOY_BTN_1(read_input())) waitvsync();
}

void game_over_screen(void) {
    unsigned int timer = 0;

    clrscr();
    jingle(0);
    draw_centered(8,  "G A M E   O V E R", COLOR_RED);
    gotoxy(12, 12);
    textcolor(COLOR_WHITE);
    cprintf("YOUR SCORE: %05u", score);
    if (score >= high_score && score > 0) {
        high_score = score;
        draw_centered(14, "** NEW HIGH SCORE **", COLOR_YELLOW);
    }
    draw_centered(18, "PRESS FIRE TO CONTINUE", COLOR_GREEN);

    while (JOY_BTN_1(read_input())) waitvsync();
    while (1) {
        waitvsync();
        timer++;
        if (timer & 0x20)
            draw_centered(18, "PRESS FIRE TO CONTINUE", COLOR_GREEN);
        else
            draw_centered(18, "                      ", COLOR_BLACK);
        if (JOY_BTN_1(read_input())) break;
    }
    while (JOY_BTN_1(read_input())) waitvsync();
}

void cave_complete_fx(void) {
    unsigned char i;
    for (i = 0; i < 20; i++) {
        waitvsync();
        bordercolor((i & 1) ? COLOR_WHITE : COLOR_GREEN);
        bgcolor((i & 2) ? COLOR_BLACK : COLOR_GREEN);
    }
    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
}

int main(void) {
    unsigned char result;

    joy_install(joy_static_stddrv);
    init_sound();
    install_charset();
    POKE(AGENT_INPUT, 0);       /* tape buffer may hold garbage at boot */

    while (1) {
        title_screen();
        lives = 3;
        score = 0;
        level = 0;

        while (lives > 0) {
            gen_cave(level);
            result = play_cave();
            if (result) {
                score += time_left;     /* time bonus */
                cave_complete_fx();
                level++;
            } else {
                lives--;
            }
        }
        if (score > high_score) high_score = score;
        game_over_screen();
    }

    return 0;
}

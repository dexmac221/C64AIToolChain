#include <c64.h>
#include <joystick.h>
#include <stdlib.h>

#define PLAY_X 4
#define PLAY_Y 4
#define PLAY_W 32
#define PLAY_H 19

#define MAX_OBJECTS 7
#define TYPE_NONE 0
#define TYPE_CRYSTAL 1
#define TYPE_METEOR 2
#define TYPE_REPAIR 3

#define BLACK 0
#define WHITE 1
#define RED 2
#define CYAN 3
#define PURPLE 4
#define GREEN 5
#define BLUE 6
#define YELLOW 7
#define ORANGE 8
#define LTRED 10
#define GREY1 11
#define LTGREEN 13
#define LTBLUE 14

#define SCREEN ((unsigned char*)0x0400)
#define COLOR_RAM ((unsigned char*)0xD800)
#define SPRITE_PTRS ((unsigned char*)0x07F8)
#define SPRITE_DATA 0x3000

#define SPR_BLOCK_SHIP 192
#define SPR_BLOCK_CRYSTAL 193
#define SPR_BLOCK_METEOR 194
#define SPR_BLOCK_REPAIR 195

#define VIC_SPR_ENA (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y (*(unsigned char*)0xD017)
#define VIC_SPR_PRIO (*(unsigned char*)0xD01B)
#define SPR_X(n) (*(unsigned char*)(0xD000 + (n) * 2))
#define SPR_Y(n) (*(unsigned char*)(0xD001 + (n) * 2))
#define SPR_COL(n) (*(unsigned char*)(0xD027 + (n)))

#define BORDER (*(unsigned char*)0xD020)
#define BG_COLOR (*(unsigned char*)0xD021)
#define VIC_MEM (*(unsigned char*)0xD018)

static unsigned char obj_x[MAX_OBJECTS];
static unsigned char obj_y[MAX_OBJECTS];
static unsigned char obj_type[MAX_OBJECTS];

static unsigned char player_x;
static unsigned char lives;
static unsigned int score;
static unsigned char wave;
static unsigned char tick;
static unsigned char game_over;
static unsigned char demo_mode;
static unsigned char combo;
static unsigned char combo_timer;
static unsigned char spawn_timer;
static unsigned char fall_timer;

static unsigned char screen_code(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (unsigned char)(ch - 'A' + 1);
    }
    if (ch >= 'a' && ch <= 'z') {
        return (unsigned char)(ch - 'a' + 1);
    }
    if (ch == '#') {
        return 160;
    }
    return (unsigned char)ch;
}

static void put_char(unsigned char x, unsigned char y, char ch, unsigned char color) {
    unsigned int idx;
    idx = (unsigned int)y * 40 + x;
    SCREEN[idx] = screen_code(ch);
    COLOR_RAM[idx] = color;
}

static void put_text(unsigned char x, unsigned char y, const char* text, unsigned char color) {
    while (*text) {
        put_char(x, y, *text, color);
        ++x;
        ++text;
    }
}

static void put_uint(unsigned char x, unsigned char y, unsigned int value, unsigned char width, unsigned char color) {
    unsigned char i;
    char digits[5];

    for (i = 0; i < 5; ++i) {
        digits[i] = '0';
    }
    for (i = 0; i < 5; ++i) {
        digits[4 - i] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (i = 5 - width; i < 5; ++i) {
        put_char(x, y, digits[i], color);
        ++x;
    }
}

static void clear_screen(unsigned char color) {
    unsigned int i;
    for (i = 0; i < 1000; ++i) {
        SCREEN[i] = 32;
        COLOR_RAM[i] = color;
    }
}

static void wait_frame(void) {
    while (VIC.rasterline == 250) {
    }
    while (VIC.rasterline != 250) {
    }
}

static void wait_frames(unsigned int frames) {
    while (frames > 0) {
        wait_frame();
        --frames;
    }
}

static void sid_beep(unsigned int freq, unsigned char waveform) {
    SID.v1.freq = freq;
    SID.v1.pw = 0x0800;
    SID.v1.ad = 0x00;
    SID.v1.sr = 0xF0;
    SID.amp = 15;
    SID.v1.ctrl = waveform | 0x01;
}

static void sid_off(void) {
    SID.v1.ctrl = 0;
}

static void seed_rng(void) {
    unsigned int seed;
    seed = (*(unsigned char*)0xDC04) | ((unsigned int)(*(unsigned char*)0xDC05) << 8);
    srand(seed);
}

static void sprite_copy(unsigned char block, const unsigned char* data) {
    unsigned char* dest;
    unsigned char i;
    dest = (unsigned char*)(SPRITE_DATA + ((unsigned int)(block - SPR_BLOCK_SHIP) * 64));
    for (i = 0; i < 63; ++i) {
        dest[i] = data[i];
    }
    dest[63] = 0;
}

static const unsigned char ship_sprite[63] = {
    0x00,0x18,0x00, 0x00,0x3C,0x00, 0x00,0x7E,0x00,
    0x00,0xFF,0x00, 0x01,0xFF,0x80, 0x03,0xFF,0xC0,
    0x07,0xDB,0xE0, 0x0F,0x99,0xF0, 0x1F,0x18,0xF8,
    0x3E,0x18,0x7C, 0x7C,0x3C,0x3E, 0xF8,0x7E,0x1F,
    0x30,0xFF,0x0C, 0x00,0xFF,0x00, 0x00,0x7E,0x00,
    0x00,0x3C,0x00, 0x00,0x18,0x00, 0x00,0x24,0x00,
    0x00,0x42,0x00, 0x00,0x81,0x00, 0x01,0x00,0x80
};

static const unsigned char crystal_sprite[63] = {
    0x00,0x18,0x00, 0x00,0x3C,0x00, 0x00,0x7E,0x00,
    0x00,0xFF,0x00, 0x01,0xFF,0x80, 0x03,0xFF,0xC0,
    0x07,0xE7,0xE0, 0x0F,0xC3,0xF0, 0x1F,0x81,0xF8,
    0x0F,0xC3,0xF0, 0x07,0xE7,0xE0, 0x03,0xFF,0xC0,
    0x01,0xFF,0x80, 0x00,0xFF,0x00, 0x00,0x7E,0x00,
    0x00,0x3C,0x00, 0x00,0x18,0x00, 0x00,0x18,0x00,
    0x00,0x00,0x00, 0x00,0x18,0x00, 0x00,0x00,0x00
};

static const unsigned char meteor_sprite[63] = {
    0x00,0x3C,0x00, 0x01,0xFF,0x80, 0x03,0xC3,0xC0,
    0x07,0x99,0xE0, 0x0F,0x3C,0xF0, 0x1E,0x7E,0x78,
    0x3C,0xE7,0x3C, 0x79,0xC3,0x9E, 0x73,0x81,0xCE,
    0x7F,0x99,0xFE, 0x3F,0xBD,0xFC, 0x1F,0xFF,0xF8,
    0x0F,0xE7,0xF0, 0x07,0xC3,0xE0, 0x03,0xFF,0xC0,
    0x01,0xFF,0x80, 0x00,0x7E,0x00, 0x00,0x3C,0x00,
    0x00,0x18,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00
};

static const unsigned char repair_sprite[63] = {
    0x00,0x18,0x00, 0x00,0x18,0x00, 0x00,0x18,0x00,
    0x00,0x18,0x00, 0x00,0x18,0x00, 0x07,0xFF,0xE0,
    0x07,0xFF,0xE0, 0x07,0xFF,0xE0, 0x00,0x18,0x00,
    0x00,0x18,0x00, 0x00,0x18,0x00, 0x00,0x18,0x00,
    0x00,0x18,0x00, 0x00,0x00,0x00, 0x03,0xFF,0xC0,
    0x07,0x81,0xE0, 0x0F,0x00,0xF0, 0x0F,0x00,0xF0,
    0x07,0x81,0xE0, 0x03,0xFF,0xC0, 0x00,0x00,0x00
};

static void set_sprite_pos(unsigned char spr, unsigned int x, unsigned char y) {
    SPR_X(spr) = (unsigned char)(x & 0xFF);
    SPR_Y(spr) = y;
    if (x > 255) {
        VIC_SPR_HI_X |= (unsigned char)(1 << spr);
    } else {
        VIC_SPR_HI_X &= (unsigned char)~(1 << spr);
    }
}

static unsigned int char_to_sprite_x(unsigned char char_x) {
    return (unsigned int)(24 + ((PLAY_X + char_x) * 8));
}

static unsigned char char_to_sprite_y(unsigned char char_y) {
    return (unsigned char)(50 + ((PLAY_Y + char_y) * 8));
}

static void init_sprites(void) {
    sprite_copy(SPR_BLOCK_SHIP, ship_sprite);
    sprite_copy(SPR_BLOCK_CRYSTAL, crystal_sprite);
    sprite_copy(SPR_BLOCK_METEOR, meteor_sprite);
    sprite_copy(SPR_BLOCK_REPAIR, repair_sprite);

    SPRITE_PTRS[0] = SPR_BLOCK_SHIP;
    SPRITE_PTRS[1] = SPR_BLOCK_CRYSTAL;
    SPRITE_PTRS[2] = SPR_BLOCK_METEOR;
    SPRITE_PTRS[3] = SPR_BLOCK_REPAIR;
    SPRITE_PTRS[4] = SPR_BLOCK_CRYSTAL;
    SPRITE_PTRS[5] = SPR_BLOCK_METEOR;
    SPRITE_PTRS[6] = SPR_BLOCK_REPAIR;
    SPRITE_PTRS[7] = SPR_BLOCK_CRYSTAL;

    SPR_COL(0) = WHITE;
    SPR_COL(1) = YELLOW;
    SPR_COL(2) = LTRED;
    SPR_COL(3) = LTGREEN;
    SPR_COL(4) = YELLOW;
    SPR_COL(5) = LTRED;
    SPR_COL(6) = LTGREEN;
    SPR_COL(7) = YELLOW;

    VIC_SPR_MCOLOR = 0;
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_PRIO = 0;
    VIC_SPR_HI_X = 0;
    VIC_SPR_ENA = 0;
}

static void draw_box(void) {
    unsigned char x;
    unsigned char y;

    for (x = 0; x < PLAY_W + 2; ++x) {
        put_char(PLAY_X - 1 + x, PLAY_Y - 1, '#', LTBLUE);
        put_char(PLAY_X - 1 + x, PLAY_Y + PLAY_H, '#', LTBLUE);
    }
    for (y = 0; y < PLAY_H; ++y) {
        put_char(PLAY_X - 1, PLAY_Y + y, '#', LTBLUE);
        put_char(PLAY_X + PLAY_W, PLAY_Y + y, '#', LTBLUE);
    }
}

static void draw_stars(void) {
    unsigned char x;
    unsigned char y;
    unsigned char pattern;

    for (y = 0; y < PLAY_H; ++y) {
        for (x = 0; x < PLAY_W; ++x) {
            pattern = (unsigned char)((x * 7 + y * 11) & 31);
            if (pattern == 0) {
                put_char(PLAY_X + x, PLAY_Y + y, '.', GREY1);
            } else if (pattern == 13) {
                put_char(PLAY_X + x, PLAY_Y + y, '.', BLUE);
            } else {
                put_char(PLAY_X + x, PLAY_Y + y, ' ', BLACK);
            }
        }
    }
}

static void draw_hud(void) {
    put_text(1, 0, demo_mode ? "SKY MINER SPR DEMO" : "SKY MINER SPR     ", LTGREEN);
    put_text(1, 1, "SC ", CYAN);
    put_uint(4, 1, score, 5, CYAN);
    put_text(11, 1, "L ", YELLOW);
    put_uint(13, 1, lives, 1, YELLOW);
    put_text(17, 1, "W ", WHITE);
    put_uint(19, 1, wave, 1, WHITE);
    put_text(23, 1, "C ", PURPLE);
    put_uint(25, 1, combo, 1, PURPLE);
}

static void reset_objects(void) {
    unsigned char i;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        obj_type[i] = TYPE_NONE;
        obj_x[i] = 0;
        obj_y[i] = 0;
    }
}

static void seed_demo_objects(void) {
    obj_type[0] = TYPE_CRYSTAL;
    obj_x[0] = 8;
    obj_y[0] = 3;
    obj_type[1] = TYPE_METEOR;
    obj_x[1] = 16;
    obj_y[1] = 5;
    obj_type[2] = TYPE_CRYSTAL;
    obj_x[2] = 24;
    obj_y[2] = 2;
    obj_type[3] = TYPE_REPAIR;
    obj_x[3] = 28;
    obj_y[3] = 8;
    obj_type[4] = TYPE_METEOR;
    obj_x[4] = 5;
    obj_y[4] = 9;
}

static void spawn_object(void) {
    unsigned char i;
    unsigned char roll;

    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == TYPE_NONE) {
            roll = (unsigned char)(rand() % 100);
            if (roll < 8) {
                obj_type[i] = TYPE_REPAIR;
            } else if (roll < 45) {
                obj_type[i] = TYPE_CRYSTAL;
            } else {
                obj_type[i] = TYPE_METEOR;
            }
            obj_x[i] = (unsigned char)(rand() % PLAY_W);
            obj_y[i] = 0;
            return;
        }
    }
}

static void update_object_sprites(void) {
    unsigned char i;
    unsigned char spr;
    unsigned char enabled;

    enabled = 1;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        spr = (unsigned char)(i + 1);
        if (obj_type[i] == TYPE_NONE) {
            continue;
        }

        if (obj_type[i] == TYPE_CRYSTAL) {
            SPRITE_PTRS[spr] = SPR_BLOCK_CRYSTAL;
            SPR_COL(spr) = YELLOW;
        } else if (obj_type[i] == TYPE_METEOR) {
            SPRITE_PTRS[spr] = SPR_BLOCK_METEOR;
            SPR_COL(spr) = LTRED;
        } else {
            SPRITE_PTRS[spr] = SPR_BLOCK_REPAIR;
            SPR_COL(spr) = LTGREEN;
        }
        set_sprite_pos(spr, char_to_sprite_x(obj_x[i]), char_to_sprite_y(obj_y[i]));
        enabled |= (unsigned char)(1 << spr);
    }

    VIC_SPR_ENA = enabled;
}

static void update_player_sprite(void) {
    set_sprite_pos(0, char_to_sprite_x(player_x), char_to_sprite_y(PLAY_H - 3));
    SPRITE_PTRS[0] = SPR_BLOCK_SHIP;
    SPR_COL(0) = demo_mode ? WHITE : CYAN;
}

static void move_player_human(void) {
    unsigned char joy;
    joy = joy_read(JOY_2);

    if ((joy & JOY_LEFT_MASK) != 0 && player_x > 1) {
        --player_x;
    }
    if ((joy & JOY_RIGHT_MASK) != 0 && player_x < PLAY_W - 2) {
        ++player_x;
    }
}

static signed char lane_score(unsigned char lane) {
    unsigned char i;
    signed char value;

    value = 0;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == TYPE_NONE) {
            continue;
        }
        if (obj_x[i] == lane || obj_x[i] + 1 == lane || (obj_x[i] > 0 && obj_x[i] - 1 == lane)) {
            if (obj_type[i] == TYPE_METEOR) {
                value -= (obj_y[i] >= PLAY_H - 6) ? 45 : 10;
            } else if (obj_type[i] == TYPE_CRYSTAL) {
                value += (signed char)(10 + obj_y[i]);
            } else {
                value += (signed char)(6 + obj_y[i]);
            }
        }
    }
    if (lane == 0 || lane == PLAY_W - 1) {
        value -= 2;
    }
    return value;
}

static void move_player_demo(void) {
    signed char left_score;
    signed char here_score;
    signed char right_score;

    here_score = lane_score(player_x);
    left_score = (player_x > 1) ? lane_score(player_x - 1) : -100;
    right_score = (player_x < PLAY_W - 2) ? lane_score(player_x + 1) : -100;

    if (left_score > here_score && left_score >= right_score) {
        --player_x;
    } else if (right_score > here_score && right_score > left_score) {
        ++player_x;
    }
}

static void move_player(void) {
    if (demo_mode) {
        move_player_demo();
    } else {
        move_player_human();
    }
}

static void update_wave(void) {
    unsigned char target;
    target = (unsigned char)(score / 120) + 1;
    if (target > 9) {
        target = 9;
    }
    wave = target;
}

static void add_score(unsigned char amount) {
    score += amount;
    if (combo < 9) {
        ++combo;
    }
    combo_timer = 80;
}

static void step_objects(void) {
    unsigned char i;

    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == TYPE_NONE) {
            continue;
        }

        if (obj_y[i] < PLAY_H - 3) {
            ++obj_y[i];
        } else {
            obj_type[i] = TYPE_NONE;
            continue;
        }

        if (obj_y[i] >= PLAY_H - 4 && obj_x[i] >= player_x - 1 && obj_x[i] <= player_x + 1) {
            if (obj_type[i] == TYPE_CRYSTAL) {
                add_score((unsigned char)(10 + combo * 2));
                sid_beep(0x1200, 0x20);
            } else if (obj_type[i] == TYPE_REPAIR) {
                add_score(20);
                if (lives < 5) {
                    ++lives;
                }
                sid_beep(0x1800, 0x20);
            } else {
                combo = 0;
                combo_timer = 0;
                if (lives > 0) {
                    --lives;
                }
                sid_beep(0x0500, 0x40);
            }
            obj_type[i] = TYPE_NONE;
        }
    }

    if (combo_timer > 0) {
        --combo_timer;
    } else {
        combo = 0;
    }

    if (lives == 0) {
        game_over = 1;
    }
}

static void title_screen(void) {
    unsigned char fire_detected;
    unsigned int wait_count;

    VIC_SPR_ENA = 0;
    VIC_MEM = 0x14;
    clear_screen(WHITE);
    BORDER = LTBLUE;
    BG_COLOR = BLACK;

    put_text(8, 5, "*** SKY MINER SPRITES ***", LTGREEN);
    put_text(5, 9, "CRYSTALS AND METEORS ARE SPRITES", CYAN);
    put_text(5, 11, "YELLOW GEM  RED ROCK  GREEN FIX", WHITE);
    put_text(6, 15, "FIRE = PLAY", WHITE);
    put_text(6, 16, "WAIT = DEMO AUTOPLAY", CYAN);

    fire_detected = 0;
    wait_count = 0;
    do {
        wait_frame();
        if ((joy_read(JOY_2) & JOY_FIRE_MASK) != 0) {
            demo_mode = 0;
            fire_detected = 1;
        }
        ++wait_count;
        if (wait_count > 170) {
            demo_mode = 1;
            fire_detected = 1;
        }
    } while (!fire_detected);

    while ((joy_read(JOY_2) & JOY_FIRE_MASK) != 0) {
        wait_frame();
    }
}

static void game_over_screen(void) {
    VIC_SPR_ENA = 0;
    VIC_MEM = 0x14;
    clear_screen(WHITE);
    put_text(12, 10, "MISSION FAILED", LTRED);
    put_text(11, 12, "SCORE ", WHITE);
    put_uint(18, 12, score, 5, WHITE);

    if (demo_mode) {
        put_text(8, 15, "DEMO RELAUNCHING", CYAN);
        wait_frames(75);
        return;
    }

    put_text(8, 15, "FIRE TO RETURN MENU", CYAN);
    while ((joy_read(JOY_2) & JOY_FIRE_MASK) == 0) {
        wait_frame();
    }
    while ((joy_read(JOY_2) & JOY_FIRE_MASK) != 0) {
        wait_frame();
    }
}

static void init_game(void) {
    VIC_MEM = 0x14;
    clear_screen(BLACK);
    init_sprites();
    BORDER = LTBLUE;
    BG_COLOR = BLACK;
    player_x = PLAY_W / 2;
    lives = 3;
    score = 0;
    wave = 1;
    tick = 0;
    game_over = 0;
    combo = 0;
    combo_timer = 0;
    spawn_timer = 0;
    fall_timer = 0;

    reset_objects();
    seed_demo_objects();
    draw_stars();
    draw_box();
    draw_hud();
    update_player_sprite();
    update_object_sprites();
}

int main(void) {
    joy_install(joy_static_stddrv);
    seed_rng();
    demo_mode = 1;

    while (1) {
        title_screen();
        init_game();

        while (!game_over) {
            unsigned char fall_rate;
            unsigned char spawn_rate;

            wait_frame();
            ++tick;
            move_player();

            spawn_rate = (unsigned char)(9 - wave);
            if (spawn_rate < 4) {
                spawn_rate = 4;
            }
            ++spawn_timer;
            if (spawn_timer >= spawn_rate) {
                spawn_object();
                spawn_timer = 0;
            }

            fall_rate = (unsigned char)(5 - (wave / 2));
            if (fall_rate < 2) {
                fall_rate = 2;
            }
            ++fall_timer;
            if (fall_timer >= fall_rate) {
                step_objects();
                fall_timer = 0;
            }

            update_wave();
            draw_hud();
            update_player_sprite();
            update_object_sprites();

            if ((tick & 0x03) == 0) {
                sid_off();
            }
        }

        game_over_screen();
    }

    return 0;
}

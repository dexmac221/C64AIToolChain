#include <c64.h>
#include <joystick.h>
#include <stdlib.h>

#define SCREEN ((unsigned char*)0x4400)
#define COLOR_RAM ((unsigned char*)0xD800)
#define SPRITE_PTRS ((unsigned char*)0x47F8)
#define SPRITE_DATA 0x7800
#define CHARSET_RAM ((unsigned char*)0x6000)

#define VIC_SPR_ENA (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR (*(unsigned char*)0xD01C)
#define VIC_SPR_MC0 (*(unsigned char*)0xD025)
#define VIC_SPR_MC1 (*(unsigned char*)0xD026)
#define VIC_SPR_DBL_X (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y (*(unsigned char*)0xD017)
#define VIC_SPR_PRIO (*(unsigned char*)0xD01B)
#define BORDER (*(unsigned char*)0xD020)
#define BG_COLOR (*(unsigned char*)0xD021)
#define VIC_MEM (*(unsigned char*)0xD018)
#define VIC_BANK (*(unsigned char*)0xDD00)

#define SPR_X(n) (*(unsigned char*)(0xD000 + (n) * 2))
#define SPR_Y(n) (*(unsigned char*)(0xD001 + (n) * 2))
#define SPR_COL(n) (*(unsigned char*)(0xD027 + (n)))

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
#define GREY2 12
#define LTGREEN 13
#define LTBLUE 14

#define SPR_BLOCK_BASE 224
#define SPR_BLOCK_SHIP0 224
#define SPR_BLOCK_SHIP1 225
#define SPR_BLOCK_BEAM 226
#define SPR_BLOCK_DRONE0 227
#define SPR_BLOCK_DRONE1 228
#define SPR_BLOCK_TURRET0 229
#define SPR_BLOCK_TURRET1 230
#define SPR_BLOCK_CORE0 231
#define SPR_BLOCK_CORE1 232

#define MAX_OBJECTS 6
#define OBJ_NONE 0
#define OBJ_DRONE 1
#define OBJ_TURRET 2
#define OBJ_CORE 3

#define SHIP_MIN_X 44
#define SHIP_MAX_X 128
#define SHIP_MIN_Y 62
#define SHIP_MAX_Y 218

static unsigned int ship_x;
static unsigned char ship_y;
static unsigned char shields;
static unsigned int score;
static unsigned char speed;
static unsigned char tick;
static unsigned char demo_mode;
static unsigned char game_over;

static unsigned char beam_active;
static unsigned int beam_x;
static unsigned char beam_y;

static unsigned char obj_type[MAX_OBJECTS];
static unsigned int obj_x[MAX_OBJECTS];
static unsigned char obj_y[MAX_OBJECTS];

static unsigned char spawn_timer;
static unsigned char deck_tick;

extern void scroll_deck_rows(void);

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

#include "sprites_mc.h"
#include "background_mc.h"

static void init_video_memory(void) {
    unsigned int i;

    VIC_BANK = (VIC_BANK & 0xFC) | 0x02;
    VIC_MEM = 0x18;
    for (i = 0; i < 2048; ++i) {
        CHARSET_RAM[i] = dreadline_charset[i];
    }
}

static const unsigned char beam_sprite[63] = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0xFF,0xFF,0xF0,
    0x7F,0xFF,0xE0, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00
};

static void sprite_copy(unsigned char block, const unsigned char* data) {
    unsigned char* dest;
    unsigned char i;
    dest = (unsigned char*)(SPRITE_DATA + ((unsigned int)(block - SPR_BLOCK_BASE) * 64));
    for (i = 0; i < 63; ++i) {
        dest[i] = data[i];
    }
    dest[63] = 0;
}

static void set_sprite_pos(unsigned char spr, unsigned int x, unsigned char y) {
    SPR_X(spr) = (unsigned char)(x & 0xFF);
    SPR_Y(spr) = y;
    if (x > 255) {
        VIC_SPR_HI_X |= (unsigned char)(1 << spr);
    } else {
        VIC_SPR_HI_X &= (unsigned char)~(1 << spr);
    }
}

static void hide_sprite(unsigned char spr) {
    set_sprite_pos(spr, 0, 0);
}

static void init_sprites(void) {
    sprite_copy(SPR_BLOCK_SHIP0, ship_sprite_0);
    sprite_copy(SPR_BLOCK_SHIP1, ship_sprite_1);
    sprite_copy(SPR_BLOCK_BEAM, beam_sprite);
    sprite_copy(SPR_BLOCK_DRONE0, drone_sprite_0);
    sprite_copy(SPR_BLOCK_DRONE1, drone_sprite_1);
    sprite_copy(SPR_BLOCK_TURRET0, turret_sprite_0);
    sprite_copy(SPR_BLOCK_TURRET1, turret_sprite_1);
    sprite_copy(SPR_BLOCK_CORE0, core_sprite_0);
    sprite_copy(SPR_BLOCK_CORE1, core_sprite_1);

    SPRITE_PTRS[0] = SPR_BLOCK_SHIP0;
    SPRITE_PTRS[1] = SPR_BLOCK_BEAM;
    SPRITE_PTRS[2] = SPR_BLOCK_DRONE0;
    SPRITE_PTRS[3] = SPR_BLOCK_TURRET0;
    SPRITE_PTRS[4] = SPR_BLOCK_CORE0;
    SPRITE_PTRS[5] = SPR_BLOCK_DRONE0;
    SPRITE_PTRS[6] = SPR_BLOCK_TURRET0;
    SPRITE_PTRS[7] = SPR_BLOCK_CORE0;

    SPR_COL(0) = WHITE;
    SPR_COL(1) = CYAN;
    SPR_COL(2) = LTRED;
    SPR_COL(3) = ORANGE;
    SPR_COL(4) = YELLOW;
    SPR_COL(5) = LTRED;
    SPR_COL(6) = ORANGE;
    SPR_COL(7) = YELLOW;

    VIC_SPR_MC0 = GREY1;
    VIC_SPR_MC1 = LTBLUE;
    VIC_SPR_MCOLOR = 0xFD;
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_PRIO = 0;
    VIC_SPR_HI_X = 0;
    VIC_SPR_ENA = 0;
}

static void draw_hud(void) {
    put_text(0, 0, demo_mode ? "DREADLINE DEMO" : "DREADLINE     ", LTGREEN);
    put_text(0, 1, "SC ", CYAN);
    put_uint(3, 1, score, 5, CYAN);
    put_text(10, 1, "SH ", YELLOW);
    put_uint(13, 1, shields, 1, YELLOW);
    put_text(17, 1, "SP ", WHITE);
    put_uint(20, 1, speed, 1, WHITE);
}

static unsigned char deck_column(unsigned char x, unsigned char phase) {
    return (unsigned char)(((unsigned int)x + phase) % DREADLINE_BG_WIDTH);
}

static unsigned char deck_screen_at(unsigned char x, unsigned char y, unsigned char phase) {
    if (y == 3 || y == 23) {
        return DREADLINE_BORDER_CHAR;
    }
    return dreadline_bg_screen[y - 4][deck_column(x, phase)];
}

static unsigned char deck_color_at(unsigned char x, unsigned char y, unsigned char phase) {
    if (y == 3 || y == 23) {
        return ((x + phase) & 1) ? GREY2 : LTBLUE;
    }
    return dreadline_bg_color[y - 4][deck_column(x, phase)];
}

static void draw_deck_frame(void) {
    unsigned char x;
    unsigned char y;
    unsigned int row;

    for (y = 3; y < 24; ++y) {
        row = (unsigned int)y * 40;
        for (x = 0; x < 40; ++x) {
            SCREEN[row + x] = deck_screen_at(x, y, deck_tick);
            COLOR_RAM[row + x] = deck_color_at(x, y, deck_tick);
        }
    }
}

static void scroll_deck(void) {
    unsigned char y;
    unsigned int row;

    ++deck_tick;
    scroll_deck_rows();
    for (y = 4; y < 23; ++y) {
        row = (unsigned int)y * 40;
        SCREEN[row + 39] = deck_screen_at(39, y, deck_tick);
        COLOR_RAM[row + 39] = deck_color_at(39, y, deck_tick);
    }
}

static void reset_objects(void) {
    unsigned char i;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        obj_type[i] = OBJ_NONE;
        obj_x[i] = 0;
        obj_y[i] = 0;
    }
}

static void seed_demo_objects(void) {
    obj_type[0] = OBJ_DRONE;
    obj_x[0] = 260;
    obj_y[0] = 82;
    obj_type[1] = OBJ_TURRET;
    obj_x[1] = 300;
    obj_y[1] = 154;
    obj_type[2] = OBJ_CORE;
    obj_x[2] = 340;
    obj_y[2] = 114;
}

static void spawn_object(void) {
    unsigned char i;
    unsigned char roll;

    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == OBJ_NONE) {
            roll = (unsigned char)(rand() % 100);
            if (roll < 45) {
                obj_type[i] = OBJ_DRONE;
            } else if (roll < 82) {
                obj_type[i] = OBJ_TURRET;
            } else {
                obj_type[i] = OBJ_CORE;
            }
            obj_x[i] = 336;
            obj_y[i] = (unsigned char)(SHIP_MIN_Y + (rand() % (SHIP_MAX_Y - SHIP_MIN_Y - 10)));
            return;
        }
    }
}

static unsigned char abs_diff_u8(unsigned char a, unsigned char b) {
    return (a > b) ? (unsigned char)(a - b) : (unsigned char)(b - a);
}

static unsigned int abs_diff_u16(unsigned int a, unsigned int b) {
    return (a > b) ? (unsigned int)(a - b) : (unsigned int)(b - a);
}

static void update_object_sprites(void) {
    unsigned char i;
    unsigned char spr;
    unsigned char enabled;
    unsigned char frame;

    enabled = 1;
    frame = (unsigned char)((tick >> 3) & 1);
    if (beam_active) {
        enabled |= 2;
    }

    for (i = 0; i < MAX_OBJECTS; ++i) {
        spr = (unsigned char)(i + 2);
        if (obj_type[i] == OBJ_NONE) {
            hide_sprite(spr);
            continue;
        }
        if (obj_type[i] == OBJ_DRONE) {
            SPRITE_PTRS[spr] = frame ? SPR_BLOCK_DRONE1 : SPR_BLOCK_DRONE0;
            SPR_COL(spr) = LTRED;
        } else if (obj_type[i] == OBJ_TURRET) {
            SPRITE_PTRS[spr] = frame ? SPR_BLOCK_TURRET1 : SPR_BLOCK_TURRET0;
            SPR_COL(spr) = ORANGE;
        } else {
            SPRITE_PTRS[spr] = frame ? SPR_BLOCK_CORE1 : SPR_BLOCK_CORE0;
            SPR_COL(spr) = YELLOW;
        }
        set_sprite_pos(spr, obj_x[i], obj_y[i]);
        enabled |= (unsigned char)(1 << spr);
    }

    VIC_SPR_ENA = enabled;
}

static void update_player_sprite(void) {
    SPRITE_PTRS[0] = ((tick >> 2) & 1) ? SPR_BLOCK_SHIP1 : SPR_BLOCK_SHIP0;
    set_sprite_pos(0, ship_x, ship_y);
    SPR_COL(0) = demo_mode ? WHITE : CYAN;
}

static void update_beam_sprite(void) {
    if (beam_active) {
        set_sprite_pos(1, beam_x, beam_y);
        SPR_COL(1) = CYAN;
    } else {
        hide_sprite(1);
    }
}

static void fire_beam(void) {
    if (!beam_active) {
        beam_active = 1;
        beam_x = ship_x + 22;
        beam_y = ship_y + 8;
        sid_beep(0x1800, 0x20);
    }
}

static void move_player_human(void) {
    unsigned char joy;
    joy = joy_read(JOY_2);

    if ((joy & JOY_LEFT_MASK) != 0 && ship_x > SHIP_MIN_X) {
        ship_x -= 2;
    }
    if ((joy & JOY_RIGHT_MASK) != 0 && ship_x < SHIP_MAX_X) {
        ship_x += 2;
    }
    if ((joy & JOY_UP_MASK) != 0 && ship_y > SHIP_MIN_Y) {
        ship_y -= 2;
    }
    if ((joy & JOY_DOWN_MASK) != 0 && ship_y < SHIP_MAX_Y) {
        ship_y += 2;
    }
    if ((joy & JOY_FIRE_MASK) != 0) {
        fire_beam();
    }
}

static signed char lane_score(unsigned char y) {
    unsigned char i;
    signed char value;

    value = 0;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == OBJ_NONE) {
            continue;
        }
        if (obj_x[i] > ship_x && obj_x[i] < ship_x + 100) {
            if (abs_diff_u8(obj_y[i], y) < 20) {
                value -= (obj_type[i] == OBJ_CORE) ? 8 : 35;
            } else if (obj_type[i] == OBJ_CORE) {
                value += 5;
            }
        }
    }
    return value;
}

static void move_player_demo(void) {
    signed char up_score;
    signed char here_score;
    signed char down_score;
    unsigned char i;

    here_score = lane_score(ship_y);
    up_score = (ship_y > SHIP_MIN_Y + 2) ? lane_score((unsigned char)(ship_y - 3)) : -100;
    down_score = (ship_y < SHIP_MAX_Y - 2) ? lane_score((unsigned char)(ship_y + 3)) : -100;

    if (up_score > here_score && up_score >= down_score) {
        ship_y -= 2;
    } else if (down_score > here_score && down_score > up_score) {
        ship_y += 2;
    }

    if (ship_x > 72) {
        --ship_x;
    } else if (ship_x < 68) {
        ++ship_x;
    }

    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] != OBJ_NONE && obj_x[i] > ship_x && obj_x[i] < 250 && abs_diff_u8(obj_y[i], ship_y) < 16) {
            fire_beam();
            return;
        }
    }
}

static void move_player(void) {
    if (demo_mode) {
        move_player_demo();
    } else {
        move_player_human();
    }
}

static void add_score(unsigned char points) {
    score += points;
    if (speed < 9 && score / 200 + 1 > speed) {
        ++speed;
    }
}

static void step_beam(void) {
    unsigned char i;

    if (!beam_active) {
        return;
    }
    beam_x += 8;
    if (beam_x > 330) {
        beam_active = 0;
        return;
    }

    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == OBJ_NONE) {
            continue;
        }
        if (abs_diff_u16(obj_x[i], beam_x) < 16 && abs_diff_u8(obj_y[i], beam_y) < 14) {
            if (obj_type[i] == OBJ_CORE) {
                add_score(35);
            } else {
                add_score(15);
            }
            obj_type[i] = OBJ_NONE;
            beam_active = 0;
            sid_beep(0x0800, 0x40);
            return;
        }
    }
}

static void step_objects(void) {
    unsigned char i;
    unsigned char drift;

    drift = (unsigned char)(2 + (speed / 3));
    for (i = 0; i < MAX_OBJECTS; ++i) {
        if (obj_type[i] == OBJ_NONE) {
            continue;
        }

        if (obj_x[i] > drift) {
            obj_x[i] -= drift;
        } else {
            obj_type[i] = OBJ_NONE;
            continue;
        }

        if (obj_type[i] == OBJ_DRONE && (tick & 7) == 0) {
            if (obj_y[i] < ship_y && obj_y[i] < SHIP_MAX_Y) {
                ++obj_y[i];
            } else if (obj_y[i] > ship_y && obj_y[i] > SHIP_MIN_Y) {
                --obj_y[i];
            }
        }

        if (abs_diff_u16(obj_x[i], ship_x) < 18 && abs_diff_u8(obj_y[i], ship_y) < 16) {
            obj_type[i] = OBJ_NONE;
            if (shields > 0) {
                --shields;
            }
            sid_beep(0x0500, 0x40);
        }
    }

    if (shields == 0) {
        game_over = 1;
    }
}

static void title_screen(void) {
    unsigned char fire_detected;
    unsigned int wait_count;

    VIC_SPR_ENA = 0;
    init_video_memory();
    clear_screen(WHITE);
    BORDER = BLUE;
    BG_COLOR = BLACK;

    put_text(8, 5, "DREADLINE", LTGREEN);
    put_text(3, 8, "ORIGINAL LOW ALTITUDE RAID", CYAN);
    put_text(3, 10, "SPRITE SHIP  BEAM  ENEMIES", WHITE);
    put_text(3, 12, "DESTROY CORES AND DODGE DRONES", YELLOW);
    put_text(6, 16, "FIRE = PLAY", WHITE);
    put_text(6, 17, "WAIT = DEMO AUTOPLAY", CYAN);

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
    clear_screen(WHITE);
    put_text(12, 10, "SHIP LOST", LTRED);
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
    init_video_memory();
    clear_screen(BLACK);
    init_sprites();
    BORDER = BLUE;
    BG_COLOR = BLACK;

    ship_x = 70;
    ship_y = 138;
    shields = 5;
    score = 0;
    speed = 1;
    tick = 0;
    game_over = 0;
    beam_active = 0;
    spawn_timer = 0;
    deck_tick = 0;

    reset_objects();
    seed_demo_objects();
    draw_deck_frame();
    draw_hud();
    update_player_sprite();
    update_beam_sprite();
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
            wait_frame();
            ++tick;

            if ((tick & 1) == 0) {
                scroll_deck();
            }

            move_player();
            step_beam();

            ++spawn_timer;
            if (spawn_timer >= (unsigned char)(38 - speed * 2)) {
                spawn_object();
                spawn_timer = 0;
            }

            if ((tick & 1) == 0) {
                step_objects();
            }

            draw_hud();
            update_player_sprite();
            update_beam_sprite();
            update_object_sprites();

            if ((tick & 3) == 0) {
                sid_off();
            }
        }

        game_over_screen();
    }

    return 0;
}

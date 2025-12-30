/*
 * Happy New Year 2026 Demo - PETASCII Edition
 * Commodore 64 - cc65
 * 
 * Features:
 * - "HAPPY NEW YEAR 2026" text display
 * - Fireworks particle system with PETASCII graphics
 * - Color cycling effects
 * - SID sound effects for explosions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#define POKE(addr, val) (*(unsigned char *)(addr) = (val))
#define PEEK(addr) (*(unsigned char *)(addr))
#define SCREEN(x, y) (1024 + (x) + (y) * 40)
#define COLOR(x, y) (55296 + (x) + (y) * 40)

/* SID chip registers */
#define SID_BASE    0xD400
#define SID_V1_FREQ_LO  (SID_BASE + 0)
#define SID_V1_FREQ_HI  (SID_BASE + 1)
#define SID_V1_PW_LO    (SID_BASE + 2)
#define SID_V1_PW_HI    (SID_BASE + 3)
#define SID_V1_CTRL     (SID_BASE + 4)
#define SID_V1_AD       (SID_BASE + 5)
#define SID_V1_SR       (SID_BASE + 6)
#define SID_V3_FREQ_LO  (SID_BASE + 14)
#define SID_V3_FREQ_HI  (SID_BASE + 15)
#define SID_V3_CTRL     (SID_BASE + 18)
#define SID_VOLUME      (SID_BASE + 24)
#define SID_V3_RANDOM   (SID_BASE + 27)

/* C64 Colors */
#define BLACK   0
#define WHITE   1
#define RED     2
#define CYAN    3
#define PURPLE  4
#define GREEN   5
#define BLUE    6
#define YELLOW  7
#define ORANGE  8
#define BROWN   9
#define LRED    10
#define DGRAY   11
#define MGRAY   12
#define LGREEN  13
#define LBLUE   14
#define LGRAY   15

/* Simple graphic characters that definitely work */
#define CHAR_DOT        46   /* . */
#define CHAR_STAR       42   /* * */
#define CHAR_PLUS       43   /* + */
#define CHAR_BLOCK      160  /* solid block */

/* Firework particle structure */
#define MAX_PARTICLES 50
#define MAX_ROCKETS 6

typedef struct {
    int x;          /* Position (fixed point 8.8) */
    int y;
    int vx;         /* Velocity */
    int vy;
    unsigned char color;
    unsigned char life;
    unsigned char active;
    unsigned char shape;  /* PETASCII character to use */
} Particle;

typedef struct {
    int x;
    int y;
    int vy;
    unsigned char active;
    unsigned char fuse;     /* Time until explosion */
} Rocket;

static Particle particles[MAX_PARTICLES];
static Rocket rockets[MAX_ROCKETS];

/* Firework colors - bright and festive */
static const unsigned char fw_colors[] = {
    WHITE, YELLOW, LRED, LGREEN, LBLUE, CYAN, ORANGE, PURPLE
};
#define NUM_FW_COLORS 8

/* Simple shapes only */
static const unsigned char petascii_shapes[] = {
    CHAR_STAR, CHAR_PLUS, CHAR_BLOCK, CHAR_DOT
};
#define NUM_SHAPES 4

/* Random number using SID noise */
static unsigned char sid_random(void) {
    return PEEK(SID_V3_RANDOM);
}

/* Initialize SID for noise-based random */
static void init_sid_random(void) {
    POKE(SID_V3_FREQ_LO, 0xFF);
    POKE(SID_V3_FREQ_HI, 0xFF);
    POKE(SID_V3_CTRL, 0x80);  /* Noise waveform */
}

/* Play explosion sound */
static void play_explosion(void) {
    POKE(SID_VOLUME, 15);
    POKE(SID_V1_AD, 0x00);    /* Instant attack, no decay */
    POKE(SID_V1_SR, 0xF9);    /* Max sustain, fast release */
    POKE(SID_V1_FREQ_LO, sid_random());
    POKE(SID_V1_FREQ_HI, 0x08 + (sid_random() & 0x07));
    POKE(SID_V1_CTRL, 0x81);  /* Noise + gate on */
}

/* Play launch sound */
static void play_launch(void) {
    POKE(SID_VOLUME, 15);
    POKE(SID_V1_AD, 0x08);
    POKE(SID_V1_SR, 0x80);
    POKE(SID_V1_FREQ_LO, 0x00);
    POKE(SID_V1_FREQ_HI, 0x10);
    POKE(SID_V1_CTRL, 0x21);  /* Sawtooth + gate */
}

/* Clear screen */
static void clear_screen(void) {
    unsigned int i;
    for (i = 0; i < 1000; i++) {
        POKE(1024 + i, 32);   /* Space character */
        POKE(55296 + i, BLACK);
    }
}

/* Draw a character with color */
static void draw_char(unsigned char x, unsigned char y, unsigned char ch, unsigned char col) {
    if (x < 40 && y < 25) {
        POKE(SCREEN(x, y), ch);
        POKE(COLOR(x, y), col);
    }
}

/* Draw the main title with color cycling */
static void draw_title(unsigned char color_offset) {
    static const unsigned char title_colors[] = {
        YELLOW, LGREEN, CYAN, LBLUE, PURPLE, LRED, ORANGE, WHITE
    };
    unsigned char i;
    unsigned char col;
    
    /* HAPPY NEW YEAR centered */
    const char *line1 = "HAPPY NEW YEAR";
    const char *line2 = "2026";
    
    /* Draw "HAPPY NEW YEAR" with rainbow effect */
    for (i = 0; i < 14; i++) {
        col = title_colors[(i + color_offset) % 8];
        draw_char(13 + i, 10, line1[i] - 'A' + 1, col);
    }
    
    /* Draw "2026" in the center below, also with color cycling */
    for (i = 0; i < 4; i++) {
        col = title_colors[(i + color_offset + 4) % 8];
        draw_char(18 + i, 12, line2[i] - '0' + 48, col);
    }
}

/* Initialize a particle for explosion */
static void spawn_particle(int x, int y, unsigned char color) {
    unsigned char i;
    signed char vx, vy;
    
    for (i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            /* Random velocity in all directions */
            vx = (signed char)((sid_random() & 0x3F) - 32);
            vy = (signed char)((sid_random() & 0x3F) - 32);
            
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx * 10;  /* Scale for fixed point */
            particles[i].vy = vy * 10;
            particles[i].color = color;
            particles[i].life = 20 + (sid_random() & 0x0F);
            particles[i].active = 1;
            particles[i].shape = petascii_shapes[sid_random() % NUM_SHAPES];
            return;
        }
    }
}

/* Create firework explosion at position */
static void explode_firework(int x, int y) {
    unsigned char i;
    unsigned char color;
    
    /* Pick a random color scheme */
    color = fw_colors[sid_random() % NUM_FW_COLORS];
    
    /* Spawn multiple particles */
    for (i = 0; i < 16; i++) {
        spawn_particle(x, y, color);
    }
    
    play_explosion();
}

/* Launch a new rocket */
static void launch_rocket(void) {
    unsigned char i;
    
    for (i = 0; i < MAX_ROCKETS; i++) {
        if (!rockets[i].active) {
            rockets[i].x = (8 + (sid_random() % 24)) << 8;  /* Random X position */
            rockets[i].y = 24 << 8;  /* Start at bottom */
            rockets[i].vy = -0x180 - (sid_random() & 0x7F);  /* Upward velocity */
            rockets[i].fuse = 15 + (sid_random() & 0x0F);    /* Explode after some time */
            rockets[i].active = 1;
            play_launch();
            return;
        }
    }
}

/* Update and draw particles */
static void update_particles(void) {
    register unsigned char i;
    register unsigned char sx, sy;
    unsigned char prev_x, prev_y;
    register Particle *p;
    unsigned char ch;
    
    for (i = 0; i < MAX_PARTICLES; i++) {
        p = &particles[i];
        if (p->active) {
            /* Store old position */
            prev_x = (p->x >> 8);
            prev_y = (p->y >> 8);
            
            /* Apply velocity */
            p->x += p->vx;
            p->y += p->vy;
            
            /* Apply gravity */
            p->vy += 12;
            
            /* Apply drag (simplified) */
            p->vx = (p->vx * 14) >> 4;
            p->vy = (p->vy * 14) >> 4;
            
            /* Get new screen position */
            sx = (p->x >> 8);
            sy = (p->y >> 8);
            
            /* Decrease life */
            p->life--;
            
            /* Check if dead */
            if (p->life == 0 || sy >= 25 || sy <= 0 || sx >= 40) {
                /* Erase before deactivating */
                if (prev_x < 40 && prev_y < 25 && prev_y > 0) {
                    POKE(SCREEN(prev_x, prev_y), 32);
                }
                p->active = 0;
                continue;
            }
            
            /* Erase old position if moved */
            if ((sx != prev_x || sy != prev_y) && prev_x < 40 && prev_y < 25 && prev_y > 0) {
                POKE(SCREEN(prev_x, prev_y), 32);
            }
            
            /* Draw particle with simple graphics */
            if (sx < 40 && sy < 25 && sy > 0) {
                /* Use particle's assigned shape, change to dot when life is low */
                if (p->life < 5) {
                    ch = CHAR_DOT;
                } else if (p->life < 10) {
                    ch = CHAR_PLUS;
                } else {
                    ch = p->shape;
                }
                POKE(SCREEN(sx, sy), ch);
                POKE(COLOR(sx, sy), p->color);
            }
        }
    }
}

/* Update rockets */
static void update_rockets(void) {
    register unsigned char i;
    register unsigned char sx, sy;
    unsigned char prev_x, prev_y;
    register Rocket *r;
    
    for (i = 0; i < MAX_ROCKETS; i++) {
        r = &rockets[i];
        if (r->active) {
            /* Erase old position */
            prev_x = (r->x >> 8);
            prev_y = (r->y >> 8);
            if (prev_x < 40 && prev_y < 25 && prev_y > 0) {
                POKE(SCREEN(prev_x, prev_y), 32);
            }
            
            /* Move rocket */
            r->y += r->vy;
            r->fuse--;
            
            /* Get screen position */
            sx = (r->x >> 8);
            sy = (r->y >> 8);
            
            /* Check for explosion */
            if (r->fuse == 0 || sy <= 3) {
                explode_firework(r->x, r->y);
                r->active = 0;
                continue;
            }
            
            /* Draw rocket trail */
            if (sx < 40 && sy < 25 && sy > 0) {
                POKE(SCREEN(sx, sy), CHAR_BLOCK);
                POKE(COLOR(sx, sy), YELLOW);
            }
        }
    }
}

/* Draw ground/horizon */
static void draw_ground(void) {
    unsigned char i;
    /* Ground line with simple block */
    for (i = 0; i < 40; i++) {
        draw_char(i, 24, CHAR_BLOCK, DGRAY);
    }
}

/* Draw stars in background - disabled */
static void draw_stars(unsigned char frame) {
    (void)frame;  /* Unused */
    /* Disabled to keep screen clean */
}

int main(void) {
    unsigned char frame = 0;
    unsigned char launch_timer = 0;
    unsigned char i;
    
    /* Initialize */
    init_sid_random();
    
    /* Clear all particles and rockets */
    for (i = 0; i < MAX_PARTICLES; i++) {
        particles[i].active = 0;
    }
    for (i = 0; i < MAX_ROCKETS; i++) {
        rockets[i].active = 0;
    }
    
    /* Set colors */
    POKE(53280, BLACK);  /* Border */
    POKE(53281, BLACK);  /* Background */
    
    /* Clear screen */
    clear_screen();
    
    /* Draw ground */
    draw_ground();
    
    /* Main loop */
    while (1) {
        /* No delay - run at full speed */
        
        /* Update frame counter */
        frame++;
        
        /* Draw title with color cycling */
        draw_title(frame >> 2);
        
        /* Draw twinkling stars */
        draw_stars(frame);
        
        /* Launch new rockets periodically */
        launch_timer++;
        if (launch_timer > 12 + (sid_random() & 0x0F)) {
            launch_rocket();
            launch_timer = 0;
        }
        
        /* Update game objects */
        update_rockets();
        update_particles();
        
        /* Turn off sound gate after some time */
        if ((frame & 0x0F) == 0) {
            POKE(SID_V1_CTRL, 0x80);  /* Gate off */
        }
    }
    
    return 0;
}

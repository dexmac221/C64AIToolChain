/*
 * Happy New Year 2026 Demo
 * Commodore 64 - cc65
 * 
 * Features:
 * - Background image with "HAPPY NEW YEAR 2026"
 * - Fireworks particle system overlay
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

/* Include image data */
#include "img.h"
#include "charmap.h"
#include "clrs.h"

/* Firework particle structure */
#define MAX_PARTICLES 60
#define MAX_ROCKETS 6

/* Particle character for fireworks */
#define PARTICLE_CHAR 255
#define ROCKET_CHAR 81  /* Ball character in PETSCII */

typedef struct {
    int x;          /* Position (fixed point 8.8) */
    int y;
    int vx;         /* Velocity */
    int vy;
    unsigned char color;
    unsigned char life;
    unsigned char active;
    unsigned char prev_x;   /* Previous screen position */
    unsigned char prev_y;
} Particle;

typedef struct {
    int x;
    int y;
    int vy;
    unsigned char active;
    unsigned char fuse;
    unsigned char prev_x;
    unsigned char prev_y;
} Rocket;

static Particle particles[MAX_PARTICLES];
static Rocket rockets[MAX_ROCKETS];

/* Firework colors */
static const unsigned char fw_colors[] = {
    WHITE, YELLOW, LRED, LGREEN, LBLUE, CYAN, ORANGE, PURPLE
};
#define NUM_FW_COLORS 8

/* Custom character pattern for particle (star/sparkle) */
static const unsigned char particle_pattern[] = {
    0x00, 0x08, 0x2A, 0x1C, 0x1C, 0x2A, 0x08, 0x00
};

/* Random number using SID noise */
static unsigned char sid_random(void) {
    return PEEK(SID_V3_RANDOM);
}

/* Initialize SID for noise-based random */
static void init_sid_random(void) {
    POKE(SID_V3_FREQ_LO, 0xFF);
    POKE(SID_V3_FREQ_HI, 0xFF);
    POKE(SID_V3_CTRL, 0x80);
}

/* Play explosion sound */
static void play_explosion(void) {
    POKE(SID_VOLUME, 15);
    POKE(SID_V1_AD, 0x00);
    POKE(SID_V1_SR, 0xF9);
    POKE(SID_V1_FREQ_LO, sid_random());
    POKE(SID_V1_FREQ_HI, 0x08 + (sid_random() & 0x07));
    POKE(SID_V1_CTRL, 0x81);
}

/* Play launch sound */
static void play_launch(void) {
    POKE(SID_VOLUME, 15);
    POKE(SID_V1_AD, 0x08);
    POKE(SID_V1_SR, 0x80);
    POKE(SID_V1_FREQ_LO, 0x00);
    POKE(SID_V1_FREQ_HI, 0x10);
    POKE(SID_V1_CTRL, 0x21);
}

/* Restore background at position */
static void restore_bg(unsigned char x, unsigned char y) {
    unsigned int offset;
    if (x < 40 && y < 25) {
        offset = x + y * 40;
        POKE(SCREEN(x, y), img[offset]);
        POKE(COLOR(x, y), clrs[offset]);
    }
}

/* Initialize a particle */
static void spawn_particle(int x, int y, unsigned char color) {
    unsigned char i;
    signed char vx, vy;
    
    for (i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            vx = (signed char)((sid_random() & 0x3F) - 32);
            vy = (signed char)((sid_random() & 0x3F) - 32);
            
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx * 8;
            particles[i].vy = vy * 8;
            particles[i].color = color;
            particles[i].life = 15 + (sid_random() & 0x0F);
            particles[i].active = 1;
            particles[i].prev_x = 255;
            particles[i].prev_y = 255;
            return;
        }
    }
}

/* Create firework explosion */
static void explode_firework(int x, int y) {
    unsigned char i;
    unsigned char color;
    
    color = fw_colors[sid_random() % NUM_FW_COLORS];
    
    for (i = 0; i < 18; i++) {
        spawn_particle(x, y, color);
    }
    
    play_explosion();
}

/* Launch a new rocket */
static void launch_rocket(void) {
    unsigned char i;
    
    for (i = 0; i < MAX_ROCKETS; i++) {
        if (!rockets[i].active) {
            rockets[i].x = (8 + (sid_random() % 24)) << 8;
            rockets[i].y = 24 << 8;
            rockets[i].vy = -0x180 - (sid_random() & 0x7F);
            rockets[i].fuse = 12 + (sid_random() & 0x0F);
            rockets[i].active = 1;
            rockets[i].prev_x = 255;
            rockets[i].prev_y = 255;
            play_launch();
            return;
        }
    }
}

/* Update particles */
static void update_particles(void) {
    register unsigned char i;
    register unsigned char sx, sy;
    register Particle *p;
    
    for (i = 0; i < MAX_PARTICLES; i++) {
        p = &particles[i];
        if (p->active) {
            /* Restore background at previous position */
            if (p->prev_x < 40 && p->prev_y < 25) {
                restore_bg(p->prev_x, p->prev_y);
            }
            
            /* Apply physics */
            p->x += p->vx;
            p->y += p->vy;
            p->vy += 12;  /* Gravity */
            p->vx = (p->vx * 14) >> 4;  /* Drag */
            p->vy = (p->vy * 14) >> 4;
            
            /* Get screen position */
            sx = (p->x >> 8);
            sy = (p->y >> 8);
            
            /* Decrease life */
            p->life--;
            
            /* Check if dead or out of bounds */
            if (p->life == 0 || sy >= 25 || sy <= 0 || sx >= 40) {
                p->active = 0;
                continue;
            }
            
            /* Draw particle */
            if (sx < 40 && sy < 25 && sy > 0) {
                POKE(SCREEN(sx, sy), PARTICLE_CHAR);
                POKE(COLOR(sx, sy), p->color);
                p->prev_x = sx;
                p->prev_y = sy;
            } else {
                p->prev_x = 255;
                p->prev_y = 255;
            }
        }
    }
}

/* Update rockets */
static void update_rockets(void) {
    register unsigned char i;
    register unsigned char sx, sy;
    register Rocket *r;
    
    for (i = 0; i < MAX_ROCKETS; i++) {
        r = &rockets[i];
        if (r->active) {
            /* Restore background at previous position */
            if (r->prev_x < 40 && r->prev_y < 25) {
                restore_bg(r->prev_x, r->prev_y);
            }
            
            /* Move rocket */
            r->y += r->vy;
            r->fuse--;
            
            /* Get screen position */
            sx = (r->x >> 8);
            sy = (r->y >> 8);
            
            /* Check for explosion */
            if (r->fuse == 0 || sy <= 2) {
                explode_firework(r->x, r->y);
                r->active = 0;
                continue;
            }
            
            /* Draw rocket */
            if (sx < 40 && sy < 25 && sy > 0) {
                POKE(SCREEN(sx, sy), ROCKET_CHAR);
                POKE(COLOR(sx, sy), ORANGE);
                r->prev_x = sx;
                r->prev_y = sy;
            } else {
                r->prev_x = 255;
                r->prev_y = 255;
            }
        }
    }
}

int main(void) {
    unsigned char frame = 0;
    unsigned char launch_timer = 0;
    unsigned int i;
    unsigned char j;
    
    /* Initialize SID random */
    init_sid_random();
    
    /* Clear particles and rockets */
    for (j = 0; j < MAX_PARTICLES; j++) {
        particles[j].active = 0;
    }
    for (j = 0; j < MAX_ROCKETS; j++) {
        rockets[j].active = 0;
    }
    
    /* Set background and border color */
    POKE(53280, BLACK);
    POKE(53281, BLACK);
    
    /* Set character memory to $3000 (12288) */
    POKE(53272, (PEEK(53272) & 0xF0) | 0x0C);
    
    /* Copy custom character set */
    for (i = 0; i < 2048; i++) {
        POKE(12288 + i, charmap[i]);
    }
    
    /* Define particle character at index 255 */
    for (j = 0; j < 8; j++) {
        POKE(12288 + PARTICLE_CHAR * 8 + j, particle_pattern[j]);
    }
    
    /* Draw the background image */
    for (i = 0; i < 1000; i++) {
        POKE(1024 + i, img[i]);
        POKE(55296 + i, clrs[i]);
    }
    
    /* Main loop */
    while (1) {
        frame++;
        
        /* Launch rockets periodically */
        launch_timer++;
        if (launch_timer > 15 + (sid_random() & 0x1F)) {
            launch_rocket();
            launch_timer = 0;
        }
        
        /* Update animation */
        update_rockets();
        update_particles();
        
        /* Turn off sound gate */
        if ((frame & 0x0F) == 0) {
            POKE(SID_V1_CTRL, 0x80);
        }
    }
    
    return 0;
}

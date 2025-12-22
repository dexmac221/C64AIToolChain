/**
 * @file main.c
 * @brief Matrix Digital Rain Effect for C64 - OPTIMIZED VERSION
 * 
 * This program creates a "Matrix" style digital rain effect on the Commodore 64.
 * Optimized for performance using direct memory pointers and pre-calculated buffers.
 */
#include <stdlib.h>
#include <string.h>
#include <conio.h>

/* VIC-II Register addresses */
#define VIC_BORDER      0xD020
#define VIC_BACKGROUND  0xD021
#define VIC_MEMSETUP    0xD018
#define VIC_RASTER      0xD012

/* Memory locations */
#define SCREEN_RAM      ((unsigned char*)0x0400)
#define COLOR_RAM       ((unsigned char*)0xD800)
#define CHARSET_RAM     ((unsigned char*)0x3000)

/* Screen dimensions */
#define SCREEN_WIDTH    40
#define SCREEN_HEIGHT   25
#define SCREEN_SIZE     1000

/* Colors - prefixed to avoid conflicts with cc65 headers */
#define COL_BLACK       0
#define COL_WHITE       1
#define COL_GREEN       5
#define COL_DGREEN      11  /* Dark green/gray for fading */
#define COL_LGREEN      13
#define COL_RED         2
#define FCOLOR          6

/* Direct memory access macros */
#ifndef POKE
#define POKE(addr,val)  (*(unsigned char*)(addr) = (val))
#endif
#ifndef PEEK
#define PEEK(addr)      (*(unsigned char*)(addr))
#endif

/* Kanji charset for matrix characters */
#include "kanji-charset.h"

/* Column state for rain effect */
static unsigned char col_active[SCREEN_WIDTH];
static unsigned char col_pos[SCREEN_WIDTH];
static unsigned char col_len[SCREEN_WIDTH];

/* Original VIC memory setup value (saved at startup) */
static unsigned char original_memsetup;

/* Adjustable effect parameters */
static unsigned char speed;      /* 1=fast, 5=slow */
static unsigned char density;    /* threshold: higher = more sparse */

/* Pre-calculated random buffer for speed */
#define RAND_BUFFER_SIZE 256
static unsigned char rand_buffer[RAND_BUFFER_SIZE];
static unsigned char rand_idx = 0;

/* Fast pseudo-random using pre-filled buffer */
static unsigned char fast_rand(void) {
    return rand_buffer[++rand_idx];
}

/* Fill random buffer once */
static void init_rand_buffer(void) {
    unsigned int i;
    for (i = 0; i < RAND_BUFFER_SIZE; ++i) {
        rand_buffer[i] = (unsigned char)rand();
    }
}

/* Fast screen clear using memset */
static void clear_screen(void) {
    memset(SCREEN_RAM, 32, SCREEN_SIZE);
}

/* Fast color fill using memset */
static void fill_color(unsigned char color) {
    memset(COLOR_RAM, color, SCREEN_SIZE);
}

/* Initialize VIC-II for black background */
static void init_vic(void) {
    POKE(VIC_BACKGROUND, COL_BLACK);
    POKE(VIC_BORDER, COL_BLACK);
    POKE(VIC_MEMSETUP, 21);
}

/* Wait for raster line - proper delay without busy loop */
static void wait_frame(void) {
    while (PEEK(VIC_RASTER) != 255);
    while (PEEK(VIC_RASTER) == 255);
}

/**
 * Matrix Effect 1 - Single column rain
 * Optimized with direct pointers and fast_rand
 * Enhanced with glow effect and character mutation
 */
void matrix1(void) {
    unsigned char col, row, k;
    unsigned int offset;
    unsigned char counter = 0;
    unsigned char *scr, *clr;
    
    init_vic();
    clear_screen();
    init_rand_buffer();
    
    /* Adjustable parameters */
    speed = 1;
    density = 230;  /* Not used much in this mode */
    
    for (;;) {
        /* Pick random column */
        col = fast_rand() % SCREEN_WIDTH;
        
        for (row = 0; row < SCREEN_HEIGHT; ++row) {
            offset = (unsigned int)row * SCREEN_WIDTH + col;
            scr = SCREEN_RAM + offset;
            clr = COLOR_RAM + offset;
            
            /* Random chance to stop */
            if ((fast_rand() & 7) == 0) {
                *clr = COL_WHITE;  /* Flash white before stopping */
                break;
            }
            
            /* Draw character - bright white head */
            *scr = (fast_rand() % 64) + 64;
            *clr = COL_WHITE;
            
            /* Fade previous positions - gradient effect */
            if (row > 0) {
                *(clr - SCREEN_WIDTH) = COL_LGREEN;
                /* Mutate character in trail */
                if ((fast_rand() & 3) == 0) {
                    *(scr - SCREEN_WIDTH) = (fast_rand() % 64) + 64;
                }
            }
            if (row > 1) {
                *(clr - SCREEN_WIDTH * 2) = COL_GREEN;
            }
            if (row > 3) {
                *(clr - SCREEN_WIDTH * 4) = COL_DGREEN;
            }
            
            /* Wait based on speed setting */
            for (k = 0; k < speed; ++k) {
                wait_frame();
            }
            
            /* Check keyboard for controls */
            if (kbhit()) {
                char key = cgetc();
                switch (key) {
                    case '+':
                        if (speed > 1) speed--;
                        break;
                    case '-':
                        if (speed < 5) speed++;
                        break;
                    case ' ':
                    case 'q':
                    case 'Q':
                        return;
                }
            }
        }
        
        /* Periodic clear */
        if (++counter > 32) {
            clear_screen();
            counter = 0;
        }
    }
}

/**
 * Matrix Effect 2 - Multiple simultaneous columns
 * Optimized with state arrays and batch updates
 * Enhanced with glow gradient and character mutation
 */
void matrix2(void) {
    unsigned char k;
    unsigned int offset;
    unsigned char *scr, *clr;
    
    init_vic();
    clear_screen();
    init_rand_buffer();
    
    /* Initialize column states */
    memset(col_active, 0, SCREEN_WIDTH);
    memset(col_pos, 0, SCREEN_WIDTH);
    
    /* Adjustable parameters */
    speed = 1;
    density = 240;
    
    for (;;) {
        /* Activate random columns based on density */
        for (k = 0; k < SCREEN_WIDTH; ++k) {
            if (!col_active[k] && (fast_rand() > density)) {
                col_active[k] = 1;
                col_pos[k] = 0;
                col_len[k] = (fast_rand() % 15) + 5;  /* Random length 5-20 */
            }
        }
        
        /* Update active columns */
        for (k = 0; k < SCREEN_WIDTH; ++k) {
            if (col_active[k]) {
                /* Only draw if within screen bounds */
                if (col_pos[k] < SCREEN_HEIGHT) {
                    offset = (unsigned int)col_pos[k] * SCREEN_WIDTH + k;
                    scr = SCREEN_RAM + offset;
                    clr = COLOR_RAM + offset;
                    
                    /* Draw current position - WHITE head for glow */
                    *scr = (fast_rand() % 64) + 64;
                    *clr = COL_WHITE;
                    
                    /* Gradient fade for trail */
                    if (col_pos[k] > 0) {
                        *(clr - SCREEN_WIDTH) = COL_LGREEN;
                        /* Random character mutation in trail */
                        if ((fast_rand() & 7) == 0) {
                            *(scr - SCREEN_WIDTH) = (fast_rand() % 64) + 64;
                        }
                    }
                    if (col_pos[k] > 1) {
                        *(clr - SCREEN_WIDTH * 2) = COL_GREEN;
                    }
                    if (col_pos[k] > 3) {
                        *(clr - SCREEN_WIDTH * 4) = COL_DGREEN;
                    }
                }
                
                /* Erase tail - only if within screen */
                if (col_pos[k] >= col_len[k]) {
                    unsigned char tail_pos = col_pos[k] - col_len[k];
                    if (tail_pos < SCREEN_HEIGHT) {
                        offset = (unsigned int)tail_pos * SCREEN_WIDTH + k;
                        *(SCREEN_RAM + offset) = 32;
                    }
                }
                
                /* Move down */
                col_pos[k]++;
                
                /* Deactivate when fully off screen */
                if (col_pos[k] >= SCREEN_HEIGHT + col_len[k]) {
                    col_active[k] = 0;
                }
            }
        }
        
        /* Wait based on speed setting */
        for (k = 0; k < speed; ++k) {
            wait_frame();
        }
        
        /* Check keyboard for controls */
        if (kbhit()) {
            char key = cgetc();
            switch (key) {
                case '+':
                    if (speed > 1) speed--;
                    break;
                case '-':
                    if (speed < 5) speed++;
                    break;
                case '1': case '2': case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':
                    density = 255 - ((key - '1') * 25);
                    break;
                case ' ':
                case 'q':
                case 'Q':
                    return;
            }
        }
    }
}

/**
 * Matrix Effect 3 - Kanji characters with custom charset
 * Uses charset at $2000 (8192)
 */
void matrix3(void) {
    unsigned char k;
    unsigned int i, offset;
    unsigned char *scr, *clr;
    unsigned char *custom_charset = (unsigned char*)0x2000;
    unsigned char old_memsetup;
    
    init_vic();
    
    /* Save current VIC memory setup */
    old_memsetup = PEEK(VIC_MEMSETUP);
    
    /* Load full kanji charset (256 chars * 8 bytes = 2048 bytes) to $2000 */
    for (i = 0; i < 2048; ++i) {
        custom_charset[i] = charmap[i];
    }
    
    /* 
     * Set VIC to use charset at $2000:
     * $D018 bits 1-3 select charset: $2000 = position 4 (4 * $800 = $2000)
     * Keep screen at $0400 (bits 4-7)
     * OR with 0x08 to set bits for $2000 charset
     */
    POKE(VIC_MEMSETUP, (PEEK(VIC_MEMSETUP) & 0xF0) | 0x08);
    
    clear_screen();
    fill_color(COL_GREEN);
    init_rand_buffer();
    
    /* Initialize column states */
    memset(col_active, 0, SCREEN_WIDTH);
    memset(col_pos, 0, SCREEN_WIDTH);
    
    /* Adjustable parameters */
    speed = 1;           /* 1=fast, 2=medium, 3=slow */
    density = 200;       /* threshold: lower = more dense */
    
    for (;;) {
        /* Activate random columns based on density */
        for (k = 0; k < SCREEN_WIDTH; ++k) {
            if (!col_active[k] && (fast_rand() > density)) {
                col_active[k] = 1;
                col_pos[k] = 0;
                col_len[k] = (fast_rand() % 12) + 4;
            }
        }
        
        /* Update active columns */
        for (k = 0; k < SCREEN_WIDTH; ++k) {
            if (col_active[k]) {
                /* Only draw if within screen bounds */
                if (col_pos[k] < SCREEN_HEIGHT) {
                    offset = (unsigned int)col_pos[k] * SCREEN_WIDTH + k;
                    scr = SCREEN_RAM + offset;
                    clr = COLOR_RAM + offset;
                    
                    /* Draw kanji character - WHITE head for glow */
                    *scr = (fast_rand() % 10) + 48;
                    *clr = COL_WHITE;
                    
                    /* Gradient fade for trail */
                    if (col_pos[k] > 0) {
                        *(clr - SCREEN_WIDTH) = COL_LGREEN;
                        /* Mutate kanji in trail */
                        if ((fast_rand() & 7) == 0) {
                            *(scr - SCREEN_WIDTH) = (fast_rand() % 10) + 48;
                        }
                    }
                    if (col_pos[k] > 1) {
                        *(clr - SCREEN_WIDTH * 2) = COL_GREEN;
                    }
                    if (col_pos[k] > 3) {
                        *(clr - SCREEN_WIDTH * 4) = COL_DGREEN;
                    }
                }
                
                /* Erase tail - only if within screen */
                if (col_pos[k] >= col_len[k]) {
                    unsigned char tail_pos = col_pos[k] - col_len[k];
                    if (tail_pos < SCREEN_HEIGHT) {
                        offset = (unsigned int)tail_pos * SCREEN_WIDTH + k;
                        *(SCREEN_RAM + offset) = 32;
                    }
                }
                
                col_pos[k]++;
                
                if (col_pos[k] >= SCREEN_HEIGHT + col_len[k]) {
                    col_active[k] = 0;
                }
            }
        }
        
        /* Wait based on speed setting */
        for (k = 0; k < speed; ++k) {
            wait_frame();
        }
        
        /* Check keyboard for controls */
        if (kbhit()) {
            char key = cgetc();
            switch (key) {
                case '+':  /* Faster */
                    if (speed > 1) speed--;
                    break;
                case '-':  /* Slower */
                    if (speed < 5) speed++;
                    break;
                case '1': case '2': case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':
                    /* Density: 1=sparse, 9=dense */
                    density = 255 - ((key - '1') * 25);
                    break;
                case ' ':  /* Exit */
                case 'q':
                case 'Q':
                    /* Restore default charset (screen $0400, charset ROM) */
                    POKE(VIC_MEMSETUP, 0x15);
                    return;
            }
        }
    }
}

/**
 * Display menu and get user choice
 */
unsigned char show_menu(void) {
    /* Restore default charset before showing menu */
    POKE(VIC_MEMSETUP, original_memsetup);
    
    clrscr();
    
    textcolor(COL_GREEN);
    cputs("\r\n");
    cputs("  === MATRIX DIGITAL RAIN ===\r\n");
    cputs("\r\n");
    cputs("  1 - MATRIX EFFECT (SINGLE)\r\n");
    cputs("  2 - MATRIX EFFECT (MULTI)\r\n");
    cputs("  3 - MATRIX KANJI (CUSTOM)\r\n");
    cputs("\r\n");
    cputs("  CONTROLS DURING EFFECT:\r\n");
    cputs("  +/-  SPEED UP/DOWN\r\n");
    cputs("  1-9  DENSITY (1=SPARSE 9=DENSE)\r\n");
    cputs("  Q    BACK TO MENU\r\n");
    cputs("\r\n");
    cputs("  SELECT (1-3): ");
    
    return cgetc();
}

int main(void) {
    unsigned char choice;
    
    /* Save original VIC memory setup at startup */
    original_memsetup = PEEK(VIC_MEMSETUP);
    
    /* Set black background for menu */
    bgcolor(COL_BLACK);
    bordercolor(COL_BLACK);
    
    for (;;) {
        choice = show_menu();
        
        switch (choice) {
            case '1':
                matrix1();
                break;
            case '2':
                matrix2();
                break;
            case '3':
                matrix3();
                break;
        }
    }
    
    return 0;
}




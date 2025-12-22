#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <serial.h>
#include <conio.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define POKE(addr, val) (*(unsigned char *)(addr) = (val))
#define POKEW(addr, val) (*(unsigned *)(addr) = (val))
#define PEEK(addr) (*(unsigned char *)(addr))
#define PEEKW(addr) (*(unsigned *)(addr))
#define PLACE(x, y) 1024 + x + y * 40
#define COLOR(x, y) 55296 + x + y * 40

#define HW "6502"

#ifdef __C64__
#undef HW
#define HW "commodore 64"
#endif // __C64__

#ifdef __C128__
#undef HW
#define HW "commodore 128"
#endif // __C64__

#include "img.h"
#include "charmap.h"
#include "clrs.h"

#define FCOLOR1 13
#define FCOLOR2 7

// SID chip registers
#define SID_BASE 0xD400
#define SID_FREQ_LO    (SID_BASE + 0)
#define SID_FREQ_HI    (SID_BASE + 1)
#define SID_PW_LO      (SID_BASE + 2)
#define SID_PW_HI      (SID_BASE + 3)
#define SID_CTRL       (SID_BASE + 4)
#define SID_AD         (SID_BASE + 5)
#define SID_SR         (SID_BASE + 6)
#define SID_VOLUME     (SID_BASE + 24)

// Note frequencies (high byte values for common notes)
// These are approximate SID frequency values
static const unsigned int note_freqs[] = {
    0,      // REST
    4291,   // C4
    4547,   // C#4
    4817,   // D4
    5103,   // D#4
    5407,   // E4
    5728,   // F4
    6069,   // F#4
    6430,   // G4
    6812,   // G#4
    7217,   // A4
    7647,   // A#4
    8101,   // B4
    8583,   // C5
    9094,   // C#5
    9634,   // D5
    10207,  // D#5
    10814,  // E5
    11457,  // F5
    12139,  // F#5
    12860,  // G5
};

// Note indices: 0=REST, 1=C4, 2=C#4, 3=D4, 4=D#4, 5=E4, 6=F4, 7=F#4, 8=G4, 9=G#4, 10=A4, 11=A#4, 12=B4, 13=C5, 14=C#5, 15=D5, 16=D#5, 17=E5, 18=F5, 19=F#5, 20=G5

// Jingle Bells melody (simplified)
// E E E | E E E | E G C D | E ...
#define NOTE_C4  1
#define NOTE_D4  3
#define NOTE_E4  5
#define NOTE_F4  6
#define NOTE_G4  8
#define NOTE_A4  10
#define NOTE_B4  12
#define NOTE_C5  13
#define NOTE_D5  15
#define NOTE_E5  17
#define NOTE_G5  20
#define REST     0

static const unsigned char melody[] = {
    // Jingle Bells - first part
    NOTE_E4, NOTE_E4, NOTE_E4, REST,
    NOTE_E4, NOTE_E4, NOTE_E4, REST,
    NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4,
    NOTE_E4, REST, REST, REST,
    NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,
    NOTE_F4, NOTE_E4, NOTE_E4, NOTE_E4,
    NOTE_E4, NOTE_D4, NOTE_D4, NOTE_E4,
    NOTE_D4, REST, NOTE_G4, REST,
    NOTE_E4, NOTE_E4, NOTE_E4, REST,
    NOTE_E4, NOTE_E4, NOTE_E4, REST,
    NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4,
    NOTE_E4, REST, REST, REST,
    NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,
    NOTE_F4, NOTE_E4, NOTE_E4, NOTE_E4,
    NOTE_G4, NOTE_G4, NOTE_F4, NOTE_D4,
    NOTE_C4, REST, REST, REST,
    0xFF  // End marker
};

static unsigned char melody_pos = 0;
static unsigned int note_timer = 0;
#define NOTE_DURATION 8

char data[40];
int sy = 25;
int sx = 40;

#define MAX_SNOW 30
#define SNOW_CHAR 255

int main()
{

    int i;
    int j;
    int k = 0;
    int offset;
    int delay;
    
    // Snow variables
    int snow_x[MAX_SNOW];
    int snow_y[MAX_SNOW];
    int snow_active[MAX_SNOW];
    unsigned char snow_pattern[] = {0x00, 0x08, 0x2A, 0x1C, 0x1C, 0x2A, 0x08, 0x00}; // Star/asterisk shape

    srand(time(NULL));

    // Initialize snow
    for(i=0; i<MAX_SNOW; i++) {
        snow_active[i] = 0;
        snow_x[i] = 0;
        snow_y[i] = 0;
    }

    // Set background and border color to black
    POKE(53281, 0);
    POKE(53272, 21);

    // Set character set to upper/lower case
    POKE(53272, (PEEK(53272) & 240) + 12);

    // Initialize character map (all 256 chars = 2048 bytes)
    // We copy all 256 chars; char 255 will be overwritten for snow below
    for (i = 0; i < 256 * 8; i++)
    {
        POKE(12288 + i, charmap[i]);
    }
    
    // Define Snow Character at index 255 (overwrites charmap[255])
    for(i=0; i<8; i++) {
        POKE(12288 + SNOW_CHAR*8 + i, snow_pattern[i]);
    }

    // Draw the image
    for (j = 0; j < 25; j++)
    {
        for (i = 0; i < 40; i++)
        {
            POKE(PLACE(i, j), img[k]);
            
            // Color logic: Top 5 rows get FCOLOR2, rest get FCOLOR1
            if (j>4)
            {
                POKE(COLOR(i, j), FCOLOR1);
            }
            else
            {
                POKE(COLOR(i, j), FCOLOR2);
            }

            k += 1;
        }
    }

    // Set border to a nice static color
    POKE(53280, 14); // Light blue border

    // Initialize SID for music
    POKE(SID_VOLUME, 15);      // Max volume
    POKE(SID_AD, 0x09);        // Attack=0, Decay=9
    POKE(SID_SR, 0x00);        // Sustain=0, Release=0
    POKE(SID_PW_LO, 0x00);     // Pulse width
    POKE(SID_PW_HI, 0x08);     // 50% duty cycle

    // Main loop: Snow animation
    while (1)
    {
        // Small delay for animation
        for(delay = 0; delay < 250; delay++);
        
        // --- Music Logic ---
        note_timer++;
        if(note_timer >= NOTE_DURATION) {
            note_timer = 0;
            
            // Get next note
            if(melody[melody_pos] == 0xFF) {
                melody_pos = 0;  // Loop the melody
            }
            
            if(melody[melody_pos] == REST) {
                // Silence - gate off
                POKE(SID_CTRL, 0x10);  // Gate off, pulse wave
            } else {
                // Play note
                unsigned int freq = note_freqs[melody[melody_pos]];
                POKE(SID_FREQ_LO, freq & 0xFF);
                POKE(SID_FREQ_HI, (freq >> 8) & 0xFF);
                POKE(SID_CTRL, 0x11);  // Gate on, pulse wave
            }
            melody_pos++;
        }
        // ------------------
        
        // --- Snow Logic ---
        // Spawn new snow at top
        if (rand() % 2 == 0) {
            for(i=0; i<MAX_SNOW; i++) {
                if(!snow_active[i]) {
                    snow_active[i] = 1;
                    snow_x[i] = rand() % 40;
                    snow_y[i] = 1; // Start at row 1 to avoid top edge issues
                    // Draw initial snow
                    POKE(PLACE(snow_x[i], snow_y[i]), SNOW_CHAR);
                    POKE(COLOR(snow_x[i], snow_y[i]), 1);
                    break;
                }
            }
        }

        // Update snow
        for(i=0; i<MAX_SNOW; i++) {
            if(snow_active[i]) {
                // Erase snow at current position - restore original image
                offset = snow_x[i] + snow_y[i] * 40;
                POKE(PLACE(snow_x[i], snow_y[i]), img[offset]);
                if (snow_y[i] > 4) {
                    POKE(COLOR(snow_x[i], snow_y[i]), FCOLOR1);
                } else {
                    POKE(COLOR(snow_x[i], snow_y[i]), FCOLOR2);
                }

                // Move down
                snow_y[i]++;
                
                // If it goes past the bottom, despawn (no accumulation)
                if(snow_y[i] > 24) {
                    snow_active[i] = 0;
                } else {
                    // Draw at new pos
                    POKE(PLACE(snow_x[i], snow_y[i]), SNOW_CHAR);
                    POKE(COLOR(snow_x[i], snow_y[i]), 1);
                }
            }
        }
        // ------------------
    }
    return 0;
}

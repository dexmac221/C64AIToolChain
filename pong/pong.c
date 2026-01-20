/*
 * Pong for C64 - Written in C using cc65
 * Following the AI Toolchain project guidelines
 */

#include <c64.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <joystick.h>

// Screen dimensions
#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 25

// Sprite pointers at end of screen RAM
#define SPRITE_PTRS ((unsigned char*)0x07F8)

// VIC-II registers
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X  (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y  (*(unsigned char*)0xD017)

// Sprite positions
#define SPR0_X (*(unsigned char*)0xD000)
#define SPR0_Y (*(unsigned char*)0xD001)
#define SPR1_X (*(unsigned char*)0xD002)
#define SPR1_Y (*(unsigned char*)0xD003)
#define SPR2_X (*(unsigned char*)0xD004)
#define SPR2_Y (*(unsigned char*)0xD005)

// Sprite colors
#define SPR0_COL (*(unsigned char*)0xD027)
#define SPR1_COL (*(unsigned char*)0xD028)
#define SPR2_COL (*(unsigned char*)0xD029)

// Colors
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
#define LTRED   10
#define GREY1   11
#define GREY2   12
#define LTGREEN 13
#define LTBLUE  14
#define GREY3   15

// Game states
#define STATE_TITLE  0
#define STATE_PLAY   1
#define STATE_SCORE  2
#define STATE_WON    3

// SID registers for sound
#define SID_FREQ_LO    (*(unsigned char*)0xD400)
#define SID_FREQ_HI    (*(unsigned char*)0xD401)
#define SID_PW_LO      (*(unsigned char*)0xD402)
#define SID_PW_HI      (*(unsigned char*)0xD403)
#define SID_CTRL       (*(unsigned char*)0xD404)
#define SID_AD         (*(unsigned char*)0xD405)
#define SID_SR         (*(unsigned char*)0xD406)
#define SID_VOLUME     (*(unsigned char*)0xD418)

// Sprite data location - use $3000 (block 192) which is safe for cc65
#define SPRITE_DATA 0x3000
#define SPRITE_BLOCK_PADDLE 192   // $3000 / 64 = 192
#define SPRITE_BLOCK_BALL   193   // $3040 / 64 = 193

// Play field boundaries (sprite coordinates)
#define FIELD_TOP     58    // Top boundary (below score area)
#define FIELD_BOTTOM  242   // Bottom boundary
#define FIELD_LEFT    24    // Left boundary
#define FIELD_RIGHT   320   // Right boundary (need MSB for > 255)

// Paddle positions (X coordinates in sprite coords)
#define PADDLE1_X     32    // Left paddle X position
#define PADDLE2_X     240   // Right paddle X position (fits in 8 bits)

// Ball speed
#define BALL_SPEED    2

// Game variables
static unsigned char paddle1_y;      // Left paddle Y (player 1)
static unsigned char paddle2_y;      // Right paddle Y (player 2 / CPU)
static unsigned char ball_x;         // Ball X position (low byte)
static unsigned char ball_x_msb;     // Ball X position MSB (for > 255)
static unsigned char ball_y;         // Ball Y position
static signed char ball_dx;          // Ball X velocity
static signed char ball_dy;          // Ball Y velocity
static unsigned char score1;         // Player 1 score
static unsigned char score2;         // Player 2 score
static unsigned char game_state;
static unsigned char frame_count;
static unsigned char serve_delay;    // Delay before serving ball
static unsigned char two_player;     // 0 = vs CPU, 1 = two players
static unsigned char winning_score;  // Score to win (default 11)

// Paddle sprite data - tall vertical bar (24 pixels high)
static const unsigned char paddle_sprite[63] = {
    // 24x21 sprite - draw a tall paddle in the center columns
    0x00, 0x18, 0x00,  // Row 0:  ...XX...
    0x00, 0x18, 0x00,  // Row 1
    0x00, 0x18, 0x00,  // Row 2
    0x00, 0x18, 0x00,  // Row 3
    0x00, 0x18, 0x00,  // Row 4
    0x00, 0x18, 0x00,  // Row 5
    0x00, 0x18, 0x00,  // Row 6
    0x00, 0x18, 0x00,  // Row 7
    0x00, 0x18, 0x00,  // Row 8
    0x00, 0x18, 0x00,  // Row 9
    0x00, 0x18, 0x00,  // Row 10
    0x00, 0x18, 0x00,  // Row 11
    0x00, 0x18, 0x00,  // Row 12
    0x00, 0x18, 0x00,  // Row 13
    0x00, 0x18, 0x00,  // Row 14
    0x00, 0x18, 0x00,  // Row 15
    0x00, 0x18, 0x00,  // Row 16
    0x00, 0x18, 0x00,  // Row 17
    0x00, 0x18, 0x00,  // Row 18
    0x00, 0x18, 0x00,  // Row 19
    0x00, 0x18, 0x00   // Row 20
};

// Ball sprite data - small 4x4 ball
static const unsigned char ball_sprite[63] = {
    0x00, 0x00, 0x00,  // Row 0
    0x00, 0x00, 0x00,  // Row 1
    0x00, 0x00, 0x00,  // Row 2
    0x00, 0x00, 0x00,  // Row 3
    0x00, 0x00, 0x00,  // Row 4
    0x00, 0x00, 0x00,  // Row 5
    0x00, 0x00, 0x00,  // Row 6
    0x00, 0x00, 0x00,  // Row 7
    0x00, 0x3C, 0x00,  // Row 8:  ..XXXX..
    0x00, 0x7E, 0x00,  // Row 9:  .XXXXXX.
    0x00, 0x7E, 0x00,  // Row 10: .XXXXXX.
    0x00, 0x3C, 0x00,  // Row 11: ..XXXX..
    0x00, 0x00, 0x00,  // Row 12
    0x00, 0x00, 0x00,  // Row 13
    0x00, 0x00, 0x00,  // Row 14
    0x00, 0x00, 0x00,  // Row 15
    0x00, 0x00, 0x00,  // Row 16
    0x00, 0x00, 0x00,  // Row 17
    0x00, 0x00, 0x00,  // Row 18
    0x00, 0x00, 0x00,  // Row 19
    0x00, 0x00, 0x00   // Row 20
};

// Wait for vertical blank (smooth animation)
void wait_vblank(void) {
    while (VIC.rasterline != 255) ;
}

// Initialize SID for sound effects
void init_sound(void) {
    SID_VOLUME = 15;    // Max volume
    SID_AD = 0x00;      // Fast attack, no decay
    SID_SR = 0xF0;      // Max sustain, fast release
}

// Play paddle hit sound (high blip)
void sound_paddle(void) {
    SID_FREQ_LO = 0x00;
    SID_FREQ_HI = 0x30;   // High pitch
    SID_CTRL = 0x21;      // Sawtooth, gate on
}

// Play wall hit sound (medium blip)
void sound_wall(void) {
    SID_FREQ_LO = 0x00;
    SID_FREQ_HI = 0x20;   // Medium pitch
    SID_CTRL = 0x11;      // Triangle, gate on
}

// Play score sound (low tone)
void sound_score(void) {
    SID_FREQ_LO = 0x00;
    SID_FREQ_HI = 0x10;   // Low pitch
    SID_CTRL = 0x21;      // Sawtooth, gate on
}

// Turn off sound
void sound_off(void) {
    SID_CTRL = 0x20;      // Gate off
}

// Copy sprite data to sprite memory
void init_sprites(void) {
    unsigned char* dest;
    unsigned char i;

    // Copy paddle sprite data to block 192 ($3000)
    dest = (unsigned char*)SPRITE_DATA;
    for (i = 0; i < 63; i++) {
        dest[i] = paddle_sprite[i];
    }

    // Copy ball sprite data to block 193 ($3040)
    dest = (unsigned char*)(SPRITE_DATA + 64);
    for (i = 0; i < 63; i++) {
        dest[i] = ball_sprite[i];
    }

    // Set sprite pointers
    SPRITE_PTRS[0] = SPRITE_BLOCK_PADDLE;  // Paddle 1
    SPRITE_PTRS[1] = SPRITE_BLOCK_PADDLE;  // Paddle 2
    SPRITE_PTRS[2] = SPRITE_BLOCK_BALL;    // Ball

    // Enable sprites 0-2
    VIC_SPR_ENA = 0x07;

    // Set colors
    SPR0_COL = WHITE;    // Paddle 1
    SPR1_COL = WHITE;    // Paddle 2
    SPR2_COL = YELLOW;   // Ball

    // No expansion or multicolor
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_MCOLOR = 0;
    VIC_SPR_HI_X = 0;
}

// Draw the playing field
void draw_field(void) {
    unsigned char y;
    unsigned char* screen = (unsigned char*)0x0400;
    unsigned char* colors = (unsigned char*)0xD800;

    // Clear screen
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    // Draw center line (dashed)
    for (y = 3; y < 24; y += 2) {
        screen[y * 40 + 20] = 0x7C;  // Vertical bar character
        colors[y * 40 + 20] = GREY2;
    }

    // Draw top and bottom boundaries
    for (y = 0; y < 40; y++) {
        screen[2 * 40 + y] = 0xC0;   // Horizontal line
        colors[2 * 40 + y] = LTBLUE;
        screen[24 * 40 + y] = 0xC0;  // Horizontal line
        colors[24 * 40 + y] = LTBLUE;
    }
}

// Initialize game positions
void init_positions(void) {
    // Paddles start in center vertically
    paddle1_y = 130;  // Center Y
    paddle2_y = 130;

    // Ball starts in center
    ball_x = 160;
    ball_x_msb = 0;
    ball_y = 150;

    // Random initial direction
    ball_dx = (rand() & 1) ? BALL_SPEED : -BALL_SPEED;
    ball_dy = (rand() & 1) ? BALL_SPEED : -BALL_SPEED;

    serve_delay = 60;  // 1 second delay before ball moves
}

// Update sprite positions on screen
void update_sprites(void) {
    // Paddle 1 (left)
    SPR0_X = PADDLE1_X;
    SPR0_Y = paddle1_y;

    // Paddle 2 (right)
    SPR1_X = PADDLE2_X;
    SPR1_Y = paddle2_y;

    // Ball
    SPR2_X = ball_x;
    SPR2_Y = ball_y;

    // Handle X MSB for ball if needed
    if (ball_x_msb) {
        VIC_SPR_HI_X |= 0x04;   // Set bit 2 for sprite 2
    } else {
        VIC_SPR_HI_X &= ~0x04;  // Clear bit 2
    }
}

// Draw score display
void draw_score(void) {
    gotoxy(8, 0);
    textcolor(WHITE);
    cprintf("P1: %02u", score1);

    gotoxy(28, 0);
    cprintf("P2: %02u", score2);

    // Show game mode
    if (!two_player) {
        gotoxy(17, 0);
        textcolor(CYAN);
        cprintf("CPU");
    }
}

// Read joystick input for paddle 1
void read_input_p1(void) {
    unsigned char joy;

    joy = joy_read(JOY_2);

    if (JOY_UP(joy)) {
        if (paddle1_y > FIELD_TOP + 10) {
            paddle1_y -= 3;
        }
    }
    if (JOY_DOWN(joy)) {
        if (paddle1_y < FIELD_BOTTOM - 21) {
            paddle1_y += 3;
        }
    }
}

// Read joystick input for paddle 2 (two player mode)
void read_input_p2(void) {
    unsigned char joy;

    joy = joy_read(JOY_1);

    if (JOY_UP(joy)) {
        if (paddle2_y > FIELD_TOP + 10) {
            paddle2_y -= 3;
        }
    }
    if (JOY_DOWN(joy)) {
        if (paddle2_y < FIELD_BOTTOM - 21) {
            paddle2_y += 3;
        }
    }
}

// Simple AI for CPU paddle
void cpu_ai(void) {
    signed char diff;
    unsigned char target_y;

    // AI tracks ball Y position
    target_y = ball_y;

    // Move every frame for faster reaction
    // Move towards ball
    diff = (signed char)(target_y - paddle2_y - 10);  // -10 to center on paddle

    if (diff > 2) {
        if (paddle2_y < FIELD_BOTTOM - 21) {
            paddle2_y += 3;  // Faster movement
        }
    } else if (diff < -2) {
        if (paddle2_y > FIELD_TOP + 10) {
            paddle2_y -= 3;  // Faster movement
        }
    }
}

// Move ball and handle collisions
void move_ball(void) {
    unsigned char new_y;
    int full_x;

    // Wait during serve delay
    if (serve_delay > 0) {
        serve_delay--;
        return;
    }

    // Calculate new position
    full_x = (int)ball_x + (ball_x_msb ? 256 : 0) + ball_dx;
    new_y = ball_y + ball_dy;

    // Wall collision (top/bottom)
    if (new_y <= FIELD_TOP || new_y >= FIELD_BOTTOM - 8) {
        ball_dy = -ball_dy;
        new_y = ball_y + ball_dy;
        sound_wall();
    }

    // Paddle 1 collision (left paddle)
    if (full_x <= PADDLE1_X + 8 && full_x >= PADDLE1_X - 4) {
        if (ball_y >= paddle1_y - 8 && ball_y <= paddle1_y + 21) {
            ball_dx = BALL_SPEED;  // Bounce right
            // Add spin based on paddle hit position
            if (ball_y < paddle1_y + 5) {
                ball_dy = -BALL_SPEED;
            } else if (ball_y > paddle1_y + 16) {
                ball_dy = BALL_SPEED;
            }
            full_x = PADDLE1_X + 9;
            sound_paddle();
        }
    }

    // Paddle 2 collision (right paddle)
    if (full_x >= PADDLE2_X - 4 && full_x <= PADDLE2_X + 8) {
        if (ball_y >= paddle2_y - 8 && ball_y <= paddle2_y + 21) {
            ball_dx = -BALL_SPEED;  // Bounce left
            // Add spin based on paddle hit position
            if (ball_y < paddle2_y + 5) {
                ball_dy = -BALL_SPEED;
            } else if (ball_y > paddle2_y + 16) {
                ball_dy = BALL_SPEED;
            }
            full_x = PADDLE2_X - 5;
            sound_paddle();
        }
    }

    // Scoring - ball goes off left side
    if (full_x < FIELD_LEFT) {
        score2++;
        sound_score();
        init_positions();
        ball_dx = BALL_SPEED;  // Serve towards player 2
        return;
    }

    // Scoring - ball goes off right side
    if (full_x > 310) {
        score1++;
        sound_score();
        init_positions();
        ball_dx = -BALL_SPEED;  // Serve towards player 1
        return;
    }

    // Update ball position
    if (full_x >= 256) {
        ball_x = (unsigned char)(full_x - 256);
        ball_x_msb = 1;
    } else if (full_x < 0) {
        ball_x = 0;
        ball_x_msb = 0;
    } else {
        ball_x = (unsigned char)full_x;
        ball_x_msb = 0;
    }
    ball_y = new_y;
}

// Check for winner
unsigned char check_winner(void) {
    if (score1 >= winning_score) {
        return 1;  // Player 1 wins
    }
    if (score2 >= winning_score) {
        return 2;  // Player 2 wins
    }
    return 0;  // No winner yet
}

// Draw title screen
void draw_title(void) {
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    gotoxy(16, 3);
    textcolor(YELLOW);
    cprintf("P O N G");

    gotoxy(10, 6);
    textcolor(WHITE);
    cprintf("FOR COMMODORE 64");

    gotoxy(7, 10);
    textcolor(CYAN);
    cprintf("1 - ONE PLAYER (VS CPU)");

    gotoxy(7, 12);
    cprintf("2 - TWO PLAYERS");

    gotoxy(8, 16);
    textcolor(GREEN);
    cprintf("FIRST TO 11 WINS!");

    gotoxy(6, 19);
    textcolor(GREY2);
    cprintf("PLAYER 1: JOYSTICK PORT 2");

    gotoxy(6, 20);
    cprintf("PLAYER 2: JOYSTICK PORT 1");

    gotoxy(5, 23);
    textcolor(GREY1);
    cprintf("AI TOOLCHAIN PROJECT 2024");
}

// Check for key press
unsigned char check_key(void) {
    if (kbhit()) {
        return cgetc();
    }
    return 0;
}

// Check for fire button
unsigned char check_fire(void) {
    unsigned char joy = joy_read(JOY_2);
    return JOY_FIRE(joy);
}

// Draw winner screen
void draw_winner(unsigned char winner) {
    gotoxy(12, 11);
    textcolor(YELLOW);
    if (winner == 1) {
        cprintf("PLAYER 1 WINS!");
    } else {
        if (two_player) {
            cprintf("PLAYER 2 WINS!");
        } else {
            cprintf("  CPU WINS!   ");
        }
    }

    gotoxy(10, 14);
    textcolor(WHITE);
    cprintf("PRESS FIRE TO CONTINUE");
}

// Main game loop
void game_loop(void) {
    unsigned char winner;

    while (1) {
        wait_vblank();
        frame_count++;

        // Turn off sound after short duration
        if ((frame_count & 7) == 0) {
            sound_off();
        }

        if (game_state == STATE_PLAY) {
            // Read player 1 input
            read_input_p1();

            // Read player 2 or CPU
            if (two_player) {
                read_input_p2();
            } else {
                cpu_ai();
            }

            // Move ball
            move_ball();

            // Update display
            update_sprites();
            draw_score();

            // Check for winner
            winner = check_winner();
            if (winner) {
                game_state = STATE_WON;
                draw_winner(winner);
            }
        } else if (game_state == STATE_WON) {
            // Wait for fire button to return to title
            if (check_fire()) {
                return;  // Return to title screen
            }
        }
    }
}

// Main entry point
int main(void) {
    unsigned char key;

    // Initialize
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    init_sound();
    init_sprites();

    // Install joystick driver
    joy_install(joy_static_stddrv);

    // Default settings
    winning_score = 11;

title_screen:
    // Show title screen
    VIC_SPR_ENA = 0;  // Hide sprites on title
    draw_title();

    // Wait for mode selection
    while (1) {
        waitvsync();
        key = check_key();

        if (key == '1') {
            two_player = 0;
            break;
        }
        if (key == '2') {
            two_player = 1;
            break;
        }
        // Also accept fire button for 1 player
        if (check_fire()) {
            two_player = 0;
            break;
        }
    }

    // Game setup
    score1 = 0;
    score2 = 0;
    game_state = STATE_PLAY;

    // Enable sprites
    VIC_SPR_ENA = 0x07;

    draw_field();
    init_positions();
    update_sprites();

    // Run game
    game_loop();

    // Return to title after game over
    goto title_screen;

    return 0;
}

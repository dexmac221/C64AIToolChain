/*
 * Pac-Man for C64 - Written in C using cc65
 * Much easier for AI to understand and modify!
 */

#include <c64.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <joystick.h>

// Screen dimensions
#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 25
#define MAZE_WIDTH    20
#define MAZE_HEIGHT   17
#define MAZE_OFFSET_X 10
#define MAZE_OFFSET_Y 3

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
#define SPR3_X (*(unsigned char*)0xD006)
#define SPR3_Y (*(unsigned char*)0xD007)
#define SPR4_X (*(unsigned char*)0xD008)
#define SPR4_Y (*(unsigned char*)0xD009)

// Sprite colors
#define SPR0_COL (*(unsigned char*)0xD027)
#define SPR1_COL (*(unsigned char*)0xD028)
#define SPR2_COL (*(unsigned char*)0xD029)
#define SPR3_COL (*(unsigned char*)0xD02A)
#define SPR4_COL (*(unsigned char*)0xD02B)

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

// Character codes (screen codes, not PETSCII)
#define CHAR_WALL   160  // Solid block (reverse space)
#define CHAR_DOT    46   // Period
#define CHAR_POWER  42   // Asterisk * for power pellet
#define CHAR_SPACE  32

// Directions
#define DIR_RIGHT 0
#define DIR_LEFT  1
#define DIR_UP    2
#define DIR_DOWN  3

// Game states
#define STATE_TITLE  0
#define STATE_PLAY   1
#define STATE_DYING  2
#define STATE_WON    3
#define STATE_LOST   4

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
#define SPRITE_BLOCK_PACMAN_OPEN_R 192   // $3000 / 64 = 192
#define SPRITE_BLOCK_PACMAN_OPEN_L 193   // $3040 / 64 = 193
#define SPRITE_BLOCK_PACMAN_OPEN_U 194   // $3080 / 64 = 194
#define SPRITE_BLOCK_PACMAN_OPEN_D 195   // $30C0 / 64 = 195
#define SPRITE_BLOCK_PACMAN_CLOSED 196   // $3100 / 64 = 196
#define SPRITE_BLOCK_GHOST         197   // $3140 / 64 = 197

// Screen to sprite coordinate conversion
// Sprite X = screen column * 8 + 24 (hardware offset)
// Sprite Y = screen row * 8 + 50 (hardware offset)
#define SPRITE_X(col) (24 + (col) * 8)
#define SPRITE_Y(row) (50 + (row) * 8)

// Maze position to sprite position
#define MAZE_SPR_X(mx) SPRITE_X(MAZE_OFFSET_X + (mx))
#define MAZE_SPR_Y(my) SPRITE_Y(MAZE_OFFSET_Y + (my))

// Game variables
static unsigned char pacman_x, pacman_y;
static unsigned char pacman_dir, pacman_next_dir;
static unsigned char ghost_x[4], ghost_y[4];
static unsigned char ghost_dir[4];
static unsigned char pacman_moved;
static unsigned int score;
static unsigned char lives;
static unsigned char dots_left;
static unsigned char game_state;
static unsigned char demo_mode;
static unsigned char frame_count;
static unsigned char power_timer;

// Maze data - 17 rows to fit screen (3 offset + 17 rows = 20, leaves 5 rows)
// Legend: # = wall, . = dot, o = power pellet, space = empty path
static const char* maze[MAZE_HEIGHT] = {
    "####################",  // Row 0
    "#........##........#",  // Row 1
    "#.##.###.##.###.##.#",  // Row 2
    "#o##.###.##.###.##o#",  // Row 3 - power pellets
    "#..................#",  // Row 4
    "#.##.#.######.#.##.#",  // Row 5
    "#....#...##...#....#",  // Row 6
    "####.###.##.###.####",  // Row 7
    "#........##........#",  // Row 8
    "####.###.##.###.####",  // Row 9
    "#........##........#",  // Row 10
    "#.##.###.##.###.##.#",  // Row 11
    "#o.#.....  .....#.o#",  // Row 12 - power pellets + center tunnel
    "##.#.##.####.##.#.##",  // Row 13
    "#..................#",  // Row 14
    "#.######.##.######.#",  // Row 15
    "####################"   // Row 16 - bottom wall
};

// Pac-Man bitmaps (8x8) stored in the first byte of each sprite row.
static const unsigned char pacman_open_r_8x8[8] = {
    0x3C, // ..XXXX..
    0x7E, // .XXXXXX.
    0xFC, // XXXXXX.. (mouth open right)
    0xF8, // XXXXX...
    0xF8, // XXXXX...
    0xFC, // XXXXXX..
    0x7E, // .XXXXXX.
    0x3C  // ..XXXX..
};

// More readable vertical mouth frames for the tiny 8x8 sprite
static const unsigned char pacman_open_u_8x8[8] = {
    0x00, // ........
    0x81, // X......X
    0xC3, // XX....XX
    0xE7, // XXX..XXX
    0xFF, // XXXXXXXX
    0xFF, // XXXXXXXX
    0x7E, // .XXXXXX.
    0x3C  // ..XXXX..
};

static const unsigned char pacman_open_d_8x8[8] = {
    0x3C, // ..XXXX..
    0x7E, // .XXXXXX.
    0xFF, // XXXXXXXX
    0xFF, // XXXXXXXX
    0xE7, // XXX..XXX
    0xC3, // XX....XX
    0x81, // X......X
    0x00  // ........
};

static const unsigned char pacman_closed_8x8[8] = {
    0x3C,
    0x7E,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0x7E,
    0x3C
};

static unsigned char reverse8(unsigned char v) {
    v = (v & 0xF0) >> 4 | (v & 0x0F) << 4;
    v = (v & 0xCC) >> 2 | (v & 0x33) << 2;
    v = (v & 0xAA) >> 1 | (v & 0x55) << 1;
    return v;
}

static void write_8x8_sprite(unsigned char* dest, const unsigned char rows[8]) {
    unsigned char r;
    unsigned char i;
    // Clear 63 bytes
    for (i = 0; i < 63; i++) dest[i] = 0;
    // Fill rows 0..7 in first byte of each row
    for (r = 0; r < 8; r++) {
        dest[r * 3] = rows[r];
    }
}

// Ghost sprite data - small 8x8 centered in 24x21 sprite
static const unsigned char ghost_sprite[] = {
    // Row 0-7: 8x8 Ghost in top-left of sprite
    0x3C, 0x00, 0x00,  // ..XXXX..
    0x7E, 0x00, 0x00,  // .XXXXXX.
    0xFF, 0x00, 0x00,  // XXXXXXXX
    0xDB, 0x00, 0x00,  // XX.XX.XX (eyes)
    0xFF, 0x00, 0x00,  // XXXXXXXX
    0xFF, 0x00, 0x00,  // XXXXXXXX
    0xFF, 0x00, 0x00,  // XXXXXXXX
    0xAA, 0x00, 0x00,  // X.X.X.X. (wavy bottom)
    // Rows 8-20: empty
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
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

// Play eat dot sound (short blip)
void sound_eat(void) {
    SID_FREQ_LO = 0x00;
    SID_FREQ_HI = 0x20;   // Medium pitch
    SID_CTRL = 0x21;      // Sawtooth, gate on
    // Sound will be turned off next frame
}

// Play power pellet sound (lower tone)
void sound_power(void) {
    SID_FREQ_LO = 0x00;
    SID_FREQ_HI = 0x10;   // Low pitch
    SID_CTRL = 0x11;      // Triangle, gate on
}

// Play death sound (descending)
void sound_die(void) {
    unsigned char i;
    for (i = 0x30; i > 0x05; i -= 2) {
        SID_FREQ_HI = i;
        SID_CTRL = 0x21;  // Sawtooth
        wait_vblank();
    }
    SID_CTRL = 0x20;      // Gate off
}

// Turn off sound
void sound_off(void) {
    SID_CTRL = 0x20;      // Gate off
}

// Copy sprite data to sprite memory
void init_sprites(void) {
    unsigned char* dest;
    unsigned char open_l[8];
    unsigned char i;
    
    // Derive directional open frames from open-right
    for (i = 0; i < 8; i++) {
        open_l[i] = reverse8(pacman_open_r_8x8[i]);
    }

    // Write Pac-Man open frames
    dest = (unsigned char*)SPRITE_DATA;              // block 192
    write_8x8_sprite(dest, pacman_open_r_8x8);
    dest = (unsigned char*)(SPRITE_DATA + 64);       // block 193
    write_8x8_sprite(dest, open_l);
    dest = (unsigned char*)(SPRITE_DATA + 128);      // block 194
    write_8x8_sprite(dest, pacman_open_u_8x8);
    dest = (unsigned char*)(SPRITE_DATA + 192);      // block 195
    write_8x8_sprite(dest, pacman_open_d_8x8);

    // Closed frame
    dest = (unsigned char*)(SPRITE_DATA + 256);      // block 196
    write_8x8_sprite(dest, pacman_closed_8x8);

    // Ghost sprite data (unchanged)
    dest = (unsigned char*)(SPRITE_DATA + 320);      // block 197
    for (i = 0; i < 63; i++) {
        dest[i] = (i < sizeof(ghost_sprite)) ? ghost_sprite[i] : 0;
    }
    
    // Set sprite pointers - using block numbers for $3000 area
    SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_OPEN_R;  // Pac-Man open (right) at $3000
    SPRITE_PTRS[1] = SPRITE_BLOCK_GHOST;   // Ghost 1 at $3080
    SPRITE_PTRS[2] = SPRITE_BLOCK_GHOST;   // Ghost 2
    SPRITE_PTRS[3] = SPRITE_BLOCK_GHOST;   // Ghost 3
    SPRITE_PTRS[4] = SPRITE_BLOCK_GHOST;   // Ghost 4
    
    // Enable sprites 0-4
    VIC_SPR_ENA = 0x1F;
    
    // Set colors
    SPR0_COL = YELLOW;   // Pac-Man
    SPR1_COL = RED;      // Blinky
    SPR2_COL = PURPLE;   // Pinky
    SPR3_COL = CYAN;     // Inky
    SPR4_COL = ORANGE;   // Clyde
    
    // No expansion
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_MCOLOR = 0;
    VIC_SPR_HI_X = 0;
}

// Draw the maze on screen
void draw_maze(void) {
    unsigned char x, y;
    char c;
    unsigned char screen_char;
    unsigned char color;
    unsigned char* screen = (unsigned char*)0x0400;
    unsigned char* colors = (unsigned char*)0xD800;
    unsigned int pos;
    
    // Clear screen first (removes title screen text)
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);
    
    dots_left = 0;
    
    for (y = 0; y < MAZE_HEIGHT; y++) {
        for (x = 0; x < MAZE_WIDTH; x++) {
            c = maze[y][x];
            pos = (y + MAZE_OFFSET_Y) * 40 + (x + MAZE_OFFSET_X);
            
            switch (c) {
                case '#':
                    screen_char = CHAR_WALL;
                    color = LTBLUE;
                    break;
                case '.':
                    screen_char = CHAR_DOT;
                    color = WHITE;
                    dots_left++;
                    break;
                case 'o':
                    screen_char = CHAR_POWER;
                    color = WHITE;
                    dots_left++;
                    break;
                default:
                    screen_char = CHAR_SPACE;
                    color = BLACK;
                    break;
            }
            
            screen[pos] = screen_char;
            colors[pos] = color;
        }
    }
}

// Initialize game positions
void init_positions(void) {
    // Pac-Man starts at row 14, column 9 (bottom open corridor)
    // Row 14 is "#..................#" - open corridor
    pacman_x = MAZE_SPR_X(9);   // Column 9 (center)
    pacman_y = MAZE_SPR_Y(14);  // Row 14
    pacman_dir = DIR_RIGHT;
    pacman_next_dir = DIR_RIGHT;
    pacman_moved = 0;
    
    // Ghosts start at row 4 (open corridor)
    // Row 4 is "#..................#"
    ghost_x[0] = MAZE_SPR_X(4);  ghost_y[0] = MAZE_SPR_Y(4);
    ghost_x[1] = MAZE_SPR_X(8);  ghost_y[1] = MAZE_SPR_Y(4);
    ghost_x[2] = MAZE_SPR_X(11); ghost_y[2] = MAZE_SPR_Y(4);
    ghost_x[3] = MAZE_SPR_X(15); ghost_y[3] = MAZE_SPR_Y(4);
    
    // Ghosts move in different directions
    ghost_dir[0] = DIR_RIGHT;
    ghost_dir[1] = DIR_DOWN;
    ghost_dir[2] = DIR_DOWN;
    ghost_dir[3] = DIR_LEFT;
}

// Update sprite positions on screen
void update_sprites(void) {
    // Pac-Man (sprite 0) - game coords are already screen coords
    SPR0_X = pacman_x;
    SPR0_Y = pacman_y;
    
    // Ghosts (sprites 1-4)
    SPR1_X = ghost_x[0];
    SPR1_Y = ghost_y[0];
    SPR2_X = ghost_x[1];
    SPR2_Y = ghost_y[1];
    SPR3_X = ghost_x[2];
    SPR3_Y = ghost_y[2];
    SPR4_X = ghost_x[3];
    SPR4_Y = ghost_y[3];
    
    // Ensure sprite 0 is still enabled (debugging)
    VIC_SPR_ENA |= 0x01;
}

static unsigned char is_near_grid(unsigned char coord, unsigned char base, unsigned char tol) {
    unsigned char m = (coord - base) & 7;
    return (m <= tol) || (m >= (unsigned char)(8 - tol));
}

static unsigned char snap_to_grid(unsigned char coord, unsigned char base) {
    unsigned int rel;
    if (coord < base) return coord;
    rel = (unsigned int)(coord - base);
    rel = (rel + 4) & 0xF8;
    return (unsigned char)(base + (unsigned char)rel);
}

static unsigned char opposite_dir(unsigned char dir) {
    return dir ^ 1;
}

// Get maze character at sprite position
static char get_maze_at(int sx, int sy) {
    unsigned char mx, my;
    int base_x, base_y;
    
    // Calculate base sprite coordinates for maze origin
    base_x = MAZE_SPR_X(0);  // = 24 + 10*8 = 104
    base_y = MAZE_SPR_Y(0);  // = 50 + 3*8 = 74
    
    // Bounds check
    if (sx < base_x || sy < base_y) return '#';
    
    // Convert sprite coords to maze cell (add 4 to center in cell)
    mx = (unsigned char)((sx - base_x + 4) / 8);
    my = (unsigned char)((sy - base_y + 4) / 8);
    
    if (mx >= MAZE_WIDTH || my >= MAZE_HEIGHT) {
        return '#';  // Out of bounds = wall
    }
    
    return maze[my][mx];
}

// Check if position is walkable
unsigned char can_move(unsigned char x, unsigned char y, unsigned char dir) {
    int nx = x, ny = y;
    char c;
    
    // Look 6 pixels ahead to check next cell
    switch (dir) {
        case DIR_RIGHT: nx += 6; break;
        case DIR_LEFT:  nx -= 6; break;
        case DIR_UP:    ny -= 6; break;
        case DIR_DOWN:  ny += 6; break;
    }
    
    c = get_maze_at(nx, ny);
    // Can walk on: dots, power pellets, empty space, ghost area
    return (c == '.' || c == 'o' || c == ' ' || c == 'G');
}

// Demo AI - simple random movement
void demo_ai(void) {
    static unsigned char timer = 0;
    unsigned char new_dir;
    
    timer++;
    if (timer < 20) return;
    timer = 0;
    
    // Try a random direction
    new_dir = rand() & 3;
    
    if (can_move(pacman_x, pacman_y, new_dir)) {
        pacman_next_dir = new_dir;
    }
}

// Move Pac-Man
void move_pacman(void) {
    unsigned char base_x = MAZE_SPR_X(0);
    unsigned char base_y = MAZE_SPR_Y(0);
    unsigned char snapped;

    // Keep Pac-Man aligned on the perpendicular axis when close to the grid
    if (pacman_dir == DIR_LEFT || pacman_dir == DIR_RIGHT) {
        if (is_near_grid(pacman_y, base_y, 2)) {
            pacman_y = snap_to_grid(pacman_y, base_y);
        }
    } else {
        if (is_near_grid(pacman_x, base_x, 2)) {
            pacman_x = snap_to_grid(pacman_x, base_x);
        }
    }

    // Try to change to desired direction
    if (pacman_next_dir != pacman_dir) {
        // If turning perpendicular, only allow if we're near the grid line and snap before turning
        if ((pacman_dir ^ pacman_next_dir) & 2) {
            if (pacman_next_dir == DIR_UP || pacman_next_dir == DIR_DOWN) {
                if (is_near_grid(pacman_x, base_x, 2)) {
                    snapped = snap_to_grid(pacman_x, base_x);
                    if (can_move(snapped, pacman_y, pacman_next_dir)) {
                        pacman_x = snapped;
                        pacman_dir = pacman_next_dir;
                    }
                }
            } else {
                if (is_near_grid(pacman_y, base_y, 2)) {
                    snapped = snap_to_grid(pacman_y, base_y);
                    if (can_move(pacman_x, snapped, pacman_next_dir)) {
                        pacman_y = snapped;
                        pacman_dir = pacman_next_dir;
                    }
                }
            }
        } else if (can_move(pacman_x, pacman_y, pacman_next_dir)) {
            // Same axis (continue straight / reverse)
            pacman_dir = pacman_next_dir;
        }
    }
    
    // Move in current direction if possible
    if (can_move(pacman_x, pacman_y, pacman_dir)) {
        pacman_moved = 1;
        switch (pacman_dir) {
            case DIR_RIGHT: pacman_x += 2; break;
            case DIR_LEFT:  pacman_x -= 2; break;
            case DIR_UP:    pacman_y -= 2; break;
            case DIR_DOWN:  pacman_y += 2; break;
        }
    } else {
        pacman_moved = 0;
    }
}

static void animate_pacman(void) {
    // Toggle open/closed while moving; closed when stopped
    if (!pacman_moved) {
        SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_CLOSED;
        return;
    }

    if (!(frame_count & 4)) {
        SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_CLOSED;
        return;
    }

    switch (pacman_dir) {
        case DIR_LEFT:  SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_OPEN_L; break;
        case DIR_UP:    SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_OPEN_U; break;
        case DIR_DOWN:  SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_OPEN_D; break;
        case DIR_RIGHT:
        default:        SPRITE_PTRS[0] = SPRITE_BLOCK_PACMAN_OPEN_R; break;
    }
}

// Check and eat dot at Pac-Man's position (only when centered on cell)
void eat_dot(void) {
    unsigned char mx, my;
    unsigned char base_x, base_y;
    unsigned int screen_pos;
    unsigned char* screen = (unsigned char*)0x0400;
    unsigned int rel;
    
    // Get maze cell under Pac-Man
    base_x = MAZE_SPR_X(0);
    base_y = MAZE_SPR_Y(0);
    
    if (pacman_x < base_x || pacman_y < base_y) return;

    // Use the sprite center (like the ASM version) so eating works even if slightly off-grid
    rel = (unsigned int)(pacman_x - base_x) + 4;
    mx = (unsigned char)(rel >> 3);
    rel = (unsigned int)(pacman_y - base_y) + 4;
    my = (unsigned char)(rel >> 3);
    
    if (mx >= MAZE_WIDTH || my >= MAZE_HEIGHT) return;
    
    // Check if this cell has a dot on screen (not already eaten)
    screen_pos = (my + MAZE_OFFSET_Y) * 40 + (mx + MAZE_OFFSET_X);
    
    if (screen[screen_pos] == CHAR_DOT) {
        screen[screen_pos] = CHAR_SPACE;
        score += 10;
        dots_left--;
        sound_eat();
    } else if (screen[screen_pos] == CHAR_POWER) {
        screen[screen_pos] = CHAR_SPACE;
        score += 50;
        dots_left--;
        power_timer = 200;
        sound_power();
    }
    
    // Check win condition
    if (dots_left == 0) {
        game_state = STATE_WON;
    }
}

// Check collision between Pac-Man and ghosts
void check_ghost_collision(void) {
    unsigned char i;
    unsigned char dx, dy;
    
    for (i = 0; i < 4; i++) {
        // Calculate distance
        dx = (pacman_x > ghost_x[i]) ? (pacman_x - ghost_x[i]) : (ghost_x[i] - pacman_x);
        dy = (pacman_y > ghost_y[i]) ? (pacman_y - ghost_y[i]) : (ghost_y[i] - pacman_y);
        
        // Collision if within 6 pixels
        if (dx < 6 && dy < 6) {
            if (power_timer > 0) {
                // Eat ghost! Send back to ghost house
                ghost_x[i] = MAZE_SPR_X(9);
                ghost_y[i] = MAZE_SPR_Y(11);
                score += 200;
            } else {
                // Pac-Man dies
                game_state = STATE_DYING;
                return;
            }
        }
    }
}

// Handle Pac-Man death
void handle_death(void) {
    sound_die();
    lives--;
    
    if (lives == 0) {
        game_state = STATE_LOST;
    } else {
        // Reset positions, continue playing
        init_positions();
        update_sprites();
        game_state = STATE_PLAY;
    }
}

// Move ghosts with grid-aligned movement
void move_ghosts(void) {
    unsigned char i;
    unsigned char new_dir;
    unsigned char start;
    unsigned char k;
    unsigned char base_x, base_y;
    unsigned char on_grid;
    unsigned char snapped;
    
    base_x = MAZE_SPR_X(0);
    base_y = MAZE_SPR_Y(0);
    
    for (i = 0; i < 4; i++) {
        // Keep ghosts aligned on the perpendicular axis when close to the grid
        if (ghost_dir[i] == DIR_LEFT || ghost_dir[i] == DIR_RIGHT) {
            if (is_near_grid(ghost_y[i], base_y, 2)) {
                ghost_y[i] = snap_to_grid(ghost_y[i], base_y);
            }
        } else {
            if (is_near_grid(ghost_x[i], base_x, 2)) {
                ghost_x[i] = snap_to_grid(ghost_x[i], base_x);
            }
        }

        // Check if ghost is on grid (aligned to 8-pixel boundary)
        on_grid = (((ghost_x[i] - base_x) & 7) == 0) && 
                  (((ghost_y[i] - base_y) & 7) == 0);
        
        // Only change direction when on grid
        if (on_grid) {
            // Check if current direction is blocked
            if (!can_move(ghost_x[i], ghost_y[i], ghost_dir[i])) {
                // Try directions in a pseudo-random order, avoiding immediate reverse if possible
                start = rand() & 3;
                for (k = 0; k < 4; k++) {
                    new_dir = (start + k) & 3;
                    if (new_dir == opposite_dir(ghost_dir[i])) continue;
                    if (can_move(ghost_x[i], ghost_y[i], new_dir)) {
                        ghost_dir[i] = new_dir;
                        break;
                    }
                }
                // If only reverse works, allow it
                if (!can_move(ghost_x[i], ghost_y[i], ghost_dir[i])) {
                    new_dir = opposite_dir(ghost_dir[i]);
                    if (can_move(ghost_x[i], ghost_y[i], new_dir)) {
                        ghost_dir[i] = new_dir;
                    }
                }
            }
            // Occasionally change direction at intersections
            else if ((rand() & 31) == 0) {
                new_dir = rand() & 3;
                if (new_dir != opposite_dir(ghost_dir[i]) && can_move(ghost_x[i], ghost_y[i], new_dir)) {
                    ghost_dir[i] = new_dir;
                }
            }
        }
        // Recovery: if blocked while off-grid, snap and pick a new direction anyway
        if (!can_move(ghost_x[i], ghost_y[i], ghost_dir[i])) {
            if (is_near_grid(ghost_x[i], base_x, 2)) {
                snapped = snap_to_grid(ghost_x[i], base_x);
                ghost_x[i] = snapped;
            }
            if (is_near_grid(ghost_y[i], base_y, 2)) {
                snapped = snap_to_grid(ghost_y[i], base_y);
                ghost_y[i] = snapped;
            }
            start = rand() & 3;
            for (k = 0; k < 4; k++) {
                new_dir = (start + k) & 3;
                if (can_move(ghost_x[i], ghost_y[i], new_dir)) {
                    ghost_dir[i] = new_dir;
                    break;
                }
            }
        }
        
        // Move ghost (2 pixels per frame like Pac-Man)
        if (can_move(ghost_x[i], ghost_y[i], ghost_dir[i])) {
            switch (ghost_dir[i]) {
                case DIR_RIGHT: ghost_x[i] += 2; break;
                case DIR_LEFT:  ghost_x[i] -= 2; break;
                case DIR_UP:    ghost_y[i] -= 2; break;
                case DIR_DOWN:  ghost_y[i] += 2; break;
            }
        }
    }
}

// Draw score
void draw_score(void) {
    gotoxy(1, 0);
    textcolor(WHITE);
    cprintf("SCORE: %04u", score);
    
    if (demo_mode) {
        gotoxy(17, 0);
        textcolor(GREEN);
        cprintf("DEMO");
    }
    
    gotoxy(35, 0);
    textcolor(YELLOW);
    cprintf("LIVES:%u", lives);
}

// Draw title screen
void draw_title(void) {
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);
    
    gotoxy(12, 5);
    textcolor(YELLOW);
    cprintf("P A C - M A N");
    
    gotoxy(10, 8);
    textcolor(WHITE);
    cprintf("FOR COMMODORE 64");
    
    gotoxy(8, 12);
    textcolor(CYAN);
    cprintf("PRESS FIRE TO START");
    
    gotoxy(10, 14);
    textcolor(GREEN);
    cprintf("OR WAIT FOR DEMO");
    
    gotoxy(6, 18);
    textcolor(GREY2);
    cprintf("USE JOYSTICK PORT 2");
    
    gotoxy(5, 22);
    textcolor(GREY1);
    cprintf("AI TOOLCHAIN PROJECT 2024");
}

// Read joystick input
void read_input(void) {
    unsigned char joy;
    
    if (demo_mode) {
        demo_ai();
        return;
    }
    
    joy = joy_read(JOY_2);
    
    if (JOY_RIGHT(joy)) pacman_next_dir = DIR_RIGHT;
    if (JOY_LEFT(joy))  pacman_next_dir = DIR_LEFT;
    if (JOY_UP(joy))    pacman_next_dir = DIR_UP;
    if (JOY_DOWN(joy))  pacman_next_dir = DIR_DOWN;
}

// Check for fire button to start game
unsigned char check_fire(void) {
    unsigned char joy = joy_read(JOY_2);
    return JOY_FIRE(joy);
}

// Main game loop
void game_loop(void) {
    while (1) {
        wait_vblank();
        frame_count++;
        
        // Turn off sound after short duration
        if ((frame_count & 3) == 0) {
            sound_off();
        }
        
        // Update power timer
        if (power_timer > 0) {
            power_timer--;
            // Flash ghost colors when power is running out
            if (power_timer < 60 && (frame_count & 4)) {
                SPR1_COL = WHITE;
                SPR2_COL = WHITE;
                SPR3_COL = WHITE;
                SPR4_COL = WHITE;
            } else if (power_timer > 0) {
                // Ghosts are blue (vulnerable)
                SPR1_COL = BLUE;
                SPR2_COL = BLUE;
                SPR3_COL = BLUE;
                SPR4_COL = BLUE;
            }
        } else {
            // Normal ghost colors
            SPR1_COL = RED;
            SPR2_COL = PURPLE;
            SPR3_COL = CYAN;
            SPR4_COL = ORANGE;
        }
        
        if (game_state == STATE_PLAY) {
            read_input();
            
            // Move every other frame for smoother animation
            if (frame_count & 1) {
                move_pacman();
                eat_dot();
            }
            animate_pacman();
            
            move_ghosts();
            check_ghost_collision();
            update_sprites();
            draw_score();
        } else if (game_state == STATE_DYING) {
            handle_death();
        } else if (game_state == STATE_WON) {
            // Level complete!
            gotoxy(15, 12);
            textcolor(YELLOW);
            cprintf("YOU WIN!");
            // Wait then restart level
            if (frame_count == 0) {  // Every 256 frames (~5 seconds)
                if (demo_mode) {
                    return;  // Return to title
                }
                draw_maze();
                init_positions();
                game_state = STATE_PLAY;
            }
        } else if (game_state == STATE_LOST) {
            gotoxy(14, 12);
            textcolor(RED);
            cprintf("GAME OVER");
            // Wait then return to title
            if (frame_count == 0) {
                return;  // Return to title screen
            }
        }
        
        // In demo mode, fire button starts game
        if (demo_mode && check_fire()) {
            return;  // Return to title, then player can start
        }
    }
}

// Main entry point
int main(void) {
    unsigned int title_timer;
    
    // Clear screen
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);
    
    // Initialize
    init_sound();
    init_sprites();
    
    // Install joystick driver
    joy_install(joy_static_stddrv);
    
title_screen:
    // Show title screen
    draw_title();
    
    // Wait for fire button or timeout for demo
    title_timer = 0;
    while (!check_fire() && title_timer < 300) {  // ~5 seconds
        waitvsync();
        ++title_timer;
    }
    
    // Game setup
    score = 0;
    lives = 3;
    power_timer = 0;
    game_state = STATE_PLAY;
    
    if (check_fire()) {
        demo_mode = 0;  // Player mode
    } else {
        demo_mode = 1;  // Demo mode
    }
    
    draw_maze();
    init_positions();
    update_sprites();
    
    // Run game
    game_loop();
    
    // Return to title after game over
    goto title_screen;
    
    return 0;
}

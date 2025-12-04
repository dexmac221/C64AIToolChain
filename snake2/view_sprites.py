#!/usr/bin/env python3
"""
C64 Sprite Viewer - Visualize sprite data from assembly source
"""

# Snake Head Sprite data (from snake.s)
sprite_head = [
    0b00000111, 0b11100000, 0b00000000,   # Row 0
    0b00011111, 0b11111000, 0b00000000,   # Row 1
    0b00111111, 0b11111100, 0b00000000,   # Row 2
    0b01111111, 0b11111110, 0b00000000,   # Row 3
    0b01111111, 0b11111110, 0b00000000,   # Row 4
    0b11111111, 0b11111111, 0b00000000,   # Row 5
    0b11111111, 0b11111111, 0b00000000,   # Row 6
    0b11110011, 0b11001111, 0b00000000,   # Row 7 - eyes
    0b11110011, 0b11001111, 0b00000000,   # Row 8 - eyes
    0b11111111, 0b11111111, 0b00000000,   # Row 9
    0b11111111, 0b11111111, 0b00000000,   # Row 10
    0b11111111, 0b11111111, 0b00000000,   # Row 11
    0b11111100, 0b00111111, 0b00000000,   # Row 12 - mouth
    0b01111111, 0b11111110, 0b00000000,   # Row 13
    0b01111111, 0b11111110, 0b00000000,   # Row 14
    0b00111111, 0b11111100, 0b00000000,   # Row 15
    0b00011111, 0b11111000, 0b00000000,   # Row 16
    0b00000111, 0b11100000, 0b00000000,   # Row 17
    0b00000000, 0b00000000, 0b00000000,   # Row 18
    0b00000000, 0b00000000, 0b00000000,   # Row 19
    0b00000000, 0b00000000, 0b00000000,   # Row 20
]

sprite_apple = [
    0b00000001, 0b10000000, 0b00000000,   # Row 0 - stem
    0b00000011, 0b11000000, 0b00000000,   # Row 1 - stem
    0b00000111, 0b11100000, 0b00000000,   # Row 2
    0b00011111, 0b11111000, 0b00000000,   # Row 3
    0b00111111, 0b11111100, 0b00000000,   # Row 4
    0b01111111, 0b11111110, 0b00000000,   # Row 5
    0b01111111, 0b11111110, 0b00000000,   # Row 6
    0b11111111, 0b11111111, 0b00000000,   # Row 7
    0b11111111, 0b11111111, 0b00000000,   # Row 8
    0b11111111, 0b11111111, 0b00000000,   # Row 9
    0b11111111, 0b11111111, 0b00000000,   # Row 10
    0b11111111, 0b11111111, 0b00000000,   # Row 11
    0b11111111, 0b11111111, 0b00000000,   # Row 12
    0b01111111, 0b11111110, 0b00000000,   # Row 13
    0b01111111, 0b11111110, 0b00000000,   # Row 14
    0b00111111, 0b11111100, 0b00000000,   # Row 15
    0b00011111, 0b11111000, 0b00000000,   # Row 16
    0b00000111, 0b11100000, 0b00000000,   # Row 17
    0b00000000, 0b00000000, 0b00000000,   # Row 18
    0b00000000, 0b00000000, 0b00000000,   # Row 19
    0b00000000, 0b00000000, 0b00000000,   # Row 20
]

def display_sprite(name, data):
    """Display sprite as ASCII art"""
    print(f"\n{'='*26}")
    print(f" {name}")
    print(f"{'='*26}")
    
    for row in range(21):
        line = ""
        for byte_idx in range(3):
            byte_val = data[row * 3 + byte_idx]
            for bit in range(8):
                if byte_val & (1 << (7 - bit)):
                    line += "██"
                else:
                    line += "  "
        print(f"|{line}|")
    print(f"{'='*26}")

# Display current sprites
display_sprite("SNAKE HEAD (Current)", sprite_head)
display_sprite("APPLE (Current)", sprite_apple)

# Better designed sprites
print("\n\n" + "="*50)
print(" IMPROVED SPRITE DESIGNS")
print("="*50)

# Better Snake Head - with tongue and more snake-like
better_snake_head = [
    0b00001111, 0b11110000, 0b00000000,   # Row 0
    0b00111111, 0b11111100, 0b00000000,   # Row 1
    0b01111111, 0b11111110, 0b00000000,   # Row 2
    0b01111111, 0b11111110, 0b00000000,   # Row 3
    0b11111111, 0b11111111, 0b00000000,   # Row 4
    0b11100111, 0b11100111, 0b00000000,   # Row 5 - eyes
    0b11100111, 0b11100111, 0b00000000,   # Row 6 - eyes
    0b11111111, 0b11111111, 0b00000000,   # Row 7
    0b11111111, 0b11111111, 0b00000000,   # Row 8
    0b11111111, 0b11111111, 0b00000000,   # Row 9
    0b01111111, 0b11111110, 0b00000000,   # Row 10
    0b01111111, 0b11111110, 0b00000000,   # Row 11
    0b00111111, 0b11111100, 0b00000000,   # Row 12
    0b00011111, 0b11111000, 0b00000000,   # Row 13
    0b00001111, 0b11110000, 0b00000000,   # Row 14
    0b00000111, 0b11100000, 0b00000000,   # Row 15
    0b00000011, 0b11000000, 0b00000000,   # Row 16
    0b00000001, 0b10000000, 0b00000000,   # Row 17 - tongue start
    0b00000011, 0b11000000, 0b00000000,   # Row 18 - tongue fork
    0b00000010, 0b01000000, 0b00000000,   # Row 19 - tongue fork
    0b00000000, 0b00000000, 0b00000000,   # Row 20
]

# Better Apple - with leaf and stem
better_apple = [
    0b00000000, 0b01100000, 0b00000000,   # Row 0 - leaf
    0b00000000, 0b11110000, 0b00000000,   # Row 1 - leaf
    0b00000001, 0b11100000, 0b00000000,   # Row 2 - stem
    0b00000001, 0b10000000, 0b00000000,   # Row 3 - stem
    0b00000111, 0b11100000, 0b00000000,   # Row 4
    0b00011111, 0b11111000, 0b00000000,   # Row 5
    0b00111111, 0b11111100, 0b00000000,   # Row 6
    0b01111111, 0b11111110, 0b00000000,   # Row 7
    0b01111111, 0b11111110, 0b00000000,   # Row 8
    0b11111111, 0b11111111, 0b00000000,   # Row 9
    0b11111111, 0b11111111, 0b00000000,   # Row 10
    0b11111111, 0b11111111, 0b00000000,   # Row 11
    0b11111111, 0b11111111, 0b00000000,   # Row 12
    0b11111111, 0b11111111, 0b00000000,   # Row 13
    0b01111111, 0b11111110, 0b00000000,   # Row 14
    0b01111111, 0b11111110, 0b00000000,   # Row 15
    0b00111111, 0b11111100, 0b00000000,   # Row 16
    0b00011111, 0b11111000, 0b00000000,   # Row 17
    0b00000111, 0b11100000, 0b00000000,   # Row 18
    0b00000000, 0b00000000, 0b00000000,   # Row 19
    0b00000000, 0b00000000, 0b00000000,   # Row 20
]

# Body segment - smaller circle
better_body = [
    0b00000000, 0b00000000, 0b00000000,   # Row 0
    0b00000000, 0b00000000, 0b00000000,   # Row 1
    0b00000000, 0b00000000, 0b00000000,   # Row 2
    0b00000111, 0b11100000, 0b00000000,   # Row 3
    0b00011111, 0b11111000, 0b00000000,   # Row 4
    0b00111111, 0b11111100, 0b00000000,   # Row 5
    0b01111111, 0b11111110, 0b00000000,   # Row 6
    0b01111111, 0b11111110, 0b00000000,   # Row 7
    0b11111111, 0b11111111, 0b00000000,   # Row 8
    0b11111111, 0b11111111, 0b00000000,   # Row 9
    0b11111111, 0b11111111, 0b00000000,   # Row 10
    0b11111111, 0b11111111, 0b00000000,   # Row 11
    0b01111111, 0b11111110, 0b00000000,   # Row 12
    0b01111111, 0b11111110, 0b00000000,   # Row 13
    0b00111111, 0b11111100, 0b00000000,   # Row 14
    0b00011111, 0b11111000, 0b00000000,   # Row 15
    0b00000111, 0b11100000, 0b00000000,   # Row 16
    0b00000000, 0b00000000, 0b00000000,   # Row 17
    0b00000000, 0b00000000, 0b00000000,   # Row 18
    0b00000000, 0b00000000, 0b00000000,   # Row 19
    0b00000000, 0b00000000, 0b00000000,   # Row 20
]

display_sprite("SNAKE HEAD (Improved)", better_snake_head)
display_sprite("APPLE (Improved)", better_apple)
display_sprite("BODY SEGMENT", better_body)

# Generate assembly code
def generate_asm(name, data):
    print(f"\n; {name}")
    print(f"sprite_{name.lower().replace(' ', '_')}:")
    for row in range(21):
        bytes_str = ", ".join([f"%{data[row*3+i]:08b}" for i in range(3)])
        print(f"    .byte {bytes_str}   ; Row {row}")
    print(f"    .byte 0                                  ; Padding")

print("\n\n" + "="*50)
print(" ASSEMBLY CODE FOR IMPROVED SPRITES")
print("="*50)

generate_asm("head", better_snake_head)
generate_asm("apple", better_apple)
generate_asm("body", better_body)

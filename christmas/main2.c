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

#define FCOLOR 6

char data[40];
int sy = 25;
int sx = 40;

int main()
{

    int i;
    int j;
    int k = 0;
    int random_val = 1;
    int value = 0;
    int counter = 0;

    srand(time(NULL));

    POKE(53281, 0);
    POKE(53272, 21);

    POKE(53272, (PEEK(53272) & 240) + 12);

    for (i = 0; i < 254 * 8; i++)
    {
        POKE(12288 + i, charmap[i]);
    }

    for (j = 0; j < 25; j++)
    {
        for (i = 0; i < 40; i++)
        {
            POKE(PLACE(i, j), img[k]);
            POKE(COLOR(i, j), FCOLOR);
            k += 1;
        }
    }

    while (1)
    {
        if (counter == 1000)
        {
            value = rand() % 5;
            counter = 0;
        }

        for (k = 0; k < 16; k++)
        {
            // if(k==FCOLOR)
            //	continue;

            POKE(53281, k);
            POKE(53280, k);

            switch (value)
            {
            case 0:
                random_val = 1;
                break;

            case 1:
                random_val = 10;
                break;

            case 2:
                random_val = 50;
                break;

            case 3:
                random_val = 100;
                break;

            case 4:
                random_val = 150;
                break;

            case 5:
                random_val = 200;
                break;

            default:
                break;
            }

            for (j = 0; j < random_val; j++)
                ;
        }

        counter++;
    }
}

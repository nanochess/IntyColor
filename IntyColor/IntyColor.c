//
//  main.c
//  IntyColor
//
//  Created by Oscar Toledo on 23/01/14.
//  Copyright (c) 2014 Oscar Toledo. All rights reserved.
//
//  Revision: Apr/01/2014. Support for IntyBASIC code generation.
//  Revision: Apr/02/2014. Support for Color Stack mode.
//  Revision: Apr/03/2014. Now finally works label in arguments. Solved bug in
//                         background color.
//  Revision: Apr/08/2014. Checks BMP entry format. Support for more BMP files.
//  Revision: Apr/11/2014. Support for 32-bits BMP files.
//  Revision: Apr/12/2014. Support for GROM characters and negated bitmaps.
//  Revision: Apr/25/2014. Corrected bug in support for negated bitmaps.
//  Revision: Jul/07/2014. Corrected bug in color generation >=8 for Color
//                         Stack mode and GROM hit.
//  Revision: Nov/06/2014. Support for generating graphics using BITMAP.
//  Revision: Feb/17/2015. Corrected bug where it could not generate simple
//                         files for assembly code. Now can work with any
//                         bitmap size that is a multiple of 8. Option for
//                         base offset for bitmaps.
//  Revision: Mar/04/2015. Solved lot of bugs when handling wide images.
//  Revision: Jul/04/2015. Solved bug in syntax for data (extra comma in
//                         non-standard images) and added code to display
//                         non-standard images. Generated code shows one
//                         line of data per row (instead of two lines).
//                         Added -p option.
//  Revision: Jul/13/2015. Solved bug where starting offset for cards would
//                         move also references to GROM cards. Support for
//                         256 colors BMP. Added comments in output for
//                         version, date and command-line.
//  Revision: Jul/16/2015. For assembler added a comment of begin/end for
//                         easy extraction by automated tools (requested by
//                         First Spear)
//  Revision: Jul/17/2015. When option -p is used avoids to generate PRINT
//                         statements that go off screen inserting comments.
//                         (reported by First Spear). Warns about -p and -i
//                         not doing any effect in assembler mode.
//  Revision: Jul/20/2015. Solved bug where Background/Foreground limitations
//                         applied to Color Stack mode.
//  Revision: Aug/04/2015. Changed color table constants to hexadecimal and
//                         added color names. Replaced some variable names
//                         so code is more autocommenting. New -m option for
//                         automagical 8x8 MOB use for 3 or more colors
//                         support per card.
//  Revision: Aug/05/2015. Prefers a deeper X/Y offset to optimize MOB usage.
//                         Added support for flip X/Y in MOB. Now uses
//                         constants.bas to document MOB output. New option
//                         -c to not use constants.bas.
//  Revision: Aug/06/2015. Bunch of conversion errors now to stdout. New
//                         option -r to generate a report BMP file to show
//                         where are the problems.
//  Revision: Aug/07/2015. Allows to use MOB with assembler output. The -r
//                         option chooses file. Shows in report where MOB are
//                         used.
//  Revisión: Aug/08/2015. New option -g allowing to enter a file with exact
//                         clues of where are MOB positioned. Now MOB are
//                         drawn in report.
//  Revisión: Aug/19/2015. New options -fx and -fy for flip in X and Y
//                         coordinate (suggested by First Spear). Warns of
//                         unknown options.
//  Revision: Sep/02/2015. Updated for new SCREEN syntax.
//  Revision: Sep/24/2015. When using magic MOBs and assembler code output,
//                         it outputs MOB data in hardware sequence.
//  Revision: Sep/29/2015. Solved bug in reversing colors when using
//                         optimize_from_grom. Better syntax for assembler
//                         output.
//  Revision: Oct/14/2015. Solved another bug in reversing color when using
//                         optimize_from_grom this time in Stack Color mode.
//  Revision: Feb/26/2017. New option -a for extracting all bitmaps without
//                         checking for duplicates, useful for scrolling
//                         with GRAM redefinition.
//  Revision: Jul/25/2017. Solved bug when allocating bitmaps array, it
//                         underassigned space. Added option -e.
//  Revision: Aug/30/2017. Solved another bug when allocating bitmaps array.
//                         Option -a avoids generating card data. Added -d
//                         option for tall chunks (8x16 pixels).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define VERSION "v1.1.5 Jul/25/2017"     /* Software version */

#define BLOCK_SIZE   16         /* Before it was 18, reduced for PLAY support */

/*#define DEBUG*/

unsigned char *bitmap;          /* Origin bitmap converted to indexed colors 0-15 */
int size_x;                     /* Size X in pixels */
int size_y;                     /* Size Y in pixels */

int grom_exists;
/* Indicates if GROM file exists for using */
unsigned char grom[256 * 8];    /* Contents of GROM file */

unsigned char *bitmaps;         /* Bitmaps created */
int number_bitmaps;             /* Number of bitmaps used */

unsigned char bit[16];

unsigned short *screen;         /* Screen cards */
int size_x_cards;               /* Screen X size in cards */
int size_y_cards;               /* Screen Y size in cards */

signed char *used_color;        /* Array of used colors per card */

int stack_color;                /* Indicates stack color mode */
int current_stack;              /* Current point to color stack */
int stack[4];                   /* Stack of colors */

int mobs[24];                   /* MOB data pointer */
int mob_pointer;                /* Pointer to next MOB available */

int err_code;                   /* Final error code */
int total_errors;

int clue[8][6];
int total_clues;

/*
 ** The 16 colors available in Intellivision
 */
unsigned char colors[16 * 3] = {
    /*R*/ /*G*/ /*B*/
    0x00, 0x00, 0x00,  /* Black */
    0x00, 0x2d, 0xff,  /* Red */
    0xff, 0x3d, 0x10,  /* Blue */
    0xc9, 0xcf, 0xab,  /* Tan */
    
    0x38, 0x6b, 0x3f,  /* Dark green */
    0x00, 0xa7, 0x56,  /* Green */
    0xfa, 0xea, 0x50,  /* Yellow */
    0xff, 0xff, 0xff,  /* White */
    
    0xbd, 0xac, 0xc8,  /* Grey */
    0x24, 0xb8, 0xff,  /* Cyan */
    0xff, 0xb4, 0x1f,  /* Orange */
    0x54, 0x6e, 0x00,  /* Brown */
    
    0xff, 0x4e, 0x57,  /* Pink */
    0xa4, 0x96, 0xff,  /* Light blue */
    0x75, 0xcc, 0x80,  /* Yellow green */
    0xb5, 0x1a, 0x58,  /* Purple */
};

/*
 ** Converts from hexadecimal
 */
int from_hex(int letter)
{
    letter = toupper(letter);
    if (letter < '0')
        return 0;
    if (letter > 'F')
        return 15;
    letter -= '0';
    if (letter > 9)
        letter -= 7;
    return letter;
}

/*
 ** Converts a byte to a binary string
 */
char *binary(int data)
{
    static char string[9];
    
    string[0] = (data & 0x80) ? '1' : '0';
    string[1] = (data & 0x40) ? '1' : '0';
    string[2] = (data & 0x20) ? '1' : '0';
    string[3] = (data & 0x10) ? '1' : '0';
    string[4] = (data & 0x08) ? '1' : '0';
    string[5] = (data & 0x04) ? '1' : '0';
    string[6] = (data & 0x02) ? '1' : '0';
    string[7] = (data & 0x01) ? '1' : '0';
    string[8] = '\0';
    return string;
}

/*
 ** Check for valid bitmap in color and size for representation as a MOB
 */
int check_for_valid(int color, int x, int y, int x_size, int y_size, int two_high, int *xo, int *yo)
{
    int x1;
    int y1;
    int x2;
    int y2;
    int c;
    
    if (x + 8 * x_size > size_x || y + 8 * y_size > size_y)
        return 0;
    
    /* Get the offset for the MOB (trying to optimize) */
    if (*yo == -1) {
        for (y1 = y; y1 < y + (two_high ? 16 : 8); y1++) {
            for (x1 = x; x1 < x + 8; x1++) {
                if (bitmap[y1 * size_x + x1] == color)
                    break;
            }
            if (x1 < x + 8) {
                *yo = y1 - y;
                break;
            }
        }
    }
    if (*yo == -1)  /* Impossible case */
        return 0;
    if (*xo == -1) {
        for (x1 = x; x1 < x + 8; x1++) {
            for (y1 = y + *yo; y1 < y + *yo + (two_high ? 16 : 8); y1++) {
                if (bitmap[y1 * size_x + x1] == color)
                    break;
            }
            if (y1 < y + *yo + 8) {
                *xo = x1 - x;
                break;
            }
        }
    }
    if (*xo == -1) {  /* Impossible case */
        *yo = -1;
        return 0;
    }
    x += *xo;
    y += *yo;
    
    /* Initialize the bitmap */
    memset(bit, 0, sizeof(bit));
    for (y1 = y; y1 < y + (two_high ? 16 : 8) * y_size; y1 += y_size) {
        for (x1 = x; x1 < x + 8 * x_size; x1 += x_size) {
            
            /* Check that a whole pixel can be represented with the color */
            c = 0;
            for (y2 = 0; y2 < y_size; y2++) {
                if (y1 + y2 >= size_y)
                    continue;
                for (x2 = 0; x2 < x_size; x2++) {
                    if (x1 + x2 >= size_x)
                        continue;
                    if (bitmap[(y1 + y2) * size_x + x1 + x2] == color)
                        c++;
                }
            }
            if (c > 0 && c < x_size * y_size) {
#ifdef DEBUG
                fprintf(stderr, "MOB not applied because incomplete pixel\n");
#endif
                return 0;
            }
            if (c == x_size * y_size)
                bit[(y1 - y) / y_size] |= 0x80 >> ((x1 - x) / x_size);
        }
    }
    if (two_high) {
        if (memcmp(bit, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0) {
#ifdef DEBUG
            fprintf(stderr, "MOB not applied because would not be used\n");
#endif
            return 0;
        }
    } else if (memcmp(bit, "\0\0\0\0\0\0\0\0", 8) == 0) {
#ifdef DEBUG
        fprintf(stderr, "MOB not applied because would not be used\n");
#endif
        return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "Valid color %d,x=%d,y=%d,x_size=%d,y_size=%d (xo=%d,yo=%d)\n", color, x, y, x_size, y_size, *xo, *yo);
#endif
    return 1;
}

/*
 ** Search for a bitmap in a table
 */
int search_bitmap(unsigned char *bitmap, unsigned char *table, int size)
{
    int c;
    
    for (c = 0; c < size; c++) {
        if (memcmp(&table[c * 8], &bitmap[0], 8) == 0)
            return c;
    }
    return size;
}

/*
 ** Mirror horizontally a 8x8 bitmap
 */
void mirror_x(unsigned char *bitmap)
{
    int c;
    int d;
    int v;
    
    for (c = 0; c < 8; c++) {
        v = 0;
        for (d = 0; d < 8; d++)
            v |= ((bitmap[c] >> d) & 1) << (7 - d);
        bitmap[c] = v;
    }
}

/*
 ** Mirror vertically a 8x8 bitmap
 */
void mirror_y(unsigned char *bitmap)
{
    int c;
    int v;
    
    for (c = 0; c < 4; c++) {
        v = bitmap[7 - c];
        bitmap[7 - c] = bitmap[c];
        bitmap[c] = v;
    }
}

/*
 ** Mark usage of card
 */
void mark_usage(int x, int y, int flags)
{
    int c;
    int d;
    
    for (d = 0; d < 8; d++) {
        for (c = 0; c < 8; c++) {
            bitmap[(y + d) * size_x + x + c] |= flags;
        }
    }
}

/*
 ** Only load a 8x16 bitmap
 */
int optimize_two_high(int x, int y, int color_foreground, int color_background, int reverse, int *yo)
{
    int c;
    
    c = number_bitmaps;
    memcpy(&bitmaps[number_bitmaps * 8], &bit[0], 8);
    number_bitmaps++;
    memcpy(&bitmaps[number_bitmaps * 8], &bit[8], 8);
    number_bitmaps++;
    c += 256;
    return c;
}

/*
 ** Try to optimize a bitmap using GROM
 */
int optimize_from_grom(int x, int y, int color_foreground, int color_background, int reverse, int *yo)
{
    int c;
    int d;
    
    if (memcmp(&bit[0], "\x00\x00\x00\x00\x00\x00\x00\x00", 8) == 0) {
        c = 0;
    } else {
        if (grom_exists) {  /* Try to optimize output using GROM shapes */
            if (stack_color) {
                c = 256;
                if (color_foreground < 8) {
                    c = search_bitmap(&bit[0], grom, 256);
                    if (c == 256 && reverse == 2) {
                        mirror_x(&bit[0]);
                        c = search_bitmap(&bit[0], grom, 256);
                        if (c != 256)
                            *yo |= 0x0400;  /* X-flip */
                        mirror_x(&bit[0]);
                    }
                    if (c == 256 && reverse == 2) {
                        mirror_y(&bit[0]);
                        c = search_bitmap(&bit[0], grom, 256);
                        if (c != 256)
                            *yo |= 0x0800;  /* Y-flip */
                        mirror_y(&bit[0]);
                    }
                    if (c == 256 && reverse == 2) {
                        mirror_x(&bit[0]);
                        mirror_y(&bit[0]);
                        c = search_bitmap(&bit[0], grom, 256);
                        if (c != 256)
                            *yo |= 0x0c00;  /* Y-flip */
                        mirror_x(&bit[0]);
                        mirror_y(&bit[0]);
                    }
                }
                if (c == 256 && reverse == 1) {
                    for (d = 0; d < 8; d++)
                        bit[d] = ~bit[d];
                    if (color_background < 8 &&
                        (color_foreground == stack[current_stack] || color_foreground == stack[(current_stack + 1) & 3])) {
                        c = search_bitmap(&bit[0], grom, 256);
                    }
                    if (c == 256) {
                        for (d = 0; d < 8; d++)
                            bit[d] = ~bit[d];
                        c = -1;
                    } else {
                        *yo |= 1;   /* Flip colors */
                    }
                } else if (c == 256) {
                    c = -1;
                }
            } else {
                c = search_bitmap(&bit[0], grom, 64);
                if (c == 64 && reverse == 2) {
                    mirror_x(&bit[0]);
                    c = search_bitmap(&bit[0], grom, 64);
                    if (c != 64)
                        *yo |= 0x0400;  /* X-flip */
                    mirror_x(&bit[0]);
                }
                if (c == 64 && reverse == 2) {
                    mirror_y(&bit[0]);
                    c = search_bitmap(&bit[0], grom, 64);
                    if (c != 64)
                        *yo |= 0x0800;  /* Y-flip */
                    mirror_y(&bit[0]);
                }
                if (c == 64 && reverse == 2) {
                    mirror_x(&bit[0]);
                    mirror_y(&bit[0]);
                    c = search_bitmap(&bit[0], grom, 64);
                    if (c != 64)
                        *yo |= 0x0c00;  /* Y-flip */
                    mirror_x(&bit[0]);
                    mirror_y(&bit[0]);
                }
                if (c == 64 && reverse == 1) {
                    for (d = 0; d < 8; d++)
                        bit[d] = ~bit[d];
                    if (color_background < 8) {
                        c = search_bitmap(&bit[0], grom, 64);
                    }
                    if (c == 64) {
                        for (d = 0; d < 8; d++)
                            bit[d] = ~bit[d];
                        c = -1;
                    } else {
                        *yo |= 1;   /* Flip colors */
                    }
                } else if (c == 64) {
                    c = -1;
                }
            }
        } else {
            c = -1;
        }
        if (c == -1) {  /* Not in GROM? try to assign a GRAM card */
            c = search_bitmap(&bit[0], bitmaps, number_bitmaps);
            if (c == number_bitmaps && reverse == 2) {
                mirror_x(&bit[0]);
                c = search_bitmap(&bit[0], bitmaps, number_bitmaps);
                if (c != number_bitmaps)
                    *yo |= 0x0400;  /* X-flip */
                mirror_x(&bit[0]);
            }
            if (c == number_bitmaps && reverse == 2) {
                mirror_y(&bit[0]);
                c = search_bitmap(&bit[0], bitmaps, number_bitmaps);
                if (c != number_bitmaps)
                    *yo |= 0x0800;  /* Y-flip */
                mirror_y(&bit[0]);
            }
            if (c == number_bitmaps && reverse == 2) {
                mirror_x(&bit[0]);
                mirror_y(&bit[0]);
                c = search_bitmap(&bit[0], bitmaps, number_bitmaps);
                if (c != number_bitmaps)
                    *yo |= 0x0c00;  /* Y-flip */
                mirror_x(&bit[0]);
                mirror_y(&bit[0]);
            }
            if (c == number_bitmaps) {
                
                /* Try to search a complemented GRAM card */
                if (reverse == 1) {
                    if ((stack_color && (color_background < 8 &&
                                         (color_foreground == stack[current_stack] || color_foreground == stack[(current_stack + 1) & 3]))) ||
                        (!stack_color && color_background < 8)) {
                        for (d = 0; d < 8; d++)
                            bit[d] = ~bit[d];
                        c = search_bitmap(&bit[0], bitmaps, number_bitmaps);
                        if (c < number_bitmaps) {
                            d = color_foreground;
                            color_foreground = color_background;
                            color_background = d;
                            *yo |= 1;   /* Flip colors */
                        } else {
                            for (d = 0; d < 8; d++)
                                bit[d] = ~bit[d];
                        }
                    }
                }
                if (c == number_bitmaps) {
                    if (number_bitmaps == 64) {
                        fprintf(stderr, "More than 64 defined cards in block %d,%d\n",
                                x, y);
                        err_code = 1;
                        total_errors++;
                        mark_usage(x, y, 0x10);
                        c = 0;
                    } else {
                        memcpy(&bitmaps[c * 8], &bit[0], 8);
                        number_bitmaps++;
                    }
                }
            }
            c += 256;
        }
    }
    return c;
}

/*
 ** Lookup for used colors
 */
void lookup_used_colors(int x, int y)
{
    char used[16];
    int c;
    int d;
    signed char *current_used;
    
    /* Look for the colors in the block */
    current_used = &used_color[((y / 8 * size_x_cards) + (x / 8)) * 16];
    memset(current_used, -1, 16);
    memset(used, 0, sizeof(used));
    for (c = 0; c < 8; c++) {
        for (d = 0; d < 8; d++) {
            if (bitmap[(y + c) * size_x + x + d] < 16)
                used[bitmap[(y + c) * size_x + x + d]] = 1;
        }
    }
    if (stack_color) {
        d = 0;
        for (c = 0; c < 16; c++) {
            if (c != stack[0] && c != stack[1] && c != stack[2] && c != stack[3])
                continue;
            if (used[c])
                current_used[d++] = c;
        }
        for (c = 0; c < 16; c++) {
            if (c == stack[0] || c == stack[1] || c == stack[2] || c == stack[3])
                continue;
            if (used[c])
                current_used[d++] = c;
        }
    } else {
        d = 0;
        for (c = 0; c < 16; c++) {
            if (used[c])
                current_used[d++] = c;
        }
    }
}

/*
 ** Main program
 */
int main(int argc, char *argv[])
{
    FILE *a;
    int x;
    int y;
    int c;
    int d;
    int n;
    unsigned char buffer[54 + 1024];  /* Header and palette */
    unsigned char color_replacement[32];
    int color_replacement_size = 0;
    unsigned short *ap;
    
    int arg;
    int intybasic = 0;
    int use_bitmap = 0;
    int use_print = 0;
    int magic_mobs = 0;
    int base_offset = 0;
    int use_constants = 1;
    int stub = 1;
    int clues = 0;
    int flip_x = 0;
    int flip_y = 0;
    int wants_all = 0;
    int tall_chunks = 0;
    int step_card;
    char *generate_report = NULL;
    char *grom_file = NULL;
    char *label = "screen";
    char *input_file;
    
    time_t actual;
    struct tm *date;
    
    signed char *current_used;
    int e;
    int color_foreground;
    int color_background;
    int mobs_found;
    
    actual = time(0);
    date = localtime(&actual);
    if (argc < 3) {
        fprintf(stderr, "\nConverter from BMP to Intellivision Background/Foreground format\n");
        fprintf(stderr, VERSION "  by Oscar Toledo G. http://nanochess.org\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage:\n\n");
        fprintf(stderr, "    intycolor image.bmp image.asm [label]\n");
        fprintf(stderr, "        Creates image for use with assembler code\n\n");
        fprintf(stderr, "    intycolor -b [-n] [-p] [-i] image.bmp image.bas [label]\n");
        fprintf(stderr, "        Creates image for use with IntyBASIC code\n\n");
        fprintf(stderr, "    -n     Removes stub code for display in IntyBASIC mode\n");
        fprintf(stderr, "    -p     Uses PRINT in IntyBASIC mode\n");
        fprintf(stderr, "    -o20   Initial offset for cards is 20 (0-63 is valid)\n");
        fprintf(stderr, "    -m     Tries to use MOBs for more than 2 colors per card\n");
        fprintf(stderr, "    -c     Doesn't use constants.bas for -m option\n");
        fprintf(stderr, "    -i     Generates BITMAP statements instead of DATA\n");
        fprintf(stderr, "    -r output.bmp  Generate BMP report of conversion in file\n");
        fprintf(stderr, "                   red = error, green = GRAM, yellow = GROM\n");
        fprintf(stderr, "                   grey = MOB\n");
        fprintf(stderr, "    -g clue.txt    Exact clues for -m, text file up to 8 lines:\n");
        fprintf(stderr, "                   x,y,color[,x_zoom(1-2),y_zoom(1-4),0/1]\n");
        fprintf(stderr, "                   The final 0/1 indicates 8x8 or 8x16\n");
        fprintf(stderr, "                   Suggestion: run with empty text file and\n");
        fprintf(stderr, "                   option -r to see what cards require MOBs\n");
        fprintf(stderr, "    -x p/grom.bin  Path and file for grom.bin, by default it\n");
        fprintf(stderr, "                   searches in current path for grom.bin\n");
        fprintf(stderr, "    -fx    Flip image along the X-coordinate (mirror)\n");
        fprintf(stderr, "    -fy    Flip image along the Y-coordinate\n");
        fprintf(stderr, "    -a     All 8x8 cards as continuous bitmap in output,\n");
        fprintf(stderr, "           doesn't generate card data.\n");
        fprintf(stderr, "    -e45d2 Replace color 4 with 5 and d with 2 before process,\n");
        fprintf(stderr, "           useful to recreate same image with other colors.\n");
        fprintf(stderr, "    -d     Process image in chunks of 16 pixels high, useful\n");
        fprintf(stderr, "           to create MOB bitmaps.\n");
        fprintf(stderr, "    -v     Process 8x8 cards in vertical direction first.\n");
        fprintf(stderr, "           Useful for horizontal scrolling bitmaps and -a option.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "By default intycolor creates images for use with Intellivision\n");
        fprintf(stderr, "Background/Foreground video format, you can use 8 primary\n");
        fprintf(stderr, "colors and 16 background colors for each 8x8 block.\n\n");
        fprintf(stderr, "Using the flag -s0123 creates images for use with Color Stack\n");
        fprintf(stderr, "mode, the 0123 can be replaced with the sequence of colors\n");
        fprintf(stderr, "you'll program in the Color Stack registers (hexadecimal 0-f)\n\n");
        fprintf(stderr, "intycolor will warn you if your image cannot be represented\n");
        fprintf(stderr, "by a real Intellivision\n\n");
        fprintf(stderr, "It can use GROM characters if you provide grom.bin in current\n");
        fprintf(stderr, "directory.\n\n");
        fprintf(stderr, "It requires a BMP file of 8/24/32 bits, remember Intellivision\n");
        fprintf(stderr, "screen is a fixed size of 160x96 pixels but this utility will\n");
        fprintf(stderr, "accept any multiple of 8 pixels in any coordinate.\n\n");
        fprintf(stderr, "The -a option is for working over monochrome bitmaps and\n");
        fprintf(stderr, "generating a continous bitmap for scrolling with more than\n");
        fprintf(stderr, "the limit of GRAM definitions (the program must define the\n");
        fprintf(stderr, "bitmaps as the scrolling goes on).\n");
        return 0;
    }
    
    /* Check for GROM to use shapes */
    a = fopen(grom_file ? grom_file : "grom.bin", "rb");
    if (a != NULL) {
        fread(grom, 1, sizeof(grom), a);
        fclose(a);
        grom_exists = 1;
    } else {
        grom_exists = 0;
    }
    
    /* Process arguments */
    arg = 1;
    while (arg < argc && argv[arg][0] == '-') {
        c = tolower(argv[arg][1]);
        if (c == 'f') {
            d = tolower(argv[arg][2]);
            if (d == 'x')
                flip_x = 1;
            else if (d == 'y')
                flip_y = 1;
            else
                fprintf(stderr, "Unknown option: %s\n", argv[arg]);
        } else if (c == 'm') {  /* -m MOBs mode */
            magic_mobs = 1;
        } else if (c == 'o') {  /* -o11 Initial GRAM card number */
            base_offset = atoi(argv[arg] + 2);
        } else if (c == 'b') {  /* -b IntyBASIC mode */
            intybasic = 1;
        } else if (c == 'c') {  /* -c Doesn't use constants */
            use_constants = 0;
        } else if (c == 's') {  /* -s0000 Use Color Stack mode */
            stack_color = 1;
            if (strlen(argv[arg]) != 6) {
                fprintf(stderr, "Warning: Ignoring -s argument as size is invalid\n");
            } else {
                stack[0] = from_hex(argv[arg][2]);
                stack[1] = from_hex(argv[arg][3]);
                stack[2] = from_hex(argv[arg][4]);
                stack[3] = from_hex(argv[arg][5]);
            }
        } else if (c == 'p') {  /* -p Use PRINT statement (IntyBASIC) */
            use_print = 1;
        } else if (c == 'i') {  /* -i Use BITMAP instead of DATA (IntyBASIC) */
            use_bitmap = 1;
        } else if (c == 'n') {  /* -n Remove stub from output */
            stub = 0;
        } else if (c == 'r') {     /* -r Generate report */
            arg++;
            if (arg < argc)
                generate_report = argv[arg];
            else
                fprintf(stderr, "Error: missing filename for option -r\n");
        } else if (c == 'x') {  /* -x path to grom.bin */
            arg++;
            if (arg < argc)
                grom_file = argv[arg];
            else
                fprintf(stderr, "Error: missing filename for option -x\n");
        } else if (c == 'g') {     /* -g Clue file */
            arg++;
            if (arg < argc) {
                a = fopen(argv[arg], "r");
                if (a != NULL) {
                    char line[256];
                    int c;
                    int x;
                    int y;
                    int color;
                    int x_size;
                    int y_size;
                    int two_high;
                    
                    clues = 1;
                    while (fgets(line, sizeof(line) - 1, a)) {
                        c = sscanf(line, "%d,%d,%d,%d,%d,%d", &x, &y, &color, &x_size, &y_size, &two_high);
                        if (c == 0)
                            continue;
                        if (total_clues == 8) {
                            fprintf(stderr, "Error: clues for more than 8 MOBs\n");
                            break;
                        }
                        if (c < 6)
                            two_high = 0;
                        if (c < 5)
                            y_size = 0;
                        if (c < 4)
                            x_size = 0;
                        if (c < 3)
                            color = 0;
                        if (c < 2)
                            y = 0;
                        if (c < 1)
                            x = 0;
                        if (c < 3)
                            fprintf(stderr, "Error: less than 3 numbers in line of clue file\n");
                        clue[total_clues][0] = x;
                        clue[total_clues][1] = y;
                        clue[total_clues][2] = color;
                        clue[total_clues][3] = x_size;
                        clue[total_clues][4] = y_size;
                        clue[total_clues][5] = two_high;
                        total_clues++;
                    }
                    fclose(a);
                } else {
                    fprintf(stderr, "Error: unable to open clue file %s\n", argv[arg]);
                }
            } else {
                fprintf(stderr, "Error: missing filename for option -g\n");
            }
        } else if (c == 'a') {      /* -a All bitmaps */
            wants_all = 1;
        } else if (c == 'e') {      /* -e Color replacement */
            char *ap1 = &argv[arg][2];
            
            while (isxdigit(ap1[0]) && isxdigit(ap1[1])) {
                if (color_replacement_size < 32) {
                    color_replacement[color_replacement_size++] = from_hex(ap1[0]);
                    color_replacement[color_replacement_size++] = from_hex(ap1[1]);
                } else {
                    fprintf(stderr, "Error: too many color replacements in option -e\n");
                }
                ap1 += 2;
            }
        } else if (c == 'd') {      /* -d 16 pixels high chunks */
            tall_chunks = 1;
        } else if (c == 'v') {      /* -v process in vertical direction */
            tall_chunks = 2;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg]);
        }
        arg++;
    }
    if (!intybasic) {
        if (use_print)
            fprintf(stderr, "Warning: argument -p is ignored when in assembler mode\n");
        if (use_bitmap)
            fprintf(stderr, "Warning: argument -i is ignored when in assembler mode\n");
        if (stub == 0)
            fprintf(stderr, "Warning: argument -n is ignored when in assembler mode\n");
        if (!use_constants)
            fprintf(stderr, "Warning: argument -c is ignored when in assembler mode\n");
    } else {
        if (!use_constants && !magic_mobs)
            fprintf(stderr, "Warning: argument -c is ignored if not using -m\n");
    }
    if (wants_all) {
        if (magic_mobs) {
            fprintf(stderr, "Warning: argument -m is ignored if using -a\n");
            magic_mobs = 0;
        }
    }
    if (arg >= argc) {
        fprintf(stderr, "Missing input file name\n");
        exit(2);
    }
    
    /* Open input file */
    fprintf(stdout, "Processing: %s\n", argv[arg]);
    a = fopen(input_file = argv[arg], "rb");
    arg++;
    if (a == NULL) {
        fprintf(stderr, "Missing input file\n");
        exit(2);
    }
    fread(buffer, 1, 54 + 1024, a);
    if (buffer[0] != 'B' || buffer[1] != 'M') {
        fprintf(stderr, "The input file is not in BMP format\n");
        fclose(a);
        exit(3);
    }
    if (buffer[0x1c] != 8 && buffer[0x1c] != 24 && buffer[0x1c] != 32) {
        fprintf(stderr, "The input file is in %d bits format (not 8/24/32)\n", buffer[0x1c]);
        fclose(a);
        exit(3);
    }
    if (buffer[0x1c] == 8) {
        if (buffer[0x2e] != 0 || (buffer[0x2f] != 0 && buffer[0x2f] != 1)) {
            fprintf(stderr, "Unsupported palette for 8 bits format\n");
            fclose(a);
            exit(3);
        }
    }
    if (buffer[0x1e] != 0 || buffer[0x1f] != 0 || buffer[0x20] != 0 || buffer[0x21] != 0) {
        fprintf(stderr, "Cannot handle compressed input files (codec 0x%08x)\n", (buffer[0x21] << 24) | (buffer[0x20] << 16) | (buffer[0x1f] << 8) | (buffer[0x1e]));
        fclose(a);
        exit(3);
    }
    size_x = buffer[0x12] | (buffer[0x13] << 8);
    size_y = buffer[0x16] | (buffer[0x17] << 8);
    if (size_y >= 32768)
        size_y -= 65536;
    if (size_y >= 0) {
        n = 0;
    } else {
        size_y = -size_y;
        n = 1;
    }
    if ((size_x & 7) != 0) {
        fprintf(stderr, "The input file doesn't measure a multiple of 8 in X size (it's %d pixels)\n", size_x);
        fclose(a);
        exit(3);
    }
    if ((size_y & 7) != 0) {
        fprintf(stderr, "The input file doesn't measure a multiple of 8 in Y size (it's %d pixels)\n", size_y);
        fclose(a);
        exit(3);
    }
    if (size_x == 0 || size_y == 0) {
        fprintf(stderr, "There's a weird BMP file in the input. I'm scared...\n");
        fclose(a);
        exit(3);
    }
    size_x_cards = size_x / 8;
    size_y_cards = size_y / 8;
    if (wants_all) {
        bitmaps = malloc(size_x_cards * size_y_cards * 8 * sizeof(char));
    } else {
        bitmaps = malloc(64 * 8 * sizeof(char));
    }
    if (bitmaps == NULL) {
        fprintf(stderr, "Couldn't allocate bitmaps array\n");
        fclose(a);
        exit(3);
    }
    bitmap = malloc(size_x * size_y * sizeof(char));
    screen = malloc(size_x_cards * size_y_cards * sizeof(unsigned short));
    used_color = malloc(size_x_cards * size_y_cards * 16);
    if (bitmap == NULL || screen == NULL || used_color == NULL) {
        if (bitmap != NULL)
            free(bitmap);
        if (screen != NULL)
            free(screen);
        if (used_color != NULL)
            free(used_color);
        fprintf(stderr, "Not enough memory for bitmap\n");
        fclose(a);
        exit(3);
    }
    for (c = 0; c < size_x_cards * size_y_cards * 16; c++)
        used_color[c] = -1;
    
    /* Read image and approximate any color to the local palette */
    fseek(a, buffer[10] | (buffer[11] << 8) | (buffer[12] << 16) | (buffer[13] << 24), SEEK_SET);
    for (y = n ? 0 : size_y - 1; n ? y < size_y : y >= 0; n ? y++ : y--) {
        for (x = 0; x < size_x; x++) {
            int best_color;
            int best_difference;
            
            if (buffer[0x1c] == 8) {            /* 256 color */
                fread(buffer, 1, 1, a);
                memcpy(buffer, buffer + 54 + buffer[0] * 4, 4);
            } else if (buffer[0x1c] == 24) {    /* 24 bits */
                fread(buffer, 1, 3, a);
            } else {                            /* 32 bits */
                fread(buffer, 1, 4, a);
            }
            best_color = 0;
            best_difference = 100000;
            for (c = 0; c < 16; c++) {
                d = (buffer[2] - colors[c * 3]) * (buffer[2] - colors[c * 3])
                + (buffer[1] - colors[c * 3 + 1]) * (buffer[1] - colors[c * 3 + 1])
                + (buffer[0] - colors[c * 3 + 2]) * (buffer[0] - colors[c * 3 + 2]);
                if (d < best_difference) {
                    best_difference = d;
                    best_color = c;
                }
            }
            for (c = 0; c < color_replacement_size; c += 2) {
                if (best_color == color_replacement[c]) {
                    best_color = color_replacement[c + 1];
                    break;
                }
            }
            bitmap[(flip_y ? size_y - 1 - y : y) * size_x + (flip_x ? size_x - 1 - x : x)] = best_color;
        }
    }
    fclose(a);
    
    /* Make note of used colors per 8x8 block */
    err_code = 0;
    current_stack = 0;
    ap = screen;
    for (y = 0; y < size_y; y += 8) {
        for (x = 0; x < size_x; x += 8) {  /* For each 8x8 block */
            lookup_used_colors(x, y);
        }
    }
    
    /* Generate automagically the MOBs */
    if (magic_mobs) {
        if (clues) {
            int step;
            
            for (step = 0; step < 2; step++) {
                for (n = 0; n < total_clues; n++) {
                    int best_size;
                    int best_color;
                    int best_xo;
                    int best_yo;
                    char best_bit[8];
                    int xo;
                    int yo;
                    
                    if (step == 0 && clue[n][5] == 0)  /* First step, only 8x16 */
                        continue;
                    if (step == 1 && clue[n][5] == 1)  /* Second step, only 8x8 */
                        continue;
                    fprintf(stdout, "Clue %d at x=%d,y=%d: ", n + 1, clue[n][0], clue[n][1]);
                    x = clue[n][0] & 0xf8;
                    xo = clue[n][0] & 0x07;
                    y = clue[n][1] & 0xf8;
                    yo = clue[n][1] & 0x07;
                    best_size = 0;
                    best_color = 0;
                    best_xo = 0;
                    best_yo = 0;
                    memset(best_bit, 0, sizeof(best_bit));
                    if (clue[n][3] != 0 && clue[n][4] != 0 && check_for_valid(clue[n][2], x, y, clue[n][3], clue[n][4], clue[n][5], &xo, &yo)) {
                        if (clue[n][3] * clue[n][4] > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x500 | (clue[n][3] << 4) | clue[n][4];
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    } else if (check_for_valid(clue[n][2], x, y, 2, 4, clue[n][5], &xo, &yo)) {
                        if (2 * 4 > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x524;
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    } else if (check_for_valid(clue[n][2], x, y, 2, 2, clue[n][5], &xo, &yo)) {
                        if (2 * 2 > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x422;
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    } else if (check_for_valid(clue[n][2], x, y, 1, 4, clue[n][5], &xo, &yo)) {
                        if (1 * 4 > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x314;
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    } else if (check_for_valid(clue[n][2], x, y, 2, 1, clue[n][5], &xo, &yo)) {
                        if (2 * 1 > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x221;
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    } else if (check_for_valid(clue[n][2], x, y, 1, 2, clue[n][5], &xo, &yo)) {
                        if (1 * 2 > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x112;
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    } else if (check_for_valid(clue[n][2], x, y, 1, 1, clue[n][5], &xo, &yo)) {
                        if (1 * 1 > best_size || xo > best_xo || yo > best_yo) {
                            best_size = 0x011;
                            best_color = clue[n][2];
                            best_xo = xo;
                            best_yo = yo;
                            memcpy(best_bit, bit, sizeof(best_bit));
                        }
                    }
                    if (!best_size) {
                        fprintf(stdout, "not applied\n");
                        continue;
                    }
                    fprintf(stdout, "applied at %dx%d (8x%d)\n", (best_size & 0xf0) >> 4, best_size & 0x0f,
                            clue[n][5] ? 16 : 8);
                    for (d = best_yo; d < (best_size & 15) * (clue[n][5] ? 16 : 8) + best_yo; d++) {
                        for (c = best_xo; c < ((best_size >> 4) & 15) * 8 + best_xo; c++) {
                            if (bitmap[(y + d) * size_x + (x + c)] == best_color)
                                bitmap[(y + d) * size_x + (x + c)] = -1;
                        }
                    }
                    for (d = best_yo; d < (best_size & 15) * (clue[n][5] ? 16 : 8) + best_yo; d += 7) {
                        for (c = best_xo; c < ((best_size >> 4) & 15) * 8 + best_xo; c += 7) {
                            lookup_used_colors((x + c) & -8, (y + d) & -8);
                        }
                    }
                    memcpy(bit, best_bit, 8);
                    if (clue[n][5] != 0)
                        c = optimize_two_high(x, y, best_color, 0, 2, &best_yo);
                    else
                        c = optimize_from_grom(x, y, best_color, 0, 2, &best_yo);
                    if (c >= 256)
                        c += base_offset;
                    mobs[mob_pointer++] = (x + 8 + best_xo) | 0x0200 | ((best_size & 0xf0) == 0x20 ? 0x0400 : 0);
                    mobs[mob_pointer++] = (y + 8 + best_yo) | (clue[n][5] ? 0x0080 : 0) | ((best_size & 0x0f) != 0x04 ? (best_size & 0x0f) != 0x02 ? 0x0100 : 0x0200 : 0x0300);
                    mobs[mob_pointer++] = (c << 3) | (best_color & 7) | ((best_color & 8) << 9);
                }
            }
        } else {
            int out_of_mobs = 0;
            
            do {
                mobs_found = 0;
                for (y = 0; y < size_y; y += 8) {
                    for (x = 0; x < size_x; x += 8) {  /* For each 8x8 block */
                        int best_size;
                        int best_color;
                        int best_xo;
                        int best_yo;
                        char best_bit[8];
                        int xo;
                        int yo;
                        
                        current_used = &used_color[((y / 8 * size_x_cards) + (x / 8)) * 16];
                        if (current_used[2] == -1)  /* More than 2 colors? */
                            continue;               /* No, continue */
                        if (mob_pointer == 24) {    /* All MOBs used? */
                            if (!out_of_mobs) {
                                out_of_mobs = 1;
                                fprintf(stdout, "Not enough MOBs for further cards with more than 2 colors\n");
                            }
                            continue;               /* Yes, continue */
                        }
                        
                        /* Create the biggest possible MOB from it */
                        best_size = 0;
                        best_color = 0;
                        best_xo = 0;
                        best_yo = 0;
                        memset(best_bit, 0, sizeof(best_bit));
                        for (n = 15; n >= 0; n--) {
                            if (current_used[n] == -1)
                                continue;
                            if (stack_color) {
                                if (best_size > 0 && best_color != stack[0] && best_color != stack[1] && best_color != stack[2] && best_color != stack[3] && (current_used[n] == stack[0] || current_used[n] == stack[1] || current_used[n] == stack[2] || current_used[n] == stack[3]))
                                    continue;
                            }
                            xo = -1;
                            yo = -1;
                            if (check_for_valid(current_used[n], x, y, 2, 4, 0, &xo, &yo)) {
                                if (2 * 4 > best_size || xo > best_xo || yo > best_yo) {
                                    best_size = 0x524;
                                    best_color = current_used[n];
                                    best_xo = xo;
                                    best_yo = yo;
                                    memcpy(best_bit, bit, sizeof(best_bit));
                                }
                            } else if (check_for_valid(current_used[n], x, y, 2, 2, 0, &xo, &yo)) {
                                if (2 * 2 > best_size || xo > best_xo || yo > best_yo) {
                                    best_size = 0x422;
                                    best_color = current_used[n];
                                    best_xo = xo;
                                    best_yo = yo;
                                    memcpy(best_bit, bit, sizeof(best_bit));
                                }
                            } else if (check_for_valid(current_used[n], x, y, 1, 4, 0, &xo, &yo)) {
                                if (1 * 4 > best_size || xo > best_xo || yo > best_yo) {
                                    best_size = 0x314;
                                    best_color = current_used[n];
                                    best_xo = xo;
                                    best_yo = yo;
                                    memcpy(best_bit, bit, sizeof(best_bit));
                                }
                            } else if (check_for_valid(current_used[n], x, y, 2, 1, 0, &xo, &yo)) {
                                if (2 * 1 > best_size || xo > best_xo || yo > best_yo) {
                                    best_size = 0x221;
                                    best_color = current_used[n];
                                    best_xo = xo;
                                    best_yo = yo;
                                    memcpy(best_bit, bit, sizeof(best_bit));
                                }
                            } else if (check_for_valid(current_used[n], x, y, 1, 2, 0, &xo, &yo)) {
                                if (1 * 2 > best_size || xo > best_xo || yo > best_yo) {
                                    best_size = 0x112;
                                    best_color = current_used[n];
                                    best_xo = xo;
                                    best_yo = yo;
                                    memcpy(best_bit, bit, sizeof(best_bit));
                                }
                            } else if (check_for_valid(current_used[n], x, y, 1, 1, 0, &xo, &yo)) {
                                if (1 * 1 > best_size || xo > best_xo || yo > best_yo) {
                                    best_size = 0x011;
                                    best_color = current_used[n];
                                    best_xo = xo;
                                    best_yo = yo;
                                    memcpy(best_bit, bit, sizeof(best_bit));
                                }
                            }
                        }
                        for (d = best_yo; d < (best_size & 15) * 8 + best_yo; d++) {
                            for (c = best_xo; c < ((best_size >> 4) & 15) * 8 + best_xo; c++) {
                                if (bitmap[(y + d) * size_x + (x + c)] == best_color)
                                    bitmap[(y + d) * size_x + (x + c)] = -1;
                            }
                        }
                        for (d = best_yo; d < (best_size & 15) * 8 + best_yo; d += 7) {
                            for (c = best_xo; c < ((best_size >> 4) & 15) * 8 + best_xo; c += 7) {
                                lookup_used_colors((x + c) & -8, (y + d) & -8);
                            }
                        }
                        memcpy(bit, best_bit, 8);
                        c = optimize_from_grom(x, y, best_color, 0, 2, &best_yo);
                        if (c >= 256)
                            c += base_offset;
                        mobs[mob_pointer++] = (x + 8 + best_xo) | 0x0200 | ((best_size & 0xf0) == 0x20 ? 0x0400 : 0);
                        mobs[mob_pointer++] = (y + 8 + best_yo) | ((best_size & 0x0f) != 0x04 ? (best_size & 0x0f) != 0x02 ? 0x0100 : 0x0200 : 0x0300);
                        mobs[mob_pointer++] = (c << 3) | (best_color & 7) | ((best_color & 8) << 9);
                        mobs_found = 1;
                    }
                }
            } while (mobs_found) ;
        }
    }
    
    /* Generate the bitmap */
    total_errors = 0;
    step_card = 0;
    x = 0;
    y = 0;
    while (1) {     /* Process each 8x8 block */
        
        /* Per mode */
        current_used = &used_color[((y / 8 * size_x_cards) + (x / 8)) * 16];
        if (current_used[2] != -1) {  /* Too many colors in block */
            fprintf(stdout, "Too many colors in block x=%d,y=%d (", x, y);
            for (e = 0; e < 16 && current_used[e] != -1; e++) {
                if (e)
                    fprintf(stdout, ",");
                fprintf(stdout, "%d", current_used[e]);
            }
            fprintf(stdout, ")\n");
            err_code = 1;
            total_errors++;
            mark_usage(x, y, 0x10);
        }
        color_foreground = current_used[0];
        if (color_foreground == -1)
            color_foreground = 1;
        color_background = current_used[1];  /* Note it can be -1 */
        
        /* Align color per mode */
        if (wants_all) {
            if (color_background == -1)
                color_background = 0;
            if (color_foreground <= color_background) {
                c = color_foreground;
                color_foreground = color_background;
                color_background = c;
            }
            if (color_foreground == 0 && color_background == 0)
                color_foreground = 7;
        } else if (stack_color) {
            if (color_foreground == stack[current_stack]
                && color_background != -1) {
                c = color_foreground;
                color_foreground = color_background;
                color_background = c;
            }
            if (color_foreground == stack[(current_stack + 1) & 3]
                && color_background != stack[current_stack]) {
                c = color_foreground;
                color_foreground = color_background;
                color_background = c;
            }
            if (color_background != -1
                && color_background != stack[current_stack]
                && color_background != stack[(current_stack + 1) & 3]) {
                fprintf(stdout,
                        "Background color %d not aligned with color stack (%d or %d) in block x=%d,y=%d\n",
                        color_background, stack[current_stack], stack[(current_stack + 1) & 3], x, y);
                err_code = 1;
                total_errors++;
                mark_usage(x, y, 0x10);
            }
        } else {
            if (color_foreground > 7) {
                c = color_foreground;
                color_foreground = color_background;
                color_background = c;
            }
            if (color_foreground > 7) {
                fprintf(stdout,
                        "Foreground color %d outside of primary colors in block x=%d,y=%d\n",
                        color_foreground, x, y);
                err_code = 1;
                total_errors++;
                mark_usage(x, y, 0x10);
            }
            if (color_background == -1) {
                color_background = color_foreground;
                color_foreground = -1;
            }
            if (color_foreground == 0 && color_background != -1 && color_background < 8) {
                c = color_foreground;
                color_foreground = color_background;
                color_background = c;
            }
        }
        
        /* Convert to bitmap */
        for (c = 0; c < 8; c++) {
            bit[c] = 0;
            for (d = 0; d < 8; d++) {
                if ((bitmap[(y + c) * size_x + x + d] & 0x0f) == color_foreground)
                    bit[c] |= 0x80 >> d;
            }
        }
        d = 0;
        if (wants_all) {
            memcpy(&bitmaps[number_bitmaps * 8], &bit[0], 8);
            number_bitmaps++;
            c = 256;
        } else {
            c = optimize_from_grom(x, y, color_foreground, color_background, 1, &d);
            if (d != 0) {
                d = color_foreground;
                color_foreground = color_background;
                color_background = d;
            }
            if (color_foreground == -1)
                color_foreground = 0;
            if (c >= 256) {
                c += base_offset;
            } else {
                mark_usage(x, y, 0x20);
            }
        }
        
        /* Generate final value for BACKTAB */
        if (stack_color) {
            if (color_background == stack[current_stack])
                d = 0;
            else if (color_background == stack[(current_stack + 1) & 3]) {
                d = 0x2000;
                current_stack = (current_stack + 1) & 3;
            } else {
                d = 0;
            }
            *ap++ = (c << 3) | (color_foreground & 7) | ((color_foreground & 8) << 9) | d;
        } else {
            if (color_background == -1)
                color_background = 0;
            *ap++ = (c << 3) | color_foreground | ((color_background & 3) << 9) | ((color_background & 0x04) << 11) | ((color_background & 0x08) << 9);
        }
        
        /* Advance to next 8x8 bitmap */
        if (tall_chunks == 2) {  /* -v */
            y = y + 8;
            if (y >= size_y) {
                y = 0;
                x = x + 8;
                if (x >= size_x)
                    break;
            }
        } else if (tall_chunks == 1) {  /* -d */
            if (step_card == 0 && y + 8 < size_y) {
                y = y + 8;
                step_card = 1;
            } else {
                if (step_card == 1)
                    y = y - 8;
                x = x + 8;
                step_card = 0;
                if (x >= size_x) {
                    x = 0;
                    y = y + 16;
                    if (y >= size_y)
                        break;
                }
            }
        } else {
            x = x + 8;
            if (x >= size_x) {
                x = 0;
                y = y + 8;
                if (y >= size_y)
                    break;
            }
        }
    }
    if (total_errors > 1)
        fprintf(stderr, "Found %d errors while converting image \"%s\".\n", total_errors, input_file);
    else if (total_errors == 1)
        fprintf(stderr, "Found %d error while converting image \"%s\".\n", total_errors, input_file);
    
    /* Generate report file */
    if (generate_report) {
        a = fopen(generate_report, "wb");
        if (a == NULL) {
            fprintf(stderr, "Unable to write report file \"%s\"\n", generate_report);
            err_code = 2;
        } else {
            char header[54];
            
            memset(header, 0, sizeof(header));
            header[0x00] = 'B';     /* Header */
            header[0x01] = 'M';
            c = size_x * size_y * 3 + 54;
            header[0x02] = c;       /* Complete size of file */
            header[0x03] = c >> 8;
            header[0x04] = c >> 16;
            header[0x05] = c >> 24;
            c = 54;
            header[0x0a] = c;       /* Size of header plus palette */
            c = 40;
            header[0x0e] = c;       /* Size of header */
            header[0x12] = size_x;
            header[0x13] = size_x >> 8;
            header[0x16] = size_y;
            header[0x17] = size_y >> 8;
            header[0x1a] = 0x01;    /* 1 plane */
            header[0x1c] = 0x18;    /* 24 bits */
            c = size_x * size_y * 3;
            header[0x22] = c;       /* Complete size of file */
            header[0x23] = c >> 8;
            header[0x24] = c >> 16;
            header[0x25] = c >> 24;
            c = 0x0ec4;             /* 96 dpi */
            header[0x26] = c;       /* X */
            header[0x27] = c >> 8;
            header[0x2a] = c;       /* Y */
            header[0x2b] = c >> 8;
            fwrite(header, 1, sizeof(header), a);
            
            /* Annotate where are used MOBs */
            for (c = 0; c < mob_pointer; c += 3) {
                int sx;
                int sy;
                int x1;
                int y1;
                int x2;
                int y2;
                int d;
                
                x = (mobs[c] & 0xff) - 8;
                y = (mobs[c + 1] & 0x7f) - 8;
                if ((mobs[c] & 0x0400) != 0)
                    sx = 2;
                else
                    sx = 1;
                if ((mobs[c + 1] & 0x0300) == 0x0100)
                    sy = 1;
                else if ((mobs[c + 1] & 0x0300) == 0x0200)
                    sy = 2;
                else if ((mobs[c + 1] & 0x0300) == 0x0400)
                    sy = 4;
                else
                    sy = 0; /* Shouldn't happen */
                for (y1 = 0; y1 < (mobs[c + 1] & 0x80 ? 16 : 8); y1++) {
                    for (x1 = 0; x1 < 8; x1++) {
                        if (mobs[c + 2] & 0x0800)
                            d = bitmaps[(mobs[c + 2] & 0x07f8) + y1];
                        else
                            d = grom[(mobs[c + 2] & 0x07f8) + y1];
                        if ((d & (0x80 >> x1)) == 0)
                            continue;
                        for (y2 = 0; y2 < sy; y2++) {
                            for (x2 = 0; x2 < sx; x2++) {
                                bitmap[(y + y1 * sy + y2) * size_x + (x + x1 * sx + x2)] = 0x40 | (mobs[c + 2] & 7) | ((mobs[c + 2] & 0x1000) >> 9);
                            }
                        }
                    }
                }
            }
            for (y = size_y - 1; y >= 0; y--) {
                for (x = 0; x < size_x; x++) {
                    header[0x00] = colors[(bitmap[y * size_x + x] & 0x0f) * 3];
                    header[0x01] = colors[(bitmap[y * size_x + x] & 0x0f) * 3 + 1];
                    header[0x02] = colors[(bitmap[y * size_x + x] & 0x0f) * 3 + 2];
                    c = (header[0x00] * 30 + header[0x01] * 59 + header[0x02] * 11) / 100;
                    header[0x00] = 0;
                    header[0x01] = 0;
                    header[0x02] = 0;
                    if (bitmap[y * size_x + x] & 0x10) {  /* Error in red */
                        header[0x02] = c;
                    }
                    if (bitmap[y * size_x + x] & 0x20) {  /* GROM in cyan */
                        header[0x00] = c;
                        header[0x01] = c;
                        header[0x02] = 0;
                    }
                    if ((bitmap[y * size_x + x] & 0x70) == 0) {  /* GRAM in green */
                        header[0x00] = 0;
                        header[0x01] = c;
                        header[0x02] = 0;
                    }
                    if (bitmap[y * size_x + x] & 0x40)  /* MOB in use */
                        header[0x00] = header[0x01] = header[0x02] = c;
                    fwrite(header, 1, 3, a);
                }
            }
            fclose(a);
        }
    }
    
    /* Open output file and write result */
    if (arg >= argc) {
        fprintf(stderr, "Missing output file name\n");
        free(bitmap);
        free(screen);
        exit(2);
    }
    a = fopen(argv[arg], "w");
    if (a == NULL) {
        fprintf(stderr, "Error while opening output file\n");
        free(bitmap);
        free(screen);
        exit(2);
    }
    arg++;
    if (arg < argc)
        label = argv[arg];
    if (intybasic == 1) {  /* IntyBASIC mode */
        fprintf(a, "\tREM IntyColor " VERSION "\n");
        fprintf(a, "\tREM Command: ");
        for (c = 0; c < argc; c++) {
            char *b;
            
            b = strchr(argv[c], ' ');
            if (b != NULL)
                fprintf(a, "\"%s\" ", argv[c]);
            else
                fprintf(a, "%s ", argv[c]);
        }
        fprintf(a, "\n");
        fprintf(a, "\tREM Created: %s\n", asctime(date));
        if (stub) {
            fprintf(a, "\tREM stub for showing image\n");
            if (use_constants)
                fprintf(a, "\tINCLUDE \"constants.bas\"\n");
            if (stack_color)
                fprintf(a, "\tMODE 0,%d,%d,%d,%d\n", stack[0], stack[1], stack[2], stack[3]);
            else
                fprintf(a, "\tMODE 1\n");
            for (c = 0; c < number_bitmaps; c += BLOCK_SIZE) {
                fprintf(a, "\tWAIT\n");
                fprintf(a, "\tDEFINE %d,%d,%s_bitmaps_%d\n",
                        c + base_offset,
                        (c + BLOCK_SIZE) >= number_bitmaps ? number_bitmaps - c : BLOCK_SIZE,
                        label, c / BLOCK_SIZE);
            }
            if (magic_mobs) {
                for (c = 0; c < mob_pointer; c += 3) {
                    if (use_constants) {
                        fprintf(a, "\tSPRITE %d,", c/ 3);
                        if (mobs[c] & 0x0200)
                            fprintf(a, "VISIBLE+");
                        if (mobs[c] & 0x0400)
                            fprintf(a, "ZOOMX2+");
                        fprintf(a, "%d,", mobs[c] & 0x00ff);
                        if ((mobs[c + 1] & 0x0080) == 0x0080) {
                            fprintf(a, "DOUBLEY+");
                        }
                        if ((mobs[c + 1] & 0x0300) == 0x0100) {
                            fprintf(a, "ZOOMY2+");
                        } else if ((mobs[c + 1] & 0x0300) == 0x0200) {
                            fprintf(a, "ZOOMY4+");
                        } else if ((mobs[c + 1] & 0x0300) == 0x0300) {
                            fprintf(a, "ZOOMY8+");
                        }
                        if ((mobs[c + 1] & 0x0c00) == 0x0400) {
                            fprintf(a, "FLIPX+");
                        } else if ((mobs[c + 1] & 0x0c00) == 0x0800) {
                            fprintf(a, "FLIPY+");
                        } else if ((mobs[c + 1] & 0x0c00) == 0x0c00) {
                            fprintf(a, "MIRROR+");
                        }
                        fprintf(a, "%d,", mobs[c + 1] & 0x007f);
                        switch (mobs[c + 2] & 0x1007) {
                            case 0x0000:    fprintf(a, "SPR_BLACK+");       break;
                            case 0x0001:    fprintf(a, "SPR_BLUE+");        break;
                            case 0x0002:    fprintf(a, "SPR_RED+");         break;
                            case 0x0003:    fprintf(a, "SPR_TAN+");         break;
                            case 0x0004:    fprintf(a, "SPR_DARKGREEN+");   break;
                            case 0x0005:    fprintf(a, "SPR_GREEN+");       break;
                            case 0x0006:    fprintf(a, "SPR_YELLOW+");      break;
                            case 0x0007:    fprintf(a, "SPR_WHITE+");       break;
                            case 0x1000:    fprintf(a, "SPR_GREY+");        break;
                            case 0x1001:    fprintf(a, "SPR_CYAN+");        break;
                            case 0x1002:    fprintf(a, "SPR_ORANGE+");      break;
                            case 0x1003:    fprintf(a, "SPR_BROWN+");       break;
                            case 0x1004:    fprintf(a, "SPR_PINK+");        break;
                            case 0x1005:    fprintf(a, "SPR_LIGHTBLUE+");   break;
                            case 0x1006:    fprintf(a, "SPR_YELLOWGREEN+"); break;
                            case 0x1007:    fprintf(a, "SPR_PURPLE+");      break;
                        }
                        if (mobs[c + 2] & 0x0800)
                            fprintf(a, "SPR%02d\n", (mobs[c + 2] & 0x07f8) / 8);
                        else
                            fprintf(a, "$%04x\n", mobs[c + 2] & 0x07f8);
                    } else {
                        fprintf(a, "\tSPRITE %d,$%04x,$%04x,$%04x\n", c / 3, mobs[c], mobs[c + 1], mobs[c + 2]);
                    }
                }
            }
            fprintf(a, "\tWAIT\n");
            if (use_print) {
                for (c = 0; c < size_x_cards * size_y_cards; c++) {
                    if (c >= size_x_cards * 12)         /* Avoid writing off-screen */
                        fprintf(a, "'");
                    if (c % size_x_cards == 0)
                        fprintf(a, "\tPRINT AT %d,", c / size_x_cards * 20);
                    fprintf(a, "$%04X", screen[c]);
                    if (c % size_x_cards == size_x_cards - 1 || c + 1 == size_x_cards * size_y_cards) {
                        fprintf(a, "\n");
                    } else {
                        if (c % size_x_cards == 20)     /* Avoid writing off-screen */
                            fprintf(a, "'");
                        fprintf(a, ",");
                    }
                }
            } else {
                if (size_x != 160) {
#if 0
                    fprintf(a, "\tFOR y = 0 TO %d\n", size_y_cards < 12 ? size_y_cards - 1 : 11);
                    fprintf(a, "\tFOR x = 0 TO %d\n", size_x_cards < 20 ? size_x_cards - 1 : 19);
                    fprintf(a, "\tPRINT AT y * 20 + x,%s_cards(y * %d + x)\n", label, size_x_cards);
                    fprintf(a, "\tNEXT x\n");
                    fprintf(a, "\tNEXT y\n");
#endif
                    fprintf(a, "\tSCREEN %s_cards,0,0,%d,%d,%d\n", label, size_x_cards > 20 ? 20 : size_x_cards, size_y_cards > 12 ? 12 : size_y_cards, size_x_cards);
                } else {
                    if (size_y < 96)
                        fprintf(a, "\tSCREEN %s_cards,0,0,20,%d\n", label, size_y_cards);
                    else
                        fprintf(a, "\tSCREEN %s_cards\n", label);
                }
            }
            fprintf(a, "loop:\n");
            fprintf(a, "\tGOTO loop\n\n");
        }
        fprintf(a, "\t' %d bitmaps\n", number_bitmaps);
        for (c = 0; c < number_bitmaps; c++) {
            if (c % BLOCK_SIZE == 0)
                fprintf(a, "%s_bitmaps_%d:\n", label, c / BLOCK_SIZE);
            if (use_bitmap) {
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 0]));
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 1]));
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 2]));
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 3]));
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 4]));
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 5]));
                fprintf(a, "\tBITMAP \"%s\"\n", binary(bitmaps[c * 8 + 6]));
                fprintf(a, "\tBITMAP \"%s\"\n\n", binary(bitmaps[c * 8 + 7]));
            } else {
                fprintf(a, "\tDATA $%02X%02X,$%02X%02X,$%02X%02X,$%02X%02X\n",
                        bitmaps[c * 8 + 1], bitmaps[c * 8 + 0],
                        bitmaps[c * 8 + 3], bitmaps[c * 8 + 2],
                        bitmaps[c * 8 + 5], bitmaps[c * 8 + 4],
                        bitmaps[c * 8 + 7], bitmaps[c * 8 + 6]);
            }
        }
        fprintf(a, "\n");
        if (!use_print && !wants_all) {
            fprintf(a, "\tREM %dx%d cards\n", size_x_cards, size_y_cards);
            fprintf(a, "%s_cards:\n", label);
            for (c = 0; c < size_x_cards * size_y_cards; c++) {
                if (c % size_x_cards == 0)
                    fprintf(a, "\tDATA ");
                fprintf(a, "$%04X", screen[c]);
                if (c % size_x_cards == size_x_cards - 1 || c + 1 == size_x_cards * size_y_cards)
                    fprintf(a, "\n");
                else
                    fprintf(a, ",");
            }
        }
    } else {  /* Assembler code mode */
        fprintf(a, "\t; IntyColor " VERSION "\n");
        fprintf(a, "\t; Command: ");
        for (c = 0; c < argc; c++) {
            char *b;
            
            b = strchr(argv[c], ' ');
            if (b != NULL)
                fprintf(a, "\"%s\" ", argv[c]);
            else
                fprintf(a, "%s ", argv[c]);
        }
        fprintf(a, "\n");
        fprintf(a, "\t; Created: %s\n\n", asctime(date));
        if (magic_mobs && mob_pointer > 0) {
            fprintf(a, "\t; %d mobs\n", mob_pointer / 3);
            fprintf(a, "%s_mobs_total:\tequ %d\n", label, mob_pointer / 3);
            fprintf(a, "%s_mobs_x:", label);
            fprintf(a, "\tDECLE ");
            for (c = 0; c < 24; c += 3) {
                fprintf(a, "$%04X", mobs[c]);
                if (c + 3 < 24)
                    fprintf(a, ",");
            }
            fprintf(a, "\t; X\n");
            fprintf(a, "%s_mobs_y:", label);
            fprintf(a, "\tDECLE ");
            for (c = 0; c < 24; c += 3) {
                fprintf(a, "$%04X", mobs[c + 1]);
                if (c + 3 < 24)
                    fprintf(a, ",");
            }
            fprintf(a, "\t; Y\n");
            fprintf(a, "%s_mobs_a:", label);
            fprintf(a, "\tDECLE ");
            for (c = 0; c < 24; c += 3) {
                fprintf(a, "$%04X", mobs[c + 2]);
                if (c + 3 < 24)
                    fprintf(a, ",");
            }
            fprintf(a, "\t; Attribute\n");
            fprintf(a, "\n");
        }
        fprintf(a, "\t; %d bitmaps\n", number_bitmaps);
        fprintf(a, "\t; Begin %s_bitmaps\n", label);
        fprintf(a, "%s_bitmaps:\n", label);
        for (c = 0; c < number_bitmaps; c++) {
            fprintf(a, "\tDECLE $%02X%02X,$%02X%02X,$%02X%02X,$%02X%02X\n",
                    bitmaps[c * 8 + 1], bitmaps[c * 8 + 0],
                    bitmaps[c * 8 + 3], bitmaps[c * 8 + 2],
                    bitmaps[c * 8 + 5], bitmaps[c * 8 + 4],
                    bitmaps[c * 8 + 7], bitmaps[c * 8 + 6]);
        }
        fprintf(a, "\t; End %s_bitmaps\n", label);
        fprintf(a, "\n");
        if (!wants_all) {
            fprintf(a, "\t; %dx%d cards\n", size_x_cards, size_y_cards);
            fprintf(a, "%s_cards:\n", label);
            for (c = 0; c < size_x_cards * size_y_cards; c++) {
                if (c % size_x_cards == 0)
                    fprintf(a, "\tDECLE ");
                fprintf(a, "$%04X", screen[c]);
                if (c % size_x_cards == size_x_cards - 1 || c + 1 == size_x_cards * size_y_cards)
                    fprintf(a, "\n");
                else
                    fprintf(a, ",");
            }
        }
    }
    fclose(a);
    free(bitmap);
    return 0;
}


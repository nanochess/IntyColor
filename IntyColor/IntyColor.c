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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define VERSION "v0.9 Aug/05/2015"     /* Software version */

#define BLOCK_SIZE   16         /* Before it was 18, reduced for PLAY support */

unsigned char *bitmap;          /* Origin bitmap converted to indexed colors 0-15 */
int size_x;                     /* Size X in pixels */
int size_y;                     /* Size Y in pixels */

int grom_exists;
/* Indicates if GROM file exists for using */
unsigned char grom[256 * 8];    /* Contents of GROM file */

unsigned char bitmaps[64 * 8];  /* Bitmaps created */
int number_bitmaps;             /* Number of bitmaps used */

unsigned char bit[8];

unsigned short *screen;         /* Screen cards */
int size_x_cards;               /* Screen X size in cards */
int size_y_cards;               /* Screen Y size in cards */

signed char *used_color;        /* Array of used colors per card */

int stack_color;                /* Indicates stack color mode */
int current_stack;              /* Current point to color stack */
int stack[4];                   /* Stack of colors */

int mobs[24];
int mob_pointer;

int err_code;

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
int check_for_valid(int color, int x, int y, int x_size, int y_size, int *xo, int *yo)
{
    int x1;
    int y1;
    int x2;
    int y2;
    signed char *current_used;
    int c;
    
    if (x + 8 * x_size > size_x || y + 8 * y_size > size_y)
        return 0;
    
    /* Get the offset for the MOB (trying to optimize) */
    *xo = -1;
    *yo = -1;
    for (y1 = y; y1 < y + 8; y1++) {
        for (x1 = x; x1 < x + 8; x1++) {
            if (bitmap[y1 * size_x + x1] == color)
                break;
        }
        if (x1 < x + 8) {
            *yo = y1 - y;
            break;
        }
    }
    if (*yo == -1)  /* Impossible case */
        return 0;
    for (x1 = x; x1 < x + 8; x1++) {
        for (y1 = y + *yo; y1 < y + *yo + 8; y1++) {
            if (bitmap[y1 * size_x + x1] == color)
                break;
        }
        if (y1 < y + *yo + 8) {
            *xo = x1 - x;
            break;
        }
    }
    if (*xo == -1)  /* Impossible case */
        return 0;
    x += *xo;
    y += *yo;
    if (x + 8 * x_size > size_x) {
        x -= *xo;
        *xo = 0;
    }
    if (y + 8 * y_size > size_y) {
        y -= *yo;
        *yo = 0;
    }
    
    /* Initialize the bitmap */
    memset(bit, 0, sizeof(bit));
    for (y1 = y; y1 < y + 8 * y_size; y1 += y_size) {
        for (x1 = x; x1 < x + 8 * x_size; x1 += x_size) {
            current_used = &used_color[((y1 / 8 * size_x_cards) + (x1 / 8)) * 16];
            c = 0;
            for (c = 0; c < 16 && current_used[c] != -1; c++) {
                if (current_used[c] == color)
                    break;
            }
            if (c >= 16 || current_used[c] == -1)  /* Does use it the color? */
                return 0;  /* No */
            
            /* Check that a whole pixel can be represented with the color */
            c = 0;
            for (y2 = 0; y2 < y_size; y2++) {
                for (x2 = 0; x2 < x_size; x2++) {
                    if (bitmap[(y1 + y2) * size_x + x1 + x2] == color)
                        c++;
                }
            }
            if (c > 0 && c < x_size * y_size)
                return 0;
            if (c == x_size * y_size)
                bit[(y1 - y) / y_size] |= 0x80 >> ((x1 - x) / x_size);
        }
    }
/*    fprintf(stderr, "Valid color %d,x=%d,y=%d,x_size=%d,y_size=%d (xo=%d,yo=%d)\n", color, x, y, x_size, y_size, *xo, *yo);*/
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
                        d = color_foreground;
                        color_foreground = color_background;
                        color_background = d;
                    }
                } else if (c == 256) {
                    c = -1;
                }
            } else {
                c = search_bitmap(&bit[0], grom, 64);
                if (c == 64 && reverse == 2) {
                    mirror_x(&bit[0]);
                    c = search_bitmap(&bit[0], grom, 64);
                    if (c != 256)
                        *yo |= 0x0400;  /* X-flip */
                    mirror_x(&bit[0]);
                }
                if (c == 64 && reverse == 2) {
                    mirror_y(&bit[0]);
                    c = search_bitmap(&bit[0], grom, 64);
                    if (c != 256)
                        *yo |= 0x0800;  /* Y-flip */
                    mirror_y(&bit[0]);
                }
                if (c == 64 && reverse == 2) {
                    mirror_x(&bit[0]);
                    mirror_y(&bit[0]);
                    c = search_bitmap(&bit[0], grom, 64);
                    if (c != 256)
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
                        d = color_foreground;
                        color_foreground = color_background;
                        color_background = d;
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
    unsigned short *ap;
    
    int arg;
    int intybasic = 0;
    int use_bitmap = 0;
    int use_print = 0;
    int magic_mobs = 0;
    int base_offset = 0;
    int use_constants = 1;
    int stub = 1;
    char *label = "screen";
    
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
        fprintf(stderr, "    -n    Removes stub code for display in IntyBASIC mode\n");
        fprintf(stderr, "    -p    Uses PRINT in IntyBASIC mode\n");
        fprintf(stderr, "    -o20  Starts offset for cards in 20 (0-63 is valid)\n");
        fprintf(stderr, "    -m    Tries to use MOBs for more than 2 colors per card\n");
        fprintf(stderr, "    -c    Doesn't use constants.bas for -m option\n");
        fprintf(stderr, "    -i    Generates BITMAP statements instead of DATA\n\n");
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
        fprintf(stderr, "It requires a BMP file of 24/32 bits and for screens it\n");
        fprintf(stderr, "requires a fixed size of 160x96 or any multiple of 8 pixels.\n");
        return 0;
    }
    
    /* Check for GROM to use shapes */
    a = fopen("grom.bin", "rb");
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
        if (c == 'm')  /* -m MOBs mode */
            magic_mobs = 1;
        if (c == 'o')  /* -o11 Initial GRAM card number */
            base_offset = atoi(argv[arg] + 2);
        if (c == 'b')  /* -b IntyBASIC mode */
            intybasic = 1;
        if (c == 'c')  /* -c Doesn't use constants */
            use_constants = 0;
        if (c == 's') {  /* -s0000 Use Color Stack mode */
            stack_color = 1;
            if (strlen(argv[arg]) != 6) {
                fprintf(stderr, "Warning: Ignoring -s argument as size is invalid");
            } else {
                stack[0] = from_hex(argv[arg][2]);
                stack[1] = from_hex(argv[arg][3]);
                stack[2] = from_hex(argv[arg][4]);
                stack[3] = from_hex(argv[arg][5]);
            }
        }
        if (c == 'p')  /* -p Use PRINT statement (IntyBASIC) */
            use_print = 1;
        if (c == 'i')  /* -i Use BITMAP instead of DATA (IntyBASIC) */
            use_bitmap = 1;
        if (c == 'n')  /* -n Remove stub from output */
            stub = 0;
        arg++;
    }
    if (!intybasic) {
        if (use_print)
            fprintf(stderr, "Warning: argument -p is ignored when in assembler mode\n");
        if (use_bitmap)
            fprintf(stderr, "Warning: argument -i is ignored when in assembler mode\n");
        if (stub == 0)
            fprintf(stderr, "Warning: argument -n is ignored when in assembler mode\n");
        if (magic_mobs)
            fprintf(stderr, "Warning: argument -m is ignored when in assembler mode\n");
    } else {
        if (!use_constants && !magic_mobs)
            fprintf(stderr, "Warning: argument -c is ignored if not using -m\n");
    }
    if (arg >= argc) {
        fprintf(stderr, "Missing input file name\n");
        exit(2);
    }
    
    /* Open input file */
    a = fopen(argv[arg], "rb");
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
        fprintf(stderr, "The input file is not in 8/24/32-bits format\n");
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
        fprintf(stderr, "Cannot handle compressed input files\n");
        fclose(a);
        exit(3);
    }
    size_x = buffer[0x12] | (buffer[0x13] << 8);
    size_y = buffer[0x16] | (buffer[0x17] << 8);
    if (size_y >= 32768)
        size_y -= 65536;
    if ((size_x & 7) != 0) {
        fprintf(stderr, "The input file doesn't measure a multiple of 8 in X size\n");
        fclose(a);
        exit(3);
    }
    if ((size_y & 7) != 0) {
        fprintf(stderr, "The input file doesn't measure a multiple of 8 in Y size\n");
        fclose(a);
        exit(3);
    }
    if (size_y >= 0) {
        n = 0;
    } else {
        size_y = -size_y;
        n = 1;
    }
    size_x_cards = size_x / 8;
    size_y_cards = size_y / 8;
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
            bitmap[y * size_x + x] = best_color;
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
                    if (mob_pointer == 24)      /* All MOBs used? */
                        continue;               /* Yes, continue */
                    
                    /* Create the biggest possible MOB from it */
                    best_size = 0;
                    best_color = 0;
                    best_xo = 0;
                    best_yo = 0;
                    memset(best_bit, 0, sizeof(best_bit));
                    for (n = 15; n >= 0; n--) {
                        if (current_used[n] == -1)
                            continue;
                        if (check_for_valid(current_used[n], x, y, 2, 4, &xo, &yo)) {
                            if (2 * 4 > best_size || xo > best_xo || yo > best_yo) {
                                best_size = 0x524;
                                best_color = current_used[n];
                                best_xo = xo;
                                best_yo = yo;
                                memcpy(best_bit, bit, sizeof(best_bit));
                            }
                        } else if (check_for_valid(current_used[n], x, y, 2, 2, &xo, &yo)) {
                            if (2 * 2 > best_size || xo > best_xo || yo > best_yo) {
                                best_size = 0x422;
                                best_color = current_used[n];
                                best_xo = xo;
                                best_yo = yo;
                                memcpy(best_bit, bit, sizeof(best_bit));
                            }
                        } else if (check_for_valid(current_used[n], x, y, 1, 4, &xo, &yo)) {
                            if (1 * 4 > best_size || xo > best_xo || yo > best_yo) {
                                best_size = 0x314;
                                best_color = current_used[n];
                                best_xo = xo;
                                best_yo = yo;
                                memcpy(best_bit, bit, sizeof(best_bit));
                            }
                        } else if (check_for_valid(current_used[n], x, y, 2, 1, &xo, &yo)) {
                            if (2 * 1 > best_size || xo > best_xo || yo > best_yo) {
                                best_size = 0x221;
                                best_color = current_used[n];
                                best_xo = xo;
                                best_yo = yo;
                                memcpy(best_bit, bit, sizeof(best_bit));
                            }
                        } else if (check_for_valid(current_used[n], x, y, 1, 2, &xo, &yo)) {
                            if (1 * 2 > best_size || xo > best_xo || yo > best_yo) {
                                best_size = 0x112;
                                best_color = current_used[n];
                                best_xo = xo;
                                best_yo = yo;
                                memcpy(best_bit, bit, sizeof(best_bit));
                            }
                        } else if (check_for_valid(current_used[n], x, y, 1, 1, &xo, &yo)) {
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
    
    /* Generate the bitmap */
    for (y = 0; y < size_y; y += 8) {
        for (x = 0; x < size_x; x += 8) {  /* For each 8x8 block */
            /* Per mode */
            current_used = &used_color[((y / 8 * size_x_cards) + (x / 8)) * 16];
            if (current_used[2] != -1) {  /* Too many colors in block */
                fprintf(stderr, "Too many colors in block %d,%d (", x, y);
                for (e = 0; e < 16 && current_used[e] != -1; e++) {
                    if (e)
                        fprintf(stderr, ",");
                    fprintf(stderr, "%d", current_used[e]);
                }
                fprintf(stderr, ")\n");
                err_code = 1;
                break;
            }
            color_foreground = current_used[0];
            if (color_foreground == -1)
                color_foreground = 1;
            color_background = current_used[1];  /* Note it can be -1 */
            
            /* Align color per mode */
            if (stack_color) {
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
                    fprintf(stderr,
                            "Background color %d not aligned with color stack (%d or %d) in block %d,%d\n",
                            color_background, stack[current_stack], stack[(current_stack + 1) & 3], x, y);
                    err_code = 1;
                }
            } else {
                if (color_foreground > 7) {
                    c = color_foreground;
                    color_foreground = color_background;
                    color_background = c;
                }
                if (color_foreground > 7) {
                    fprintf(stderr,
                            "Foreground color %d outside of primary colors in block %d,%d\n",
                            color_foreground, x, y);
                    err_code = 1;
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
                    if (bitmap[(y + c) * size_x + x + d] == color_foreground)
                        bit[c] |= 0x80 >> d;
                }
            }
            c = optimize_from_grom(x, y, color_foreground, color_background, 1, NULL);
            if (color_foreground == -1)
                color_foreground = 0;
            if (c >= 256)
                c += base_offset;
            
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
                        if ((mobs[c + 1] & 0x0300) == 0x0100) {
                            fprintf(a, "ZOOMY2+");
                        } else if ((mobs[c + 1] & 0x0300) == 0x0200) {
                            fprintf(a, "ZOOMY4+");
                        } else if ((mobs[c + 1] & 0x0300) == 0x0300) {
                            fprintf(a, "ZOOMY8+");
                        } else if ((mobs[c + 1] & 0x0c00) == 0x0400) {
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
                    fprintf(a, "\tFOR y = 0 TO %d\n", size_y_cards < 12 ? size_y_cards - 1 : 11);
                    fprintf(a, "\tFOR x = 0 TO %d\n", size_x_cards < 20 ? size_x_cards - 1 : 19);
                    fprintf(a, "\tPRINT AT y * 20 + x,%s_cards(y * %d + x)\n", label, size_x_cards);
                    fprintf(a, "\tNEXT x\n");
                    fprintf(a, "\tNEXT y\n");
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
        if (!use_print) {
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
        fprintf(a, "\t; Created: %s\n", asctime(date));
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
        fprintf(a, "\t\n");
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
    fclose(a);
    free(bitmap);
    return 0;
}


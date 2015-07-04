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
//                         non-standard images.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BLOCK_SIZE   16  // Before 18, reduced for PLAY support

char *bitmap;

int grom_exists;
unsigned char grom[256 * 8];

unsigned char bitmaps[64 * 8];
int number_bitmaps;

unsigned char bit[8];

unsigned short *screen;

int stack_color;
int current_stack;
int stack[4];

/*
** The 16 colors available in Intellivision
*/
unsigned char colors[16 * 3] = {
    0, 0, 0,
    0, 45, 255,
    255, 61, 16,
    201, 207, 171,
    
    56, 107, 63,
    0, 167, 86,
    250, 234, 80,
    255, 255, 255,
    
    189, 172, 200,
    36, 184, 255,
    255, 180, 31,
    84, 110, 0,
    
    255, 78, 87,
    164, 150, 255,
    117, 204, 128,
    181, 26, 88,
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
    unsigned char buffer[32];
    unsigned short *ap;
    int best_color;
    int best_difference;
    int arg;
    int intybasic;
    int stub;
    int use_bitmap;
    char *label = "screen";
    int size_x;
    int size_y;
    int size_x_block;
    int err_code;
    int base_offset;
    
    if (argc < 3) {
        fprintf(stderr, "\nConverter from BMP to Intellivision Background/Foreground format\n");
        fprintf(stderr, "v0.8 Jul/04/2015  by Oscar Toledo G. http://nanochess.org\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage:\n\n");
        fprintf(stderr, "    intycolor image.bmp image.asm [label]\n");
        fprintf(stderr, "        Creates image for use with assembler code\n\n");
        fprintf(stderr, "    intycolor -b [-n] [-i] image.bmp image.bas [label]\n");
        fprintf(stderr, "        Creates image for use with IntyBASIC code\n\n");
        fprintf(stderr, "    -n    Removes stub in IntyBASIC code\n");
        fprintf(stderr, "    -o20  Starts offset for cards in 20 (0-63 is valid)\n");
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
    a = fopen("grom.bin", "rb");
    if (a != NULL) {
        fread(grom, 1, sizeof(grom), a);
        fclose(a);
        grom_exists = 1;
    } else {
        grom_exists = 0;
    }
    intybasic = 0;
    stack_color = 0;
    use_bitmap = 0;
    base_offset = 0;
    stub = 1;
    arg = 1;
    while (arg < argc && argv[arg][0] == '-') {
        c = tolower(argv[arg][1]);
        if (c == 'b')
            intybasic = 1;
        if (c == 'o')
            base_offset = atoi(argv[arg] + 2);
        if (c == 's') {
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
        if (c == 'n')
            stub = 0;
        if (c == 'i')
            use_bitmap = 1;
        arg++;
    }
    if (arg >= argc) {
        fprintf(stderr, "Missing input file name\n");
        exit(2);
    }
    a = fopen(argv[arg], "rb");
    arg++;
    if (a == NULL) {
        fprintf(stderr, "Missing input file\n");
        exit(2);
    }
    fread(buffer, 1, 32, a);
    if (buffer[0] != 'B' || buffer[1] != 'M') {
        fprintf(stderr, "The input file is not in BMP format\n");
        fclose(a);
        exit(3);
    }
    if (buffer[0x1c] != 24 && buffer[0x1c] != 32) {
        fprintf(stderr, "The input file is not in 24/32-bits format\n");
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
        fprintf(stderr, "The input file doesn't measure a multiple of 8 in X size\n");
        fclose(a);
        exit(3);
    }
    if (size_y >= 0) {
        n = 0;
    } else {
        size_y = -size_y;
        n = 1;
    }
    size_x_block = size_x / 8;
    bitmap = malloc(size_x * size_y * sizeof(char));
    screen = malloc((size_x_block) * (size_y / 8) * sizeof(unsigned short));
    if (bitmap == NULL || screen == NULL) {
        if (bitmap != NULL)
            free(bitmap);
        if (screen != NULL)
            free(screen);
        fprintf(stderr, "Not enough memory for bitmap\n");
        fclose(a);
        exit(3);
    }
    fseek(a, buffer[10] | (buffer[11] << 8) | (buffer[12] << 16) | (buffer[13] << 24), SEEK_SET);
    for (y = n ? 0 : size_y - 1; n ? y < size_y : y >= 0; n ? y++ : y--) {
        for (x = 0; x < size_x; x++) {
            if (buffer[0x1c] == 24)
                fread(buffer, 1, 3, a);
            else
                fread(buffer, 1, 4, a);
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
    err_code = 0;
    current_stack = 0;
    ap = screen;
    for (y = 0; y < size_y; y += 8) {
        for (x = 0; x < size_x; x += 8) {
            best_color = -1;
            best_difference = -1;
            for (c = 0; c < 8; c++) {
                for (d = 0; d < 8; d++) {
                    if (bitmap[(y + c) * size_x + x + d] == best_color)
                        ;
                    else if (bitmap[(y + c) * size_x + x + d] == best_difference)
                        ;
                    else if (best_color == -1)
                        best_color = bitmap[(y + c) * size_x + x + d];
                    else if (best_difference == -1)
                        best_difference = bitmap[(y + c) * size_x + x + d];
                    else {
                        fprintf(stderr, "Third color %d (before %d and %d) in block %d,%d\n", bitmap[(y + c) * size_x + x + d], best_color, best_difference, x, y);
                        err_code = 1;
                        break;
                    }
                }
            }
            if (best_color > 7) {
                c = best_color;
                best_color = best_difference;
                best_difference = c;
            }
            if (best_color > 7) {
                fprintf(stderr,
                        "Foreground color %d outside of primary colors in block %d,%d\n",
                        best_color, x, y);
                err_code = 1;
            }
            if (stack_color) {
                if (best_color == stack[current_stack]
                    && best_difference != -1) {
                    c = best_color;
                    best_color = best_difference;
                    best_difference = c;
                }
                if (best_color == stack[(current_stack + 1) & 3]
                    && best_difference != stack[current_stack]) {
                    c = best_color;
                    best_color = best_difference;
                    best_difference = c;
                }
                if (best_difference != -1
                    && best_difference != stack[current_stack]
                    && best_difference != stack[(current_stack + 1) & 3]) {
                    fprintf(stderr,
                            "Background color %d not aligned with color stack (%d or %d) in block %d,%d\n",
                            best_difference, stack[current_stack], stack[(current_stack + 1) & 3], x, y);
                    err_code = 1;
                }
            } else {
                if (best_difference == -1) {
                    best_difference = best_color;
                    best_color = -1;
                }
                if (best_color == 0 && best_difference != -1 && best_difference < 8) {
                    c = best_color;
                    best_color = best_difference;
                    best_difference = c;
                }
            }
            for (c = 0; c < 8; c++) {
                bit[c] = 0;
                for (d = 0; d < 8; d++) {
                    if (bitmap[(y + c) * size_x + x + d] == best_color)
                        bit[c] |= 0x80 >> d;
                }
            }
            if (memcmp(&bit[0], "\x00\x00\x00\x00\x00\x00\x00\x00", 8) == 0) {
                c = 0;
            } else {
                if (grom_exists) {
                    if (stack_color) {
                        c = 256;
                        if (best_color < 8) {
                            for (c = 0; c < 256; c++) {
                                if (memcmp(&grom[c * 8], &bit[0], 8) == 0)
                                    break;
                            }
                        }
                        if (c == 256) {
                            for (d = 0; d < 8; d++)
                                bit[d] = ~bit[d];
                            if (best_difference < 8 &&
                                (best_color == stack[current_stack] || best_color == stack[(current_stack + 1) & 3])) {
                                for (c = 0; c < 256; c++) {
                                    if (memcmp(&grom[c * 8], &bit[0], 8) == 0)
                                        break;
                                }
                            }
                            if (c == 256) {
                                for (d = 0; d < 8; d++)
                                    bit[d] = ~bit[d];
                                c = -1;
                            } else {
                                d = best_color;
                                best_color = best_difference;
                                best_difference = d;
                            }
                        }
                    } else {
                        for (c = 0; c < 64; c++) {
                            if (memcmp(&grom[c * 8], &bit[0], 8) == 0)
                                break;
                        }
                        if (c == 64) {
                            for (d = 0; d < 8; d++)
                                bit[d] = ~bit[d];
                            if (best_difference < 8) {
                                for (c = 0; c < 64; c++) {
                                    if (memcmp(&grom[c * 8], &bit[0], 8) == 0)
                                        break;
                                }
                            }
                            if (c == 64) {
                                for (d = 0; d < 8; d++)
                                    bit[d] = ~bit[d];
                                c = -1;
                            } else {
                                d = best_color;
                                best_color = best_difference;
                                best_difference = d;
                            }
                        }
                    }
                } else {
                    c = -1;
                }
                if (c == -1) {
                    for (c = 0; c < number_bitmaps; c++) {
                        if (memcmp(&bitmaps[c * 8], &bit[0], 8) == 0)
                            break;
                    }
                    if (c == number_bitmaps) {
                        if ((stack_color && (best_difference < 8 &&
                                             (best_color == stack[current_stack] || best_color == stack[(current_stack + 1) & 3]))) ||
                            (!stack_color && best_difference < 8)) {
                            for (d = 0; d < 8; d++)
                                bit[d] = ~bit[d];
                            for (c = 0; c < number_bitmaps; c++) {
                                if (memcmp(&bitmaps[c * 8], &bit[0], 8) == 0)
                                    break;
                            }
                            if (c < number_bitmaps) {
                                d = best_color;
                                best_color = best_difference;
                                best_difference = d;
                            } else {
                                for (d = 0; d < 8; d++)
                                    bit[d] = ~bit[d];
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
            if (best_color == -1)
                best_color = 0;
            if (stack_color) {
                if (best_difference == stack[current_stack])
                    d = 0;
                else if (best_difference == stack[(current_stack + 1) & 3]) {
                    d = 0x2000;
                    current_stack = (current_stack + 1) & 3;
                } else
                    d = 0;
                *ap++ = ((c + base_offset) << 3) | (best_color & 7) | ((best_color & 8) << 9) | d;
            } else {
                if (best_difference == -1)
                    best_difference = 0;
                *ap++ = ((c + base_offset) << 3) | best_color | ((best_difference & 3) << 9) | ((best_difference & 0x04) << 11) | ((best_difference & 0x08) << 9);
            }
        }
    }
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
    if (intybasic == 1) {
        if (stub) {
            fprintf(a, "\tREM stub for showing image\n");
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
            fprintf(a, "\tWAIT\n");
            if (size_x != 160) {
                fprintf(a, "\tFOR y = 0 TO %d\n", size_y / 8 < 12 ? size_y / 8 - 1 : 11);
                fprintf(a, "\tFOR x = 0 TO %d\n", size_x_block < 20 ? size_x_block - 1 : 19);
                fprintf(a, "\tPRINT AT y * 20 + x,%s_cards(y * %d + x)\n", label, size_x_block);
                fprintf(a, "\tNEXT x\n");
                fprintf(a, "\tNEXT y\n");
            } else {
                if (size_y < 96)
                    fprintf(a, "\tSCREEN %s_cards,0,0,20,%d\n", label, size_y / 8);
                else
                    fprintf(a, "\tSCREEN %s_cards\n", label);
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
        fprintf(a, "\tREM %dx%d cards\n", size_x_block, size_y / 8);
        fprintf(a, "%s_cards:\n", label);
        for (c = 0; c < size_x_block * (size_y / 8); c++) {
            if (c % size_x_block == 0)
                fprintf(a, "\tDATA ");
            fprintf(a, "$%04X", screen[c]);
            if (c % size_x_block == size_x_block - 1 || c + 1 == size_x_block * (size_y / 8))
                fprintf(a, "\n");
            else
                fprintf(a, ",");
        }
    } else {
        fprintf(a, "\t; %d bitmaps\n", number_bitmaps);
        fprintf(a, "%s_bitmaps:\n", label);
        for (c = 0; c < number_bitmaps; c++) {
            fprintf(a, "\tDECLE $%02X%02X,$%02X%02X,$%02X%02X,$%02X%02X\n",
                    bitmaps[c * 8 + 1], bitmaps[c * 8 + 0],
                    bitmaps[c * 8 + 3], bitmaps[c * 8 + 2],
                    bitmaps[c * 8 + 5], bitmaps[c * 8 + 4],
                    bitmaps[c * 8 + 7], bitmaps[c * 8 + 6]);
        }
        fprintf(a, "\t\n");
        fprintf(a, "\t; %dx%d cards\n", size_x_block, size_y / 8);
        fprintf(a, "%s_cards:\n", label);
        for (c = 0; c < size_x_block * (size_y / 8); c++) {
            if (c % size_x_block == 0)
                fprintf(a, "\tDECLE ");
            fprintf(a, "$%04X", screen[c]);
            if (c % size_x_block == size_x_block - 1 || c + 1 == size_x_block * (size_y / 8))
                fprintf(a, "\n");
            else
                fprintf(a, ",");
        }
    }
    fclose(a);
    free(bitmap);
    return 0;
}

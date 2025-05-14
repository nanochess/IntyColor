IntyColor converter v1.4.0 May/14/2025
by Oscar Toledo Gutierrez. http://nanochess.org/

This utility converts BMP/PNG files to Intellivision graphics format, generates both
assembly and IntyBASIC code.
 
Usage:

    intycolor image.bmp image.asm [label]
        Creates image for use with assembler code

    intycolor -b [-n] [-p] [-i] image.bmp image.bas [label]
        Creates image for use with IntyBASIC code

    -n     Removes stub code for display in IntyBASIC mode
    -p     Uses PRINT in IntyBASIC mode
    -o20   Initial offset for cards is 20 (0-63 is valid)
    -m     Tries to use MOBs for more than 2 colors per card
    -c     Doesn't use constants.bas for -m option
    -i     Generates BITMAP statements instead of DATA
    -i2    Generates BITMAP statements using X and .
    -r output.bmp  Generate BMP report of conversion in file
                   red = error, green = GRAM, yellow = GROM
                   grey = MOB
    -g clue.txt    Exact clues for -m, text file up to 8 lines:
                   x,y,color[,x_zoom(1-2),y_zoom(1-4),0/1]
                   The final 0/1 indicates 8x8 or 8x16;
                   Suggestion: run with empty text file and
                   option -r to see what cards require MOBs
    -x p/grom.bin  Path and file for grom.bin, by default it
                   searches in current path for grom.bin
    -fx    Flip image along the X-coordinate (mirror)
    -fy    Flip image along the Y-coordinate
    -a     All 8x8 cards as continuous bitmap in output
    -e45d2 Replace color 4 with 5 and d with 2 before process,
           useful to recreate same image with other colors.
    -d     Process image in chunks of 16 pixels high, useful
           to create MOB bitmaps.
    -v     Process 8x8 cards in vertical direction first.
           Useful for horizontal scrolling bitmaps and -a option.
    -k4    Add 4 blank cards to generated data
    -kx4   Pad generated data to a multiple of 4 cards
    -q16   Define bitmaps in blocks of 16 cards (default)
           When not using music player in IntyBASIC, limit is 18.
           When using ECS music player in IntyBASIC, limit is 13.
    -t     Tutorvision mode (GRAM supports 256 defined shapes)
           Only available for Color Stack mode.

By default intycolor creates images for use with Intellivision
Background/Foreground video format, you can use 8 primary
colors and 16 background colors for each 8x8 block.

Using the flag -s0123 creates images for use with Color Stack
mode, the 0123 can be replaced with the sequence of colors
you'll program in the Color Stack registers (hexadecimal 0-f)
intycolor will warn you if your image cannot be represented
by a real Intellivision

If you add the plus sign (for example -s0000+) it will try to
to replace solid 4x4 blocks with Colored Squares cards
(allowing 4 colors per 8x8 block).

It can use GROM characters if you provide grom.bin in current
directory.

It requires a BMP file of 8/24/32 bits, remember Intellivision
screen is a fixed size of 160x96 pixels but this utility will
accept any multiple of 8 pixels in any coordinate.

The -a option is for working over monochrome bitmaps and
generating a continous bitmap for scrolling with more than
the limit of GRAM definitions (the program must define the
bitmaps as the scrolling goes on).

/****************************************************************************
* FILE:      common.h														*
* CONTENTS:  Common definitions												*
* COPYRIGHT: MadLab Ltd. 2025,26											*
* AUTHOR:    James Hutchby													*
* UPDATED:   21/05/26														*
****************************************************************************/

#include "pico/stdlib.h"


#define DEBUG					// define if debugging

#define SIZE 240				// display size in pixels
#define WIDTH 240				// display width in pixels
#define HEIGHT 240				// display height in pixels

#define NUM_LCDS 6				// number of LCD displays

// standard colours
#define BLACK 0x0000
#define BLUE 0x001f
#define RED 0xf800
#define GREEN 0x07e0
#define CYAN 0x07ff
#define MAGENTA 0xf81f
#define YELLOW 0xffe0
#define WHITE 0xffff
#define ORANGE 0xfc00

#define NUM_FONTS 14			// number of fonts including 'matrix'

#define NUM_FXS 14				// number of transition effects

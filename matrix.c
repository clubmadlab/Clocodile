/****************************************************************************
* FILE:      matrix.c														*
* CONTENTS:  Matrix raining code effect										*
* COPYRIGHT: MadLab Ltd. 2026												*
* AUTHOR:    James Hutchby													*
* UPDATED:   21/05/26														*
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "pico/rand.h"

#include "common.h"
#include "lcd.h"


//---------------------------------------------------------------------------
// constants
//---------------------------------------------------------------------------

#define CHAR_WIDTH 32
#define CHAR_HEIGHT 32
#define TAIL_LEN 7
#define NUM_STRINGS 5

#define X_MIN (WIDTH*1/8)
#define X_MAX (WIDTH*7/8)
#define X_WIDTH ((X_MAX-X_MIN)/NUM_STRINGS)
#define X_SPREAD (X_WIDTH-CHAR_WIDTH)

#define SPEED_MIN 1
#define SPEED_SPREAD 3


//---------------------------------------------------------------------------
// variables
//---------------------------------------------------------------------------

static uint16_t* digit_buffers[1+TAIL_LEN][10];
static int head_x[NUM_LCDS][NUM_STRINGS], head_y[NUM_LCDS][NUM_STRINGS];
static int tail_x[NUM_LCDS][NUM_STRINGS][TAIL_LEN], tail_y[NUM_LCDS][NUM_STRINGS][TAIL_LEN];
static int head_cnt[NUM_LCDS][NUM_STRINGS];
static int speed[NUM_LCDS][NUM_STRINGS];


//---------------------------------------------------------------------------
// linkage
//---------------------------------------------------------------------------

extern const uint16_t __in_flash() font14_zero[];
extern const uint16_t __in_flash() font14_one[];
extern const uint16_t __in_flash() font14_two[];
extern const uint16_t __in_flash() font14_three[];
extern const uint16_t __in_flash() font14_four[];
extern const uint16_t __in_flash() font14_five[];
extern const uint16_t __in_flash() font14_six[];
extern const uint16_t __in_flash() font14_seven[];
extern const uint16_t __in_flash() font14_eight[];
extern const uint16_t __in_flash() font14_nine[];


//---------------------------------------------------------------------------
// functions
//---------------------------------------------------------------------------

static void copy_image(const uint16_t* s, uint16_t* d, float brightness)
{
	for (int y = 0; y < CHAR_HEIGHT/2; y++)
	{
		for (int x = 0; x < CHAR_WIDTH/2; x++)
		{
			uint16_t r = (*s >> 11) & 0x1f;
			uint16_t g = (*s >> 5) & 0x3f;
			uint16_t b = (*s >> 0) & 0x1f;

			s++;

			r = (uint16_t) round(r * brightness);
			g = (uint16_t) round(g * brightness);
			b = (uint16_t) round(b * brightness);

			d[x] = d[x + CHAR_WIDTH/2] = (r << 11) | (g << 5) | b;
		}

		d += CHAR_WIDTH;
	}
}


static void clear_tail(uint lcd, uint string)
{
	for (int i = 0; i < TAIL_LEN; i++)
		tail_x[lcd-1][string][i] = tail_y[lcd-1][string][i] = -1;
}

static void draw_tail(uint lcd, uint string, uint digit)
{
	for (int i = 0; i < TAIL_LEN; i++)
	{
		int x = tail_x[lcd-1][string][i];
		int y = tail_y[lcd-1][string][i];
		if (x == -1 && y == -1) continue;;
		DrawChar(lcd, x, y, CHAR_WIDTH, CHAR_HEIGHT, digit_buffers[1 + i][digit]);
	}
}

static void add_tail(uint lcd, uint string, int x, int y)
{
	for (int i = TAIL_LEN - 1; i > 0; i--)
	{
		tail_x[lcd-1][string][i] = tail_x[lcd-1][string][i-1];
		tail_y[lcd-1][string][i] = tail_y[lcd-1][string][i-1];
	}
	tail_x[lcd-1][string][0] = x;
	tail_y[lcd-1][string][0] = y;
}


// initialises the matrix effect
void InitMatrix()
{
	// allocate digit buffers
	for (uint n = 0; n < 1 + TAIL_LEN; n++)
		for (uint i = 0; i < 10; i++)
			digit_buffers[n][i] = malloc(CHAR_WIDTH/2 * CHAR_HEIGHT * sizeof(uint16_t));

	// fade digits
	float brightness = 1.0f;
	for (uint n = 0; n < 1 + TAIL_LEN; n++)
	{
		if (n == TAIL_LEN) brightness = 0;
		copy_image(font14_zero, digit_buffers[n][0], brightness);
		copy_image(font14_one, digit_buffers[n][1], brightness);
		copy_image(font14_two, digit_buffers[n][2], brightness);
		copy_image(font14_three, digit_buffers[n][3], brightness);
		copy_image(font14_four, digit_buffers[n][4], brightness);
		copy_image(font14_five, digit_buffers[n][5], brightness);
		copy_image(font14_six, digit_buffers[n][6], brightness);
		copy_image(font14_seven, digit_buffers[n][7], brightness);
		copy_image(font14_eight, digit_buffers[n][8], brightness);
		copy_image(font14_nine, digit_buffers[n][9], brightness);
		brightness *= 0.7f;
	}

	for (uint lcd = 1; lcd <= NUM_LCDS; lcd++)
	{
		for (uint string = 0; string < NUM_STRINGS; string++)
		{
			head_x[lcd-1][string] = head_y[lcd-1][string] = -1;
			head_cnt[lcd-1][string] = CHAR_HEIGHT;

			clear_tail(lcd, string);
		}
	}
}


// matrix effect executive
void DoMatrix(uint lcd, uint digit)
{
	for (uint string = 0; string < NUM_STRINGS; string++)
	{
		if (head_x[lcd-1][string] == -1 && head_y[lcd-1][string] == -1)
		{
			head_x[lcd-1][string] = X_MIN + X_WIDTH/2 + string * X_WIDTH - CHAR_WIDTH/2 + (get_rand_32() % X_SPREAD) - X_SPREAD/2;
			head_y[lcd-1][string] = -(get_rand_32() % (CHAR_HEIGHT*2));
			speed[lcd-1][string] = SPEED_MIN + (get_rand_32() % (SPEED_SPREAD + 1));
		}

		head_cnt[lcd-1][string] -= speed[lcd-1][string];
		if (head_cnt[lcd-1][string] <= 0)
		{
			add_tail(lcd, string, head_x[lcd-1][string], head_y[lcd-1][string]);
			draw_tail(lcd, string, digit);
			head_cnt[lcd-1][string] = CHAR_HEIGHT;
		}
		else
		{
			int x = tail_x[lcd-1][string][0];
			int y = tail_y[lcd-1][string][0];
			if (x != -1 || y != -1) DrawChar(lcd, x, y, CHAR_WIDTH, CHAR_HEIGHT, digit_buffers[1][digit]);
		}

		DrawChar(lcd, head_x[lcd-1][string], head_y[lcd-1][string], CHAR_WIDTH, CHAR_HEIGHT, digit_buffers[0][digit]);

		head_y[lcd-1][string] += speed[lcd-1][string];
		if (head_y[lcd-1][string] > HEIGHT) head_x[lcd-1][string] = head_y[lcd-1][string] = -1;
	}
}

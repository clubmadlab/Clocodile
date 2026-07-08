/****************************************************************************
* FILE:      lcd.c															*
* CONTENTS:  LCD display													*
* COPYRIGHT: MadLab Ltd. 2025,26											*
* AUTHOR:    James Hutchby													*
* UPDATED:   27/05/26														*
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/rand.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#include "common.h"
#include "lcd.h"
#include "lcd.pio.h"


//---------------------------------------------------------------------------
// port assignments
//---------------------------------------------------------------------------

// serial clock (idle high)
#define SCL1_PIN 10
#define SCL2_PIN 13
#define SCL3_PIN 18
#define SCL4_PIN 28
#define SCL5_PIN 4
#define SCL6_PIN 7

// serial data
#define SDA1_PIN 9
#define SDA2_PIN 12
#define SDA3_PIN 17
#define SDA4_PIN 27
#define SDA5_PIN 3
#define SDA6_PIN 6

// data/command (high = data, low = command)
#define DC1_PIN 8
#define DC2_PIN 11
#define DC3_PIN 16
#define DC4_PIN 26
#define DC5_PIN 2
#define DC6_PIN 5

// resets (active low)
#define RES1_PIN 0
#define RES2_PIN 1

// LCD type (1 = 7-pin LCDs, 2 = 8-pin LCDs)
#define LCD_TYPE 1


//---------------------------------------------------------------------------
// constants
//---------------------------------------------------------------------------

#define SPI_SPEED 48000000L			// SPI speed in Hz

#define NUM_DMA NUM_LCDS			// number of DMA channels


//---------------------------------------------------------------------------
// GC9A01 commands
//---------------------------------------------------------------------------

#define SWRESET 0x01				// software reset (not documented)

// level 1
#define RDDID 0x04					// read display identification information
#define RDDST 0x09					// read display status
#define SLPOUT 0x11					// sleep out mode
#define PTLON 0x12					// partial mode on
#define NORON 0x13					// normal display mode on
#define INVOFF 0x20					// display inversion off
#define INVON 0x21					// display inversion on
#define DISPOFF 0x28				// display off
#define DISPON 0x29					// display on
#define CASET 0x2a					// column address set
#define RASET 0x2b					// row address set
#define RAMWR 0x2c					// memory write
#define PTLAR 0x30					// partial area
#define VSCRDEF 0x33				// vertical scrolling definition
#define TEOFF 0x34					// tearing effect line off
#define TEON 0x35					// tearing effect line on
#define MADCTL 0x36					// memory access control
#define VSCSAD 0x37					// vertical scrolling start address
#define IDMOFF 0x38					// idle mode off
#define IDMON 0x39					// idle mode on
#define COLMOD 0x3a					// pixel format set
#define WRMEMC 0x3c					// write memory continue
#define STE 0x44					// set tear scanline
#define GSCAN 0x45					// get scanline
#define WRDISBV 0x51				// write display brightness
#define WRCTRLD 0x53				// write CTRL display
#define RDID1 0xda					// read ID1
#define RDID2 0xdb					// read ID2
#define RDID3 0xdc					// read ID3

// level 2
#define RGB_CONTROL 0xb0			// RGB interface signal control
#define BLANKING_CONTROL 0xb5		// blanking porch control
#define DISPLAY_CONTROL 0xb6		// display function control
#define TEARING_CONTROL 0xba		// tearing effect control
#define INTERFACE_CONTROL 0xf6		// interface control

// level 3
#define FRAMERATE 0xe8				// frame rate
#define SPI2DATA 0xe9				// SPI 2DATA control
#define POWER_CONTROL1 0xc1			// power control 1
#define POWER_CONTROL2 0xc3			// power control 2
#define POWER_CONTROL3 0xc4			// power control 3
#define POWER_CONTROL4 0xc9			// power control 4
#define POWER_CONTROL7 0xa7			// power control 7
#define INTER_ENABLE1 0xfe			// inter register enable1
#define INTER_ENABLE2 0xef			// inter register enable2
#define SET_GAMMA1 0xf0				// set gamma1
#define SET_GAMMA2 0xf1				// set gamma2
#define SET_GAMMA3 0xf2				// set gamma3
#define SET_GAMMA4 0xf3				// set gamma4

#define MADCTL_MY (1<<7)			// page address order
#define MADCTL_MX (1<<6)			// column address order
#define MADCTL_MV (1<<5)			// page/column order
#define MADCTL_ML (1<<4)			// line address order
#define MADCTL_RGB (0<<3)			// RGB order
#define MADCTL_BGR (1<<3)			// BGR order
#define MADCTL_MH (1<<2)			// display data latch order


//---------------------------------------------------------------------------
// linkage
//---------------------------------------------------------------------------

extern uint CurrentFont;
extern uint CurrentFx;

static void wait_pio_idle(uint);


//---------------------------------------------------------------------------
// variables
//---------------------------------------------------------------------------

// DMA pointers, current and transition images
static uint16_t* dma_pnt1[NUM_DMA];
static uint16_t* dma_pnt2[NUM_DMA];

// line counters
static uint16_t dma_cnt[NUM_DMA];

// transition depth [0, SIZE]
uint TransitionDepth;

// transition stage
uint TransitionStage = 0;

static uint dma_fx[NUM_DMA];
static uint16_t black_line[WIDTH];
static uint16_t white_line[WIDTH];
static uint dissolve[HEIGHT/2];
static uint flip[NUM_DMA];
static float delta1[NUM_DMA], delta2[NUM_DMA];
static float ycoord1[NUM_DMA], ycoord2[NUM_DMA];


//---------------------------------------------------------------------------
// functions
//---------------------------------------------------------------------------

// serial clock pin
static uint scl_pin(uint lcd)
{
	switch (lcd)
	{
		case 1: return SCL1_PIN;
		case 2: return SCL2_PIN;
		case 3: return SCL3_PIN;
		case 4: return SCL4_PIN;
		case 5: return SCL5_PIN;
		case 6: return SCL6_PIN;
	}
	return 0;
}

// serial data pin
static uint sda_pin(uint lcd)
{
	switch (lcd)
	{
		case 1: return SDA1_PIN;
		case 2: return SDA2_PIN;
		case 3: return SDA3_PIN;
		case 4: return SDA4_PIN;
		case 5: return SDA5_PIN;
		case 6: return SDA6_PIN;
	}
	return 0;
}

// data/command pin
static uint dc_pin(uint lcd)
{
	switch (lcd)
	{
		case 1: return DC1_PIN;
		case 2: return DC2_PIN;
		case 3: return DC3_PIN;
		case 4: return DC4_PIN;
		case 5: return DC5_PIN;
		case 6: return DC6_PIN;
	}
	return 0;
}

// lcd = 1 to 6
#define lcd_pio(lcd) ((lcd) <= 3 ? pio0 : pio1)			// PIO instance, 0 or 1
#define lcd_sm(lcd) (((lcd) - 1) % 4)					// state machine instance, 0 to 3
#define lcd_dma(lcd) ((lcd) - 1)						// DMA channel, 0 to 5

static inline bool pio_sm_is_enabled(PIO pio, uint sm)
{
	check_pio_param(pio);
	check_sm_param(sm);
	return pio->ctrl & (1u << sm);
}


// bit bangs a byte to the display
static inline void bitbang_byte(uint lcd, uint8_t b)
{
	for (int n = 7; n >= 0; n--)
	{
		gpio_put(sda_pin(lcd), b & (1 << n) ? 1 : 0);

		gpio_put(scl_pin(lcd), 0);
		gpio_put(scl_pin(lcd), 1);
	}
}

// transmits a command byte to the display
static inline void tx_command(uint lcd, uint8_t b)
{
	// command
	gpio_put(dc_pin(lcd), 0);
	bitbang_byte(lcd, b);
}

// transmits a data byte to the display
static inline void tx_data(uint lcd, uint8_t b)
{
	// data
	gpio_put(dc_pin(lcd), 1);
	bitbang_byte(lcd, b);
}

// transmits a data word to the display
static inline void tx_word(uint lcd, uint16_t w)
{
	// data
	gpio_put(dc_pin(lcd), 1);
	bitbang_byte(lcd, w >> 8);
	bitbang_byte(lcd, w & 0xff);
}


// selects GPIO output
static void select_gpio(uint lcd)
{
	pio_sm_set_enabled(lcd_pio(lcd), lcd_sm(lcd), false);

	gpio_set_dir(scl_pin(lcd), GPIO_OUT);
	gpio_put(scl_pin(lcd), 1);
	gpio_set_function(scl_pin(lcd), GPIO_FUNC_SIO);

	gpio_set_dir(sda_pin(lcd), GPIO_OUT);
	gpio_put(sda_pin(lcd), 0);
	gpio_set_function(sda_pin(lcd), GPIO_FUNC_SIO);

	gpio_set_dir(dc_pin(lcd), GPIO_OUT);
	gpio_set_function(dc_pin(lcd), GPIO_FUNC_SIO);
}

// selects PIO output
static void select_pio(uint lcd)
{
	pio_sm_set_enabled(lcd_pio(lcd), lcd_sm(lcd), true);

	uint pin_mask = (1u << sda_pin(lcd)) | (1u << scl_pin(lcd));
	pio_sm_set_pindirs_with_mask(lcd_pio(lcd), lcd_sm(lcd), pin_mask, pin_mask);
	pio_sm_set_pins_with_mask(lcd_pio(lcd), lcd_sm(lcd), (1u << scl_pin(lcd)), pin_mask);

    gpio_set_function(scl_pin(lcd), lcd_pio(lcd) == pio0 ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
    gpio_set_function(sda_pin(lcd), lcd_pio(lcd) == pio0 ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);

	// data
	gpio_put(dc_pin(lcd), 1);
}


// initialises all the displays
void InitAllDisplays()
{
	// clear black and white lines
	for (uint i = 0; i < WIDTH; i++)
	{
		black_line[i] = BLACK;
		white_line[i] = WHITE;
	}

	// randomise dissolve order
	for (uint i = 0; i < HEIGHT/2; i++) dissolve[i] = i;
	for (uint i = 0; i < HEIGHT/4; i++)
	{
		uint swap1 = (get_rand_32() % HEIGHT/2);
		uint swap2 = (get_rand_32() % HEIGHT/2);
		uint temp = dissolve[swap1];
		dissolve[swap1] = dissolve[swap2];
		dissolve[swap2] = temp;
	}

	// configure ports
	for (uint i = 1; i <= NUM_LCDS; i++) select_gpio(i);

	// configure reset pins
	gpio_init(RES1_PIN);
	gpio_set_dir(RES1_PIN, GPIO_OUT);
	gpio_put(RES1_PIN, 1);
	gpio_init(RES2_PIN);
	gpio_set_dir(RES2_PIN, GPIO_OUT);
	gpio_put(RES2_PIN, 1);

	// reset displays
	gpio_put(RES1_PIN, 1);
	gpio_put(RES2_PIN, 1);
	sleep_ms(10);
	gpio_put(RES1_PIN, 0);
	gpio_put(RES2_PIN, 0);
	sleep_ms(10);
	gpio_put(RES1_PIN, 1);
	gpio_put(RES2_PIN, 1);
	sleep_ms(150);

	for (uint i = 0; i < NUM_DMA; i++) dma_cnt[i] = 0;

	for (uint i = 1; i <= NUM_LCDS; i++) InitDisplay(i);

	dma_set_irq0_channel_mask_enabled((1u << NUM_DMA) - 1, true);
}


static void dma_handler()
{
	uint depth = TransitionDepth & ~1u;

	for (uint dma_chan = 0; dma_chan < NUM_DMA; dma_chan++)
	{
		if (!(dma_hw->ints0 & (1u << dma_chan))) continue;

		// clear the interrupt request
		dma_hw->ints0 = 1u << dma_chan;

		if (dma_cnt[dma_chan] == 0) continue;

		bool odd = dma_cnt[dma_chan] & 1;

		switch (dma_fx[dma_chan])
		{
		// clear screen
		case 0:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
			break;

		// simple replacement
		case 1:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// scroll down both
		case 2:
			if (HEIGHT - dma_cnt[dma_chan] < depth)
			{
				dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
				if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			}
			else
			{
				dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt1[dma_chan], WIDTH/2);
				if (odd) dma_pnt1[dma_chan] += WIDTH/2;
			}
			break;

		// scroll down over
		case 3:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// overwrite
		case 4:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// slide in from the left
		case 5:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// overwrite from the left
		case 6:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// blinds
		case 7:
			void* line = CurrentFont == 12 ? white_line : black_line;
			if (depth <= HEIGHT/2)
			{
				bool blank = ((HEIGHT - dma_cnt[dma_chan]) % (HEIGHT/4)) < depth/2;
				if (blank) dma_channel_transfer_from_buffer_now(dma_chan, line, WIDTH/2);
				else dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt1[dma_chan], WIDTH/2);
				if (odd) dma_pnt1[dma_chan] += WIDTH/2;
			}
			else
			{
				bool blank = ((HEIGHT - dma_cnt[dma_chan]) % (HEIGHT/4)) >= (depth - HEIGHT/2)/2;
				if (blank) dma_channel_transfer_from_buffer_now(dma_chan, line, WIDTH/2);
				else dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
				if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			}
			break;

		// expand outwards from centre
		case 8:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// expand outwards from four corners in turn
		case 9:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// slide outwards from four corners in turn
		case 10:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// expand outwards from centre in a spiral
		case 11:
			#define SPIRAL_SQUARES 7
			#define SPIRAL_TILE (SIZE/SPIRAL_SQUARES)
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], (SIZE/SPIRAL_SQUARES)/2);
			if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			break;

		// erase outwards from centre then expand outwards
		case 12:
			if (depth <= HEIGHT/2)
			{
				void* line = CurrentFont == 12 ? white_line : black_line;
				dma_channel_transfer_from_buffer_now(dma_chan, line, depth);
			}
			else
			{
				depth -= HEIGHT/2;
				dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth);
				if (odd) dma_pnt2[dma_chan] += WIDTH/2;
			}
			break;

		// dissolve
		case 13:
			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
			break;

		// flip clock
		case 14:
			if (flip[dma_chan] > 0)
			{
				flip[dma_chan]--;
				ycoord2[dma_chan] += delta2[dma_chan];
				int y = (int) (ycoord2[dma_chan]/2);
				dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan] + y * WIDTH/2, WIDTH/2);
			}
			else
			{
				int y = (int) (ycoord1[dma_chan]/2);
				dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt1[dma_chan] + y * WIDTH/2, WIDTH/2);
				ycoord1[dma_chan] += delta1[dma_chan];
			}
			break;
		}

		dma_cnt[dma_chan]--;
	}
}


// initialises an LCD display, fed with LCD number (1-6)
void InitDisplay(uint lcd)
{
	// 2 system clocks per SPI clock
	uint32_t clock_mhz = clock_get_hz(clk_sys) / 1000000L;
	uint32_t spi_mhz = (SPI_SPEED * 2) / 1000000L;

	uint16_t div = (uint16_t) (clock_mhz / spi_mhz);
	uint32_t rem = clock_mhz - spi_mhz * div;
	uint8_t frac = (uint8_t) ((rem * 256L) / spi_mhz);

	uint offset = pio_add_program(lcd_pio(lcd), &lcd_program);
	lcd_program_init(lcd_pio(lcd), lcd_sm(lcd), offset, sda_pin(lcd), scl_pin(lcd), div, frac);

	dma_channel_claim(lcd_dma(lcd));
	dma_channel_config config = dma_channel_get_default_config(lcd_dma(lcd));
	channel_config_set_read_increment(&config, true);
	dma_channel_set_write_addr(lcd_dma(lcd), &lcd_pio(lcd)->txf[lcd_sm(lcd)], false);
	channel_config_set_write_increment(&config, false);
	channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
	channel_config_set_dreq(&config, (lcd_pio(lcd) == pio0 ? DREQ_PIO0_TX0 : DREQ_PIO1_TX0) + lcd_sm(lcd));
	channel_config_set_enable(&config, true);
	dma_channel_set_irq0_enabled(lcd_dma(lcd), true);
	dma_channel_set_irq1_enabled(lcd_dma(lcd), false);
	dma_channel_set_config(lcd_dma(lcd), &config, false);

	irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
	irq_set_enabled(DMA_IRQ_0, true);

	select_gpio(lcd);

	// software reset
	tx_command(lcd, SWRESET);
	sleep_ms(50);

	tx_command(lcd, INTER_ENABLE2);

	tx_command(lcd, 0xEB);
	tx_data(lcd, 0x14);

	tx_command(lcd, INTER_ENABLE1);
	tx_command(lcd, INTER_ENABLE2);

	tx_command(lcd, 0xEB);
	tx_data(lcd, 0x14);

	tx_command(lcd, 0x84);
	tx_data(lcd, 0x40);

	tx_command(lcd, 0x85);
	tx_data(lcd, 0xFF);

	tx_command(lcd, 0x86);
	tx_data(lcd, 0xFF);

	tx_command(lcd, 0x87);
	tx_data(lcd, 0xFF);

	tx_command(lcd, 0x88);
	tx_data(lcd, 0x0A);

	tx_command(lcd, 0x89);
	tx_data(lcd, 0x21);

	tx_command(lcd, 0x8A);
	tx_data(lcd, 0x00);

	tx_command(lcd, 0x8B);
	tx_data(lcd, 0x80);

	tx_command(lcd, 0x8C);
	tx_data(lcd, 0x01);

	tx_command(lcd, 0x8D);
	tx_data(lcd, 0x01);

	tx_command(lcd, 0x8E);
	tx_data(lcd, 0xFF);

	tx_command(lcd, 0x8F);
	tx_data(lcd, 0xFF);

	tx_command(lcd, DISPLAY_CONTROL);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x00);

	// set memory data access control - BGR
	tx_command(lcd, MADCTL);
	#if LCD_TYPE == 1
	tx_data(lcd, MADCTL_MX | MADCTL_BGR);
	#endif
	#if LCD_TYPE == 2
	tx_data(lcd, MADCTL_BGR);
	#endif

	// set colour mode
	tx_command(lcd, COLMOD);
	// 16-bit colour
	tx_data(lcd, 0x55);

	tx_command(lcd, 0x90);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x08);

	tx_command(lcd, 0xBD);
	tx_data(lcd, 0x06);

	tx_command(lcd, 0xBC);
	tx_data(lcd, 0x00);

	tx_command(lcd, 0xFF);
	tx_data(lcd, 0x60);
	tx_data(lcd, 0x01);
	tx_data(lcd, 0x04);

	tx_command(lcd, POWER_CONTROL2);
	tx_data(lcd, 0x13);
	tx_command(lcd, POWER_CONTROL3);
	tx_data(lcd, 0x13);
	tx_command(lcd, POWER_CONTROL4);
	tx_data(lcd, 0x22);

	tx_command(lcd, 0xBE);
	tx_data(lcd, 0x11);

	tx_command(lcd, 0xE1);
	tx_data(lcd, 0x10);
	tx_data(lcd, 0x0E);

	tx_command(lcd, 0xDF);
	tx_data(lcd, 0x21);
	tx_data(lcd, 0x0c);
	tx_data(lcd, 0x02);

	tx_command(lcd, SET_GAMMA1);
	tx_data(lcd, 0x45);
	tx_data(lcd, 0x09);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x26);
	tx_data(lcd, 0x2A);

	tx_command(lcd, SET_GAMMA2);
	tx_data(lcd, 0x43);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x72);
	tx_data(lcd, 0x36);
	tx_data(lcd, 0x37);
	tx_data(lcd, 0x6F);

	tx_command(lcd, SET_GAMMA3);
	tx_data(lcd, 0x45);
	tx_data(lcd, 0x09);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x26);
	tx_data(lcd, 0x2A);

	tx_command(lcd, SET_GAMMA4);
	tx_data(lcd, 0x43);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x72);
	tx_data(lcd, 0x36);
	tx_data(lcd, 0x37);
	tx_data(lcd, 0x6F);

	tx_command(lcd, 0xED);
	tx_data(lcd, 0x1B);
	tx_data(lcd, 0x0B);

	tx_command(lcd, 0xAE);
	tx_data(lcd, 0x77);

	tx_command(lcd, 0xCD);
	tx_data(lcd, 0x63);

	tx_command(lcd, 0x70);
	tx_data(lcd, 0x07);
	tx_data(lcd, 0x07);
	tx_data(lcd, 0x04);
	tx_data(lcd, 0x0E);
	tx_data(lcd, 0x0F);
	tx_data(lcd, 0x09);
	tx_data(lcd, 0x07);
	tx_data(lcd, 0x08);
	tx_data(lcd, 0x03);

	tx_command(lcd, FRAMERATE);
	tx_data(lcd, 0x34);

	tx_command(lcd, 0x62);
	tx_data(lcd, 0x18);
	tx_data(lcd, 0x0D);
	tx_data(lcd, 0x71);
	tx_data(lcd, 0xED);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x18);
	tx_data(lcd, 0x0F);
	tx_data(lcd, 0x71);
	tx_data(lcd, 0xEF);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x70);

	tx_command(lcd, 0x63);
	tx_data(lcd, 0x18);
	tx_data(lcd, 0x11);
	tx_data(lcd, 0x71);
	tx_data(lcd, 0xF1);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x18);
	tx_data(lcd, 0x13);
	tx_data(lcd, 0x71);
	tx_data(lcd, 0xF3);
	tx_data(lcd, 0x70);
	tx_data(lcd, 0x70);

	tx_command(lcd, 0x64);
	tx_data(lcd, 0x28);
	tx_data(lcd, 0x29);
	tx_data(lcd, 0xF1);
	tx_data(lcd, 0x01);
	tx_data(lcd, 0xF1);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x07);

	tx_command(lcd, 0x66);
	tx_data(lcd, 0x3C);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0xCD);
	tx_data(lcd, 0x67);
	tx_data(lcd, 0x45);
	tx_data(lcd, 0x45);
	tx_data(lcd, 0x10);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x00);

	tx_command(lcd, 0x67);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x3C);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x01);
	tx_data(lcd, 0x54);
	tx_data(lcd, 0x10);
	tx_data(lcd, 0x32);
	tx_data(lcd, 0x98);

	tx_command(lcd, 0x74);
	tx_data(lcd, 0x10);
	tx_data(lcd, 0x85);
	tx_data(lcd, 0x80);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x00);
	tx_data(lcd, 0x4E);
	tx_data(lcd, 0x00);

	tx_command(lcd, 0x98);
	tx_data(lcd, 0x3e);
	tx_data(lcd, 0x07);

	tx_command(lcd, TEON);
	tx_data(lcd, 0x00);

	// display inversion on
	tx_command(lcd, INVON);

	// exit sleep mode
	tx_command(lcd, SLPOUT);
	sleep_ms(50);

	// screen on
	tx_command(lcd, DISPON);
	sleep_ms(50);

	ClearDisplay(lcd, BLACK);

	// normal display on
	tx_command(lcd, NORON);
	sleep_ms(10);
}


// turns a display on
void DisplayOn(uint lcd)
{
	select_gpio(lcd);

	// screen on
	tx_command(lcd, DISPON);
}

// turns a display off
void DisplayOff(uint lcd)
{
	select_gpio(lcd);

	// screen off
	tx_command(lcd, DISPOFF);
}


// turns display highlighting on
void HighlightOn(uint lcd)
{
	select_gpio(lcd);

	tx_command(lcd, INVOFF);
}

// turns display highlighting off
void HighlightOff(uint lcd)
{
	select_gpio(lcd);

	tx_command(lcd, INVON);
}


// sets up a write to the display
static void init_display_write(uint lcd, uint x, uint y, uint w, uint h)
{
	select_gpio(lcd);

	// set column address
	tx_command(lcd, CASET);
	tx_word(lcd, x);
	tx_word(lcd, x + w - 1);

	// set row address
	tx_command(lcd, RASET);
	tx_word(lcd, y);
	tx_word(lcd, y + h - 1);

	// write RAM
	tx_command(lcd, RAMWR);
}


// clears the display
void ClearDisplay(uint lcd, uint colour)
{
	init_display_write(lcd, 0, 0, WIDTH, HEIGHT);

	// clear all pixels
	for (unsigned long n = WIDTH * HEIGHT / 8; n > 0; n--)
	{
		tx_word(lcd, colour);
		tx_word(lcd, colour);
		tx_word(lcd, colour);
		tx_word(lcd, colour);
		tx_word(lcd, colour);
		tx_word(lcd, colour);
		tx_word(lcd, colour);
		tx_word(lcd, colour);
	}
}


// waits until a PIO is idle
static void wait_pio_idle(uint lcd)
{
	if (pio_sm_is_enabled(lcd_pio(lcd), lcd_sm(lcd)))
	{
		dma_channel_wait_for_finish_blocking(lcd_dma(lcd));
		while (!pio_sm_is_tx_fifo_empty(lcd_pio(lcd), lcd_sm(lcd))) tight_loop_contents();
		uint32_t SM_STALL_MASK = 1u << (PIO_FDEBUG_TXSTALL_LSB + lcd_sm(lcd));
		lcd_pio(lcd)->fdebug = SM_STALL_MASK;
		while (!(lcd_pio(lcd)->fdebug & SM_STALL_MASK)) tight_loop_contents();
	}
}


// draws a character on the display
void DrawChar(uint lcd, int x, int y, int w, int h, const uint16_t* pixels)
{
	// if completely off screen
	if (y <= -h || y >= HEIGHT) return;

	if (y < 0)
	{
		pixels += -y * w/2;
		h -= -y;
		y = 0;
	}

	if (y + h > HEIGHT)
	{
		h -= y + h - HEIGHT;
	}

	wait_pio_idle(lcd);

	init_display_write(lcd, x, y, w, h);

	select_pio(lcd);

	dma_cnt[lcd_dma(lcd)] = 0;
	dma_channel_transfer_from_buffer_now(lcd_dma(lcd), pixels, w/2 * h);
}


// draws a double bitmap image on the display (pixels doubled vertically and horizontally)
void DrawImage(uint lcd, uint16_t* pixels)
{
	TransitionImage(lcd, 1, NULL, pixels);
}


// clears the entire display
void ClearImage(uint lcd)
{
	TransitionImage(lcd, 0, NULL, NULL);
}


// transitions from one bitmap image (pixels1) to another (pixels2)
void TransitionImage(uint lcd, uint fx, uint16_t* pixels1, uint16_t* pixels2)
{
	uint dma_chan = lcd_dma(lcd);

	while (dma_cnt[dma_chan] > 0) tight_loop_contents();

	wait_pio_idle(lcd);

	dma_fx[dma_chan] = fx;

	int x, y;
	uint depth = TransitionDepth & ~1u;

	switch (fx)
	{
	// clear screen
	case 0:
		init_display_write(lcd, 0, 0, WIDTH, HEIGHT);

		select_pio(lcd);

		dma_pnt2[dma_chan] = black_line;
		dma_cnt[dma_chan] = HEIGHT - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);

		break;

	// simple replacement
	case 1:
		init_display_write(lcd, 0, 0, WIDTH, HEIGHT);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2;
		dma_cnt[dma_chan] = HEIGHT - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);

		break;

	// scroll down both
	case 2:
		init_display_write(lcd, 0, 0, WIDTH, HEIGHT);

		select_pio(lcd);

		dma_pnt1[dma_chan] = pixels1;
		dma_pnt2[dma_chan] = pixels2 + (HEIGHT/2 - depth/2) * WIDTH/2;
		dma_cnt[dma_chan] = HEIGHT - 1;

		if (depth == 0) dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt1[dma_chan], WIDTH/2);
		else dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);

		break;

	// scroll down over
	case 3:
		if (depth == 0) break;

		init_display_write(lcd, 0, 0, WIDTH, depth);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2 + (HEIGHT/2 - depth/2) * WIDTH/2;
		dma_cnt[dma_chan] = depth - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);

		break;

	// overwrite
	case 4:
		if (depth == 0) break;

		init_display_write(lcd, 0, 0, WIDTH, depth);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2;
		dma_cnt[dma_chan] = depth - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);

		break;

	// slide in from the left
	case 5:
		if (depth < 2) break;

		init_display_write(lcd, 0, 0, depth, HEIGHT);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2 + (WIDTH/2 - depth/2);
		dma_cnt[dma_chan] = HEIGHT - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);

		break;

	// overwrite from the left
	case 6:
		if (depth < 2) break;

		init_display_write(lcd, 0, 0, depth, HEIGHT);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2;
		dma_cnt[dma_chan] = HEIGHT - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);

		break;

	// blinds
	case 7:
		init_display_write(lcd, 0, 0, WIDTH, HEIGHT);

		select_pio(lcd);

		dma_pnt1[dma_chan] = pixels1;
		dma_pnt2[dma_chan] = pixels2;
		dma_cnt[dma_chan] = HEIGHT - 1;

		void* line = CurrentFont == 12 ? white_line : black_line;
		if (depth == 0) dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt1[dma_chan], WIDTH/2);
		else dma_channel_transfer_from_buffer_now(dma_chan, line, WIDTH/2);

		break;

	// expand outwards from centre
	case 8:
		if (depth < 2) break;

		x = WIDTH/2 - depth/2;
		y = HEIGHT/2 - depth/2;

		init_display_write(lcd, x, y, depth, depth);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2 + y/2 * WIDTH/2 + x/2;
		dma_cnt[dma_chan] = depth - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);

		break;

	// expand outwards from four corners in turn
	case 9:
		if (depth < 2) break;

		switch (TransitionStage)
		{
		case 0:
			// bottom right
			x = WIDTH - depth;
			y = HEIGHT - depth;
			dma_pnt2[dma_chan] = pixels2 + y/2 * WIDTH/2 + x/2;
			break;
		case 1:
			// bottom left
			x = 0;
			y = HEIGHT - depth;
			dma_pnt2[dma_chan] = pixels2 + (HEIGHT/2 - depth/2) * WIDTH/2;
			break;
		case 2:
			// top left
			x = 0;
			y = 0;
			dma_pnt2[dma_chan] = pixels2;
			break;
		case 3:
			// top right
			x = WIDTH - depth;
			y = 0;
			dma_pnt2[dma_chan] = pixels2 + (WIDTH/2 - depth/2);
			break;
		}

		init_display_write(lcd, x, y, depth, depth);

		select_pio(lcd);

		dma_cnt[dma_chan] = depth - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);

		break;

	// slide outwards from four corners in turn
	case 10:
		if (depth < 2) break;

		switch (TransitionStage)
		{
		case 0:
			// bottom right
			x = WIDTH - depth;
			y = HEIGHT - depth;
			dma_pnt2[dma_chan] = pixels2;
			break;
		case 1:
			// bottom left
			x = 0;
			y = HEIGHT - depth;
			dma_pnt2[dma_chan] = pixels2 + (WIDTH/2 - depth/2);
			break;
		case 2:
			// top left
			x = 0;
			y = 0;
			dma_pnt2[dma_chan] = pixels2 + (HEIGHT/2 - depth/2) * WIDTH/2 + (WIDTH/2 - depth/2);
			break;
		case 3:
			// top right
			x = WIDTH - depth;
			y = 0;
			dma_pnt2[dma_chan] = pixels2 + (HEIGHT/2 - depth/2) * WIDTH/2;
			break;
		}

		init_display_write(lcd, x, y, depth, depth);

		select_pio(lcd);

		dma_cnt[dma_chan] = depth - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth/2);

		break;

	// expand outwards from centre in a spiral
	case 11:
		if (depth == 0) break;

		static int spiral[SPIRAL_SQUARES * SPIRAL_SQUARES][2] =
		{
			// corner squares discarded
			{0,  0}, {-1, 0}, {-1,-1}, {0, -1}, {+1,-1}, {+1, 0}, {+1,+1},
			{0, +1}, {-1,+1}, {-2,+1}, {-2, 0}, {-2,-1}, {-2,-2}, {-1,-2},
			{0, -2}, {+1,-2}, {+2,-2}, {+2,-1}, {+2, 0}, {+2,+1}, {+2,+2},
			{+1,+2}, {0, +2}, {-1,+2}, {-2,+2}, {-3,+2}, {-3,+1}, {-3, 0},
			{-3,-1}, {-3,-2},          {-2,-3}, {-1,-3}, {0, -3}, {+1,-3},
			{+2,-3},          {+3,-2}, {+3,-1}, {+3, 0}, {+3,+1}, {+3,+2},
			         {+2,+3}, {+1,+3}, {0, +3}, {-1,+3}, {-2,+3},
		};

		#define TOTAL_SQUARES (sizeof(spiral)/(2*sizeof(int)))
		uint n = (TransitionDepth * TOTAL_SQUARES)/SIZE;
		if (n >= TOTAL_SQUARES) break;

		x = WIDTH/2 - SPIRAL_TILE/2 + spiral[n][0] * SPIRAL_TILE;
		y = HEIGHT/2 - SPIRAL_TILE/2 + spiral[n][1] * SPIRAL_TILE;

		init_display_write(lcd, x, y, SPIRAL_TILE, SPIRAL_TILE);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2 + y/2 * WIDTH/2 + x/2;
		dma_cnt[dma_chan] = SPIRAL_TILE - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], SPIRAL_TILE/2);

		break;

	// erase outwards from centre then expand outwards
	case 12:
		if (depth < 2)
		{
			break;
		}

		else if (depth <= HEIGHT/2)
		{
			x = WIDTH/2 - depth;
			y = HEIGHT/2 - depth;

			init_display_write(lcd, x, y, 2*depth, 2*depth);

			select_pio(lcd);

			dma_cnt[dma_chan] = 2*depth - 1;

			void* line = CurrentFont == 12 ? white_line : black_line;
			dma_channel_transfer_from_buffer_now(dma_chan, line, depth);
		}

		else if (depth < HEIGHT/2 + 2)
		{
			break;
		}

		else
		{
			depth -= HEIGHT/2;

			x = WIDTH/2 - depth;
			y = HEIGHT/2 - depth;

			init_display_write(lcd, x, y, 2*depth, 2*depth);

			select_pio(lcd);

			dma_pnt2[dma_chan] = pixels2 + y/2 * WIDTH/2 + x/2;
			dma_cnt[dma_chan] = 2*depth - 1;

			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], depth);
		}

		break;

	// dissolve
	case 13:
		if (TransitionDepth >= SIZE) return;

		y = dissolve[TransitionDepth/2] * 2;

		init_display_write(lcd, 0, y, WIDTH, 2);

		select_pio(lcd);

		dma_pnt2[dma_chan] = pixels2 + y/2 * WIDTH/2;
		dma_cnt[dma_chan] = 2 - 1;

		dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);

		break;

	// flip clock
	case 14:
		if (TransitionDepth < 10 || (TransitionDepth > HEIGHT/2 && TransitionDepth < HEIGHT/2 + 10)) break;

		dma_pnt1[dma_chan] = pixels1;
		dma_pnt2[dma_chan] = pixels2;
		dma_cnt[dma_chan] = HEIGHT/2 - 1;

		#define PI 3.14159f

		if (TransitionDepth <= HEIGHT/2)
		{
			init_display_write(lcd, 0, 0, WIDTH, HEIGHT/2);

			select_pio(lcd);

			float h = cos(((float) TransitionDepth)/(HEIGHT/2) * PI/2) * HEIGHT/2;
			flip[dma_chan] = HEIGHT/2 - h;

			ycoord1[dma_chan] = 0;
			delta1[dma_chan] = (HEIGHT/2) / h;
			ycoord2[dma_chan] = 0;
			delta2[dma_chan] = 1;

			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan], WIDTH/2);
		}

		else
		{
			init_display_write(lcd, 0, HEIGHT/2, WIDTH, HEIGHT/2);

			select_pio(lcd);

			float h = sin(((float) (TransitionDepth - HEIGHT/2))/(HEIGHT/2) * PI/2) * HEIGHT/2;
			flip[dma_chan] = h;

			ycoord1[dma_chan] = HEIGHT/2 + h;
			delta1[dma_chan] = 1;
			ycoord2[dma_chan] = HEIGHT/2;
			delta2[dma_chan] = (HEIGHT/2) / h;

			dma_channel_transfer_from_buffer_now(dma_chan, dma_pnt2[dma_chan] + HEIGHT/4 * WIDTH/2, WIDTH/2);
		}

		break;
	}
}

/****************************************************************************
* FILE:      main.c															*
* CONTENTS:  Clocodile mainline												*
* COPYRIGHT: MadLab Ltd. 2026												*
* AUTHOR:    James Hutchby													*
* UPDATED:   05/07/26														*
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#include "pico/aon_timer.h"
#include "pico/util/queue.h"
#include "pico/util/datetime.h"
#include "boards/pico.h"
#include "hardware/irq.h"
#include "hardware/claim.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"

#include "common.h"
#include "main.h"
#include "lcd.h"
#include "matrix.h"
#include "ntp.h"
#include "cli.h"


//---------------------------------------------------------------------------
// notes
//---------------------------------------------------------------------------

// debug probe: orange = clock, black = ground, yellow = data
// USB driver (Zadig): libusbK (v3.1.0.0)

// on power-up PICO W LED flashes twice and flashes twice again after Wi-Fi
// connection complete

// MODE + 12/24 buttons toggles dimmed displays

// settings changed via pushbuttons saved to flash memory after 10s idle

// .\picotool.exe info -a

// graphic credits: pngwing.com, rawpixel.com, magnific.com, vecteezy.com


//---------------------------------------------------------------------------
// port assignments
//---------------------------------------------------------------------------

// pushbuttons
#define S1_PIN 14		// MODE
#define S2_PIN 15		// 12/24
#define S3_PIN 19		// <
#define S4_PIN 20		// >
#define S5_PIN 21		// +
#define S6_PIN 22		// -


//---------------------------------------------------------------------------
// constants
//---------------------------------------------------------------------------

#define SSID_MAX_LENGTH 32
#define PASSWORD_MAX_LENGTH 63

// button flags
#define BUTTON_MODE (1<<0)
#define BUTTON_24 (1<<1)
#define BUTTON_PREV (1<<2)
#define BUTTON_NEXT (1<<3)
#define BUTTON_INC (1<<4)
#define BUTTON_DEC (1<<5)

// poll delay in ms
#define POLL_DELAY 20

// digital rain
#define MATRIX_FONT NUM_FONTS

// highlight timeout in ms
#define HIGHLIGHT_TIMEOUT 5000

// save settings timeout in ms
#define SETTINGS_TIMEOUT 10000


//---------------------------------------------------------------------------
// linkage
//---------------------------------------------------------------------------

extern const uint16_t __in_flash() font1_zero[];
extern const uint16_t __in_flash() font1_one[];
extern const uint16_t __in_flash() font1_two[];
extern const uint16_t __in_flash() font1_three[];
extern const uint16_t __in_flash() font1_four[];
extern const uint16_t __in_flash() font1_five[];
extern const uint16_t __in_flash() font1_six[];
extern const uint16_t __in_flash() font1_seven[];
extern const uint16_t __in_flash() font1_eight[];
extern const uint16_t __in_flash() font1_nine[];

extern const uint16_t __in_flash() font3_zero[];
extern const uint16_t __in_flash() font3_one[];
extern const uint16_t __in_flash() font3_two[];
extern const uint16_t __in_flash() font3_three[];
extern const uint16_t __in_flash() font3_four[];
extern const uint16_t __in_flash() font3_five[];
extern const uint16_t __in_flash() font3_six[];
extern const uint16_t __in_flash() font3_seven[];
extern const uint16_t __in_flash() font3_eight[];
extern const uint16_t __in_flash() font3_nine[];

extern const uint16_t __in_flash() font4_zero[];
extern const uint16_t __in_flash() font4_one[];
extern const uint16_t __in_flash() font4_two[];
extern const uint16_t __in_flash() font4_three[];
extern const uint16_t __in_flash() font4_four[];
extern const uint16_t __in_flash() font4_five[];
extern const uint16_t __in_flash() font4_six[];
extern const uint16_t __in_flash() font4_seven[];
extern const uint16_t __in_flash() font4_eight[];
extern const uint16_t __in_flash() font4_nine[];

extern const uint16_t __in_flash() font5_zero[];
extern const uint16_t __in_flash() font5_one[];
extern const uint16_t __in_flash() font5_two[];
extern const uint16_t __in_flash() font5_three[];
extern const uint16_t __in_flash() font5_four[];
extern const uint16_t __in_flash() font5_five[];
extern const uint16_t __in_flash() font5_six[];
extern const uint16_t __in_flash() font5_seven[];
extern const uint16_t __in_flash() font5_eight[];
extern const uint16_t __in_flash() font5_nine[];

extern const uint16_t __in_flash() font6_zero[];
extern const uint16_t __in_flash() font6_one[];
extern const uint16_t __in_flash() font6_two[];
extern const uint16_t __in_flash() font6_three[];
extern const uint16_t __in_flash() font6_four[];
extern const uint16_t __in_flash() font6_five[];
extern const uint16_t __in_flash() font6_six[];
extern const uint16_t __in_flash() font6_seven[];
extern const uint16_t __in_flash() font6_eight[];
extern const uint16_t __in_flash() font6_nine[];

extern const uint16_t __in_flash() font7_zero[];
extern const uint16_t __in_flash() font7_one[];
extern const uint16_t __in_flash() font7_two[];
extern const uint16_t __in_flash() font7_three[];
extern const uint16_t __in_flash() font7_four[];
extern const uint16_t __in_flash() font7_five[];
extern const uint16_t __in_flash() font7_six[];
extern const uint16_t __in_flash() font7_seven[];
extern const uint16_t __in_flash() font7_eight[];
extern const uint16_t __in_flash() font7_nine[];

extern const uint16_t __in_flash() font8_zero[];
extern const uint16_t __in_flash() font8_one[];
extern const uint16_t __in_flash() font8_two[];
extern const uint16_t __in_flash() font8_three[];
extern const uint16_t __in_flash() font8_four[];
extern const uint16_t __in_flash() font8_five[];
extern const uint16_t __in_flash() font8_six[];
extern const uint16_t __in_flash() font8_seven[];
extern const uint16_t __in_flash() font8_eight[];
extern const uint16_t __in_flash() font8_nine[];

extern const uint16_t __in_flash() font9_zero[];
extern const uint16_t __in_flash() font9_one[];
extern const uint16_t __in_flash() font9_two[];
extern const uint16_t __in_flash() font9_three[];
extern const uint16_t __in_flash() font9_four[];
extern const uint16_t __in_flash() font9_five[];
extern const uint16_t __in_flash() font9_six[];
extern const uint16_t __in_flash() font9_seven[];
extern const uint16_t __in_flash() font9_eight[];
extern const uint16_t __in_flash() font9_nine[];

extern const uint16_t __in_flash() font10_zero[];
extern const uint16_t __in_flash() font10_one[];
extern const uint16_t __in_flash() font10_two[];
extern const uint16_t __in_flash() font10_three[];
extern const uint16_t __in_flash() font10_four[];
extern const uint16_t __in_flash() font10_five[];
extern const uint16_t __in_flash() font10_six[];
extern const uint16_t __in_flash() font10_seven[];
extern const uint16_t __in_flash() font10_eight[];
extern const uint16_t __in_flash() font10_nine[];

extern const uint16_t __in_flash() font11_zero[];
extern const uint16_t __in_flash() font11_one[];
extern const uint16_t __in_flash() font11_two[];
extern const uint16_t __in_flash() font11_three[];
extern const uint16_t __in_flash() font11_four[];
extern const uint16_t __in_flash() font11_five[];
extern const uint16_t __in_flash() font11_six[];
extern const uint16_t __in_flash() font11_seven[];
extern const uint16_t __in_flash() font11_eight[];
extern const uint16_t __in_flash() font11_nine[];

extern const uint16_t __in_flash() font12_zero[];
extern const uint16_t __in_flash() font12_one[];
extern const uint16_t __in_flash() font12_two[];
extern const uint16_t __in_flash() font12_three[];
extern const uint16_t __in_flash() font12_four[];
extern const uint16_t __in_flash() font12_five[];
extern const uint16_t __in_flash() font12_six[];
extern const uint16_t __in_flash() font12_seven[];
extern const uint16_t __in_flash() font12_eight[];
extern const uint16_t __in_flash() font12_nine[];

extern const uint16_t __in_flash() font13_zero[];
extern const uint16_t __in_flash() font13_one[];
extern const uint16_t __in_flash() font13_two[];
extern const uint16_t __in_flash() font13_three[];
extern const uint16_t __in_flash() font13_four[];
extern const uint16_t __in_flash() font13_five[];
extern const uint16_t __in_flash() font13_six[];
extern const uint16_t __in_flash() font13_seven[];
extern const uint16_t __in_flash() font13_eight[];
extern const uint16_t __in_flash() font13_nine[];

extern uint TransitionDepth;
extern uint TransitionStage;

static void get_time(bool);
static int update_time();
static void init_font();
static void init_fx();
static void load_font(int);
static bool do_buttons(int);
static int poll_buttons();
static void flash_LED();


//---------------------------------------------------------------------------
// variables
//---------------------------------------------------------------------------

// Wi-Fi credentials
char WiFiSSID[SSID_MAX_LENGTH+1] = "";
char WiFiPassword[PASSWORD_MAX_LENGTH+1] = "";

static bool WiFiOkay;

// UTC
static int hours = 0, minutes = 0, seconds = 0, prev_seconds = -1;
static struct tm tm = {.tm_year = 126, .tm_mon = 0, .tm_mday = 1, .tm_wday = 4, .tm_yday = 0,
  .tm_hour = 0, .tm_min = 0, .tm_sec = 0};

// current font
uint CurrentFont = 1;

// current transition effect
uint CurrentFx = 1;

// true if 24-hour mode, false if 12-hour mode
bool Display24 = true;

// offset from UTC in minutes (-12 hours to +14 hours)
int TimeZone = 0;

// true if displays dimmed
bool DisplayDimmed = false;

// NTP update period in seconds
uint32_t NTPUpdatePeriod = 0;

// current display to adjust
static uint CurrentDisplay = 1;
static bool DisplayHighlighted = false;
static uint32_t HighlightTimer = 0;

static uint32_t SettingsTimer = 0;

static uint16_t* digit_buffers[10];

static int current_digits[NUM_LCDS];
static int next_digits[NUM_LCDS];
static bool update_flags[NUM_LCDS];


//---------------------------------------------------------------------------
// main entry point
//---------------------------------------------------------------------------

int main()
{
	stdio_init_all();

	LoadSettings();

	// initialise pushbuttons
	gpio_init(S1_PIN);
	gpio_set_dir(S1_PIN, GPIO_IN);
	gpio_pull_up(S1_PIN);
	gpio_init(S2_PIN);
	gpio_set_dir(S2_PIN, GPIO_IN);
	gpio_pull_up(S2_PIN);
	gpio_init(S3_PIN);
	gpio_set_dir(S3_PIN, GPIO_IN);
	gpio_pull_up(S3_PIN);
	gpio_init(S4_PIN);
	gpio_set_dir(S4_PIN, GPIO_IN);
	gpio_pull_up(S4_PIN);
	gpio_init(S5_PIN);
	gpio_set_dir(S5_PIN, GPIO_IN);
	gpio_pull_up(S5_PIN);
	gpio_init(S6_PIN);
	gpio_set_dir(S6_PIN, GPIO_IN);
	gpio_pull_up(S6_PIN);

	// initialise all LCD displays (claim DMA before cyw43_arch_init)
	InitAllDisplays();

	// initialise Wi-Fi
	if (cyw43_arch_init() == 0)
	{
		WiFiOkay = true;

		// double flash Pico W LED
		flash_LED();
		flash_LED();
	}
	else
	{
		WiFiOkay = false;
		printf("Wi-Fi initialisation failed!\n");
	}

	// allocate digit buffers
	for (uint n = 0; n < 10; n++)
		digit_buffers[n] = malloc(WIDTH/2 * HEIGHT/2 * sizeof(uint16_t));

	// initialise matrix digital rain effect
	InitMatrix();

	for (uint n = 1; n <= NUM_LCDS; n++) current_digits[n-1] = -1;

	// initialise current font
	init_font();

	#ifdef DEBUG
	// while (!stdio_usb_connected()) ;
	#endif

	// sign on
	printf("\n");
	printf("Clocodile v1.0\n");
	printf("Written by James Hutchby\n");
	printf("www.madlab.org\n");
	printf("@clubmadlab\n\n");

	// initialise Network Time Protocol
	InitNTP();

	// connect to Wi-Fi
	ConnectWiFi();

	// default time
	SetTime(0, 0, 0);

	// initialise command line interpreter
	InitCLI();


//---------------------------------------------------------------------------
// main loop
//---------------------------------------------------------------------------

	while (true)
	{
		watchdog_enable(60 * 1000, 1);

		DoCLI();

		if (HighlightTimer != 0)
		{
			uint32_t delta = to_ms_since_boot(get_absolute_time()) - HighlightTimer;
			if (delta > HIGHLIGHT_TIMEOUT)
			{
				HighlightTimer = 0;
				DisplayHighlighted = false;
				HighlightOff(CurrentDisplay);
			}
		}

		if (SettingsTimer != 0)
		{
			uint32_t delta = to_ms_since_boot(get_absolute_time()) - SettingsTimer;
			if (delta > SETTINGS_TIMEOUT)
			{
				SettingsTimer = 0;
				SaveSettings();
			}
		}

		if (WiFiOkay) cyw43_arch_poll();

		int buttons;

		if (CurrentFont == MATRIX_FONT)
		{
			watchdog_update();

			get_time(false);

			for (uint n = 1; n <= NUM_LCDS; n++) DoMatrix(n, next_digits[n-1]);

			sleep_ms(2);

			buttons = poll_buttons();
			do_buttons(buttons);

			continue;
		}

		while (true)
		{
			watchdog_update();

			get_time(false);

			if (seconds != prev_seconds) break;

			sleep_ms(10);

			buttons = poll_buttons();
			do_buttons(buttons);
		}

		prev_seconds = seconds;

		update_flags[0] = current_digits[0] != next_digits[0];
		update_flags[1] = current_digits[1] != next_digits[1];
		update_flags[2] = current_digits[2] != next_digits[2];
		update_flags[3] = current_digits[3] != next_digits[3];
		update_flags[4] = current_digits[4] != next_digits[4];
		update_flags[5] = current_digits[5] != next_digits[5];

		if (CurrentFont != MATRIX_FONT)
		{
			buttons = update_time();
			do_buttons(buttons);
		}

		for (uint n = 1; n <= NUM_LCDS; n++) current_digits[n-1] = next_digits[n-1];
	}
}


// connects to Wi-Fi
bool ConnectWiFi()
{
	watchdog_disable();

	if (!WiFiOkay) return false;

	// if no Wi-Fi network
	if (WiFiSSID[0] == '\0') return false;

	// enable Wi-Fi station mode
	cyw43_arch_disable_sta_mode();
	cyw43_arch_enable_sta_mode();

	printf("Connecting to Wi-Fi...\n");
	printf("(Ctrl+C to abort - up to 30s delay)\n");

	for (int retries = 0; retries < 10; retries++)
	{
		#define CTRL_C 3
		if (getchar_timeout_us(0) == CTRL_C) return false;

		if (cyw43_arch_wifi_connect_timeout_ms(WiFiSSID, WiFiPassword, CYW43_AUTH_WPA2_AES_PSK, 30000))
		{
			printf("Failed to connect.\n");
			continue;
		}
		else
		{
			printf("Connected.\n");
			uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
			printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

			// double flash LED
			flash_LED();
			flash_LED();

			// enable Network Time Protocol
			EnableNTP();

			return true;
		}
	}

	printf("Retries exhausted.\n");

	return false;
}


// gets the current time
static void get_time(bool copy)
{
	aon_timer_get_time_calendar(&tm);
	hours = tm.tm_hour;
	minutes = tm.tm_min;
	seconds = tm.tm_sec;

	// UTC to local time
	if (TimeZone > 0)
	{
		minutes += TimeZone % 60;
		if (minutes >= 60) {minutes -= 60; hours++;}
		hours += TimeZone / 60;
		if (hours >= 24) hours -= 24;
	}
	else if (TimeZone < 0)
	{
		minutes -= (-TimeZone) % 60;
		if (minutes < 0) {minutes += 60; hours--;}
		hours -= (-TimeZone) / 60;
		if (hours < 0) hours += 24;
	}

	if (!Display24)
	{
		if (hours == 0) hours = 12;
		else if (hours > 12) hours -= 12;
	}

	next_digits[0] = hours / 10;
	next_digits[1] = hours % 10;
	next_digits[2] = minutes / 10;
	next_digits[3] = minutes % 10;
	next_digits[4] = seconds / 10;
	next_digits[5] = seconds % 10;

	if (copy) for (uint n = 1; n <= NUM_LCDS; n++) current_digits[n-1] = next_digits[n-1];
}


// sets the current time
void SetTime(int hours, int mins, int secs)
{
	tm.tm_hour = hours;
	tm.tm_min = mins;
	tm.tm_sec = secs;

	aon_timer_start_calendar(&tm);
	sleep_ms(10);

	prev_seconds = -1;

	printf("Time set to: %02d:%02d:%02d\n", hours, mins, secs);
}


// adjusts the current time
void AdjustTime(int delta_hours, int delta_mins, int delta_secs)
{
	aon_timer_get_time_calendar(&tm);
	hours = tm.tm_hour;
	minutes = tm.tm_min;
	seconds = tm.tm_sec;

	hours += delta_hours;
	if (hours < 0) hours += 24; else hours %= 24;

	minutes += delta_mins;
	if (minutes < 0) minutes += 60; else minutes %= 60;

	seconds += delta_secs;
	if (seconds < 0) seconds += 60; else seconds %= 60;

	SetTime(hours, minutes, seconds);
}


static int update_time()
{
	TransitionDepth = 0;

	int buttons = 0;

	while (true)
	{
		watchdog_update();

		for (uint n = 1; n <= NUM_LCDS; n++)
		{
			if (!update_flags[n-1]) continue;
			if (current_digits[n-1] < 0) current_digits[n-1] = 0;
			TransitionImage(n, CurrentFx, digit_buffers[current_digits[n-1]], digit_buffers[next_digits[n-1]]);
		}

		static const int ms[] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 10, 20, 5, 20};
		sleep_ms(ms[CurrentFx]);

		static const int delta[] = {SIZE+1, SIZE+1, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 8, 2, 8};
		TransitionDepth += delta[CurrentFx];

		if (TransitionDepth > SIZE)
		{
			TransitionStage = (TransitionStage + 1) % 4;
			break;
		}

		buttons = poll_buttons();
		if ((buttons & ~BUTTON_MODE) != 0) break;
	}

	for (uint n = 1; n <= NUM_LCDS; n++) update_flags[n-1] = false;

	return buttons;
}


static void init_font()
{
	watchdog_update();

	// load current font
	load_font(CurrentFont);

	init_fx();
}

static void init_fx()
{
	for (uint n = 1; n <= NUM_LCDS; n++)
	{
		int digit = current_digits[n-1];
		if (digit < 0) digit = 0;
		if (CurrentFont == MATRIX_FONT)
		{
			ClearImage(n);
			DoMatrix(n, digit);
		}
		else
		{
			DrawImage(n, digit_buffers[digit]);
		}
	}

	sleep_ms(20);
}


static void copy_image(const uint16_t* s, uint16_t* d)
{
	if (DisplayDimmed)
	{
		for (uint i = 0; i < WIDTH/2 * HEIGHT/2; i++)
			*d++ = (*s++ >> 1) & 0x7bef;
	}
	else
	{
		for (uint i = 0; i < WIDTH/2 * HEIGHT/2; i++)
			*d++ = *s++;
	}
}

static void convert_image_blue_to_red(uint16_t* s)
{
	for (uint i = 0; i < WIDTH/2 * HEIGHT/2; i++)
	{
		*s++ = (*s & 0x1f) << 11;
	}
}

static void load_font(int n)
{
	switch (n)
	{
	// blue 7-segment
	case 1:
		copy_image(font1_zero, digit_buffers[0]);
		copy_image(font1_one, digit_buffers[1]);
		copy_image(font1_two, digit_buffers[2]);
		copy_image(font1_three, digit_buffers[3]);
		copy_image(font1_four, digit_buffers[4]);
		copy_image(font1_five, digit_buffers[5]);
		copy_image(font1_six, digit_buffers[6]);
		copy_image(font1_seven, digit_buffers[7]);
		copy_image(font1_eight, digit_buffers[8]);
		copy_image(font1_nine, digit_buffers[9]);
		break;

	// red 7-segment
	case 2:
		copy_image(font1_zero, digit_buffers[0]);
		copy_image(font1_one, digit_buffers[1]);
		copy_image(font1_two, digit_buffers[2]);
		copy_image(font1_three, digit_buffers[3]);
		copy_image(font1_four, digit_buffers[4]);
		copy_image(font1_five, digit_buffers[5]);
		copy_image(font1_six, digit_buffers[6]);
		copy_image(font1_seven, digit_buffers[7]);
		copy_image(font1_eight, digit_buffers[8]);
		copy_image(font1_nine, digit_buffers[9]);
		for (int i = 0; i < 10; i++) convert_image_blue_to_red(digit_buffers[i]);
		break;

	// amber backlight 7-segment
	case 3:
		copy_image(font3_zero, digit_buffers[0]);
		copy_image(font3_one, digit_buffers[1]);
		copy_image(font3_two, digit_buffers[2]);
		copy_image(font3_three, digit_buffers[3]);
		copy_image(font3_four, digit_buffers[4]);
		copy_image(font3_five, digit_buffers[5]);
		copy_image(font3_six, digit_buffers[6]);
		copy_image(font3_seven, digit_buffers[7]);
		copy_image(font3_eight, digit_buffers[8]);
		copy_image(font3_nine, digit_buffers[9]);
		break;

	// 'scope
	case 4:
		copy_image(font4_zero, digit_buffers[0]);
		copy_image(font4_one, digit_buffers[1]);
		copy_image(font4_two, digit_buffers[2]);
		copy_image(font4_three, digit_buffers[3]);
		copy_image(font4_four, digit_buffers[4]);
		copy_image(font4_five, digit_buffers[5]);
		copy_image(font4_six, digit_buffers[6]);
		copy_image(font4_seven, digit_buffers[7]);
		copy_image(font4_eight, digit_buffers[8]);
		copy_image(font4_nine, digit_buffers[9]);
		break;

	// flip clock
	case 5:
		copy_image(font5_zero, digit_buffers[0]);
		copy_image(font5_one, digit_buffers[1]);
		copy_image(font5_two, digit_buffers[2]);
		copy_image(font5_three, digit_buffers[3]);
		copy_image(font5_four, digit_buffers[4]);
		copy_image(font5_five, digit_buffers[5]);
		copy_image(font5_six, digit_buffers[6]);
		copy_image(font5_seven, digit_buffers[7]);
		copy_image(font5_eight, digit_buffers[8]);
		copy_image(font5_nine, digit_buffers[9]);
		break;

	// flowers
	case 6:
		copy_image(font6_zero, digit_buffers[0]);
		copy_image(font6_one, digit_buffers[1]);
		copy_image(font6_two, digit_buffers[2]);
		copy_image(font6_three, digit_buffers[3]);
		copy_image(font6_four, digit_buffers[4]);
		copy_image(font6_five, digit_buffers[5]);
		copy_image(font6_six, digit_buffers[6]);
		copy_image(font6_seven, digit_buffers[7]);
		copy_image(font6_eight, digit_buffers[8]);
		copy_image(font6_nine, digit_buffers[9]);
		break;

	// nixie
	case 7:
		copy_image(font7_zero, digit_buffers[0]);
		copy_image(font7_one, digit_buffers[1]);
		copy_image(font7_two, digit_buffers[2]);
		copy_image(font7_three, digit_buffers[3]);
		copy_image(font7_four, digit_buffers[4]);
		copy_image(font7_five, digit_buffers[5]);
		copy_image(font7_six, digit_buffers[6]);
		copy_image(font7_seven, digit_buffers[7]);
		copy_image(font7_eight, digit_buffers[8]);
		copy_image(font7_nine, digit_buffers[9]);
		break;

	// retro
	case 8:
		copy_image(font8_zero, digit_buffers[0]);
		copy_image(font8_one, digit_buffers[1]);
		copy_image(font8_two, digit_buffers[2]);
		copy_image(font8_three, digit_buffers[3]);
		copy_image(font8_four, digit_buffers[4]);
		copy_image(font8_five, digit_buffers[5]);
		copy_image(font8_six, digit_buffers[6]);
		copy_image(font8_seven, digit_buffers[7]);
		copy_image(font8_eight, digit_buffers[8]);
		copy_image(font8_nine, digit_buffers[9]);
		break;

	// golden
	case 9:
		copy_image(font9_zero, digit_buffers[0]);
		copy_image(font9_one, digit_buffers[1]);
		copy_image(font9_two, digit_buffers[2]);
		copy_image(font9_three, digit_buffers[3]);
		copy_image(font9_four, digit_buffers[4]);
		copy_image(font9_five, digit_buffers[5]);
		copy_image(font9_six, digit_buffers[6]);
		copy_image(font9_seven, digit_buffers[7]);
		copy_image(font9_eight, digit_buffers[8]);
		copy_image(font9_nine, digit_buffers[9]);
		break;

	// animals
	case 10:
		copy_image(font10_zero, digit_buffers[0]);
		copy_image(font10_one, digit_buffers[1]);
		copy_image(font10_two, digit_buffers[2]);
		copy_image(font10_three, digit_buffers[3]);
		copy_image(font10_four, digit_buffers[4]);
		copy_image(font10_five, digit_buffers[5]);
		copy_image(font10_six, digit_buffers[6]);
		copy_image(font10_seven, digit_buffers[7]);
		copy_image(font10_eight, digit_buffers[8]);
		copy_image(font10_nine, digit_buffers[9]);
		break;

	// cartoons
	case 11:
		copy_image(font11_zero, digit_buffers[0]);
		copy_image(font11_one, digit_buffers[1]);
		copy_image(font11_two, digit_buffers[2]);
		copy_image(font11_three, digit_buffers[3]);
		copy_image(font11_four, digit_buffers[4]);
		copy_image(font11_five, digit_buffers[5]);
		copy_image(font11_six, digit_buffers[6]);
		copy_image(font11_seven, digit_buffers[7]);
		copy_image(font11_eight, digit_buffers[8]);
		copy_image(font11_nine, digit_buffers[9]);
		break;

	// balloons
	case 12:
		copy_image(font12_zero, digit_buffers[0]);
		copy_image(font12_one, digit_buffers[1]);
		copy_image(font12_two, digit_buffers[2]);
		copy_image(font12_three, digit_buffers[3]);
		copy_image(font12_four, digit_buffers[4]);
		copy_image(font12_five, digit_buffers[5]);
		copy_image(font12_six, digit_buffers[6]);
		copy_image(font12_seven, digit_buffers[7]);
		copy_image(font12_eight, digit_buffers[8]);
		copy_image(font12_nine, digit_buffers[9]);
		break;

	// coloured bold
	case 13:
		copy_image(font13_zero, digit_buffers[0]);
		copy_image(font13_one, digit_buffers[1]);
		copy_image(font13_two, digit_buffers[2]);
		copy_image(font13_three, digit_buffers[3]);
		copy_image(font13_four, digit_buffers[4]);
		copy_image(font13_five, digit_buffers[5]);
		copy_image(font13_six, digit_buffers[6]);
		copy_image(font13_seven, digit_buffers[7]);
		copy_image(font13_eight, digit_buffers[8]);
		copy_image(font13_nine, digit_buffers[9]);
		break;
	}
}


// sets the Wi-Fi network name
void SetSSID(const char* s)
{
	strncpy(WiFiSSID, s, SSID_MAX_LENGTH);
	SaveSettings();

	printf("SSID set to: %s\n", WiFiSSID);

	DisableNTP();

	ConnectWiFi();
}

// sets the Wi-Fi password
void SetPassword(const char* s)
{
	strncpy(WiFiPassword, s, PASSWORD_MAX_LENGTH);
	SaveSettings();

	printf("Password set to: %s\n", WiFiPassword);

	DisableNTP();

	ConnectWiFi();
}

// sets the current font
void SetFont(int font)
{
	CurrentFont = font;
	SaveSettings();

	printf("Font set to: #%d\n", CurrentFont);

	init_font();
}

// sets the current effect
void SetFx(int fx)
{
	CurrentFx = fx;
	SaveSettings();

	printf("Fx set to: #%d\n", CurrentFx);

	init_fx();
}

// toggles 12-hour/24-hour display mode
bool ToggleDisplay()
{
	Display24 = !Display24;
	SaveSettings();

	printf("Display set to: %s-hour mode\n", Display24 ? "24" : "12");

	get_time(true);
	init_fx();
	return Display24;
}

// sets the time zone
void SetZone(int minutes)
{
	TimeZone = minutes;
	SaveSettings();

	printf("Time zone set to: %d minutes\n", TimeZone);

	get_time(true);
	init_fx();
}


// pushbuttons executive
static bool do_buttons(int buttons)
{
	if ((buttons & ~BUTTON_MODE) == 0) return false;

	if ((buttons & BUTTON_MODE) != 0)
	{
		if ((buttons & BUTTON_24) != 0)
		{
			DisplayDimmed = !DisplayDimmed;
			init_font();
		}

		if ((buttons & BUTTON_PREV) != 0)
		{
			if (CurrentFont == 1) CurrentFont = NUM_FONTS; else CurrentFont--;
			SettingsTimer = to_ms_since_boot(get_absolute_time());
			init_font();
		}
		else if ((buttons & BUTTON_NEXT) != 0)
		{
			if (CurrentFont == NUM_FONTS) CurrentFont = 1; else CurrentFont++;
			SettingsTimer = to_ms_since_boot(get_absolute_time());
			init_font();
		}

		if ((buttons & BUTTON_INC) != 0)
		{
			if (CurrentFx == NUM_FXS) CurrentFx = 1; else CurrentFx++;
			SettingsTimer = to_ms_since_boot(get_absolute_time());
			init_fx();
		}
		else if ((buttons & BUTTON_DEC) != 0)
		{
			if (CurrentFx == 1) CurrentFx = NUM_FXS; else CurrentFx--;
			SettingsTimer = to_ms_since_boot(get_absolute_time());
			init_fx();
		}
	}
	else
	{
		if ((buttons & BUTTON_24) != 0)
		{
			Display24 = !Display24;
			SettingsTimer = to_ms_since_boot(get_absolute_time());
			get_time(true);
			init_fx();
		}

		if ((buttons & (BUTTON_PREV | BUTTON_NEXT | BUTTON_INC | BUTTON_DEC)) != 0)
			HighlightTimer = to_ms_since_boot(get_absolute_time());

		if ((buttons & BUTTON_PREV) != 0)
		{
			if (!DisplayHighlighted)
			{
				DisplayHighlighted = true;
				HighlightOn(CurrentDisplay);
			}
			else
			{
				HighlightOff(CurrentDisplay);
				if (CurrentDisplay == 1) CurrentDisplay = NUM_LCDS; else CurrentDisplay--;
				HighlightOn(CurrentDisplay);
			}
			init_fx();
		}
		else if ((buttons & BUTTON_NEXT) != 0)
		{
			if (!DisplayHighlighted)
			{
				DisplayHighlighted = true;
				HighlightOn(CurrentDisplay);
			}
			else
			{
				HighlightOff(CurrentDisplay);
				if (CurrentDisplay == NUM_LCDS) CurrentDisplay = 1; else CurrentDisplay++;
				HighlightOn(CurrentDisplay);
			}
			init_fx();
		}

		if ((buttons & BUTTON_INC) != 0)
		{
			if (!DisplayHighlighted)
			{
				DisplayHighlighted = true;
				HighlightOn(CurrentDisplay);
			}
			else
			{
				switch (CurrentDisplay)
				{
				case 1:
					AdjustTime(+10, 0, 0);
					break;
				case 2:
					AdjustTime(+1, 0, 0);
					break;
				case 3:
					AdjustTime(0, +10, 0);
					break;
				case 4:
					AdjustTime(0, +1, 0);
					break;
				case 5:
					AdjustTime(0, 0, +10);
					break;
				case 6:
					AdjustTime(0, 0, +1);
					break;
				}
				DisableNTP();
			}
			init_fx();
		}
		else if ((buttons & BUTTON_DEC) != 0)
		{
			if (!DisplayHighlighted)
			{
				DisplayHighlighted = true;
				HighlightOn(CurrentDisplay);
			}
			else
			{
				switch (CurrentDisplay)
				{
				case 1:
					AdjustTime(-10, 0, 0);
					break;
				case 2:
					AdjustTime(-1, 0, 0);
					break;
				case 3:
					AdjustTime(0, -10, 0);
					break;
				case 4:
					AdjustTime(0, -1, 0);
					break;
				case 5:
					AdjustTime(0, 0, -10);
					break;
				case 6:
					AdjustTime(0, 0, -1);
					break;
				}
				DisableNTP();
			}
			init_fx();
		}
	}

	return true;
}


// polls the pushbuttons, returns bit flags or 0 if not pressed
static int poll_buttons()
{
	static bool wait_release = false;

	#define pressed1 (!gpio_get(S1_PIN))
	#define pressed2 (!gpio_get(S2_PIN))
	#define pressed3 (!gpio_get(S3_PIN))
	#define pressed4 (!gpio_get(S4_PIN))
	#define pressed5 (!gpio_get(S5_PIN))
	#define pressed6 (!gpio_get(S6_PIN))

	int buttons = 0;

	if (wait_release)
	{
		if (!pressed2 && !pressed3 && !pressed4 && !pressed5 && !pressed6)
		{
			wait_release = false;
			sleep_ms(POLL_DELAY);
		}
	}
	else
	{
		if (pressed2) buttons |= BUTTON_24;
		else if (pressed3) buttons |= BUTTON_PREV;
		else if (pressed4) buttons |= BUTTON_NEXT;
		else if (pressed5) buttons |= BUTTON_INC;
		else if (pressed6) buttons |= BUTTON_DEC;

		if (buttons != 0)
		{
			if (pressed1) buttons |= BUTTON_MODE;
			wait_release = true;
			sleep_ms(POLL_DELAY);
		}
	}

	return buttons;
}


// flashes the PICO W LED
static void flash_LED()
{
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
	sleep_ms(250);
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
	sleep_ms(250);

	watchdog_update();
}


//---------------------------------------------------------------------------
// saving/loading settings
//---------------------------------------------------------------------------

// saved settings
typedef struct
{
	uint32_t Magic;
	char WiFiSSID[SSID_MAX_LENGTH+1];
	char WiFiPassword[PASSWORD_MAX_LENGTH+1];
	uint CurrentFont;
	uint CurrentFx;
	bool Display24;
	int TimeZone;
	uint32_t NTPUpdatePeriod;
} Settings;

static Settings settings;

// flash address (last sector in memory)
#define SETTINGS_FLASH_OFFSET ((0x400L - 3) * FLASH_SECTOR_SIZE)

#define MAGIC (*((uint32_t*) "CLOC"))

// saves data to flash
// fed with flash offset, pointer to data
static void SaveFlash(uint32_t offset, uint8_t* p)
{
	uint32_t interrupts = save_and_disable_interrupts();

	flash_range_erase(offset, FLASH_SECTOR_SIZE);
	flash_range_program(offset, p, FLASH_PAGE_SIZE);

	restore_interrupts(interrupts);
}

// loads settings from non-volatile memory
void LoadSettings()
{
	settings = *((Settings*) (XIP_BASE + SETTINGS_FLASH_OFFSET));

	if (settings.Magic != MAGIC) return;

	strncpy(WiFiSSID, settings.WiFiSSID, SSID_MAX_LENGTH);
	strncpy(WiFiPassword, settings.WiFiPassword, PASSWORD_MAX_LENGTH);
	CurrentFont = settings.CurrentFont;
	CurrentFx = settings.CurrentFx;
	Display24 = settings.Display24;
	TimeZone = settings.TimeZone;
	NTPUpdatePeriod = settings.NTPUpdatePeriod;
}

// saves settings to non-volatile memory
void SaveSettings()
{
	settings.Magic = MAGIC;

	strncpy(settings.WiFiSSID, WiFiSSID, SSID_MAX_LENGTH);
	strncpy(settings.WiFiPassword, WiFiPassword, PASSWORD_MAX_LENGTH);
	settings.CurrentFont = CurrentFont;
	settings.CurrentFx = CurrentFx;
	settings.Display24 = Display24;
	settings.TimeZone = TimeZone;
	settings.NTPUpdatePeriod = NTPUpdatePeriod;

	SaveFlash(SETTINGS_FLASH_OFFSET, (uint8_t*) &settings);
}


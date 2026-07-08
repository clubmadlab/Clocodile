/****************************************************************************
* FILE:      cli.c															*
* CONTENTS:  Command line interpreter										*
* COPYRIGHT: MadLab Ltd. 2026												*
* AUTHOR:    James Hutchby													*
* UPDATED:   26/05/26														*
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"

#include "common.h"
#include "main.h"
#include "cli.h"
#include "ntp.h"


//---------------------------------------------------------------------------
// commands
//---------------------------------------------------------------------------

// SSID <string> - specifies the Wi-Fi network name
// Password <string> - specifies the Wi-Fi password
// Time <hours>:<mins>:<secs> | <hours>:<mins> - sets the current time (disables network time updates and resets the time zone)
// Display - toggles 12-hour or 24-hour mode
// Zone <hours>:<mins> | <hours> - specifies the time zone, hours = -12 to +14
// Font <n> - specifies the current font, n = 1 to 14
// Fx <n> - specifies the current transition effect, n = 1 to 14
// Synch <hours>:<mins>:<secs> | <hours>:<mins> | <hours> - network time update period, 0 to disable
// Help - displays this list of commands
// Help font - displays a list of available fonts
// Help fx - displays a list of available transition effects


//---------------------------------------------------------------------------
// constants
//---------------------------------------------------------------------------

#define RX_BUFFER_LEN 100		// receive buffer length


//---------------------------------------------------------------------------
// variables
//---------------------------------------------------------------------------

// receive buffer
static int RX_bytes;
static char RX_buffer[RX_BUFFER_LEN+1];

// command strings
static char* commands[] =
{
	"SSID",
	"Password",
	"Time",
	"Display",
	"Zone",
	"Font",
	"Fx",
	"Synch",
	"Help"
};


//---------------------------------------------------------------------------
// functions
//---------------------------------------------------------------------------

// initialises CLI
void InitCLI()
{
	RX_bytes = 0;

	printf("Type Help for a list of commands.\n");
}


static void help()
{
	printf("Commands:\n");
	printf(" SSID <string> - specifies the Wi-Fi network name\n");
	printf(" Password <string> - specifies the Wi-Fi password\n");
	printf(" Time <hours>:<mins>:<secs> | <hours>:<mins> - sets the current time "
	  "(disables network time updates and resets the time zone)\n");
	printf(" Display - toggles 12-hour or 24-hour mode\n");
	printf(" Zone <hours>:<mins> | <hours> - specifies the time zone, hours = -12 to +14\n");
	printf(" Font <n> - specifies the current font, n = 1 to 14\n");
	printf(" Fx <n> - specifies the current transition effect, n = 1 to 14\n");
	printf(" Synch <hours>:<mins>:<secs> | <hours>:<mins> | <hours> - network time update period, 0 to disable\n");
	printf(" Help - displays this list of commands\n");
	printf(" Help font - displays a list of available fonts\n");
	printf(" Help fx - displays a list of available transition effects\n\n");
}

static void help_font()
{
	printf("Fonts:\n");
	printf(" 1 = blue 7-segment LED\n");
	printf(" 2 = red 7-segment LED\n");
	printf(" 3 = amber backlight 7-segment LCD\n");
	printf(" 4 = 'scope\n");
	printf(" 5 = flip digits\n");
	printf(" 6 = flowers\n");
	printf(" 7 = Nixie tubes\n");
	printf(" 8 = retro\n");
	printf(" 9 = golden\n");
	printf(" 10 = animals\n");
	printf(" 11 = cartoons\n");
	printf(" 12 = balloons\n");
	printf(" 13 = coloured bold\n");
	printf(" 14 = Matrix digital rain\n\n");
}

static void help_fx()
{
	printf("Transition effects:\n");
	printf(" 1 = simple replacement\n");
	printf(" 2 = scroll down both\n");
	printf(" 3 = scroll down over\n");
	printf(" 4 = overwrite\n");
	printf(" 5 = slide in from the left\n");
	printf(" 6 = overwrite from the left\n");
	printf(" 7 = blinds\n");
	printf(" 8 = expand outwards from centre\n");
	printf(" 9 = expand outwards from four corners in turn\n");
	printf(" 10 = slide outwards from four corners in turn\n");
	printf(" 11 = expand outwards from centre in a spiral\n");
	printf(" 12 = erase outwards from centre then expand outwards\n");
	printf(" 13 = dissolve\n");
	printf(" 14 = flip digits\n\n");
}


// CLI executive
void DoCLI()
{
	static bool CRorLF = false;
	int ret;

	while (true)
	{
		int c = getchar_timeout_us(0);
		if (c == PICO_ERROR_TIMEOUT) return;

		if (CRorLF && (c == '\r' || c == '\n'))
		{
			CRorLF = false;
			return;
		}

		CRorLF = (c == '\r' || c == '\n');

		if (CRorLF)
		{
			RX_buffer[RX_bytes] = '\0';
			break;
		}
		else
		{
			RX_buffer[RX_bytes++] = c;
			if (RX_bytes < RX_BUFFER_LEN) continue;
			RX_buffer[RX_bytes] = '\0';
			break;
		}
	}

	RX_bytes = 0;

	if (RX_buffer[0] == '\0')
	{
		help();
		return;
	}

	static char command[RX_BUFFER_LEN+1];
	static char argument[RX_BUFFER_LEN+1];
	command[0] = argument[0] = '\0';
	ret = sscanf(RX_buffer, "%s %s", command, argument);
	if (ret < 1) return;

	int hours, mins, secs, com;

	for (com = 0; com < sizeof(commands)/sizeof(char*); com++)
	{
		if (strcasecmp(command, commands[com]) == 0) break;
	}

	switch (com)
	{
	case 0:
		SetSSID(argument);
		break;

	case 1:
		SetPassword(argument);
		break;

	case 2:
		hours = mins = secs = 0;
		ret = sscanf(argument, "%d:%d:%d", &hours, &mins, &secs);
		if (ret < 2) return;
		if (hours < 0 || hours > 23 || mins < 0 || mins >= 60 || secs < 0 || secs >= 60) printf("Invalid argument\n");
		else {DisableNTP(); SetZone(0); SetTime(hours, mins, secs);}
		break;

	case 3:
		ToggleDisplay();
		break;

	case 4:
		hours = mins = 0;
		ret = sscanf(argument, "%d:%d", &hours, &mins);
		if (ret < 1) return;
		if (hours < -12 || hours > +14 || mins < 0 || mins >= 60) printf("Invalid argument\n");
		else SetZone(hours < 0 ? hours * 60 - mins : hours * 60 + mins);
		break;

	case 5:
		int font = 1;
		ret = sscanf(argument, "%d", &font);
		if (ret < 1) return;
		if (font < 1 || font > NUM_FONTS) printf("Invalid argument\n");
		else SetFont(font);
		break;

	case 6:
		int fx = 1;
		ret = sscanf(argument, "%d", &fx);
		if (ret < 1) return;
		if (fx < 1 || fx > NUM_FXS) printf("Invalid argument\n");
		else SetFx(fx);
		break;

	case 7:
		hours = mins = secs = 0;
		ret = sscanf(argument, "%d:%d:%d", &hours, &mins, &secs);
		if (ret < 1) return;
		if (hours < 0 || hours > 23 || mins < 0 || mins >= 60 || secs < 0 || secs >= 60) printf("Invalid argument\n");
		else SetNTPUpdatePeriod(hours * 60 * 60 + mins * 60 + secs);
		break;

	case 8:
		if (strcasecmp(argument, "font") == 0) help_font();
		else if (strcasecmp(argument, "fx") == 0) help_fx();
		else help();
		break;

	default:
		printf("Invalid command\n");
		break;
	}
}

/*  wmtime - Window Maker dockapp that displays the time and date
    Copyright (C) 1997, 1998 Martijn Pieterse <pieterse@xs4all.nl>
    Copyright (C) 1997, 1998 Antoine Nulle <warp@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
	Code based on wmppp/wmifs

	[Orig WMMON comments]

	This code was mainly put together by looking at the
	following programs:

	asclock
		A neat piece of equip, used to display the date
		and time on the screen.
		Comes with every AfterStep installation.

		Source used:
			How do I create a not so solid window?
			How do I open a window?
			How do I use pixmaps?

	------------------------------------------------------------

	Author: Martijn Pieterse (pieterse@xs4all.nl)

	This program is distributed under the GPL license.
	(as were asclock and pppstats)

	----
	Changes:
	----
   15/07/2008 (Paul Harris, harris.pc@gmail.com)
      * Minor changes to correct build warnings
	09/10/2003 (Simon Law, sfllaw@debian.org)
		* Add -geometry support
		* Add -noseconds support
		* Make the digital clock fill the space provided
		* Eliminated exploitable static buffers
	17/05/1998 (Antoine Nulle, warp@xs4all.nl)
		* Updated version number and some other minor stuff
	16/05/1998 (Antoine Nulle, warp@xs4all.nl)
		* Added Locale support, based on original diff supplied
		  by Alen Salamun (snowman@hal9000.medinet.si)
	04/05/1998 (Martijn Pieterse, pieterse@xs4all.nl)
		* Moved the hands one pixel down.
		* Removed the RedrawWindow out of the main loop
	02/05/1998 (Martijn Pieterse, pieterse@xs4all.nl)
		* Removed a lot of code that was in the wmgeneral dir.
	02/05/1998 (Antoine Nulle, warp@xs4all.nl)
		* Updated master-xpm, hour dots where a bit 'off'
	30/04/1998 (Martijn Pieterse, pieterse@xs4all.nl)
		* Added anti-aliased hands
	23/04/1998 (Martijn Pieterse, pieterse@xs4all.nl)
		* Changed the hand lengths.. again! ;)
		* Zombies were created, so added wait code
	21/04/1998 (Martijn Pieterse, pieterse@xs4all.nl)
		* Added digital/analog switching support
	18/04/1998 (Martijn Pieterse, pieterse@xs4all.nl)
		* Started this project.
		* Copied the source from wmmon.
*/

#define _GNU_SOURCE
#include <X11/X.h>                     /* for ButtonPress, ButtonRelease, etc */
#include <X11/Xlib.h>                  /* for XEvent, XButtonEvent, etc */
#include <X11/xpm.h>
#include <ctype.h>                     /* for toupper */
#include <iconv.h>                     /* for iconv, iconv_close, etc */
#include <langinfo.h>                  /* for nl_langinfo, ABDAY_1, etc */
#include <locale.h>                    /* for NULL, setlocale, LC_ALL */
#include <math.h>                      /* for floor, cos, sin, M_PI */
#include <stddef.h>                    /* for size_t */
#include <stdio.h>                     /* for printf, asprintf, snprintf, etc */
#include <stdlib.h>                    /* for abs, free, exit, getenv */
#include <string.h>                    /* for strcmp, strdup, strncpy, etc */
#include <sys/wait.h>                  /* for waitpid, WNOHANG */
#include <time.h>                      /* for tm, time, localtime */
#include <unistd.h>                    /* for usleep */
#include "wmgeneral/misc.h"            /* for execCommand */
#include "wmgeneral/wmgeneral.h"       /* for copyXPMArea, RedrawWindow, etc */
#include "wmtime-mask.xbm"             /* for wmtime_mask_bits */
#include "wmtime-master.xpm"           /* for wmtime_master_xpm */


  /***********/
 /* Defines */
/***********/

const char* default_left_action = NULL;
const char* default_middle_action = NULL;
const char* default_right_action = NULL;

#define WMTIME_VERSION "1.3"

  /********************/
 /* Global Variables */
/********************/

int		digital = 0;
int		noseconds = 0;
char	day_of_week[7][3] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
char	mon_of_year[12][4] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
XpmIcon wmgen;
char color[256];

/* functions */
void usage(char *);
void printversion(void);

void wmtime_routine(int, char **);
void get_lang();


int main(int argc, char *argv[]) {

	int		i;
	char    *name = argv[0];
	char locale[256];

	locale[0] = 0;
	color[0] = 0;

	for (i=1; i<argc; i++) {
		char *arg = argv[i];

		if (*arg=='-') {
			switch (arg[1]) {
			case 'c' :
				if (argc > i+1) {
					strcpy(color, argv[i+1]);
					i++;
				}
				break;
			case 'd' :
				if (strcmp(arg+1, "display")
				    && strcmp(arg+1, "digital") && strcmp(arg+1, "d")) {
					usage(name);
					return 1;
				}
				if (!strcmp(arg+1, "digital") || !(strcmp(arg+1, "d")))
					digital = 1;
				break;
			case 'g' :
				if (strcmp(arg+1, "geometry")) {
					usage(name);
					return 1;
				}
				break;
			case 'n' :
				if (strcmp(arg+1, "noseconds") && strcmp(arg+1, "n")) {
					usage(name);
					return 1;
				} else {
					noseconds = 1;
				}
				break;
			case 'v' :
				printversion();
				return 0;
			case 'l':
				if (argc > i+1) {
					strcpy(locale, argv[i+1]);
					i++;
				}
				break;
			default:
				usage(name);
				return 1;
			}
		}
	}

	if (setlocale(LC_ALL, locale) == NULL)
		fprintf(stderr,
			"warning: locale '%s' not recognized; defaulting to '%s'.",
			locale, setlocale(LC_ALL, NULL));
	get_lang();

	wmtime_routine(argc, argv);
	return 0;
}

/************/
/* get_lang */
/************/
void get_lang(void)
{
	char langbuf[10], outbuf[10];
	char *inp, *outp;
	iconv_t icd;
	int i, ret;
	size_t insize, outsize;

	icd = iconv_open("ASCII//TRANSLIT", nl_langinfo(CODESET));
	if (icd == (iconv_t) -1) {
		perror("wmtime: Error allocating charset conversion descriptor");
		return;
	}

	for (i = 0; i < 7; i++) {
		strncpy(langbuf, nl_langinfo(ABDAY_1 + i), 10);
		insize = outsize = 10;
		inp = langbuf;
		outp = outbuf;
		do {
			ret = iconv(icd, &inp, &insize, &outp, &outsize);
		} while (outsize > 0 && ret > 0);
		if (strstr(outbuf,"?") != NULL) return;
		for (outp = outbuf, outsize = 0; *outp != 0 && outsize < 2;
		    outp++, outsize++)
			day_of_week[i][outsize] = toupper(*outp);
	}
	for (i = 0; i < 12; i++) {
		strncpy(langbuf, nl_langinfo(ABMON_1 + i), 10);
		insize = outsize = 10;
		inp = langbuf;
		outp = outbuf;
		do {
			ret = iconv(icd, &inp, &insize, &outp, &outsize);
		} while (outsize > 0 && ret > 0);
		if (strstr(outbuf,"?") != NULL) return;
		for (outp = outbuf, outsize = 0; *outp != 0 && outsize < 3;
		    outp++, outsize++)
			mon_of_year[i][outsize] = toupper(*outp);
	}

	iconv_close(icd);
}

Pixel scale_pixel(Pixel pixel, float scale)
{
	int red, green, blue;

	red = pixel / ( 1 << 16 );
	green = pixel % (1 << 16) / (1 << 8);
	blue = pixel % (1 << 8);

	red *= scale;
	green *= scale;
	blue *= scale;

	return red * (1 << 16) + green * (1 << 8) + blue;
}

/*******************************************************************************\
|* wmtime_routine															   *|
\*******************************************************************************/

char		*left_action = NULL;
char		*right_action = NULL;
char		*middle_action = NULL;

void DrawTime(int, int, int);
void DrawWijzer(int, int, int);
void DrawDate(int, int, int);

void wmtime_routine(int argc, char **argv) {

	rckeys		wmtime_keys[] = {
		{ "left", &left_action },
		{ "right", &right_action },
		{ "middle", &middle_action },
		{ NULL, NULL }
	};

	int			i;
	XEvent		Event;
	int			but_stat = -1;

	struct tm	*time_struct;

	long		starttime;
	long		curtime;

	char		*conffile = NULL;

	/* Scan through ~/.wmtimerc for the mouse button actions. */
	if (default_left_action) left_action = strdup(default_left_action);
	if (default_middle_action) middle_action = strdup(default_middle_action);
	if (default_right_action) right_action = strdup(default_right_action);

	/* Scan through the .rc files */
	if (asprintf(&conffile, "/etc/wmtimerc") >= 0) {
		parse_rcfile(conffile, wmtime_keys);
		free(conffile);
	}

	if (asprintf(&conffile, "%s/.wmtimerc", getenv("HOME")) >= 0) {
		parse_rcfile(conffile, wmtime_keys);
		free(conffile);
	}

	if (asprintf(&conffile, "/etc/wmtimerc.fixed") >= 0) {
		parse_rcfile(conffile, wmtime_keys);
		free(conffile);
	}

	/* set user-defined colors */
	if (color[0] != 0) {
		Window	Root;
		XColor col;
		XWindowAttributes attributes;
		int screen;
		Pixel pixel;
#define NUMSYMBOLS 10
		XpmColorSymbol user_color[NUMSYMBOLS] = {
			{NULL, "#2081B2CAAEBA", 0}, /* O */
			{NULL, "#000049244103", 0}, /* + */
			{NULL, "#00007DF771C6", 0}, /* @ */
			{NULL, "#18618A288617", 0}, /* # */
			{NULL, "#18619A699658", 0}, /* ; */
			{NULL, "#0820861779E7", 0}, /* : */
			{NULL, "#000071C66185", 0}, /* > */
			{NULL, "#000061855144", 0}, /* , */
			{NULL, "#00004D344103", 0}, /* < */
			{NULL, "#10407DF779E7", 0}  /* 1 */
		};


		/* code based on GetColor() from wmgeneral.c */
		/* we need a temporary display to parse the color */
		display = XOpenDisplay(NULL);
		screen = DefaultScreen(display);
		Root = RootWindow(display, screen);
		XGetWindowAttributes(display, Root, &attributes);

		col.pixel = 0;
		if (!XParseColor(display, attributes.colormap, color, &col)) {
			fprintf(stderr, "wmtime: can't parse %s.\n", color);
			goto draw_window;
		} else if (!XAllocColor(display, attributes.colormap, &col)) {
			fprintf(stderr, "wmtime: can't allocate %s.\n", color);
			goto draw_window;
		}

		pixel = col.pixel;

		/* replace colors from wmtime-master.xpm */
		user_color[0].pixel = pixel;
		user_color[1].pixel = scale_pixel(pixel, .4);
		user_color[2].pixel = scale_pixel(pixel, .7);
		user_color[3].pixel = scale_pixel(pixel, .8);
		user_color[4].pixel = scale_pixel(pixel, .9);
		user_color[5].pixel = scale_pixel(pixel, .8);
		user_color[6].pixel = scale_pixel(pixel, .6);
		user_color[7].pixel = scale_pixel(pixel, .5);
		user_color[8].pixel = scale_pixel(pixel, .4);
		user_color[9].pixel = scale_pixel(pixel, .7);

		wmgen.attributes.valuemask |= XpmColorSymbols;
		wmgen.attributes.numsymbols = NUMSYMBOLS;
		wmgen.attributes.colorsymbols = user_color;

		XCloseDisplay(display);
	}

draw_window:
	openXwindow(argc, argv, wmtime_master_xpm, (char*)wmtime_mask_bits, 128, 64);

	/* Mask out the right parts of the clock */
	copyXPMArea(0, 0, 128, 64, 0, 98);   /* Draw the borders */
	copyXPMArea(0, 0, 64, 64, 64, 0);    /* Draw the clock face */
	copyXPMArea(64, 98, 64, 64, 0, 0);   /* Draw the LCD background */
	setMaskXY(0, 0);

    /* add mouse region */
	AddMouseRegion(0, 5, 48, 58, 60);
	AddMouseRegion(1, 5, 5, 58, 46);

	starttime = time(0);

	curtime = time(0);
	time_struct = localtime(&curtime);

	while (1) {
		curtime = time(0);

		waitpid(0, NULL, WNOHANG);

		time_struct = localtime(&curtime);


		if (curtime >= starttime) {
			if (!digital) {
				/* Now to update the seconds */

				DrawWijzer(time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec);

				DrawDate(time_struct->tm_wday, time_struct->tm_mday, time_struct->tm_mon);

			} else {

				DrawTime(time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec);

				DrawDate(time_struct->tm_wday, time_struct->tm_mday, time_struct->tm_mon);
			}
			RedrawWindow();
		}


		while (XPending(display)) {
			XNextEvent(display, &Event);
			switch (Event.type) {
			case Expose:
				RedrawWindow();
				break;
			case DestroyNotify:
				XCloseDisplay(display);
				exit(0);
				break;
			case ButtonPress:
				but_stat = CheckMouseRegion(Event.xbutton.x, Event.xbutton.y);
				break;
			case ButtonRelease:
				i = CheckMouseRegion(Event.xbutton.x, Event.xbutton.y);
				if (but_stat == i && but_stat >= 0) {
					switch (but_stat) {
					case 0:
						digital = 1-digital;

						if (digital) {
							copyXPMArea(64, 98, 64, 64, 0, 0);
							DrawTime(time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec);
							DrawDate(time_struct->tm_wday, time_struct->tm_mday, time_struct->tm_mon);
						} else {
							copyXPMArea(0, 98, 64, 64, 0, 0);
							DrawWijzer(time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec);
							DrawDate(time_struct->tm_wday, time_struct->tm_mday, time_struct->tm_mon);
						}
						RedrawWindow();
						break;
					case 1:
						switch (Event.xbutton.button) {
						case 1:
							if (left_action)
								execCommand(left_action);
							break;
						case 2:
							if (middle_action)
								execCommand(middle_action);
							break;
						case 3:
							if (right_action)
								execCommand(right_action);
							break;
						}
					}
				}
				break;
			}
		}

		/* Sleep 0.3 seconds */
		usleep(300000L);
	}
}

/*******************************************************************************\
|* DrawTime																	   *|
\*******************************************************************************/

void DrawTime(int hr, int min, int sec) {
#define TIME_SIZE 16
	char	time[TIME_SIZE];
	char	*p = time;
	int		i,j,k=6;
	int numfields;

	/* 7x13 */

	if (noseconds) {
		snprintf(time, TIME_SIZE, "%02d:%02d ", hr, min);
		numfields = 2;
	}
	else {
		snprintf(time, TIME_SIZE, "%02d:%02d:%02d ", hr, min, sec);
		numfields = 3;
	}

	for (i=0; i < numfields; i++) {
		for (j=0; j<2; j++) {
			copyXPMArea((*p-'0')*7 + 1, 84, 8, 13, k, 18);
			k += 7;
			p++;
		}
		if (*p == ':') {
			copyXPMArea(71, 84, 5, 13, k, 18);
			k += 4;
			p++;
		}
	}
}

/*******************************************************************************\
|* DrawDate																	   *|
\*******************************************************************************/

void DrawDate(int wkday, int dom, int month) {
#define DATE_SIZE 16
	char	date[DATE_SIZE];
	char	*p = date;
	int		i,k;

	/* 7x13 */

	snprintf(date, DATE_SIZE,
			"%.2s%02d%.3s  ", day_of_week[wkday], dom, mon_of_year[month]);

	k = 5;
	for (i=0; i<2; i++) {
		if (*p < 'A')
			copyXPMArea((*p-'0')*6, 64, 6, 9, k, 49);
		else
			copyXPMArea((*p-'A')*6, 74, 6, 9, k, 49);
		k += 6;
		p++;
	}
	k = 23;
	for (i=0; i<2; i++) {
		copyXPMArea((*p-'0')*6, 64, 6, 9, k, 49);
		k += 6;
		p++;
	}
	copyXPMArea(61, 64, 4, 9, k, 49);
	k += 4;
	for (i=0; i<3; i++) {
		if (*p < 'A')
			copyXPMArea((*p-'0')*6, 64, 6, 9, k, 49);
		else
			copyXPMArea((*p-'A')*6, 74, 6, 9, k, 49);
		k += 6;
		p++;
	}
}

/*******************************************************************************\
|* DrawWijzer																   *|
\*******************************************************************************/

void DrawWijzer(int hr, int min, int sec) {

	double		psi;
	int			dx,dy;
	int			x,y;
	int			ddx,ddy;
	int			adder;
	int			k;

	int			i;

	hr %= 12;

	copyXPMArea(5+64, 5, 54, 40, 5, 5);

	/**********************************************************************/
	psi = hr * (M_PI / 6.0);
	psi += min * (M_PI / 360);

	dx = floor(sin(psi) * 22 * 0.7 + 0.5);
	dy = floor(-cos(psi) * 16 * 0.7 + 0.5);

	/* dx, dy is het punt waar we naar toe moeten.
	 * Zoek alle punten die ECHT op de lijn liggen: */

	ddx = 1;
	ddy = 1;
	if (dx < 0) ddx = -1;
	if (dy < 0) ddy = -1;

	x = 0;
	y = 0;

	if (abs(dx) > abs(dy)) {
		if (dy != 0)
			adder = abs(dx) / 2;
		else
			adder = 0;
		for (i=0; i<abs(dx); i++) {
			/* laat de kleur afhangen van de adder.
			 * adder loopt van abs(dx) tot 0 */

			k = 12 - adder / (abs(dx) / 12.0);
			copyXPMArea(79+k, 67, 1, 1, x + 31, y + 24 - ddy);

			copyXPMArea(79, 67, 1, 1, x + 31, y + 24);

			k = 12-k;
			copyXPMArea(79+k, 67, 1, 1, x + 31, y + 24 + ddy);


			x += ddx;

			adder -= abs(dy);
			if (adder < 0) {
				adder += abs(dx);
				y += ddy;
			}
		}
	} else {
		if (dx != 0)
			adder = abs(dy) / 2;
		else
			adder = 0;
		for (i=0; i<abs(dy); i++) {
			k = 12 - adder / (abs(dy) / 12.0);
			copyXPMArea(79+k, 67, 1, 1, x + 31 - ddx, y + 24);

			copyXPMArea(79, 67, 1, 1, x + 31, y + 24);

			k = 12-k;
			copyXPMArea(79+k, 67, 1, 1, x + 31 + ddx, y + 24);

			y += ddy;

			adder -= abs(dx);
			if (adder < 0) {
				adder += abs(dy);
				x += ddx;
			}
		}
	}
	/**********************************************************************/
	psi = min * (M_PI / 30.0);
	psi += sec * (M_PI / 1800);

	dx = floor(sin(psi) * 22 * 0.55 + 0.5);
	dy = floor(-cos(psi) * 16 * 0.55 + 0.5);

	/* dx, dy is het punt waar we naar toe moeten.
	 * Zoek alle punten die ECHT op de lijn liggen: */

	dx += dx;
	dy += dy;

	ddx = 1;
	ddy = 1;
	if (dx < 0) ddx = -1;
	if (dy < 0) ddy = -1;

	x = 0;
	y = 0;

	if (abs(dx) > abs(dy)) {
		if (dy != 0)
			adder = abs(dx) / 2;
		else
			adder = 0;
		for (i=0; i<abs(dx); i++) {
			/* laat de kleur afhangen van de adder.
			 * adder loopt van abs(dx) tot 0 */

			k = 12 - adder / (abs(dx) / 12.0);
			copyXPMArea(79+k, 67, 1, 1, x + 31, y + 24 - ddy);

			copyXPMArea(79, 67, 1, 1, x + 31, y + 24);

			k = 12-k;
			copyXPMArea(79+k, 67, 1, 1, x + 31, y + 24 + ddy);


			x += ddx;

			adder -= abs(dy);
			if (adder < 0) {
				adder += abs(dx);
				y += ddy;
			}
		}
	} else {
		if (dx != 0)
			adder = abs(dy) / 2;
		else
			adder = 0;
		for (i=0; i<abs(dy); i++) {
			k = 12 - adder / (abs(dy) / 12.0);
			copyXPMArea(79+k, 67, 1, 1, x + 31 - ddx, y + 24);

			copyXPMArea(79, 67, 1, 1, x + 31, y + 24);

			k = 12-k;
			copyXPMArea(79+k, 67, 1, 1, x + 31 + ddx, y + 24);

			y += ddy;

			adder -= abs(dx);
			if (adder < 0) {
				adder += abs(dy);
				x += ddx;
			}
		}
	}
	/**********************************************************************/
	if (noseconds)
		return;    /* Skip drawing the seconds. */

	psi = sec * (M_PI / 30.0);

	dx = floor(sin(psi) * 22 * 0.9 + 0.5);
	dy = floor(-cos(psi) * 16 * 0.9 + 0.5);

	/* dx, dy is het punt waar we naar toe moeten.
	 * Zoek alle punten die ECHT op de lijn liggen: */

	ddx = 1;
	ddy = 1;
	if (dx < 0) ddx = -1;
	if (dy < 0) ddy = -1;

	if (dx == 0) ddx = 0;
	if (dy == 0) ddy = 0;

	x = 0;
	y = 0;


	if (abs(dx) > abs(dy)) {
		if (dy != 0)
			adder = abs(dx) / 2;
		else
			adder = 0;
		for (i=0; i<abs(dx); i++) {
			/* laat de kleur afhangen van de adder.
			 * adder loopt van abs(dx) tot 0 */

			k = 12 - adder / (abs(dx) / 12.0);
			copyXPMArea(79+k, 70, 1, 1, x + 31, y + 24 - ddy);

			k = 12-k;
			copyXPMArea(79+k, 70, 1, 1, x + 31, y + 24);


			x += ddx;

			adder -= abs(dy);
			if (adder < 0) {
				adder += abs(dx);
				y += ddy;
			}
		}
	} else {
		if (dx != 0)
			adder = abs(dy) / 2;
		else
			adder = 0;
		for (i=0; i<abs(dy); i++) {
			k = 12 - adder / (abs(dy) / 12.0);
			copyXPMArea(79+k, 70, 1, 1, x + 31 - ddx, y + 24);

			k = 12-k;
			copyXPMArea(79+k, 70, 1, 1, x + 31, y + 24);

			y += ddy;

			adder -= abs(dx);
			if (adder < 0) {
				adder += abs(dy);
				x += ddx;
			}
		}
	}
}

/*******************************************************************************\
|* usage																	   *|
\*******************************************************************************/

void usage(char *name) {
	printf("Usage: %s [OPTION]...\n", name);
	printf("WindowMaker dockapp that displays the time and date.\n");
	printf("\n");
	printf("  -d, -digital         display the digital clock\n");
	printf("  -display DISPLAY     contact the DISPLAY X server\n");
	printf("  -geometry GEOMETRY   position the clock at GEOMETRY\n");
	printf("  -n, -noseconds       disables the second hand\n");
	printf("  -l LOCALE            set locale to LOCALE\n");
	printf("  -h                   display this help and exit\n");
	printf("  -v                   output version information and exit\n");
	printf("  -c                   set color\n");
}

/*******************************************************************************\
|* printversion																   *|
\*******************************************************************************/

void printversion(void) {
	printf("WMTime version %s\n", WMTIME_VERSION);
}

/* vim: sw=4 ts=4 columns=82
 */

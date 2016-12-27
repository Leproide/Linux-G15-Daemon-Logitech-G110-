 /*
    This file is part of g15daemon.

    g15daemon is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    g15daemon is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with g15daemon; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    (c) 2006-2008 Mike Lampard, Philip Lawatsch, and others
    
    $Revision: 391 $ -  $Date: 2008-01-02 17:02:46 +1030 (Wed, 02 Jan 2008) $ $Author: mlampard $
        
simple Clock plugin, replace the various functions with your own, and change the g15plugin_info struct below to suit,
   edit Makefile.am and compile.  Add salt and pepper to taste.  For a more advanced plugin that creates it's own lcd screens on-the-fly, 
   see the tcpserver plugin in this directory.
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <libg15.h>
#include <config.h>
#include <g15daemon.h>
#include <libg15render.h>

extern double round(double);

// clock specs:
#define CLOCK_CENTERX	25
#define CLOCK_CENTERY	21
#define CLOCK_RADIUS	20

// useful shortcuts (g15r_drawCircle() is a bit egg-shaped, or my math is :)
#define CLOCK_STARTX	(CLOCK_CENTERX-CLOCK_RADIUS-1)
#define CLOCK_STARTY	(CLOCK_CENTERY-CLOCK_RADIUS)
#define CLOCK_ENDX		(CLOCK_CENTERX+CLOCK_RADIUS+1)
#define CLOCK_ENDY		(CLOCK_CENTERY+CLOCK_RADIUS)

static int mode=1;
static int showdate=0;
static int digital=1;
g15canvas *static_canvas = NULL;

//----------------------------------------------------------------------------
// calc x,y for given minute/hour/sec (pos), cut_off is for radius variations
// ( ie. shorter/longer clock-hands, line parts....)
static void get_clock_pos(int pos, int *x, int *y, int cut_off)
{
  // pos = [0-60]

  // make sure it's in range:
  pos %= 60;

  // angles go contra-clockwise, but clock goes clockwise :), so invert clock orientation  
  pos = 60 - pos;
  
  // this math is patent-copy-trademark-protected by <Rasta Freak> :)
  double ang = 270.0 - 6.0*(double)pos;
  ang = (ang * 2.0 * M_PI) / 360.0;
  
  // simple pre-school math for naturaly dumb:
  double _x = (double)CLOCK_CENTERX + (cos(ang)*(double)(CLOCK_RADIUS+1-cut_off));
  double _y = (double)CLOCK_CENTERY + (sin(ang)*(double)(CLOCK_RADIUS-cut_off));
  
  *x = (int)(round(_x));
  *y = (int)(round(_y));
}

//----------------------------------------------------------------------------
// draw clock frame (only once, as it is stored in static_canvas)
// NOTE - coords here are hardcoded !
static void draw_static_canvas(void)
{
  g15canvas *c = static_canvas;
  int i;
  
  g15r_clearScreen (c, G15_COLOR_WHITE);
  g15r_drawCircle(c, CLOCK_CENTERX, CLOCK_CENTERY, CLOCK_RADIUS, 0, G15_COLOR_BLACK);
  g15r_drawCircle(c, CLOCK_CENTERX, CLOCK_CENTERY, 2,            1, G15_COLOR_BLACK);

  for (i=0; i<60; i+=5)
  {
	if ((i%15)==0)
	{
	  // draw number (12/3/6/9):
	  switch (i)
	  {
		case 0:
		g15r_renderString(c, (unsigned char*)"12", 0, G15_TEXT_SMALL, 22, 3);
		break;

		case 15:
		g15r_renderString(c, (unsigned char*)"3", 3, G15_TEXT_SMALL, 42, 1);
		break;

		case 30:
		g15r_renderString(c, (unsigned char*)"6", 6, G15_TEXT_SMALL, 24, -1);
		break;

		case 45:
		g15r_renderString(c, (unsigned char*)"9", 3, G15_TEXT_SMALL, 6, 1);
		break;
	  }
	}
	else
	{
	  // draw 4-pixel square dot for other hours:
	  int x1,y1,dir;
	  if (i>15 && i<45) dir=-1; else dir=1;
  	  get_clock_pos(i, &x1, &y1,  3);
	  g15r_setPixel(c, x1,     y1,     G15_COLOR_BLACK);
	  g15r_setPixel(c, x1+dir, y1,     G15_COLOR_BLACK);
	  g15r_setPixel(c, x1,     y1+dir, G15_COLOR_BLACK);
	  g15r_setPixel(c, x1+dir, y1+dir, G15_COLOR_BLACK);
	}
  }
}

static int draw_digital(g15canvas *canvas)
{
    unsigned int col = 0;
    unsigned int len=0;
    int narrows=0;
    int totalwidth=0;
    char buf[10];
    char ampm[3];
    int height = G15_LCD_HEIGHT - 1;
 
    time_t currtime = time(NULL);
    
    memset(buf,0,10);
    memset(ampm,0,3);
    if(showdate) {
        char buf2[40];
        strftime(buf2,40,"%A %e %B %Y",localtime(&currtime));
        g15r_renderString (canvas,(unsigned char *)buf2 , 0, G15_TEXT_MED, 80-((strlen(buf2)*5)/2), height-6);
        height-=10;
      }

    if(mode) {
   	strftime(buf,6,"%H:%M",localtime(&currtime));
    } else { 
        strftime(buf,6,"%l:%M",localtime(&currtime));
	strftime(ampm,3,"%p",localtime(&currtime));
    }
    if(buf[0]==49) 
    	narrows=1;
    
    len = strlen(buf); 
    if(buf[0]==' ')
     len++;
    
    if(narrows)
        totalwidth=(len*20)+(15);
    else
        totalwidth=len*20;

    for (col=0;col<len;col++) 
      {
      	int num = -1;
	if ((buf[col] > 47) && (buf[col] < 59))
	  num = buf[col] - 48;
	else if (buf[col] == 45)
	  num = 11;
	else if (buf[col] == 46)
	  num = 12;

	if (num >= 0)
	  g15r_drawBigNum (canvas, (80-(totalwidth)/2)+col*20, 1,(80-(totalwidth)/2)+(col+1)*20, height, G15_COLOR_BLACK, num);
      }
    
    if(ampm[0]!=0)
      g15r_renderString (canvas,(unsigned char *)ampm,0,G15_TEXT_LARGE,totalwidth+15,height-6);

    return G15_PLUGIN_OK;
}

static int draw_analog(g15canvas *c)
{
  int xh, yh;
  int xm, ym;
  int xs, ys;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  int h;
  h = t->tm_hour;
  h %= 12;
  h *= 5;
  h += t->tm_min * 5 / 60;
  
  get_clock_pos(h,         &xh, &yh,  9);
  get_clock_pos(t->tm_min, &xm, &ym,  6);
  get_clock_pos(t->tm_sec, &xs, &ys,  3);
 
  // put background:
  memcpy(c, static_canvas, sizeof(g15canvas));
  
  // hour
  g15r_drawLine(c, CLOCK_CENTERX-2,  CLOCK_CENTERY, xh,  yh,   G15_COLOR_BLACK);
  g15r_drawLine(c, CLOCK_CENTERX-1,  CLOCK_CENTERY, xh,  yh,   G15_COLOR_BLACK);
  g15r_drawLine(c, CLOCK_CENTERX,    CLOCK_CENTERY, xh,  yh+1, G15_COLOR_BLACK);
  g15r_drawLine(c, CLOCK_CENTERX+1,  CLOCK_CENTERY, xh,  yh,   G15_COLOR_BLACK);
  g15r_drawLine(c, CLOCK_CENTERX+2,  CLOCK_CENTERY, xh,  yh,   G15_COLOR_BLACK);

  // minute
  g15r_drawLine(c, CLOCK_CENTERX-1,  CLOCK_CENTERY, xm,  ym,   G15_COLOR_BLACK);
  g15r_drawLine(c, CLOCK_CENTERX,    CLOCK_CENTERY, xm,  ym+1, G15_COLOR_BLACK);
  g15r_drawLine(c, CLOCK_CENTERX+1,  CLOCK_CENTERY, xm,  ym,   G15_COLOR_BLACK);

  // second:
  g15r_drawLine(c, CLOCK_CENTERX,    CLOCK_CENTERY,   xs,  ys,   G15_COLOR_BLACK);
  
  //
  // draw texts:
  //
  char day[32];		// Tuesday
  char mon[32];		// March
  char year[32];	// 1234 AD
  char time[32];	// 22:33:44
  char date[32];	// 21.April

  strftime(day, sizeof(day), "%A", t);
  strftime(mon, sizeof(mon), "%B", t);
  sprintf(date, "%d.%s", t->tm_mday, mon);
  sprintf(year, "%4d AD", t->tm_year+1900);
  if(mode)
    strftime(time,sizeof(time),"%H:%M:%S",t);
  else 
    strftime(time,sizeof(time),"%r",t);
  
  if(showdate) {
  	g15r_renderString(c, (unsigned char*)time,  0, G15_TEXT_LARGE, 60, 4);
  	g15r_renderString(c, (unsigned char*)day,   1, G15_TEXT_LARGE, 60, 5);
  	g15r_renderString(c, (unsigned char*)date,  2, G15_TEXT_LARGE, 60, 6);
  	g15r_renderString(c, (unsigned char*)year,  3, G15_TEXT_LARGE, 60, 7);
  } else 
	g15r_renderString(c, (unsigned char*)time, 0, G15_TEXT_LARGE, 60, 18);

  return G15_PLUGIN_OK;
}


static int lcdclock(lcd_t *lcd)
{
    int ret = 0;
    g15canvas *canvas = (g15canvas *) malloc (sizeof (g15canvas));

    if (canvas == NULL) {
        g15daemon_log(LOG_ERR, "Unable to allocate canvas");
        return G15_PLUGIN_QUIT;
    }

   	memset(canvas->buffer, 0, G15_BUFFER_LEN);
	canvas->mode_cache = 0;
	canvas->mode_reverse = 0;
	canvas->mode_xor = 0;

    memset(lcd->buf,0,G15_BUFFER_LEN);

    if(digital)
      ret = draw_digital(canvas);
    else
      ret = draw_analog(canvas);

    memcpy (lcd->buf, canvas->buffer, G15_BUFFER_LEN);
    g15daemon_send_refresh(lcd);
    free(canvas);
    return G15_PLUGIN_OK;
}

static int myeventhandler(plugin_event_t *myevent) {
    
    lcd_t *lcd = (lcd_t*) myevent->lcd;
    config_section_t *clockcfg =NULL;
    switch (myevent->event)
    {
        case G15_EVENT_KEYPRESS:
            clockcfg = g15daemon_cfg_load_section(lcd->masterlist,"Clock");
            if(myevent->value & G15_KEY_L2){
                mode = 1^mode;
                g15daemon_cfg_write_bool(clockcfg, "24hrFormat", mode);
            }
            if(myevent->value & G15_KEY_L3) {
                showdate = 1^showdate;
                g15daemon_cfg_write_bool(clockcfg, "ShowDate", showdate);   
            }
	    if(myevent->value & G15_KEY_L4) {
	    	digital = 1^digital;
		g15daemon_cfg_write_bool(clockcfg, "Digital", digital);
	    }
//        printf("Clock plugin received keypress event : %i\n",myevent->value);
          break;
        case G15_EVENT_VISIBILITY_CHANGED:
//        printf("Clock received new visibility status (%i)\n",myevent->value);
          break;
        default:
          break;
    }
    return G15_PLUGIN_OK;
}

/* completely uncessary function called when plugin is exiting */
static void callmewhenimdone(lcd_t *lcd){
    if(static_canvas != NULL) free(static_canvas);
    return;
}

/* completely unnecessary initialisation function which could just as easily have been set to NULL in the g15plugin_info struct */
static int myinithandler(lcd_t *lcd){
    config_section_t *clockcfg = g15daemon_cfg_load_section(lcd->masterlist,"Clock");
    mode=g15daemon_cfg_read_bool(clockcfg, "24hrFormat",1);
    showdate=g15daemon_cfg_read_bool(clockcfg, "ShowDate",0);
    digital=g15daemon_cfg_read_bool(clockcfg, "Digital",1);

    static_canvas = (g15canvas*)malloc(sizeof(g15canvas));
    if (static_canvas != NULL)
      {
      	memset(static_canvas->buffer, 0, G15_BUFFER_LEN);
	static_canvas->mode_cache = 0;
	static_canvas->mode_reverse = 0;
	static_canvas->mode_xor = 0;
        draw_static_canvas();
      }

    return static_canvas == NULL ? G15_PLUGIN_QUIT : G15_PLUGIN_OK;
}

/* if no exitfunc or eventhandler, member should be NULL */
plugin_info_t g15plugin_info[] = {
    /* TYPE, name, initfunc, updatefreq, exitfunc, eventhandler, initfunc */
    {G15_PLUGIN_LCD_CLIENT, "Clock", (void*)lcdclock, 500, (void*)callmewhenimdone, (void*)myeventhandler, (void*)myinithandler},
    {G15_PLUGIN_NONE,               ""          , NULL,     0,   NULL,            NULL,           NULL}
};

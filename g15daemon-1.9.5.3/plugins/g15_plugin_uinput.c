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
    
    $Revision: 388 $ -  $Date: 2008-01-02 12:57:03 +1030 (Wed, 02 Jan 2008) $ $Author: mlampard $
    
    UINPUT key processing plugin.  receives events and sends keycodes to the linux kernel.
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

#include <config.h>
#include <g15daemon.h>
#include <pwd.h>

#ifdef HAVE_CONFIG_H
#ifdef HAVE_LINUX_UINPUT_H
#include <linux/input.h>
#include <linux/uinput.h>

#include <libg15.h>

static int uinp_fd = -1;
static config_section_t *uinput_cfg=NULL;
static int map_Lkeys = 0;

#define GKEY_OFFSET 167
#define MKEY_OFFSET 185
#define LKEY_OFFSET 189

#define G15KEY_DOWN 1
#define G15KEY_UP 0

static int g15_init_uinput(void *plugin_args) {
    
    int i=0;
    char *custom_filename;
    g15daemon_t *masterlist = (g15daemon_t*) plugin_args;
    struct uinput_user_dev uinp;
    static const char *uinput_device_fn[] = { "/dev/uinput", "/dev/input/uinput","/dev/misc/uinput",0};
    
    uinput_cfg = g15daemon_cfg_load_section(masterlist,"Keyboard OS Mapping (uinput)");
    custom_filename = g15daemon_cfg_read_string(uinput_cfg, "device",(char*)uinput_device_fn[1]);
    map_Lkeys=g15daemon_cfg_read_int(uinput_cfg, "Lkeys.mapped",0);
    
    seteuid(0);
    setegid(0);
    while (uinput_device_fn[i] && (uinp_fd = open(uinput_device_fn[i],O_RDWR))<0){
        ++i;
    }
    if(uinp_fd<0) {	/* try reading the users preference in the config */
        uinp_fd = open(custom_filename,O_RDWR);
    }
    if(uinp_fd<0){
        g15daemon_log(LOG_ERR,"Unable to open UINPUT device.  Please ensure the uinput driver is loaded into the kernel and that you have permission to open the device.");
        return -1;
    }
    /* all other processes/threads should be seteuid nobody */
     seteuid(masterlist->nobody->pw_uid);
     setegid(masterlist->nobody->pw_gid);
    
    
    memset(&uinp,0,sizeof(uinp));
    strncpy(uinp.name, "G15 Extra Keys", UINPUT_MAX_NAME_SIZE);

#ifdef HAVE_UINPUT_USER_DEV_ID
    uinp.id.version = 4;
    uinp.id.bustype = BUS_USB;
#else
    uinp.idversion = 4;
    uinp.idbus = BUS_USB;
#endif 

    ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);

    for (i=0; i<256; ++i)
        ioctl(uinp_fd, UI_SET_KEYBIT, i);

    write(uinp_fd, &uinp, sizeof(uinp));
    
    if (ioctl(uinp_fd, UI_DEV_CREATE))
    {
        g15daemon_log(LOG_ERR,"Unable to create UINPUT device.");
        return -1;
    }
    return 0;
}

void g15_exit_uinput(void *plugin_args){
    ioctl(uinp_fd, UI_DEV_DESTROY);
    close(uinp_fd);
}


static void g15_uinput_keydown(unsigned char code)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = G15KEY_DOWN;
    
    write (uinp_fd, &event, sizeof(event));
}

static void g15_uinput_keyup(unsigned char code)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = G15KEY_UP;
    
    write (uinp_fd, &event, sizeof(event));
}

    void (*keyup)(unsigned char code) = &g15_uinput_keyup;
    void (*keydown)(unsigned char code) = &g15_uinput_keydown;
#else
    void keyup(unsigned char code) { printf("Extra Keys not supported due to missing Uinput.h\n"); }
    void keydown(unsigned char code) { printf("Extra Keys not supported due to missing Uinput.h\n"); }
#endif
#endif
    
static void g15_process_keys(g15daemon_t *masterlist, unsigned int currentkeys, unsigned int lastkeys)
{
    /* 'G' keys */
    if((currentkeys & G15_KEY_G1) && !(lastkeys & G15_KEY_G1))
        keydown(GKEY_OFFSET);
    else if(!(currentkeys & G15_KEY_G1) && (lastkeys & G15_KEY_G1))
        keyup(GKEY_OFFSET);

    if((currentkeys & G15_KEY_G2) && !(lastkeys & G15_KEY_G2))
        keydown(GKEY_OFFSET+1);
    else if(!(currentkeys & G15_KEY_G2) && (lastkeys & G15_KEY_G2))
        keyup(GKEY_OFFSET+1);

    if((currentkeys & G15_KEY_G3) && !(lastkeys & G15_KEY_G3))
        keydown(GKEY_OFFSET+2);
    else if(!(currentkeys & G15_KEY_G3) && (lastkeys & G15_KEY_G3))
        keyup(GKEY_OFFSET+2);

    if((currentkeys & G15_KEY_G4) && !(lastkeys & G15_KEY_G4))
        keydown(GKEY_OFFSET+3);
    else if(!(currentkeys & G15_KEY_G4) && (lastkeys & G15_KEY_G4))
        keyup(GKEY_OFFSET+3);

    if((currentkeys & G15_KEY_G5) && !(lastkeys & G15_KEY_G5))
        keydown(GKEY_OFFSET+4);
    else if(!(currentkeys & G15_KEY_G5) && (lastkeys & G15_KEY_G5))
        keyup(GKEY_OFFSET+4);

    if((currentkeys & G15_KEY_G6) && !(lastkeys & G15_KEY_G6))
        keydown(GKEY_OFFSET+5);
    else if(!(currentkeys & G15_KEY_G6) && (lastkeys & G15_KEY_G6))
        keyup(GKEY_OFFSET+5);

    if((currentkeys & G15_KEY_G7) && !(lastkeys & G15_KEY_G7))
        keydown(GKEY_OFFSET+6);
    else if(!(currentkeys & G15_KEY_G7) && (lastkeys & G15_KEY_G7))
        keyup(GKEY_OFFSET+6);

    if((currentkeys & G15_KEY_G8) && !(lastkeys & G15_KEY_G8))
        keydown(GKEY_OFFSET+7);
    else if(!(currentkeys & G15_KEY_G8) && (lastkeys & G15_KEY_G8))
        keyup(GKEY_OFFSET+7);

    if((currentkeys & G15_KEY_G9) && !(lastkeys & G15_KEY_G9))
        keydown(GKEY_OFFSET+8);
    else if(!(currentkeys & G15_KEY_G9) && (lastkeys & G15_KEY_G9))
        keyup(GKEY_OFFSET+8);

    if((currentkeys & G15_KEY_G10) && !(lastkeys & G15_KEY_G10))
        keydown(GKEY_OFFSET+9);
    else if(!(currentkeys & G15_KEY_G10) && (lastkeys & G15_KEY_G10))
        keyup(GKEY_OFFSET+9);

    if((currentkeys & G15_KEY_G11) && !(lastkeys & G15_KEY_G11))
        keydown(GKEY_OFFSET+10);
    else if(!(currentkeys & G15_KEY_G11) && (lastkeys & G15_KEY_G11))
        keyup(GKEY_OFFSET+10);

    if((currentkeys & G15_KEY_G12) && !(lastkeys & G15_KEY_G12))
        keydown(GKEY_OFFSET+11);
    else if(!(currentkeys & G15_KEY_G12) && (lastkeys & G15_KEY_G12))
        keyup(GKEY_OFFSET+11);

    if((currentkeys & G15_KEY_G13) && !(lastkeys & G15_KEY_G13))
        keydown(GKEY_OFFSET+12);
    else if(!(currentkeys & G15_KEY_G13) && (lastkeys & G15_KEY_G13))
        keyup(GKEY_OFFSET+12);

    if((currentkeys & G15_KEY_G14) && !(lastkeys & G15_KEY_G14))
        keydown(GKEY_OFFSET+13);
    else if(!(currentkeys & G15_KEY_G14) && (lastkeys & G15_KEY_G14))
        keyup(GKEY_OFFSET+13);

    if((currentkeys & G15_KEY_G15) && !(lastkeys & G15_KEY_G15))
        keydown(GKEY_OFFSET+14);
    else if(!(currentkeys & G15_KEY_G15) && (lastkeys & G15_KEY_G15))
        keyup(GKEY_OFFSET+14);

    if((currentkeys & G15_KEY_G16) && !(lastkeys & G15_KEY_G16))
        keydown(GKEY_OFFSET+15);
    else if(!(currentkeys & G15_KEY_G16) && (lastkeys & G15_KEY_G16))
        keyup(GKEY_OFFSET+15);

    if((currentkeys & G15_KEY_G17) && !(lastkeys & G15_KEY_G17))
        keydown(GKEY_OFFSET+16);
    else if(!(currentkeys & G15_KEY_G17) && (lastkeys & G15_KEY_G17))
        keyup(GKEY_OFFSET+16);

    if((currentkeys & G15_KEY_G18) && !(lastkeys & G15_KEY_G18))
        keydown(GKEY_OFFSET+17);
    else if(!(currentkeys & G15_KEY_G18) && (lastkeys & G15_KEY_G18))
        keyup(GKEY_OFFSET+17);

    /* 'M' keys */

    if((currentkeys & G15_KEY_M1) && !(lastkeys & G15_KEY_M1))
        keydown(MKEY_OFFSET);
    else if(!(currentkeys & G15_KEY_M1) && (lastkeys & G15_KEY_M1))
        keyup(MKEY_OFFSET);

    if((currentkeys & G15_KEY_M2) && !(lastkeys & G15_KEY_M2))
        keydown(MKEY_OFFSET+1);
    else if(!(currentkeys & G15_KEY_M2) && (lastkeys & G15_KEY_M2))
        keyup(MKEY_OFFSET+1);

    if((currentkeys & G15_KEY_M3) && !(lastkeys & G15_KEY_M3))
        keydown(MKEY_OFFSET+2);
    else if(!(currentkeys & G15_KEY_M3) && (lastkeys & G15_KEY_M3))
        keyup(MKEY_OFFSET+2);

    if((currentkeys & G15_KEY_MR) && !(lastkeys & G15_KEY_MR))
        keydown(MKEY_OFFSET+3);
    else if(!(currentkeys & G15_KEY_MR) && (lastkeys & G15_KEY_MR))
        keyup(MKEY_OFFSET+3);
    
    if(map_Lkeys){
        /* 'L' keys...  */
        if((currentkeys & G15_KEY_L1) && !(lastkeys & G15_KEY_L1))
            keydown(LKEY_OFFSET);
        else if(!(currentkeys & G15_KEY_L1) && (lastkeys & G15_KEY_L1))
            keyup(LKEY_OFFSET);

        if((currentkeys & G15_KEY_L2) && !(lastkeys & G15_KEY_L2))
            keydown(LKEY_OFFSET+1);
        else if(!(currentkeys & G15_KEY_L2) && (lastkeys & G15_KEY_L2))
            keyup(LKEY_OFFSET+1);

        if((currentkeys & G15_KEY_L3) && !(lastkeys & G15_KEY_L3))
            keydown(LKEY_OFFSET+2);
        else if(!(currentkeys & G15_KEY_L3) && (lastkeys & G15_KEY_L3))
            keyup(LKEY_OFFSET+2);

        if((currentkeys & G15_KEY_L4) && !(lastkeys & G15_KEY_L4))
            keydown(LKEY_OFFSET+3);
        else if(!(currentkeys & G15_KEY_L4) && (lastkeys & G15_KEY_L4))
            keyup(LKEY_OFFSET+3);

        if((currentkeys & G15_KEY_L5) && !(lastkeys & G15_KEY_L5))
            keydown(LKEY_OFFSET+4);
        else if(!(currentkeys & G15_KEY_L5) && (lastkeys & G15_KEY_L5))
            keyup(LKEY_OFFSET+4);
    }
}


static int keyevents(plugin_event_t *myevent) {
    lcd_t *lcd = (lcd_t*) myevent->lcd;
    static int lastkeys;
    switch (myevent->event)
    {
        case G15_EVENT_KEYPRESS:{
            g15_process_keys(lcd->masterlist, myevent->value,lastkeys);
            lastkeys = myevent->value;
            break;
        }
        case G15_EVENT_VISIBILITY_CHANGED:
        case G15_EVENT_USER_FOREGROUND:
	case G15_EVENT_MLED:
        case G15_EVENT_BACKLIGHT:
        case G15_EVENT_CONTRAST:
        case G15_EVENT_REQ_PRIORITY:
        case G15_EVENT_CYCLE_PRIORITY:
        default:
            break;
    }
    return G15_PLUGIN_OK;
}


    /* if no exitfunc or eventhandler, member should be NULL */
plugin_info_t g15plugin_info[] = {
        /* TYPE, name, initfunc, updatefreq, exitfunc, eventhandler, initfunc */
   {G15_PLUGIN_CORE_OS_KB, "Linux UINPUT Keyboard Output"	, NULL, 500, (void*)g15_exit_uinput, (void*)keyevents, (void*)g15_init_uinput},
   {G15_PLUGIN_NONE,               ""          			, NULL,   0,   			NULL,            NULL,           NULL}
};

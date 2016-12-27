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

    $Revision: 381 $ -  $Date: 2008-01-01 23:41:08 +1030 (Tue, 01 Jan 2008) $ $Author: mlampard $
        
    This daemon listens on localhost port 15550 for client connections,
    and arbitrates LCD display.  Allows for multiple simultaneous clients.
    Client screens can be cycled through by pressing the 'L1' key.
*/

/* quickndirty g15daemon client example. it just connects and sends a prefab image to the server
* and remains connected until the user presses enter.. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "g15daemon_client.h"
#include "g15logo.h"
#include <errno.h>
#include <poll.h>

#include <libg15.h>

/* #define TEST_KEYHANDLER */

int main(int argc, char *argv[])
{
    int g15screen_fd, retval;
    char lcdbuffer[6880];
    unsigned int keystate;
    
    if((g15screen_fd = new_g15_screen(G15_PIXELBUF))<0){
        printf("Sorry, cant connect to the G15daemon\n");
        return 5;
    }else
        printf("Connected to g15daemon.  sending image\n");

        if(argc<2)
            retval = g15_send(g15screen_fd,(char*)logo_data,6880);
        else {
            memset(lcdbuffer,0,6880);
            memset(lcdbuffer,1,6880/2);
            retval = g15_send(g15screen_fd,(char*)lcdbuffer,6880);
        }

        printf("checking key status - press G1 to exit\n");
        
        while(1){
            keystate = 0;
            int foo = 0;

//            keystate = g15_send_cmd (g15screen_fd, G15DAEMON_GET_KEYSTATE, foo);
//while(1){
recv(g15screen_fd, &keystate, 4, 0);
            if(keystate)
                printf("keystate = %i\n",keystate);
//}
//            if(keystate & G15_KEY_G1) //G1 key.  See libg15.h for details on key values.
//                break;

            /* G2,G3 & G4 change LCD backlight */
            if(keystate & G15_KEY_G2){
                retval = g15_send_cmd (g15screen_fd, G15DAEMON_BACKLIGHT, G15_BRIGHTNESS_DARK);
            }
            if(keystate & G15_KEY_G3){
                retval = g15_send_cmd (g15screen_fd, G15DAEMON_BACKLIGHT, G15_BRIGHTNESS_MEDIUM);
                unsigned char packet[2];
                packet[0] = G15DAEMON_BACKLIGHT|G15_BRIGHTNESS_MEDIUM;
                printf("sent %i bytes\n",send(g15screen_fd, packet, 1, MSG_OOB ));

            }
            if(keystate & G15_KEY_G4){
                retval = g15_send_cmd (g15screen_fd, G15DAEMON_BACKLIGHT, G15_BRIGHTNESS_BRIGHT);
            }

/*            
            retval = g15_send_cmd (g15screen_fd, G15DAEMON_IS_FOREGROUND, foo);

            if(retval)
              printf("Hey, we are in the foreground, Doc\n");
            else
              printf("What dastardly wabbit put me in the background?\n");

            retval = g15_send_cmd (g15screen_fd, G15DAEMON_IS_USER_SELECTED, foo);
            if(retval)
              printf("You wanted me in the foreground, right Doc?\n");
            else
              printf("You dastardly wabbit !\n");
            
//            if(retval){ 
                sleep(2); 
                retval = g15_send_cmd (g15screen_fd, G15DAEMON_SWITCH_PRIORITIES, foo);
                sleep(2); 
                retval = g15_send_cmd (g15screen_fd, G15DAEMON_SWITCH_PRIORITIES, foo);
//            }
*/
                                       
//            sleep(2);
#ifdef TEST_KEYHANDLER
            /* ok.. request that all G&M keys are passed to us.. */
            retval = g15_send_cmd (g15screen_fd, G15DAEMON_KEY_HANDLER, foo);
            
            while(1){
                printf("waiting on keystate\n");
                keystate=0;
                retval = recv(g15screen_fd, &keystate , sizeof(keystate),0);
                if(keystate)
                  printf("Recieved %i as keystate",keystate);
            }
#endif

        }
        g15_close_screen(g15screen_fd);
        return 0;
}

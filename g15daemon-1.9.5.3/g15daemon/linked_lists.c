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

    $Revision: 451 $ -  $Date: 2008-01-25 15:15:04 +1030 (Fri, 25 Jan 2008) $ $Author: mlampard $
        
    This daemon listens on localhost port 15550 for client connections,
    and arbitrates LCD display.  Allows for multiple simultaneous clients.
    Client screens can be cycled through by pressing the 'L1' key.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <g15daemon.h>
#include <libg15.h>

extern lcd_t *keyhandler;
extern unsigned int client_handles_keys;
extern plugin_info_t *generic_info;

lcd_t static * ll_create_lcd () {

    lcd_t *lcd = g15daemon_xmalloc (sizeof (lcd_t));
    lcd->max_x = LCD_WIDTH;
    lcd->max_y = LCD_HEIGHT;
    lcd->backlight_state = G15_BRIGHTNESS_MEDIUM;
    lcd->mkey_state = 0;
    lcd->contrast_state = G15_CONTRAST_MEDIUM;
    lcd->state_changed = 1;
    lcd->usr_foreground = 0;
    lcd->g15plugin = g15daemon_xmalloc(sizeof (plugin_s)); 
    lcd->g15plugin->plugin_handle = NULL;
    lcd->g15plugin->info = (void*)&generic_info;
    
    return (lcd);
}

static void ll_quit_lcd (lcd_t * lcd) {
    free (lcd->g15plugin);
    free (lcd);
}


/* initialise a new masterlist, and add an initial node at the tail (used for the clock) */
g15daemon_t *ll_lcdlist_init () {
    
    g15daemon_t *masterlist = NULL;
    
    pthread_mutex_init(&lcdlist_mutex, NULL);
    pthread_mutex_lock(&lcdlist_mutex);
    
    masterlist = g15daemon_xmalloc(sizeof(g15daemon_t));
    
    masterlist->head = g15daemon_xmalloc(sizeof(lcdnode_t));
    
    masterlist->tail = masterlist->head;
    masterlist->current = masterlist->head;
    
    masterlist->head->lcd = ll_create_lcd();
    masterlist->head->lcd->mkey_state = 0;
    masterlist->head->lcd->masterlist = masterlist;
    /* first screen is the clock/menu */
    masterlist->head->lcd->g15plugin->info = NULL;
    
    masterlist->head->prev = masterlist->head;
    masterlist->head->next = masterlist->head;
    masterlist->head->list = masterlist;
    masterlist->keyboard_handler = NULL;
    masterlist->numclients = 0;
    
    pthread_mutex_unlock(&lcdlist_mutex);
    return masterlist;
}

lcdnode_t *g15daemon_lcdnode_add(g15daemon_t **masterlist) {
    
    lcdnode_t *new = NULL;
    
    pthread_mutex_lock(&lcdlist_mutex);
    new = g15daemon_xmalloc(sizeof(lcdnode_t));
    new->prev = (*masterlist)->head;
    new->next = (*masterlist)->tail; 
    new->lcd = ll_create_lcd();
    new->lcd->masterlist = (*masterlist);
    new->last_priority = NULL;
    new->list = *masterlist;
    
    (*masterlist)->head->next=new;
    (*masterlist)->current = new;
    
    (*masterlist)->head = new;
    (*masterlist)->head->list = *masterlist;
    (*masterlist)->numclients++;
    
    pthread_mutex_unlock(&lcdlist_mutex);
    
    return new;
}

/* cycle through connected client displays */
void g15daemon_lcdnode_cycle(g15daemon_t *masterlist)
{
    lcdnode_t *current_screen = NULL;
    pthread_mutex_lock(&lcdlist_mutex);
skip:
    current_screen = masterlist->current;
    
    g15daemon_send_event(current_screen->lcd, G15_EVENT_VISIBILITY_CHANGED, SCR_HIDDEN);
    do
    {
        masterlist->current->lcd->usr_foreground=0;        

        if(masterlist->tail == masterlist->current){
            masterlist->current = masterlist->head;
        }else{
            masterlist->current = masterlist->current->prev;
        }
    } 
    while (current_screen != masterlist->current);

    if(masterlist->tail == masterlist->current) {
        masterlist->current = masterlist->head;
    } else {
        masterlist->current = masterlist->current->prev;
    }

    if(masterlist->current->lcd->never_select==1) {
       goto skip;
    }

    masterlist->current->last_priority =  masterlist->current;
    pthread_mutex_unlock(&lcdlist_mutex);

    masterlist->current->lcd->usr_foreground=1;
    g15daemon_send_event(masterlist->current->lcd, G15_EVENT_VISIBILITY_CHANGED, SCR_VISIBLE);

}

void g15daemon_lcdnode_remove (lcdnode_t *oldnode) {
    
    g15daemon_t **masterlist = NULL;
    lcdnode_t **prev = NULL;
    lcdnode_t **next = NULL;
    
    pthread_mutex_lock(&lcdlist_mutex);
    
    masterlist = &oldnode->list;
    prev = &oldnode->prev;
    next = &oldnode->next;
    
    ll_quit_lcd(oldnode->lcd);
    (*masterlist)->numclients--;
    if((*masterlist)->current == oldnode) {
        if((*masterlist)->current!=(*masterlist)->head){
            (*masterlist)->current = oldnode->next;
        } else {
            (*masterlist)->current = oldnode->prev;
        }
        (*masterlist)->current->lcd->state_changed = 1;
    }
    
    if(&oldnode->lcd == (void*)keyhandler) {
        client_handles_keys = 0;
        keyhandler = NULL;
        g15daemon_log(LOG_WARNING,"Client key handler quit, going back to defaults");
    }
    
    if((*masterlist)->head!=oldnode){
        (*next)->prev = oldnode->prev;
        (*prev)->next = oldnode->next;
    }else{
        (*prev)->next = (*masterlist)->tail;
        (*masterlist)->head = oldnode->prev;
    }
    g15daemon_send_refresh((*masterlist)->current->lcd);

    free(oldnode);
    
    pthread_mutex_unlock(&lcdlist_mutex);
}

void ll_lcdlist_destroy(g15daemon_t **masterlist) {
    int i = 0;
    
    while ((*masterlist)->head != (*masterlist)->tail) {
        i++;
        g15daemon_lcdnode_remove((*masterlist)->head);
    }
    
    free((*masterlist)->tail->lcd);
    free((*masterlist)->tail);
    free(*masterlist);
    
    pthread_mutex_destroy(&lcdlist_mutex);
}


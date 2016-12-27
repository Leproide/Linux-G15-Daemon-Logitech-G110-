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

    $Revision: 449 $ -  $Date: 2008-01-24 17:42:07 +1030 (Thu, 24 Jan 2008) $ $Author: mlampard $
        
    This daemon listens on localhost port 15550 for client connections,
    and arbitrates LCD display.  Allows for multiple simultaneous clients.
    Client screens can be cycled through by pressing the 'L1' key.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include <sys/time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "g15daemon.h"
#include <libg15.h>
#include <stdarg.h>
#include <libg15render.h>

extern unsigned int g15daemon_debug;
extern volatile int leaving;
#define G15DAEMON_PIDFILE "/var/run/g15daemon.pid"

pthread_cond_t lcd_refresh = PTHREAD_COND_INITIALIZER;


/* if no exitfunc or eventhandler, member should be NULL */
const plugin_info_t generic_info[] = {
    /* TYPE, name, initfunc, updatefreq, exitfunc, eventhandler */
    {G15_PLUGIN_LCD_CLIENT, "BackwardCompatible", NULL, 0, NULL, (void*)internal_generic_eventhandler},
    {G15_PLUGIN_NONE,        ""          , NULL,     0,   NULL, NULL}
};

/* handy function from xine_utils.c */
void *g15daemon_xmalloc(size_t size) {
    void *ptr;

    /* prevent xmalloc(0) of possibly returning NULL */
    if( !size )
        size++;

    if((ptr = calloc(1, size)) == NULL) {
        g15daemon_log(LOG_WARNING, "g15_xmalloc() failed: %s.\n", strerror(errno));
        return NULL;
    }
    return ptr;
}

static int refresh_pending=0;
void g15daemon_init_refresh() {
  pthread_condattr_t attr;
  pthread_cond_init(&lcd_refresh, &attr);
}

void g15daemon_send_refresh(lcd_t *lcd) {
    if(lcd==lcd->masterlist->current->lcd||lcd->state_changed) {
      pthread_cond_broadcast(&lcd_refresh);
      refresh_pending++;
    }
}

void g15daemon_wait_refresh() {
    pthread_mutex_t dummy_mutex;
    struct timespec timeout;
    int retval;
    if(refresh_pending<1) {
      /* Create a dummy mutex which doesn't unlock for sure while waiting. */
      pthread_mutex_init(&dummy_mutex, NULL);
      pthread_mutex_lock(&dummy_mutex);
start:
      time(&timeout.tv_sec);
      timeout.tv_sec += 1;
      timeout.tv_nsec = 0L;

      retval=pthread_cond_timedwait(&lcd_refresh, &dummy_mutex, &timeout);
      if(!leaving && retval == ETIMEDOUT)
        goto start;
      pthread_mutex_unlock(&dummy_mutex);
      pthread_mutex_destroy(&dummy_mutex);
    }
    refresh_pending--;
}

void g15daemon_quit_refresh() {
    pthread_cond_destroy(&lcd_refresh);
}

int uf_return_running(){
    int fd;
    char pidtxt[128];
    int pid;
    int l;
    
    if ((fd = open(G15DAEMON_PIDFILE, O_RDWR, 0644)) < 0) {
            return -1;
    }
    if((l = read(fd,pidtxt,sizeof(pidtxt)-1)) < 0) {
        unlink (G15DAEMON_PIDFILE);
        close(fd);
        return -1;
    }
    if((pid = atoi(pidtxt)) <= 0) {
        g15daemon_log(LOG_ERR,"pidfile corrupt");
        unlink(G15DAEMON_PIDFILE);
        close(fd);
        return -1;
    }
    if((kill(pid,0) != 0) && errno != EPERM ) {
        g15daemon_log(LOG_ERR,"Process died - removing pidfile");
        unlink(G15DAEMON_PIDFILE);
        close(fd);
        return -1;
    }
    
    return pid;
    
}

int uf_create_pidfile() {
    
    char pidtxt[128];
    size_t l;
    int fd;
    
    if(!uf_return_running() &&  (fd = open(G15DAEMON_PIDFILE, O_CREAT|O_RDWR|O_EXCL, 0644)) < 0) {
        g15daemon_log(LOG_ERR,"previous G15Daemon process died.  removing pidfile");
        unlink(G15DAEMON_PIDFILE);
    }
    if ((fd = open(G15DAEMON_PIDFILE, O_CREAT|O_RDWR|O_EXCL, 0644)) < 0) {
        return 1;
    }
    
    snprintf(pidtxt, sizeof(pidtxt), "%lu\n", (unsigned long) getpid());

    if (write(fd, pidtxt, l = strlen(pidtxt)) != l) {
        g15daemon_log(LOG_WARNING, "write(): %s", strerror(errno));
        unlink(G15DAEMON_PIDFILE);
    }
    
    if(fd>0) {
        close(fd);
        return 0;
    }
    return 1;
}


/* syslog wrapper */
int g15daemon_log (int priority, const char *fmt, ...) {
   va_list argp;
   va_start (argp, fmt);
   int real_prio=0;
   
   switch(g15daemon_debug) {
     case 0:
     case 1:
      real_prio = LOG_WARNING;
      break;
     case 2:
      real_prio = LOG_INFO;
      break;
     default:
      real_prio = LOG_DEBUG;
   }

   if(real_prio>=priority) {
    if(g15daemon_debug == 0) {
     vsyslog(priority, fmt, argp);
    } else {
     vfprintf(stderr,fmt,argp);
     fprintf(stderr,"\n");
    }
   }
   va_end (argp);
   
   return 0;
}

/* takes a 6880 byte (1byte==1pixel) buffer and packs it into libg15 format. */
void g15daemon_convert_buf(lcd_t *lcd, unsigned char * orig_buf)
{
    unsigned int x,y,val;
    for(x=0;x<160;x++)
        for(y=0;y<43;y++)
	  {
		unsigned int pixel_offset = y * LCD_WIDTH + x;
    		unsigned int byte_offset = pixel_offset / 8;
    		unsigned int bit_offset = 7-(pixel_offset % 8);

		val = orig_buf[x+(y*160)];

    		if (val)
        		lcd->buf[byte_offset] = lcd->buf[byte_offset] | 1 << bit_offset;
    		else
        		lcd->buf[byte_offset] = lcd->buf[byte_offset]  &  ~(1 << bit_offset);
	  }
}

/* wrap the libg15 functions */
int uf_write_buf_to_g15(lcd_t *lcd)
{
    int retval = 0;
#ifdef LIBUSB_BLOCKS
    retval = writePixmapToLCD(lcd->buf);
#else
    pthread_mutex_lock(&g15lib_mutex);
    retval = writePixmapToLCD(lcd->buf);
    pthread_mutex_unlock(&g15lib_mutex);
#endif    
    return retval;
}

int uf_read_keypresses(unsigned int *keypresses, unsigned int timeout) 
{
    int retval=0;
#ifdef LIBUSB_BLOCKS
    retval = getPressedKeys(keypresses, timeout);
#else
    pthread_mutex_lock(&g15lib_mutex);
    retval = getPressedKeys(keypresses, timeout);
    pthread_mutex_unlock(&g15lib_mutex);
#endif    
    return retval;
}

/* Sleep routine (hackish). */
void g15daemon_sleep(int seconds) {
    pthread_mutex_t dummy_mutex;
    static pthread_cond_t dummy_cond = PTHREAD_COND_INITIALIZER;
    struct timespec timeout;

    /* Create a dummy mutex which doesn't unlock for sure while waiting. */
    pthread_mutex_init(&dummy_mutex, NULL);
    pthread_mutex_lock(&dummy_mutex);

    timeout.tv_sec = time(NULL) + seconds;
    timeout.tv_nsec = 0;

    pthread_cond_timedwait(&dummy_cond, &dummy_mutex, &timeout);

    /*    pthread_cond_destroy(&dummy_cond); */
    pthread_mutex_unlock(&dummy_mutex);
    pthread_mutex_destroy(&dummy_mutex);
}

/* microsecond sleep routine. */
int g15daemon_usleep(int useconds) {
    
    struct timeval timeout;
    if(useconds < 10000)
      useconds = 10000;
    timeout.tv_sec = useconds / 1000000;
    timeout.tv_usec = useconds % 1000000;
    return select (0,0,0,0,&timeout);
}

/* millisecond sleep routine. */
int g15daemon_msleep(int milliseconds) {
    
    return g15daemon_usleep(milliseconds*1000);
}


unsigned int g15daemon_gettime_ms(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (tv.tv_sec*1000+tv.tv_usec/1000);
}

/* generic event handler used unless overridden (only loading a plugin will override currently)*/
int internal_generic_eventhandler(plugin_event_t *event) {
    
    lcd_t *lcd = (lcd_t*) event->lcd;
    
    switch (event->event)
    {
        case G15_EVENT_KEYPRESS: {
            if(lcd->g15plugin->plugin_handle){ /* loadable plugin */
                 /* plugin had null for eventhandler therefore doesnt want events.. throw them away */
            }
        }
        break;
        case G15_EVENT_VISIBILITY_CHANGED:
          break;
        default:
          break;
    }
    return G15_PLUGIN_OK;
}


/* free all memory used by the config subsystem */
void uf_conf_free(g15daemon_t *list)
{
    config_section_t *section=list->config->sections;
    config_items_t *tmpitem=NULL;
    config_section_t *tmpsection=section;
    
    while(tmpsection!=NULL){
        tmpitem=section->items;
        
        if(section->sectionname){
            free(section->sectionname);
            while(section->items!=NULL){
                if(tmpitem->key!=NULL) 
                    free(tmpitem->key);
                if(tmpitem->value!=NULL)
                    free(tmpitem->value);
                tmpitem=section->items->next;
                free(section->items);
                section->items=tmpitem;
            }
        }
        tmpsection=section->next;
        free(section);
        section=tmpsection;
    }
    free(section);
    free(list->config);
}

/* write the config file with all keys/sections */
int uf_conf_write(g15daemon_t *list,char *filename)
{
    int config_fd=-1;
    config_section_t *foo=list->config->sections;
    config_items_t * item=NULL;
    char line[1024];
    
    config_fd = open(filename,O_CREAT|O_RDWR|O_TRUNC);
    if(config_fd){
    snprintf(line,1024,"# G15Daemon Configuration File\n# any items entered before a [section] header\n# will be in the Global config space\n# comments you wish to keep should start with a semicolon';'\n");
    write(config_fd,line,strlen(line));
    while(foo!=NULL){
        item=foo->items;
        memset(line,0,1024);
        if(foo->sectionname!=NULL){
            snprintf(line,1024,"\n[%s]\n",foo->sectionname);
            write(config_fd,line,strlen(line));
            while(item!=NULL){
                memset(line,0,1024);
                if(item->key!=NULL){
                    if(item->key[0]==';') // comment
                        snprintf(line,1024,"%s\n",item->key);
                    else
                        snprintf(line,1024,"%s: %s\n",item->key, item->value);
                    write(config_fd,line,strlen(line));
                }
                item=item->next;
            }
        }
        foo=foo->next;
    }
    close(config_fd);
    return 0;
    }
    return -1;
}

/* search the list for valid section name return pointer to section, or NULL otherwise */
config_section_t* uf_search_confsection(g15daemon_t *list,char *sectionname){
    config_section_t *section=list->config->sections;
    
    while(section!=NULL){
        if(0==strcmp((char*)section->sectionname,(char*)sectionname))
            break;
        section=section->next;
    }
    return section;
}

/* search the list for valid key called "key" in section named "section" return pointer to item or NULL */
config_items_t* uf_search_confitem(config_section_t *section, char *key){
    
    config_items_t * item=NULL;
    
    if(section!=NULL){
        item=section->items;
        while(item!=NULL){
            if(0==strcmp((char*)item->key,(char*)key))
                break;
            item=item->next;
        }
    }
    return item;
}


/* return pointer to section, or create a new section if it doesnt exist */
config_section_t *g15daemon_cfg_load_section(g15daemon_t *masterlist,char *name) {
    
    config_section_t *new = NULL;
    if((new=uf_search_confsection(masterlist,name))!=NULL)
        return new;
    new = g15daemon_xmalloc(sizeof(config_section_t));
    new->head = new;
    new->next = NULL;;
    new->sectionname=strdup(name);
    if(!masterlist->config->sections){
        masterlist->config->sections=new;
        masterlist->config->sections->head = new;
    } else {
        masterlist->config->sections->head->next=new;
        masterlist->config->sections->head = new;
    }
    return new;
}

/* cleanup whitespace */
char * uf_remove_whitespace(char *str){
    int z=0;
    if(str==NULL)
        str=strdup(" ");
    while(isspace(str[z])&&str[z])
        z++;
    str+=z;
    return str;
}

/* add a new key, or update the value of an already existing key, or return -1 if section doesnt exist */
int g15daemon_cfg_write_string(config_section_t *section, char *key, char *val){
    
    config_items_t *new = NULL;

    if(section==NULL)
        return -1;
    
    if((uf_search_confitem(section, key))){
        free(new);
        new=uf_search_confitem(section, key);
        new->value=strdup(val);
    }else{
        new=g15daemon_xmalloc(sizeof(config_items_t));
        new->head=new;
        new->next=NULL;
        
        new->key=strdup(key);
        new->value=strdup(val);
        if(!section->items){
            new->prev=NULL;
            section->items=new;
            section->items->head=new;
        }else{
            new->prev=section->items->head;
            section->items->head->next=new;
            section->items->head=new;
        }
    }
    return 0;
}

/* remoe a key/value pair from named section */
int g15daemon_cfg_remove_key(config_section_t *section, char *key){
    
    config_items_t *new = NULL;
    config_items_t *next = NULL;
    config_items_t *prev = NULL;
    if(section==NULL)
        return -1;
    
    if((uf_search_confitem(section, key))){
        new=uf_search_confitem(section, key);
        prev=new->prev;
        next=new->next;
        if(prev){
            prev->next=new->next;
        }else{
             if(!next) {
                    free(section->items);
                    section->items=NULL;
                }
                else
                    section->items=next;
            }

        if(new->head==new){
            if(prev){
                prev->head = prev;
                section->items->head=prev;
            }
            if(!prev){
                if(!next) {
                    free(section->items);
                    section->items=NULL;
                }
            }
        }
        if(next){
            if(prev)
            	next->prev=prev;
            else
                next->prev=NULL;
        }
        free(new->key);
        free(new->value);
        free(new);
    }
    return 0;
}

int g15daemon_cfg_write_int(config_section_t *section, char *key, int val) {
    char tmp[1024];
    snprintf(tmp,1024,"%i",val);
    return g15daemon_cfg_write_string(section, key, tmp);
}

int g15daemon_cfg_write_float(config_section_t *section, char *key, double val) {
    char tmp[1024];
    snprintf(tmp,1024,"%f",val);
    return g15daemon_cfg_write_string(section, key, tmp);
}

/* simply write value as On or Off depending on whether val>0 */
int g15daemon_cfg_write_bool(config_section_t *section, char *key, unsigned int val) {
    char tmp[1024];
    snprintf(tmp,1024,"%s",val?"On":"Off");
    return g15daemon_cfg_write_string(section, key, tmp);
}

/* the config read functions will either return a value from the config file, or the default value, which will be written to the config file if the key doesnt exist */

/* return bool as int from key in sectionname */
int g15daemon_cfg_read_bool(config_section_t *section, char *key, int defaultval) {
    
    int retval=0;
    config_items_t *item = uf_search_confitem(section, key);
    if(item){
        retval = 1^retval?(strncmp(item->value,"On",2)==0):1;
           return retval;
    }
    g15daemon_cfg_write_bool(section, key, defaultval);
    return defaultval;
}

/* return int from key in sectionname */
int g15daemon_cfg_read_int(config_section_t *section, char *key, int defaultval) {
    
    config_items_t *item = uf_search_confitem(section, key);
    if(item){
           return atoi(item->value);
    }
    g15daemon_cfg_write_int(section, key, defaultval);
    return defaultval;
}

/* return float from key in sectionname */
double g15daemon_cfg_read_float(config_section_t *section, char *key, double defaultval) {
    
    config_items_t *item = uf_search_confitem(section, key);
    if(item){
           return atof(item->value);
    }
    g15daemon_cfg_write_float(section, key, defaultval);
    return defaultval;
}

/* return string value from key in sectionname */
char* g15daemon_cfg_read_string(config_section_t *section, char *key, char *defaultval) {
    
    config_items_t *item = uf_search_confitem(section, key);
    if(item){
           return item->value;
    }
    g15daemon_cfg_write_string(section, key, defaultval);
    return defaultval;
}


int uf_conf_open(g15daemon_t *list, char *filename) {

    char *buffer, *lines;
    int config_fd=-1;
    char *sect;
    char *start;
    char *bar = NULL;
    int i;
    struct stat stats;
	
    list->config=g15daemon_xmalloc(sizeof(configfile_t));
    list->config->sections=NULL;

    if (lstat(filename, &stats) == -1)
        return -1;
    if (!(config_fd = open(filename, O_RDWR)))
        return -1;

    buffer = g15daemon_xmalloc(stats.st_size + 1);
        
    if (read(config_fd, buffer, stats.st_size) != stats.st_size)
    {
        free(buffer);
        close(config_fd);
        return -1;
    }
    close(config_fd);
    buffer[stats.st_size] = '\0';
        
    lines=strtok_r(buffer,"\n",&bar);
    config_section_t *section=NULL;
    while(lines!=NULL){
        sect=strdup(lines);
        
        i=0;
        while(isspace(sect[i])){
            i++;
        }
        start=sect+i;
        if(start[0]=='#'){
	   /* header - ignore */
           /* comments start with ; and can be produced like so:
             g15daemon_cfg_write_string(noload_cfg,"; Plugins in this section will not be loaded on start","");
             the value parameter must not be used.
           */
        } else if(strrchr(start,']')) { /* section title */
            char sectiontitle[1024];
            memset(sectiontitle,0,1024);
            strncpy(sectiontitle,start+1,strlen(start)-2);
            section = g15daemon_cfg_load_section(list,sectiontitle);
        }else{
            /*section keys */
            if(section==NULL){
                /* create an internal section "Global" which is the default for items not under a [section] header */
                section=g15daemon_cfg_load_section(list,"Global");
            }
            char *foo=NULL;
            char *key = uf_remove_whitespace( strtok_r(start,":=",&foo) );
            char *val = uf_remove_whitespace( strtok_r(NULL,":=", &foo) );
            
            g15daemon_cfg_write_string(section,key,val);
        }        
        free(sect);
        lines=strtok_r(NULL,"\n",&bar);
    }

    free(buffer);
    return 0;
}

int uf_screendump_pbm(unsigned char *buffer,char *filename) {
    FILE *f;
    int x,y;
    #define WIDTH 40
    g15canvas *canvas=g15daemon_xmalloc(sizeof(g15canvas));
    memcpy(canvas->buffer,buffer,LCD_BUFSIZE);
    f = fopen(filename,"w+");
    fprintf(f,"P1\n160 43\n");
    fprintf(f,"# G15 screendump - %s\n\n",filename);
    for(y=0;y<43;y++)
      for(x=0;x<160;x++) {
        fprintf(f,"%i",g15r_getPixel(canvas,x,y));
            if(x%WIDTH==WIDTH-1)
              fprintf(f,"\n");
        }

        fclose(f);
    free(canvas);
    return 0;
}


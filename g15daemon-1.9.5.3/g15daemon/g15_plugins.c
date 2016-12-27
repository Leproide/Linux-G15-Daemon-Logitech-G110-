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

    $Revision: 408 $ -  $Date: 2008-01-09 12:07:02 +1030 (Wed, 09 Jan 2008) $ $Author: mlampard $
    
    This daemon listens on localhost port 15550 for client connections,
    and arbitrates LCD display.  Allows for multiple simultaneous clients.
    Client screens can be cycled through by pressing the 'L1' key.
    
    g15_plugin.c
    simple plugin loader - loads each plugin and runs each one in it's own thread. a bit expensive probably 
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <config.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <libg15.h>
#include <g15daemon.h>
#include <dlfcn.h>
#include <pwd.h>

#include <libg15render.h>

extern volatile int leaving;

void * g15daemon_dlopen_plugin(char *name,unsigned int library) {

    void * handle;
    char * error;
    static int deepbind = 0;
        
    int mode = library==1?RTLD_GLOBAL:RTLD_LOCAL;
#ifdef RTLD_DEEPBIND
    mode|=RTLD_DEEPBIND; /* set ordering of symbols so plugin uses its
                            own symbols in preference to ours */
    if(!deepbind)
      g15daemon_log(LOG_INFO,"G15Daemon Plugin_Loader - DEEPBIND Flag available.  Using it.\n");
    deepbind=1;
#endif

    g15daemon_log(LOG_INFO,"PRELOADING %s",name);
    /* remove any pending errors */
    dlerror();

    handle = dlopen (name,RTLD_LAZY | mode | RTLD_NODELETE);
    dlclose(handle);
    dlerror();
    
    handle = dlopen (name,RTLD_NOW | mode | RTLD_NOLOAD);
    
    if(!handle) { /* FIXME: retry once more if load failed */
      dlerror();
      g15daemon_log(LOG_ERR, "Initialisation Failed.  Retrying..");
      handle = dlopen (name, RTLD_LAZY | mode );
    }

    if ((error = dlerror()) != NULL)  {
        g15daemon_log (LOG_ERR, "Plugin_Loader - Error loading %s - %s\n", name, error);
        return(NULL);
    }
    
    return handle;
}

int g15daemon_dlclose_plugin(void *handle) {

    char * error;
    dlclose(handle);
    error = dlerror();
    if(error != NULL)
      g15daemon_log(LOG_ERR, "Error from dlclose %s\n",error);

    return 0;
}

void run_lcd_client(plugin_t *plugin_args) {
    plugin_info_t *info = plugin_args->info;
    int plugin_retval 	= G15_PLUGIN_OK;
    
    int (*plugin_init)(void *client_args) = (void*)info->plugin_init;
    int (*plugin)(void *client_args) = (void*)info->plugin_run;
    void (*plugin_close)(void *client_args) = (void*)info->plugin_exit;
    
    lcdnode_t *display = (lcdnode_t*)plugin_args->args;
    lcd_t *client_lcd = (lcd_t*)display->lcd;
    
    if(plugin_init!=NULL) {
        plugin_retval=(*plugin_init)((void*)client_lcd);
    }

    /* run the plugin thread every 'update_msecs' milliseconds */
    while(!leaving && (plugin_retval!=G15_PLUGIN_QUIT)){
        plugin_retval = (*plugin)((void*)client_lcd);
        if(info->update_msecs<50)
            info->update_msecs = 50;
        g15daemon_msleep(info->update_msecs);
    }
    
    if(plugin_close!=NULL){
        (*plugin_close)((void*)client_lcd);
    }

    if(!leaving)
        g15daemon_lcdnode_remove(display);
}

void run_advanced_client(plugin_t *plugin_args)
{ 
    plugin_info_t *info = plugin_args->info;
    
    int (*plugin_init)(void *client_args) = (void*)plugin_args->info->plugin_init;
    int (*plugin_run)(void *client_args) = (void*)plugin_args->info->plugin_run;
    int (*plugin_close)(void *client_args) = (void*)plugin_args->info->plugin_exit;

    /*initialise */
    if(plugin_init){
        if((*plugin_init)(plugin_args->args)!=G15_PLUGIN_OK)
            return;
    }

    if(plugin_args->info->event_handler && plugin_args->type==G15_PLUGIN_CORE_OS_KB){
        g15daemon_t *masterlist = (g15daemon_t*)plugin_args->args;
        pthread_mutex_lock(&lcdlist_mutex);
        (masterlist->keyboard_handler) = (void*)plugin_args->info->event_handler;
        pthread_mutex_unlock(&lcdlist_mutex);
    }

    if(plugin_run) {
        while(((*plugin_run)(plugin_args->args))==G15_PLUGIN_OK && !leaving){
            if(info->update_msecs<50)
                info->update_msecs = 50;
            g15daemon_msleep(info->update_msecs);
        }
    }else{
        while(!leaving){
            g15daemon_msleep(500);
        }
    }
    if(plugin_close) {
        (*plugin_close)(plugin_args->args);
    }
}

void *plugin_thread(plugin_t *plugin_args) {
    plugin_info_t *info = plugin_args->info;
    /* int (*event)(plugin_event_t *event) = (void*)plugin_args->info->event_handler; */
    void *handle = plugin_args->plugin_handle;
    
    if(plugin_args->info->plugin_run!=NULL||plugin_args->info->event_handler!=NULL) {
        g15daemon_log(LOG_ERR,"Plugin \"%s\" boot successful.",info->name);
    } else {
        return NULL;
    }
    
    if(plugin_args->type == G15_PLUGIN_LCD_CLIENT){
        g15daemon_log(LOG_INFO,"Starting plugin thread \"%s\" in standard mode\n",info->name);
    	run_lcd_client(plugin_args);
    }
    else if(plugin_args->type == G15_PLUGIN_CORE_OS_KB||plugin_args->type == G15_PLUGIN_LCD_SERVER) {
        g15daemon_log(LOG_INFO,"Starting plugin thread \"%s\" in advanced mode\n",info->name);
        run_advanced_client(plugin_args);
    }

    if(plugin_args)
        free(plugin_args);

    g15daemon_log(LOG_INFO,"Removed plugin %s",info->name);
    g15daemon_dlclose_plugin(handle);

    return NULL;
}

int g15_count_plugins(char *plugin_directory) {
    
    DIR *directory;
    struct dirent *ep;
    char pluginname[2048];
    int count=0;
    
    directory = opendir (plugin_directory);
    if (directory != NULL)
    {
       while ((ep = readdir (directory))) {
            if(strstr(ep->d_name,".so")){
                strcpy(pluginname, plugin_directory);
                strncat(pluginname,"/",1);
                strncat(pluginname,ep->d_name,200);
                ++count;
            }
        }
        (void) closedir (directory);
    }
    else
      count = -1;
  
    return count;  
}

int g15_plugin_load (g15daemon_t *masterlist, char *filename) {

    void * plugin_handle = NULL;
    config_section_t *plugin_cfg = g15daemon_cfg_load_section(masterlist,"PLUGINS");
    
    pthread_t client_connection;
    pthread_attr_t attr;
    lcdnode_t *clientnode;
    char *error_str;
    
    if((plugin_handle = g15daemon_dlopen_plugin(filename,G15_PLUGIN_NONSHARED))!=NULL) {
        plugin_t  *plugin_args=malloc(sizeof(plugin_t));
        plugin_args->info = dlsym(plugin_handle, "g15plugin_info");

        error_str=dlerror();
          
        if(error_str!=NULL)
          g15daemon_log(LOG_ERR,"g15_plugin_load: %s %s\n",filename,error_str);

        if(!plugin_args->info) { /* if it doesnt have a valid struct, we should just load it as a library... but we dont at the moment FIXME */
            g15daemon_log(LOG_ERR,"%s is not a valid g15daemon plugin.  Unloading\n",filename);
            g15daemon_dlclose_plugin(plugin_handle);
            dlerror();
            return -1;
        }
        
        if(strncasecmp("Load",g15daemon_cfg_read_string(plugin_cfg, plugin_args->info->name,"Load"),5)!=0)
        {
        
            g15daemon_log(LOG_ERR, "\"%s\" Plugin disabled in g15daemon.conf - not running\n",plugin_args->info->name);
            g15daemon_dlclose_plugin(plugin_handle);
            return -1;
        } 	

        g15daemon_log(LOG_WARNING, "Booting plugin \"%s\"",plugin_args->info->name);

        plugin_args->type = plugin_args->info->type;
        /* assign the generic eventhandler if the plugin doesnt provide one - the generic one does nothing atm. FIXME*/
        if(plugin_args->info->event_handler==NULL)
            plugin_args->info->event_handler = (void*)internal_generic_eventhandler;
        
    
        if(plugin_args->type == G15_PLUGIN_LCD_CLIENT) {
            //g15daemon_t *foolist = (g15daemon_t*)*masterlist;
            /* FIXME we should just sort out the linked list stuff instead of overriding it */
            if((int)masterlist->numclients>0){
                clientnode = g15daemon_lcdnode_add(&masterlist);
            }else {
                clientnode = masterlist->tail;
                masterlist->numclients++;
            }
            plugin_args->plugin_handle = plugin_handle;
            memcpy(clientnode->lcd->g15plugin,plugin_args,sizeof(plugin_s));
            plugin_args->args = clientnode;
        } else if(plugin_args->type == G15_PLUGIN_CORE_OS_KB || 
                  plugin_args->type == G15_PLUGIN_CORE_KB_INPUT ||
                          plugin_args->type == G15_PLUGIN_LCD_SERVER) 
                  {
                      plugin_args->args = masterlist;
                      plugin_args->plugin_handle = plugin_handle;
                  }

                  memset(&attr,0,sizeof(pthread_attr_t));
                  pthread_attr_init(&attr);
                  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
                  pthread_attr_setstacksize(&attr,64*1024); /* set stack to 64k - dont need 8Mb */
                  if (pthread_create(&client_connection, &attr, (void*)plugin_thread, plugin_args) != 0) {
                      g15daemon_log(LOG_ERR,"Unable to create client thread.");
                  } else {
                      pthread_detach(client_connection);
                  }
    }
    return 0;
}


int g15_open_all_plugins(g15daemon_t *masterlist, char *plugin_directory) {
    
    DIR *directory;
    struct dirent *ep;
    char pluginname[2048];
    int loadcount = 0;
    int count = g15_count_plugins(plugin_directory);
    config_section_t *load_cfg = g15daemon_cfg_load_section(masterlist,"PLUGIN_LOAD_ORDER");

   if(!uf_search_confitem(load_cfg,"TotalPlugins") ||
         (g15daemon_cfg_read_int(load_cfg,"TotalPlugins",0)!=count) ||
     	 (g15daemon_cfg_read_string(load_cfg,"0","")[0]=='/')) {

      g15daemon_log(LOG_INFO,"Number of plugins has changed. Rebuilding load order.");

      directory = opendir (plugin_directory);
      if (directory != NULL && count)
      {
         g15daemon_log(LOG_WARNING,"Attempting load of %i plugins",count);
         while ((ep = readdir (directory))) {
            if(strstr(ep->d_name,".so")){
                strcpy(pluginname, plugin_directory);
                strncat(pluginname,"/",1);
                strncat(pluginname,ep->d_name,200);
                if(g15_plugin_load (masterlist, pluginname)==0){
                  char tmp[10];
                  sprintf(tmp,"%i",loadcount);
                  g15daemon_cfg_write_string(load_cfg,tmp,ep->d_name);
                  loadcount++;

                }
                g15daemon_msleep(20);
            }
        }
        (void) closedir (directory);
        
        g15daemon_cfg_write_int(load_cfg,"TotalPlugins",loadcount);
        
        g15daemon_log(LOG_WARNING,"Successfully loaded %i of %i plugins.",loadcount,count);
        return loadcount;
      }
      else
        g15daemon_log (LOG_ERR,"Unable to open the directory: %s",plugin_directory);
    }else{
      int i;
      g15daemon_log(LOG_INFO,"Loading %i plugins named in g15daemon.conf.",count);
      for(i=0;i<count;i++){
        char tmp[10];
        char plugin_fname[1024];
        sprintf(tmp,"%i",i);
        strcpy(plugin_fname,PLUGINDIR);
        strncat(plugin_fname,"/",1);
        strncat(plugin_fname,g15daemon_cfg_read_string(load_cfg,tmp,""),128);
        g15_plugin_load(masterlist,plugin_fname);
        g15daemon_msleep(20);
      }
      return count;
    }
    return 0;
}

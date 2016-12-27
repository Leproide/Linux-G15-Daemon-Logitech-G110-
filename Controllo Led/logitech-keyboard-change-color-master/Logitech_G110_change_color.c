// Userspace USB driver for changing the Logitech G110 keyboard backlight color
// Copyleft 2011, Tom Van Braeckel <tomvanbraeckel@gmail.com>
// Credits: Tom Van Braeckel, Rich Budman
// Licensed under GPLv2

#include <stdio.h>
#include <usb.h>
#include <stdlib.h>

// The USB sniffer logs show a value of 0x00000022 for the RequestTypeReservedBits,
// but this driver only works when using 0x00000021 (which is also what the specs dictate).
#define CONTROLFLAGS 0x21

// This drivers is for device 046d:c22b
#define VENDORID 0x046d		// Logitech
#define PRODUCTID 0xc22b	// G110 G-keys

// microseconds to wait between changing the color a little bit
#define FADESPEED 100000 	// µs

// this is the most important function, called by various functions below
static int change_color(usb_dev_handle *handle, char* buffer) {
	int written = usb_control_msg(handle, CONTROLFLAGS, 0x00000009, 0x00000307, 0x00000000, buffer, 0x00000005, 5000);
	if (written != 5) {
		fprintf(stderr, "Setting color failed, error code %d\nThis happens sporadically on some machines.\n", written);
	}
	return written;
}

// go from red to blue to red to blue to...
// brightness is fixed and determinted by prefix[4]
static void loop_through_colors(usb_dev_handle *handle, char*prefix) {
	int i;
	while(1) {
		// change the color gradually
		for (i=1;i<255;i++) {
			prefix[1]=i;
			change_color(handle,prefix);
			usleep(FADESPEED);
		}
		// and gradually back
		for (i=255;i>0;i--) {
			prefix[1]=i;
			change_color(handle,prefix);
			usleep(FADESPEED);
		}
	}
}

// Credit for this function: http://www.jespersaur.com/drupal/node/25
static struct usb_device *findKeyboard (int vendor, int product) {
  struct usb_bus *bus;
  struct usb_device *dev;
  struct usb_bus *busses;

  usb_init();
  //usb_set_debug(3);
  usb_find_busses();
  usb_find_devices();
  busses = usb_get_busses();

  for (bus = busses; bus; bus = bus->next)
    for (dev = bus->devices; dev; dev = dev->next)
      if ((dev->descriptor.idVendor == vendor) && (dev->descriptor.idProduct == product))
        return dev;

  return NULL;
}

int main (int argc,char **argv) {
  struct usb_device * dev;
  dev = findKeyboard(VENDORID,PRODUCTID);
  if (dev == NULL) {
    fprintf(stderr, "Error: keyboard not found!\n");
    return 1;
  } else {
	  struct usb_dev_handle *handle;
	  handle = usb_open(dev);
	  if (dev == NULL) {
	    fprintf(stderr, "Error: could not open usb device!\n");
	    return 1;
	  } else {

		// detach kernel driver (if there is any attached)
		int detachResult = usb_detach_kernel_driver_np(handle, 0);
		if (detachResult < 0) {
			// -61 = -ENODATA = No data available ?

			// let's not fuss about this, since it probably means the device is not attached with a kernel driver
			//fprintf(stderr, "Warning: Could not detach interface 0 (errnr %d)!\n", detachResult);
		}

		// set configuration
		int setConfigResult = usb_set_configuration(handle, 1);	// we choose 1, like the bConfigurationValue in the dump
		if (setConfigResult < 0) {
			// -16 = EBUSY = device or resource is busy
			fprintf(stderr, "Could not set configuration (errnr %d)!\n", setConfigResult);
		  }

		// set single color if an argument was given, otherwise loop through colors
                unsigned char prefix [] = {0x07, 0x00, 0x00, 0x00, 0xff};
		if ( argc > 1 ) {
                     int i = atoi( argv[1] );
                     if ( i < 0 || i > 255 ) {
                         printf("Error: invalid color range (0-255)\n");
                     } else {  
                         prefix[1] = atoi( argv[1] );
                         change_color(handle, (char*)prefix);
                     }
                 } else {
                     loop_through_colors(handle, (char*)prefix);
                 }

		if (usb_close(handle) < 0) {
		  fprintf(stderr, "Could not close usb device!\n");
		  return 1;
		}
  }

  return 0;
  }
}

# Linux G15 Daemon [Logitech G110 and others keyboards]
G15 Daemon + G15 Macro + Led Control for G110 and other Logitech keyboard on Linux 

#### Video Tutorial:

* ITALIAN https://www.youtube.com/watch?v=AnyfWFa_Ku0

* ENGLISH https://www.youtube.com/watch?v=8gAT-BbyOWw

#### Install:

* cd libusb1-1.4.1
* sudo python setup.py install

---

* cd libg15-1.2.7 - G110 Patch
* ./configure
* make
* sudo make install

---

* cd g15daemon-1.9.5.3
* ./configure
* make
* sudo make install

In case you have this error "libg15render-1.2 not found" see this https://github.com/Leproide/Linux-G15-Daemon-Logitech-G110-/issues/2

---

* cd g15macro-1.0.3
* ./configure
* (in case of errors: yum groupinstall "X Software Development")
* make
* sudo make install


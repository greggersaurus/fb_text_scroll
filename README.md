Frame Buffer Text Scroll
========================

Introduction
------------

The idea for this project was to provide example code for creating a scrolling
 message to be displayed using a Linux framebuffer device. The Raspberry Pi 
 Sense HAT comes with an 8x8 multi-color LED matrix and there is a Linux driver 
 that creates a framebuffer device to accessing this peripheral. This code was
 originally created as a simple demo for someone starting off with a Raspberry
 Pi equipped with a Sense HAT module who wanted to write some C code that 
 interfaces with one of the system's hardware peripherals.

Design 
------

The main code relies on three main pieces: A framebuffer to display the 
 character on, LUTs to define how a particular character is to be displayed and
 an event device (i.e. joystick) to control the color and scroll speed of the
 text.

### Drawing to the Frame Buffer

At each update interval all the columns in the framebuffer are shifted to the 
 left by one column while the rightmost column is filled in with new data 
 (i.e. the next column of the current character being written out).

### Character Display

A LUT with entries containing character size, spacing and an array indicating 
 which pixels should be set is used to determine which pixels should be on or
 off in order to display a particular character.

### Event Device Input

The Sense HAT joystick is the primary input for this application. The UP and
 DOWN positions are used to cycle through different colors for displaying the 
 font, while the RIGHT and LEFT positions are used to speed up or slow down
 the scroll speed.

Font Generation
---------------

The LUTs that dictate how a character is to be displayed on the framebuffer
 are generated using the bdf2struct.sh script. This script takes Glyph Bitmap
 Distribution Format (BDF) font data and converts it into structs, which get
 mapped into a LUT for access in main.cpp.

bdfchars.h, bdfchars_lut.h and bitmap_array.h are all revisioned so that this
 application can be built without needing to rerun bdf2struct.sh.
 
Execution 
---------

The application takes a text file as an optional input argument. The text from
 this file is written to the Sense HAT 8x8 LED matrix. 

A systemd service file (fb_text_scroll.service) is provided so that this 
 application can be setup to run on boot of the Raspberry Pi. Move this
 script to /lib/systemd/system/fb_text_scroll.service and then run 
 systemctl enable fb_text_scroll.service to have the application run on boot.

Improvements and Considerations
-------------------------------

The Sense HAT python library (http://pythonhosted.org/sense-hat/api/#led-matrix)
 has functionality for scrolling text on the 8x8 LED matrix. It might make more
 sense to use that than trying to adapt and improve this code at this point. 
 This code really should have been designed to be more modular and extensible 
 (i.e. a class for scrolling text to any Linux framebuffer). This could all be
 separated out with a bit of work, but it not conducive to proper reuse in its
 current state.


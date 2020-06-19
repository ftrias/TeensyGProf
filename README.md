TeensyGProf
=====================
by Fernando Trias

Implementation of gprof profiler for Teensy 3 and 4 platform from PJRC.

This document is written for Linux/Mac. It is possible to follow a similar procedure for Windows.

See license.txt for licenses used by code.


Requires 
--------------

1. Build gprof cross-compiled for ARM or use the binaries in "binaries" directory.  Instructions for building are beyond the scope of this text. Look up how to install your own cross-compiler, including arm-none-eabi-gprof. Normally this is part of a build, but Teensyduino does not include it.

2. Python 2.7 or higher for automatic processing.

3. Modify Teensyduino files as shown in *Patches* section at bottom of this document.


Overview
-------------

GProf is a sampling profiler that does two things:

1. Samples the current instruction every 1 millisecond. That is, at each millisecond it looks to see what function is currently running and keeps a counter of how many times this happens. Over time, this gives an approximate value of how much time each function consumes.

2. Keep track of which function calls which. It will use this to create an estimate of the cumulative time spent in a function and all functions it calls.

After running for a while (you determine the amount of time), a file "gmon.out" is written  out to a serial port with the function counters and data. A python program listens for this file on the serial port and then gprof will cross-reference this file the original executable by gprof to generate a table of execution times.


Installation Instructions
--------------

1. Install ZIP file as Arudino library.

2. Modify files as described in *Patches* section.

3. Build or copy arm-none-eabi-gprof and modify `readfile.py` to point to it.


Running Instructions
--------------

1. Start Arduino and open TeensyGProf example.

2. Select menu Tools / Profile / On.

3. Select a USB Type that includes Serial (preferably Dual Serial USB).

4. Compile and upload.

5. Run `python readfile.py --serial /dev/cu.usb.usbmodem123456` [substitute actual usb serial device].

`readfile.py` will open the serial port, print out anything it receives, filtering and processing the special `gmon.out` data. It will write out the `gmon.out` file and then run `gprof` to show the outout.

The library also supports writing the `gmon.out` file from midi, in hex or to an SD card. See `TeensyGProf.h`.


Short Example
----------------

Compile the following with `Dual Serial USB` support and `Profile / On`.

```C++
#include "TeensyGProf.h"

volatile int sum = 0;

void do_something() {
  for(int i=0; i<1000; i++) {
    sum += 1;
  }
}

void setup() {
  // collect for 5000 milliseconds and send on second USB Serial
  gprof.begin(&SerialUSB1, 5000); 
}

void loop() {
  do_something();
}
```

You can view the sketch output with the Serial Monitor and collect the profile data with the `readfile.py` script as in:

```
python readfile.py --serial /dev/cu.usb.usbmodem123456
```

The script will go into a loop processing all the runs that it detects until you terminate it by pressing Ctrl-C. In this way, you can recompile and restart Teensy without having to restart `readfile.py`.


Patches
---------------

The files below must be added (or modified if already existing). They are located in the Teensyduino install directory, which is a part of Arduino. On the Mac, they are in `/Applications/Arduino.app/Contents/Java/hardware/teensy/avr`.

1. In the same directory as `boards.txt`, place a copy of `boards.local.txt`. If the file exists, add to the end. This will add the menu options.

2. In the same directory as above, copy `platform.local.txt` (or append to it). This will modify the compile stage of cpp files to add the profiling option. It will also add an additional build step that copies the elf file (renaming a section--see below) to standard location so that `readfile.py` can find it. The value for `profelf` must match the value in `readfile.py`.


Implementation details
--------------

1. TeensyGProf will commandeer the `systick_isr` handler. It will process it's own profiling before handing control to the original `systick_isr` handler. Thus EventResponder and all other timing code should be unaffected. The sampling data will be stored in RAM.

2. It adds the `-pg` compiler flag to cpp files. This causes the compiler to add a call to `_gnu_mcount_nc` at the start of every function. That's how it keeps track of the call stack. Call stacks (called Arcs) are also stored in RAM.

3. You can configure the amount of RAM memory used by the sampler in Step 1 and the call tracker in Step 2. The more memory you allocate, the more accurate your results. Look at file `gmon.h` and modify `HASHFRACTION` and `ARCDENSITY`.

4. If you call `grpof.begin()` and pass milliseconds it will start a timer that upon termination executes `gprof.end()`, which outputs the data. Otherwise you must call `gprof.end()`. That processes all the data and outputs the contents of `gmon.out` to the desired port in the format requested. This file, along with a copy of the `elf` file is used by gprof to generate a report. You can customize the output method by subclassing class `GProfOutput`. For example, you could send this file via a network or HTTP.

5. For some reason, Teensy 4 puts it's code in a section called `.text.itcm`. Gprof expects it in a section called `.text`, which is the standard in Linux. Teensy 3 puts it in the right place. So the `platform.local.txt` files tells arduino to run `objcopy` to rename the section.


Todo
--------------

* Clean up source code. Code was ported from a destop implementation and could use some comments and optimization for ARM.

* Create a script that will make all necessary modifications to Teensyduino files.

* Right now, it only profiles C++ code. If I add profiling to C files, it won't work. I've tried adding `__attribute__((no_instrument_section))` to many of the basic C functions like ResetHandler(), etc. but I can't find the right combination of functions to change. With more time, I'm sure this could be solved. However, since C++ is the default language for Arduino files and almost all libraries are written in C++, this may not be a big problem.


References
---------------

For ARM solution this project is based on see: 
https://mcuoneclipse.com/2015/08/23/tutorial-using-gnu-profiling-gprof-with-arm-cortex-m/

For an interesting overview of gprof:
http://wwwcdf.pd.infn.it/localdoc/gprof.pdf

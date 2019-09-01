TeensyGProf
=====================

Implementation of gprof profiler for Teensy 3/4 platform from PJRC.

This document is written for Linux/Mac. It's probably possible to follow a
similar procedure for Windows.

See license.txt for licenses used by code.


Requires 
--------------

1. Install the ZIP file as an Arduino library (Sketch/Include Library/Add ZIP Library).

2. Modify Teensyduino files as shown in *Patches* section at bottom of this document.

3. Build of gprof for ARM or use provided binaries in "binaries" directory. 
Instructions for building are beyond the scope of this
text. Look up how to install your own cross-compiler, including arm-none-eabi-gprof. 
Normally this is part of
a build, but Teensyduino does not include it.

4. Modify `readfile.py` so the variable `gprof` has correct path to executable from step 2.

5. Python installed.


Overview
-------------

The profiler does two things:

1. Samples the current instruction every 1 millisecond. That is
at each millisecond it looks to see what function is currently running and
keeps a counter of how many times this happens. Over time, this gives
an approximate value of how much time each function consumes.

2. Keep track of which function calls which. It will use this to create an
estimate of the cumulative time spent in a function and all functions it calls.

After running for a while (you determine this time), a file "gmon.out" is written 
out to a serial port with the function counters and data. A python program listens for this file
on the serial port and then this file is cross-referenced by gprof with 
the original executable to 
generate a table of execution times.


Quick Instructions
--------------

1. Modify files as described in *Patches* section
2. Build or copy arm-none-eabi-gprof & modify `readfile.py` to point to it
3. Open up Arduino and TeensyGProf example
4. Select Tools/Optimize/Profile
5. Select a USB Type that includes Serial
6. Compile and upload
7. Run `./readfile.py --serial /dev/cu.usb*` [or substitute actual usb serial]

`readfile.py` will open the serial port, print out anything it receives
and process the special `gmon.out` data. It will write out the `gmon.out` file and then
run `gprof` showing the outout.

The library also supports writing the `gmon.out` file from midi, in hex or to an SD card. See
`TEENSYPROF_OUT` in `TeensyGProf.h` and implementation in `TeensyFile.cpp`.


Short Example
----------------

```C++
void setup() {
  Serial.begin(115200);
  TeensyProf_init_stream(&Serial1);
}

void loop() {
  static long start = millis();
  do_something();
  if (start && millis() - start > 10000) {   // after 10 seconds
    gprof_end();                             // write out profile data
    start = 0;
  }
}
```


Todo
--------------

* Clean up source code. Code was ported from a destop implementation and could use
some comments and optimization for ARM.

* Create a script that will make all necessary modifications to Teensyduino files.

* Right now, it only profiles C++ code. If I add profiling to C files, it won't work. I've tried adding
`__attribute__((no_instrument_section))` to many of the basic C functions like ResetHandler(), etc.
but I can't find the right combination of functions to change. With more time, I'm sure this could
be solved. However, since C++ is the default language for Arduino files and almost all libraries are
written in C++, this may not be a big problem.


References
---------------

For ARM solution this project is based on see: 
https://mcuoneclipse.com/2015/08/23/tutorial-using-gnu-profiling-gprof-with-arm-cortex-m/

For an interesting overview of gprof:
http://wwwcdf.pd.infn.it/localdoc/gprof.pdf


Patches
---------------

The files below must be patched. They are located in the Teensyduino install directory, which is
a part of Arduino. On the Mac, they are in `/Applications/Arduino.app/Contents/Java/hardware/teensy`.

The patches should not affect any other functionality. Arduino should work the same as before.

The modifications to boards.txt add a new menu option Tools/Optimize/Profile. This option will add
profiling to most of the code and turn off optimizations that might confuse the profiler.

The source code changes are to the linker file. On Teensy 4, all the code is moved to a section
called `.text` (the standard name that gprof expects) instead of `.text.itcm`. Teensy 3 does this correctly.

```diff
*** avr/boards.txt	2019-08-24 07:58:47.000000000 -0400
--- avr.prof/boards.txt	2019-08-24 07:47:38.000000000 -0400
***************
*** 23,28 ****
--- 23,29 ----
  teensy40.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib
  teensy40.build.flags.dep=-MMD
  teensy40.build.flags.optimize=-Os
+ teensy40.build.flags.profile=
  teensy40.build.flags.cpu=-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16
  teensy40.build.flags.defs=-D__IMXRT1062__ -DTEENSYDUINO=147
  teensy40.build.flags.cpp=-std=gnu++14 -fno-exceptions -fpermissive -fno-rtti -fno-threadsafe-statics -felide-constructors -Wno-error=narrowing
***************
*** 112,117 ****
--- 113,122 ----
  teensy40.menu.opt.ogstd=Debug
  teensy40.menu.opt.ogstd.build.flags.optimize=-Og
  teensy40.menu.opt.ogstd.build.flags.ldspecs=
+ teensy40.menu.opt.pgstd=Profile
+ teensy40.menu.opt.pgstd.build.flags.optimize=-O0 -g
+ teensy40.menu.opt.pgstd.build.flags.profile=-pg
+ teensy40.menu.opt.pgstd.build.flags.ldspecs=
  #teensy40.menu.opt.oglto=Debug with LTO
  #teensy40.menu.opt.oglto.build.flags.optimize=-Og -flto -fno-fat-lto-objects
  #teensy40.menu.opt.oglto.build.flags.ldspecs=-fuse-linker-plugin
***************
*** 194,199 ****
--- 199,205 ----
  #teensy4b1.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib
  #teensy4b1.build.flags.dep=-MMD
  #teensy4b1.build.flags.optimize=-Os
+ #teensy4b1.build.flags.profile=
  #teensy4b1.build.flags.cpu=-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16
  #teensy4b1.build.flags.defs=-D__IMXRT1052__ -DTEENSYDUINO=147
  #teensy4b1.build.flags.cpp=-std=gnu++14 -fno-exceptions -fpermissive -fno-rtti -fno-threadsafe-statics -felide-constructors -Wno-error=narrowing
***************
*** 367,372 ****
--- 373,379 ----
  teensy36.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib
  teensy36.build.flags.dep=-MMD
  teensy36.build.flags.optimize=-Os
+ teensy36.build.flags.profile=
  teensy36.build.flags.cpu=-mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fsingle-precision-constant
  teensy36.build.flags.defs=-D__MK66FX1M0__ -DTEENSYDUINO=147
  teensy36.build.flags.cpp=-fno-exceptions -fpermissive -felide-constructors -std=gnu++14 -Wno-error=narrowing -fno-rtti
***************
*** 572,577 ****
--- 579,585 ----
  teensy35.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib
  teensy35.build.flags.dep=-MMD
  teensy35.build.flags.optimize=-Os
+ teensy35.build.flags.profile=
  teensy35.build.flags.cpu=-mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fsingle-precision-constant
  teensy35.build.flags.defs=-D__MK64FX512__ -DTEENSYDUINO=147
  teensy35.build.flags.cpp=-fno-exceptions -fpermissive -felide-constructors -std=gnu++14 -Wno-error=narrowing -fno-rtti
***************
*** 767,772 ****
--- 775,781 ----
  teensy31.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib
  teensy31.build.flags.dep=-MMD
  teensy31.build.flags.optimize=-Os
+ teensy31.build.flags.profile=
  teensy31.build.flags.cpu=-mthumb -mcpu=cortex-m4 -fsingle-precision-constant
  teensy31.build.flags.defs=-D__MK20DX256__ -DTEENSYDUINO=147
  teensy31.build.flags.cpp=-fno-exceptions -fpermissive -felide-constructors -std=gnu++14 -Wno-error=narrowing -fno-rtti
***************
*** 881,886 ****
--- 890,899 ----
  teensy31.menu.opt.ogstd=Debug
  teensy31.menu.opt.ogstd.build.flags.optimize=-Og
  teensy31.menu.opt.ogstd.build.flags.ldspecs=
+ teensy31.menu.opt.pgstd=Profile
+ teensy31.menu.opt.pgstd.build.flags.optimize=-O0 -g
+ teensy31.menu.opt.pgstd.build.flags.profile=-pg
+ teensy31.menu.opt.pgstd.build.flags.ldspecs=
  teensy31.menu.opt.oglto=Debug with LTO
  teensy31.menu.opt.oglto.build.flags.optimize=-Og -flto -fno-fat-lto-objects
  teensy31.menu.opt.oglto.build.flags.ldspecs=-fuse-linker-plugin
```

These modifications will add the Profile options to the compile stage and also copy the final
executable to `/tmp/build.elf` so that gprof can use it later.

```diff
*** avr/platform.txt	2019-08-24 07:58:47.000000000 -0400
--- avr.prof/platform.txt	2019-08-23 22:32:46.000000000 -0400
***************
*** 22,28 ****
  recipe.hooks.sketch.prebuild.1.pattern="{compiler.path}precompile_helper" "{runtime.platform.path}/cores/{build.core}" "{build.path}" "{compiler.path}{build.toolchain}{build.command.g++}" -x c++-header {build.flags.optimize} {build.flags.common} {build.flags.dep} {build.flags.cpp} {build.flags.cpu} {build.flags.defs} -DARDUINO={runtime.ide.version} -DF_CPU={build.fcpu} -D{build.usbtype} -DLAYOUT_{build.keylayout} "-I{runtime.platform.path}/cores/{build.core}" "{build.path}/pch/Arduino.h" -o "{build.path}/pch/Arduino.h.gch"
  
  ## Compile c++ files
! recipe.cpp.o.pattern="{compiler.path}{build.toolchain}{build.command.g++}" -c {build.flags.optimize} {build.flags.common} {build.flags.dep} {build.flags.cpp} {build.flags.cpu} {build.flags.defs} -DARDUINO={runtime.ide.version} -DF_CPU={build.fcpu} -D{build.usbtype} -DLAYOUT_{build.keylayout} "-I{build.path}/pch" {includes} "{source_file}" -o "{object_file}"
  
  ## Compile c files
  recipe.c.o.pattern="{compiler.path}{build.toolchain}{build.command.gcc}" -c {build.flags.optimize} {build.flags.common} {build.flags.dep} {build.flags.c} {build.flags.cpu} {build.flags.defs} -DARDUINO={runtime.ide.version} -DF_CPU={build.fcpu} -D{build.usbtype} -DLAYOUT_{build.keylayout} {includes} "{source_file}" -o "{object_file}"
--- 22,28 ----
  recipe.hooks.sketch.prebuild.1.pattern="{compiler.path}precompile_helper" "{runtime.platform.path}/cores/{build.core}" "{build.path}" "{compiler.path}{build.toolchain}{build.command.g++}" -x c++-header {build.flags.optimize} {build.flags.common} {build.flags.dep} {build.flags.cpp} {build.flags.cpu} {build.flags.defs} -DARDUINO={runtime.ide.version} -DF_CPU={build.fcpu} -D{build.usbtype} -DLAYOUT_{build.keylayout} "-I{runtime.platform.path}/cores/{build.core}" "{build.path}/pch/Arduino.h" -o "{build.path}/pch/Arduino.h.gch"
  
  ## Compile c++ files
! recipe.cpp.o.pattern="{compiler.path}{build.toolchain}{build.command.g++}" -c {build.flags.optimize} {build.flags.profile} {build.flags.common} {build.flags.dep} {build.flags.cpp} {build.flags.cpu} {build.flags.defs} -DARDUINO={runtime.ide.version} -DF_CPU={build.fcpu} -D{build.usbtype} -DLAYOUT_{build.keylayout} "-I{build.path}/pch" {includes} "{source_file}" -o "{object_file}"
  
  ## Compile c files
  recipe.c.o.pattern="{compiler.path}{build.toolchain}{build.command.gcc}" -c {build.flags.optimize} {build.flags.common} {build.flags.dep} {build.flags.c} {build.flags.cpu} {build.flags.defs} -DARDUINO={runtime.ide.version} -DF_CPU={build.fcpu} -D{build.usbtype} -DLAYOUT_{build.keylayout} {includes} "{source_file}" -o "{object_file}"
***************
*** 49,54 ****
--- 49,55 ----
  recipe.hooks.postbuild.1.pattern="{compiler.path}stdout_redirect" "{build.path}/{build.project_name}.lst" "{compiler.path}{build.toolchain}{build.command.objdump}" -d -S -C "{build.path}/{build.project_name}.elf"
  recipe.hooks.postbuild.2.pattern="{compiler.path}stdout_redirect" "{build.path}/{build.project_name}.sym" "{compiler.path}{build.toolchain}{build.command.objdump}" -t -C "{build.path}/{build.project_name}.elf"
  recipe.hooks.postbuild.3.pattern="{compiler.path}teensy_post_compile" "-file={build.project_name}" "-path={build.path}" "-tools={compiler.path}" "-board={build.board}"
+ recipe.hooks.postbuild.4.pattern="cp" "{build.path}/{build.project_name}.elf" "/tmp/build.elf"
  
  ## Compute size
  recipe.size.pattern="{compiler.path}{build.toolchain}{build.command.size}" -A "{build.path}/{build.project_name}.elf"
```

For some reason Teensy 4 does not put all code in a section called `.text` which is what
gprof expects. Teensy 3 does this so it does not need to be modified.

```diff
*** avr/cores/teensy4/imxrt1062.ld	2019-08-24 07:58:46.000000000 -0400
--- avr.prof/cores/teensy4/imxrt1062.ld	2019-08-22 07:09:45.000000000 -0400
***************
*** 30,36 ****
  		. = ALIGN(16);
  	} > FLASH
  
! 	.text.itcm : {
  		. = . + 32; /* MPU to trap NULL pointer deref */
  		*(.fastrun)
  		*(.text*)
--- 30,36 ----
  		. = ALIGN(16);
  	} > FLASH
  
! 	.text : {
  		. = . + 32; /* MPU to trap NULL pointer deref */
  		*(.fastrun)
  		*(.text*)
***************
*** 55,63 ****
  		. = ALIGN(16);
  	} > RAM
  
! 	_stext = ADDR(.text.itcm);
! 	_etext = ADDR(.text.itcm) + SIZEOF(.text.itcm);
! 	_stextload = LOADADDR(.text.itcm);
  
  	_sdata = ADDR(.data);
  	_edata = ADDR(.data) + SIZEOF(.data);
--- 55,63 ----
  		. = ALIGN(16);
  	} > RAM
  
! 	_stext = ADDR(.text);
! 	_etext = ADDR(.text) + SIZEOF(.text);
! 	_stextload = LOADADDR(.text);
  
  	_sdata = ADDR(.data);
  	_edata = ADDR(.data) + SIZEOF(.data);
***************
*** 69,79 ****
  	_heap_start = ADDR(.bss.dma) + SIZEOF(.bss.dma);
  	_heap_end = ORIGIN(RAM) + LENGTH(RAM);
  
! 	_itcm_block_count = (SIZEOF(.text.itcm) + 0x7FFE) >> 15;
  	_flexram_bank_config = 0xAAAAAAAA | ((1 << (_itcm_block_count * 2)) - 1);
  	_estack = ORIGIN(DTCM) + ((16 - _itcm_block_count) << 15);
  
! 	_flashimagelen = SIZEOF(.text.progmem) + SIZEOF(.text.itcm) + SIZEOF(.data);
  	_teensy_model_identifier = 0x24;
  
  	.debug_info     0 : { *(.debug_info) }
--- 69,79 ----
  	_heap_start = ADDR(.bss.dma) + SIZEOF(.bss.dma);
  	_heap_end = ORIGIN(RAM) + LENGTH(RAM);
  
! 	_itcm_block_count = (SIZEOF(.text) + 0x7FFE) >> 15;
  	_flexram_bank_config = 0xAAAAAAAA | ((1 << (_itcm_block_count * 2)) - 1);
  	_estack = ORIGIN(DTCM) + ((16 - _itcm_block_count) << 15);
  
! 	_flashimagelen = SIZEOF(.text.progmem) + SIZEOF(.text) + SIZEOF(.data);
  	_teensy_model_identifier = 0x24;
  
  	.debug_info     0 : { *(.debug_info) }
```

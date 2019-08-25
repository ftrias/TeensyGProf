/************
 * 
 * Copyright 2019 by Fernando Trias. All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ************/

#include <Arduino.h>
#include "TeensyGProf.h"

#if TEENSYPROF_OUT==1 //"SDCARD"

#include <SD.h>
#include <SPI.h>

// #define TeensyProf_open open
// #define TeensyProf_write write
// #define TeensyProf_close close

int GPROF_MOSI = 7;
int GPROF_SCK = 14;
int GPROF_CD = 10;

extern "C" void TeensyProf_init_sd(int mosi, int sck, int cd) {
  GPROF_MOSI = mosi;
  GPROF_SCK = sck;
  GPROF_CD = cd;
}

File myFile;

int file_lib_flag = 1;
int file_lib_error = 1;
int file_lib_init() {
  if (!file_lib_flag) {
    SPI.setMOSI(GPROF_MOSI);
    SPI.setSCK(GPROF_SCK);
    if (SD.begin(GPROF_CD)) {
      file_lib_flag = 1;
      return 1;
    }
    return 0;
  }
  return 1;
}
extern "C" int TeensyProf_open(const char *fn, int flags, int perm) {
  file_lib_init();
  SD.remove(fn);
  myFile = SD.open(fn, FILE_WRITE);
  if (!myFile) return -1;
  file_lib_error = 0;
  return 1;
}
extern "C" int TeensyProf_write(int fp, const void *data, int len) {
  if (file_lib_error) return -1;
  if (!myFile) return -1;
  return myFile.write((char*)data, len);
}
extern "C" int TeensyProf_close(int fp) {
  if (file_lib_error) return -1;
  if (!myFile) return -1;
  myFile.close();
  file_lib_error = 1;
  return 1;
}

#elif TEENSYPROF_OUT==2 //"SERIALFILE"

Stream *mystream = &Serial;

extern "C" void TeensyProf_init_stream(void *s) {
  mystream = (Stream *) s;
}

extern "C" int TeensyProf_open(const char *fn, int flags, int perm) {
  char x[256];
  x[0] = 0;
  strcat(x, "wb");
  strcat(x, ":");
  strcat(x, fn);
  mystream->write((uint8_t)0x01);
  mystream->write((uint8_t)0x01);
  mystream->write((uint8_t)strlen(x));
  mystream->write(x, strlen(x));
  return 1;
}

extern "C" int TeensyProf_write(int fp, const void *data, int length) {
  int len;
  const uint8_t *d = (const uint8_t *)data;
  while (1) {
    if (length > 64) len = 64;
    else len = length;
    mystream->write((uint8_t)0x01);
    mystream->write((uint8_t)0x04);
    mystream->write((uint8_t)len);
    mystream->write((uint8_t *)d, len);
    d += len;
    length -= len;
    if (length <=0) break;
  }
  return length;
}

extern "C" int TeensyProf_close(int fp) {
  mystream->write((uint8_t)0x01);
  mystream->write((uint8_t)0x02);
  mystream->write((uint8_t)0x01);
  mystream->write((uint8_t)0x00);
  return 1;
}

#elif TEENSYPROF_OUT==3 //"MIDIFILE"

extern "C" int TeensyProf_open(const char *fn, int flags, int perm) {
  uint8_t x[256];
  x[0] = 0x01;
  strcpy((char*)x+1, fn);
  usbMIDI.sendSysEx(strlen(fn)+1, x, false);
  usbMIDI.send_now();
  return 1;
}

extern "C" int TeensyProf_write(int fp, const void *data, int length) {
  uint8_t x[64];
  x[0] = 0xF0;
  x[1] = 0x03;
  int len;
  const uint8_t *raw = (const uint8_t *)data;
  while (1) {
    if (length > 24) len = 24;
    else len = length;
    int xi = 2;
    for (int i=0; i<len; i++) {
      x[xi++] = raw[i] >> 4;
      x[xi++] = raw[i] & 0x0F;
    }
    x[xi] = 0xF7;
    usbMIDI.sendSysEx(xi+1, x, true);
    usbMIDI.send_now();
    raw += len;
    length -= len;
    if (length <=0) break;
    // delay(1);
  }
  return length;
}

extern "C" int TeensyProf_close(int fp) {
  uint8_t x[] = {0xF0, 0x02, 0x00, 0x00, 0x00, 0xF7};
  usbMIDI.sendSysEx(sizeof(x), x, true);
  usbMIDI.send_now();
  return 1;
}

#elif TEENSYPROF_OUT==4 //"HEXFILE"

Stream *mystream = &Serial;

extern "C" void TeensyProf_init_stream(void *s) {
  mystream = (Stream *) s;
}

extern "C" int TeensyProf_open(const char *fn, int flags, int perm) {
  mystream->print("START:");
  mystream->println(fn);
  return 1;
}

extern "C" int TeensyProf_write(int fp, const void *data, int length) {
  static const char *hex = "0123456789ABCDEF";
  static int column = 0;
  for (int i=0; i<length; i++) {
    if (++column > 80) {
      mystream->println();
      column = 1;
    }
    mystream->print(hex[((uint8_t *)data)[i] >> 4]);
    mystream->print(hex[((uint8_t *)data)[i] & 0x0F]);
    // delay(1);
  }
  return length;
}

extern "C" int TeensyProf_close(int fp) {
  mystream->println();
  mystream->println("END");
  return 1;
}

#endif

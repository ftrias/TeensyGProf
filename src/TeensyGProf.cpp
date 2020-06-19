#include <Arduino.h>
#include "TeensyGProf.h"
#include "EventResponder.h"

MillisTimer timer;
EventResponder complete;

GProfOutput *gout = NULL;

GProf gprof;

int GProf::begin(GProfOutput *output, unsigned long milliseconds) {
    if (gout) delete gout;
    gout = output;
    if (milliseconds) {
        complete.attach((EventResponderFunction)gprof_end);
        timer.begin(milliseconds, complete);
    }
    return gprof_start();
}

int GProf::begin(Stream *output, unsigned long milliseconds) {
    // this causes a memory leak!!
    GProfOutputFile *out = new GProfOutputFile();
    out->begin(output);
    return begin(out, milliseconds);
}

int GProf::end(GProfOutput *output) {
    if (output) {
        if (gout) delete gout;
        gout = output;
    }
    if (gprof_end()) {
        Serial.println("Error in gprof_end().");
    }
    // while(1) { asm volatile("wfi"); }
    return 0;
}


#ifdef __cplusplus
extern "C" {
#endif

int TeensyProf_open(const char *fn, int flags, int perm) {
    if (gout == NULL) {
        GProfOutputHex *out = new GProfOutputHex();
        out->begin(&Serial);
        gout = out;
    }
    return gout->open(fn, flags, perm);
}
int TeensyProf_write(int fp, const void *data, int length) {
    return gout->write(data, length);
}
int TeensyProf_close(int fp) {
    return gout->close();
}

#ifdef __cplusplus
}


#include <SD.h>
#include <SPI.h>

int GPROF_MOSI = 7;
int GPROF_SCK = 14;
int GPROF_CD = 10;

void GProfOutputSD::begin(int mosi, int sck, int cd) {
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
int GProfOutputSD::open(const char *fn, int flags, int perm) {
  file_lib_init();
  SD.remove(fn);
  myFile = SD.open(fn, FILE_WRITE);
  if (!myFile) return -1;
  file_lib_error = 0;
  return 1;
}
int GProfOutputSD::write(const void *data, int len) {
  if (file_lib_error) return -1;
  if (!myFile) return -1;
  return myFile.write((char*)data, len);
}
int GProfOutputSD::close() {
  if (file_lib_error) return -1;
  if (!myFile) return -1;
  myFile.close();
  file_lib_error = 1;
  return 1;
}

Stream *mystream = &Serial;

void GProfOutputFile::begin(Stream *s) {
  mystream = s;
}

int GProfOutputFile::open(const char *fn, int flags, int perm) {
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

int GProfOutputFile::write(const void *data, int length) {
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

int GProfOutputFile::close() {
  mystream->write((uint8_t)0x01);
  mystream->write((uint8_t)0x02);
  mystream->write((uint8_t)0x01);
  mystream->write((uint8_t)0x00);
  return 1;
}

#ifdef MIDI_INTERFACE

int GProfOutputMIDI::open(const char *fn, int flags, int perm) {
  uint8_t x[256];
  x[0] = 0x01;
  strcpy((char*)x+1, fn);
  usbMIDI.sendSysEx(strlen(fn)+1, x, false);
  usbMIDI.send_now();
  return 1;
}

int GProfOutputMIDI::write(const void *data, int length) {
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

int GProfOutputMIDI::close() {
  uint8_t x[] = {0xF0, 0x02, 0x00, 0x00, 0x00, 0xF7};
  usbMIDI.sendSysEx(sizeof(x), x, true);
  usbMIDI.send_now();
  return 1;
}

#endif

void GProfOutputHex::begin(Stream *s) {
  mystream = s;
}

int GProfOutputHex::open(const char *fn, int flags, int perm) {
  mystream->print("START:");
  mystream->println(fn);
  return 1;
}

int GProfOutputHex::write(const void *data, int length) {
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

int GProfOutputHex::close() {
  mystream->println();
  mystream->println("END");
  return 1;
}

#endif

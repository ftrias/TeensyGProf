#ifndef GPROF_H_INCLUDED
#define GPROF_H_INCLUDED

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

int gprof_end();
int gprof_memory();
int gprof_start();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class GProfOutput {
private:
    int fp;
public:
    virtual ~GProfOutput() {}
    virtual int open(const char *fn, int flags, int perm) = 0;
    virtual int write(const void *data, int length) = 0;
    virtual int close() = 0;
};

class GProfOutputSD : public GProfOutput {
private:
    int fp;
public:
    virtual ~GProfOutputSD() {}
    virtual int open(const char *fn, int flags, int perm);
    virtual int write(const void *data, int length);
    virtual int close();
    void begin(int mosi, int sck, int cd);
};

class GProfOutputHex : public GProfOutput {
private:
    int fp;
public:
    virtual ~GProfOutputHex() {}
    virtual int open(const char *fn, int flags, int perm);
    virtual int write(const void *data, int length);
    virtual int close();
    void begin(Stream *s); // pass (Stream *)
};

class GProfOutputFile : public GProfOutput {
private:
    int fp;
public:
    virtual ~GProfOutputFile() {}
    virtual int open(const char *fn, int flags, int perm);
    virtual int write(const void *data, int length);
    virtual int close();
    void begin(Stream *s); // pass (Stream *)
};

#ifdef MIDI_INTERFACE

class GProfOutputMIDI : public GProfOutput {
private:
    int fp;
public:
    virtual int open(const char *fn, int flags, int perm);
    virtual int write(const void *data, int length);
    virtual int close();
    void begin(Stream *s); // pass (Stream *)
};

#endif

class GProf {
public:
    int begin(GProfOutput *output = NULL, unsigned long milliseconds = 0);
    int begin(Stream *output, unsigned long milliseconds = 0);
    int end(GProfOutput *output = NULL);
};

extern GProf gprof;

#endif

#ifdef __cplusplus
extern "C" {
#endif

int TeensyProf_open(const char *fn, int flags, int perm);
int TeensyProf_write(int fp, const void *data, int length);
int TeensyProf_close(int fp);

#ifdef __cplusplus
}
#endif

#endif
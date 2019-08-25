#ifndef GPERF_H_INCLUDED
#define GPERF_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

int gprof_end();
int gprof_memory();
int gprof_start();

#ifdef __cplusplus
}
#endif

#define TEENSYPROF_OUT 2

#endif

#ifndef     P7R_TIMING_H_
#define     P7R_TIMING_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"


uint64_t get_timestamp_ms_current(void);
uint64_t get_timestamp_ms_by_diff(uint64_t diff);

#endif      // P7R_TIMING_H_

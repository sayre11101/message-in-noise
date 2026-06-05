#ifndef DSP_H
#define DSP_H

#include "adc_buf.h"
#include <stdint.h>

void init_dsp_filter(void);

// Processes a frame, updates the floor tracker, and returns the decoded bit.
// Pass NULL for the pointers if you don't need the raw math.
int process_dsp_frame(const int16_t *frame, int64_t *opt_cross_prod,
                      int64_t *opt_mag_sq);

#endif // DSP_H
#ifndef DSP_MIXER_H
#define DSP_MIXER_H

#include "adc_buf.h"

void dsp_mixer_init(void);

// Matches your exact old process_dsp_frame signature
int dsp_mixer_process(const int16_t *frame, int64_t *opt_cross_prod,
                      int64_t *opt_mag_sq);

#endif
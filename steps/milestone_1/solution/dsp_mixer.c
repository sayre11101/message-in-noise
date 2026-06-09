#include "dsp_mixer.h"
#include "adc_buf.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef LO_TABLE_SIZE
#define LO_TABLE_SIZE 131072
#endif

#ifndef FILTER_TAPS
#define FILTER_TAPS 65536
#endif

static int32_t fir_coef[FILTER_TAPS];

static int16_t lo_table_I[LO_TABLE_SIZE];
static int16_t lo_table_Q[LO_TABLE_SIZE];

static int16_t history_buffer_I[FILTER_TAPS];
static int16_t history_buffer_Q[FILTER_TAPS];

static int32_t prev_I = 0;
static int32_t prev_Q = 0;

static int head = 0;
static int lo_index = 0;

void dsp_mixer_init(void) {
  double fc = 3.0 / SAMPLE_RATE;

  for (int i = 0; i < FILTER_TAPS; i++) {
    int n = i - (FILTER_TAPS / 2);
    double h_n = 0.0;

    if (n == 0) {
      h_n = 2.0 * fc;
    } else {
      h_n = sin(2.0 * M_PI * fc * n) / (M_PI * n);
    }

    double blackman = 0.42 - 0.5 * cos(2.0 * M_PI * i / (FILTER_TAPS - 1)) +
                      0.08 * cos(4.0 * M_PI * i / (FILTER_TAPS - 1));
    h_n *= blackman;

    // The Golden State: Q30 format scaling
    fir_coef[i] = (int32_t)(h_n * (1 << 30));
  }

  for (int i = 0; i < LO_TABLE_SIZE; i++) {
    double phase = 2.0 * M_PI * NOMINAL_CARRIER_FREQ * i / SAMPLE_RATE;
    double lo_i = cos(phase);
    double lo_q = -sin(phase);

    lo_table_I[i] = (int16_t)(lo_i * 32767.0);
    lo_table_Q[i] = (int16_t)(lo_q * 32767.0);
  }

  memset(history_buffer_I, 0, sizeof(history_buffer_I));
  memset(history_buffer_Q, 0, sizeof(history_buffer_Q));
  prev_I = 0;
  prev_Q = 0;
  head = 0;
  lo_index = 0;
}

int dsp_mixer_process(const int16_t *adc_buf, int64_t *cp_out,
                      int64_t *ms_out) {
  int bit_val = 0;

  for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
    int32_t mixed_I = ((int32_t)adc_buf[i] * lo_table_I[lo_index]) >> 15;
    int32_t mixed_Q = ((int32_t)adc_buf[i] * lo_table_Q[lo_index]) >> 15;

    lo_index++;
    if (lo_index >= LO_TABLE_SIZE) {
      lo_index = 0;
    }

    history_buffer_I[head] = (int16_t)mixed_I;
    history_buffer_Q[head] = (int16_t)mixed_Q;

    // 64-bit Accumulator to prevent integer overflow
    int64_t sum_I = 0;
    int64_t sum_Q = 0;

    for (int tap = 0; tap < FILTER_TAPS; tap++) {
      // BITWISE AND MASK: Allows GCC to vectorize the loop, solving the
      // timeout!
      int idx = (head - tap + FILTER_TAPS) & (FILTER_TAPS - 1);

      sum_I += (int64_t)history_buffer_I[idx] * fir_coef[tap];
      sum_Q += (int64_t)history_buffer_Q[idx] * fir_coef[tap];
    }

    // Shift by 16 exactly ONCE at the end of the loop
    int32_t filtered_I = (int32_t)(sum_I >> 16);
    int32_t filtered_Q = (int32_t)(sum_Q >> 16);

    int64_t cp =
        ((int64_t)prev_I * filtered_Q) - ((int64_t)prev_Q * filtered_I);
    int64_t ms =
        ((int64_t)filtered_I * filtered_I) + ((int64_t)filtered_Q * filtered_Q);

    prev_I = filtered_I;
    prev_Q = filtered_Q;

    if (cp > 0) {
      bit_val = 1;
    } else {
      bit_val = 0;
    }

    *cp_out = cp;
    *ms_out = ms;

    head = (head + 1) & (FILTER_TAPS - 1);
  }

  return bit_val;
}
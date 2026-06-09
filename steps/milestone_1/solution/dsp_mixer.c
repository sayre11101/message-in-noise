#include "dsp_mixer.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FILTER_TAPS 65536
#define LO_TABLE_SIZE 32768

_Static_assert((FILTER_TAPS % SAMPLES_PER_FRAME) == 0,
               "FILTER_TAPS must be an exact multiple of SAMPLES_PER_FRAME!");
_Static_assert((LO_TABLE_SIZE % SAMPLES_PER_FRAME) == 0,
               "LO_TABLE_SIZE must be a multiple of SAMPLES_PER_FRAME!");

static int32_t fir_coef[FILTER_TAPS];
static int16_t lo_table_I[LO_TABLE_SIZE];
static int16_t lo_table_Q[LO_TABLE_SIZE];

static int16_t history_buffer_I[FILTER_TAPS];
static int16_t history_buffer_Q[FILTER_TAPS];

static int head = 0;
static int lo_index = 0;
static int64_t prev_I = 0;
static int64_t prev_Q = 0;

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

int dsp_mixer_process(const int16_t *frame, int64_t *opt_cross_prod,
                      int64_t *opt_mag_sq) {

  for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
    int16_t lo_i = lo_table_I[lo_index];
    int16_t lo_q = lo_table_Q[lo_index];

    history_buffer_I[head + i] = (int16_t)(((int32_t)frame[i] * lo_i) >> 15);
    history_buffer_Q[head + i] = (int16_t)(((int32_t)frame[i] * lo_q) >> 15);

    lo_index = (lo_index + 1) & (LO_TABLE_SIZE - 1);
  }

  head = (head + SAMPLES_PER_FRAME) % FILTER_TAPS;

  int64_t acc_I = 0;
  int64_t acc_Q = 0;

  int read_idx = head;
  for (int i = 0; i < FILTER_TAPS; i++) {
    acc_I += (int64_t)history_buffer_I[read_idx] * fir_coef[i];
    acc_Q += (int64_t)history_buffer_Q[read_idx] * fir_coef[i];
    read_idx = (read_idx + 1) % FILTER_TAPS;
  }

  acc_I >>= 16;
  acc_Q >>= 16;

  int64_t cross_prod = (prev_I * acc_Q) - (acc_I * prev_Q);
  int64_t mag_sq = (acc_I * acc_I) + (acc_Q * acc_Q);

  prev_I = acc_I;
  prev_Q = acc_Q;

  if (opt_cross_prod)
    *opt_cross_prod = cross_prod;
  if (opt_mag_sq)
    *opt_mag_sq = mag_sq;

  if (mag_sq < 10000LL) {
    return 1;
  }

  return (cross_prod > 0) ? 1 : 0;
}
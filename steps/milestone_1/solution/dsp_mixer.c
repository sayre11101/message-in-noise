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

static inline __attribute((always_inline)) int32_t to_q31(int64_t x) {
  return (int32_t)(x >> 31);
}

// GUARANTEE the block copy optimization and LO wrap are mathematically safe
_Static_assert((FILTER_TAPS % SAMPLES_PER_FRAME) == 0,
               "FILTER_TAPS must be an exact multiple of SAMPLES_PER_FRAME!");
_Static_assert((LO_TABLE_SIZE % SAMPLES_PER_FRAME) == 0,
               "LO_TABLE_SIZE must be a multiple of SAMPLES_PER_FRAME!");

static double fir_coef[FILTER_TAPS];

// Precalculated LO Look-Up Tables
static double lo_table_I[LO_TABLE_SIZE];
static double lo_table_Q[LO_TABLE_SIZE];

// TWO history buffers for the premixed Baseband I and Q streams
static double history_buffer_I[FILTER_TAPS];
static double history_buffer_Q[FILTER_TAPS];

// Array Heads and Pointers
static int head = 0;
static int lo_index = 0;

static int64_t prev_I = 0;
static int64_t prev_Q = 0;

// RESTORED NAME: dsp_mixer_init
void dsp_mixer_init(void) {
  // 1. Generate the pure Real Low-Pass Filter (Baseband)
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

    // Notice: No LO baked in! Just the pure Real low-pass filter.
    fir_coef[i] = h_n;
  }

  // 2. Generate the Exact Period Local Oscillator LUT
  for (int i = 0; i < LO_TABLE_SIZE; i++) {
    double phase = 2.0 * M_PI * NOMINAL_CARRIER_FREQ * i / SAMPLE_RATE;
    // Standard downconversion rotation: e^(-jwt) = cos(wt) - j*sin(wt)
    double lo_i = cos(phase);
    double lo_q = -sin(phase);

    lo_table_I[i] = lo_i;
    lo_table_Q[i] = lo_q;
  }

  memset(history_buffer_I, 0, sizeof(history_buffer_I));
  memset(history_buffer_Q, 0, sizeof(history_buffer_Q));
  prev_I = 0;
  prev_Q = 0;
  head = 0;
  lo_index = 0;
}

// RESTORED NAME: dsp_mixer_process
int dsp_mixer_process(const int16_t *frame, int64_t *opt_cross_prod,
                      int64_t *opt_mag_sq) {
  // --- 1. The Ultra-Fast Double Down Converter ---
  for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
    // Normalize ADC integer to [-1.0, 1.0] fractional space
    double frame_val = ldexp((double)(frame[i]), -31);

    // Look up the precalculated LO values
    history_buffer_I[head + i] = frame_val * lo_table_I[lo_index];
    history_buffer_Q[head + i] = frame_val * lo_table_Q[lo_index];

    // Advance the LO index and wrap flawlessly
    lo_index = (lo_index + 1) & (LO_TABLE_SIZE - 1);
  }

  // Advance the head pointer cleanly
  head = (head + SAMPLES_PER_FRAME) % FILTER_TAPS;

  // Use 'double' to safely accumulate the microscopic products!
  double acc_I = 0.0;
  double acc_Q = 0.0;

  // --- 2. The Baseband Low-Pass Filter ---
  int read_idx = head;
  for (int i = 0; i < FILTER_TAPS; i++) {
    // Both I and Q use the exact same real low-pass coefficients!
    acc_I += history_buffer_I[read_idx] * fir_coef[i];
    acc_Q += history_buffer_Q[read_idx] * fir_coef[i];
    read_idx = (read_idx + 1) % FILTER_TAPS;
  }

  // --- 3. Scale Back to Integer Space ---
  // Restore integer magnitude before taking MagSq/Cross-Product
  int64_t int_acc_I = (int64_t)ldexp(acc_I, 31);
  int64_t int_acc_Q = (int64_t)ldexp(acc_Q, 31);

  int64_t cross_prod = (prev_I * int_acc_Q) - (int_acc_I * prev_Q);
  int64_t mag_sq = (int_acc_I * int_acc_I) + (int_acc_Q * int_acc_Q);

  prev_I = int_acc_I;
  prev_Q = int_acc_Q;

  // Output raw math if pointers were provided
  if (opt_cross_prod)
    *opt_cross_prod = cross_prod;
  if (opt_mag_sq)
    *opt_mag_sq = mag_sq;

  // --- 4. Squelch ---
  if (mag_sq < 10000LL) {
    return 1; // Squelched, force UART to IDLE (Mark)
  }

  // --- 5. ZERO-CROSSING GLORY ---
  return (cross_prod > 0) ? 1 : 0;
}
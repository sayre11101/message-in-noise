#include "dsp.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FILTER_TAPS 65536
#define LO_TABLE_SIZE 32768

// GUARANTEE the block copy optimization and LO wrap are mathematically safe
_Static_assert((FILTER_TAPS % SAMPLES_PER_FRAME) == 0,
               "FILTER_TAPS must be an exact multiple of SAMPLES_PER_FRAME!");
_Static_assert((LO_TABLE_SIZE % SAMPLES_PER_FRAME) == 0,
               "LO_TABLE_SIZE must be a multiple of SAMPLES_PER_FRAME!");

// Only ONE set of pure Real FIR coefficients needed now!
static int32_t fir_coef[FILTER_TAPS];

// Precalculated LO Look-Up Tables
static int16_t lo_table_I[LO_TABLE_SIZE];
static int16_t lo_table_Q[LO_TABLE_SIZE];

// TWO history buffers for the premixed Baseband I and Q streams
static int16_t history_buffer_I[FILTER_TAPS];
static int16_t history_buffer_Q[FILTER_TAPS];

// Array Heads and Pointers
static int head = 0;
static int lo_index = 0;

static int64_t prev_I = 0;
static int64_t prev_Q = 0;

void init_dsp_filter(void) {
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
    fir_coef[i] = (int32_t)(h_n * (1 << 30));
  }

  // 2. Generate the Exact Period Local Oscillator LUT
  for (int i = 0; i < LO_TABLE_SIZE; i++) {
    double phase = 2.0 * M_PI * NOMINAL_CARRIER_FREQ * i / SAMPLE_RATE;
    // Standard downconversion rotation: e^(-jwt) = cos(wt) - j*sin(wt)
    double lo_i = cos(phase);
    double lo_q = -sin(phase);

    // Scale to full 16-bit signed integer range
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

int process_dsp_frame(const int16_t *frame, int64_t *opt_cross_prod,
                      int64_t *opt_mag_sq) {

  // --- 1. The Ultra-Fast Integer Down Converter ---
  for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
    // Look up the precalculated LO values
    int16_t lo_i = lo_table_I[lo_index];
    int16_t lo_q = lo_table_Q[lo_index];

    // Integer multiply and bit-shift back down to 16-bit to prevent growth
    history_buffer_I[head + i] = (int16_t)(((int32_t)frame[i] * lo_i) >> 15);
    history_buffer_Q[head + i] = (int16_t)(((int32_t)frame[i] * lo_q) >> 15);

    // Advance the LO index and wrap flawlessly at 32768
    // (Using a bitwise AND is faster than modulo for powers of 2!)
    lo_index = (lo_index + 1) & (LO_TABLE_SIZE - 1);
  }

  // Advance the head pointer cleanly
  head = (head + SAMPLES_PER_FRAME) % FILTER_TAPS;

  int64_t acc_I = 0;
  int64_t acc_Q = 0;

  // --- 2. The Baseband Low-Pass Filter ---
  int read_idx = head;
  for (int i = 0; i < FILTER_TAPS; i++) {
    // Both I and Q use the exact same real low-pass coefficients!
    acc_I += (int64_t)history_buffer_I[read_idx] * fir_coef[i];
    acc_Q += (int64_t)history_buffer_Q[read_idx] * fir_coef[i];
    read_idx = (read_idx + 1) % FILTER_TAPS;
  }

  // Prevent overflow before calculating magnitude/cross-product
  acc_I >>= 16;
  acc_Q >>= 16;

  int64_t cross_prod = (prev_I * acc_Q) - (acc_I * prev_Q);
  int64_t mag_sq = (acc_I * acc_I) + (acc_Q * acc_Q);

  prev_I = acc_I;
  prev_Q = acc_Q;

  // Output raw math if pointers were provided
  if (opt_cross_prod)
    *opt_cross_prod = cross_prod;
  if (opt_mag_sq)
    *opt_mag_sq = mag_sq;

  // --- 3. Squelch ---
  if (mag_sq < 10000LL) {
    return 1; // Squelched, force UART to IDLE (Mark)
  }

  // --- 4. ZERO-CROSSING GLORY ---
  // Because you physically premixed down to 0 Hz, there is no DC offset!
  return (cross_prod > 0) ? 1 : 0;
}
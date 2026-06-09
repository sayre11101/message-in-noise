#include "adc_buf.h"
#include "dsp_mixer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EVALUATION_SETTLE_TIME_SEC 5.0

// Helper function to format large integers with commas
void format_commas(int64_t n, char *out) {
  int c = 0;
  char temp[64];
  int sign = (n < 0) ? 1 : 0;
  if (n < 0)
    n = -n;
  if (n == 0) {
    strcpy(out, "0");
    return;
  }
  while (n > 0) {
    temp[c++] = (n % 10) + '0';
    n /= 10;
    if (n > 0 && c % 4 == 3) { // After 3 digits, insert a comma
      temp[c++] = ',';
    }
  }
  if (sign)
    temp[c++] = '-';

  // Reverse the string
  for (int i = 0; i < c; i++) {
    out[i] = temp[c - 1 - i];
  }
  out[c] = '\0';
}

// Helper function to format floats with commas
void format_double_commas(double n, char *out) {
  int64_t int_part = (int64_t)n;
  double frac_part = n - (double)int_part;
  if (frac_part < 0)
    frac_part = -frac_part;

  char int_str[64];
  format_commas(int_part, int_str);
  sprintf(out, "%s.%02d", int_str, (int)(frac_part * 100.0));
}

static double generate_gaussian_noise(double stddev) {
  if (stddev <= 0.0)
    return 0.0;
  double u1 = ((double)rand() / RAND_MAX);
  double u2 = ((double)rand() / RAND_MAX);
  if (u1 == 0.0)
    u1 = 1e-10;
  return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2) * stddev;
}

int main(int argc, char *argv[]) {
  srand(time(NULL));

  double target_amp = 500.0;
  double int_amp = 0.0;
  double noise_stddev = 0.0;
  double int_freq_offset = 15.0;

  if (argc > 1)
    target_amp = atof(argv[1]);
  if (argc > 2)
    int_amp = atof(argv[2]);
  if (argc > 3)
    noise_stddev = atof(argv[3]);
  if (argc > 4)
    int_freq_offset = atof(argv[4]);

  printf("\n[INFO] Evaluating DSP (Target Amp: %.1f, Int Amp: %.1f, Noise "
         "StdDev: %.1f, Int Offset: %.1fHz)...\n",
         target_amp, int_amp, noise_stddev, int_freq_offset);

  dsp_mixer_init();
  adc_buf_t frame;
  int bit_val = -1;
  int64_t cp = 0, ms = 0;

  char str_ms[64];
  char str_amp[64];

  double current_phase_sig = 0.0;
  double current_phase_int = 0.0;
  double dt = 1.0 / SAMPLE_RATE;

  double mark_freq = NOMINAL_CARRIER_FREQ + FSK_PEAK_DEVIATION;
  double space_freq = NOMINAL_CARRIER_FREQ - FSK_PEAK_DEVIATION;
  double int_freq = NOMINAL_CARRIER_FREQ + int_freq_offset;

  int frames_to_wait =
      (int)(EVALUATION_SETTLE_TIME_SEC * SAMPLE_RATE / SAMPLES_PER_FRAME);

  // ==============================================================================
  // BRANCH A: SQUELCH MEASUREMENT MODE (No Target Signal)
  // ==============================================================================
  if (target_amp == 0.0) {
    for (int f = 0; f < frames_to_wait; f++) {
      for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        current_phase_int += 2.0 * M_PI * int_freq * dt;
        if (current_phase_int > 2.0 * M_PI)
          current_phase_int -= 2.0 * M_PI;

        double sample = (int_amp * sin(current_phase_int)) +
                        generate_gaussian_noise(noise_stddev);

        if (sample > 32767.0)
          sample = 32767.0;
        if (sample < -32768.0)
          sample = -32768.0;
        frame.adc_buf[i] = (int16_t)sample;
      }
      bit_val = dsp_mixer_process(frame.adc_buf, &cp, &ms);
    }

    double actual_amplitude = sqrt((double)ms);
    format_commas(ms, str_ms);
    format_double_commas(actual_amplitude, str_amp);

    printf("[MEASUREMENT] Final MagSq: %s\n", str_ms);
    printf("[MEASUREMENT] Actual Amplitude: %s\n", str_amp);
    printf("PASS: Measurement complete.\n");
    return 0;
  }

  // ==============================================================================
  // BRANCH B: SIGNAL ISOLATION MODE (Normal Mark/Space Test)
  // ==============================================================================

  // --- Phase 1: MARK TONE ---
  for (int f = 0; f < frames_to_wait; f++) {
    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
      current_phase_sig += 2.0 * M_PI * mark_freq * dt;
      current_phase_int += 2.0 * M_PI * int_freq * dt;

      if (current_phase_sig > 2.0 * M_PI)
        current_phase_sig -= 2.0 * M_PI;
      if (current_phase_int > 2.0 * M_PI)
        current_phase_int -= 2.0 * M_PI;

      double sample = (target_amp * sin(current_phase_sig)) +
                      (int_amp * sin(current_phase_int)) +
                      generate_gaussian_noise(noise_stddev);

      if (sample > 32767.0)
        sample = 32767.0;
      if (sample < -32768.0)
        sample = -32768.0;
      frame.adc_buf[i] = (int16_t)sample;
    }
    bit_val = dsp_mixer_process(frame.adc_buf, &cp, &ms);
  }

  int expected_mark_bit = bit_val;
  double actual_amplitude_mark = sqrt((double)ms);
  format_commas(ms, str_ms);
  format_double_commas(actual_amplitude_mark, str_amp);

  printf("[INFO] Mark Tone Settled. Discriminator Natural Polarity: %d\n",
         expected_mark_bit);
  printf("[MEASUREMENT] Mark Tone MagSq: %s\n", str_ms);
  printf("[MEASUREMENT] Mark Tone Amplitude: %s\n", str_amp);

  // --- Phase 2: SPACE TONE ---
  for (int f = 0; f < frames_to_wait; f++) {
    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
      current_phase_sig += 2.0 * M_PI * space_freq * dt;
      current_phase_int += 2.0 * M_PI * int_freq * dt;

      if (current_phase_sig > 2.0 * M_PI)
        current_phase_sig -= 2.0 * M_PI;
      if (current_phase_int > 2.0 * M_PI)
        current_phase_int -= 2.0 * M_PI;

      double sample = (target_amp * sin(current_phase_sig)) +
                      (int_amp * sin(current_phase_int)) +
                      generate_gaussian_noise(noise_stddev);

      if (sample > 32767.0)
        sample = 32767.0;
      if (sample < -32768.0)
        sample = -32768.0;
      frame.adc_buf[i] = (int16_t)sample;
    }
    bit_val = dsp_mixer_process(frame.adc_buf, &cp, &ms);
  }

  double actual_amplitude_space = sqrt((double)ms);
  format_commas(ms, str_ms);
  format_double_commas(actual_amplitude_space, str_amp);

  printf("[MEASUREMENT] Space Tone MagSq: %s\n", str_ms);
  printf("[MEASUREMENT] Space Tone Amplitude: %s\n", str_amp);

  if (bit_val == expected_mark_bit) {
    printf("FAIL: Filter failed to distinguish Space tone from Mark tone. "
           "Output remained %d.\n",
           bit_val);
    return 1;
  }

  printf("PASS: FIR Low-Pass Filter successfully isolated baseband signals.\n");
  return 0;
}
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
    if (n > 0 && c % 4 == 3)
      temp[c++] = ',';
  }
  if (sign)
    temp[c++] = '-';
  for (int i = 0; i < c; i++)
    out[i] = temp[c - 1 - i];
  out[c] = '\0';
}

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
  int mode = 0;
  double sig_amp = 0.0, sig_offset = 0.0, int_amp = 0.0, int_offset = 0.0,
         noise_stddev = 0.0;

  if (argc > 1)
    mode = atoi(argv[1]);
  if (argc > 2)
    sig_amp = atof(argv[2]);
  if (argc > 3)
    sig_offset = atof(argv[3]);
  if (argc > 4)
    int_amp = atof(argv[4]);
  if (argc > 5)
    int_offset = atof(argv[5]);
  if (argc > 6)
    noise_stddev = atof(argv[6]);

  adc_buf_t frame;
  int bit_val = -1;
  int64_t cp = 0, ms = 0;
  char str_ms[64], str_cp[64], str_amp[64];
  double current_phase_sig = 0.0, current_phase_int = 0.0,
         dt = 1.0 / SAMPLE_RATE;
  int frames_to_wait =
      (int)(EVALUATION_SETTLE_TIME_SEC * SAMPLE_RATE / SAMPLES_PER_FRAME);

  if (mode == 0) {
    dsp_mixer_init();
    printf("\n[INFO] MEASUREMENT MODE (Sig Amp: %.1f @ %.1fHz, Int Amp: %.1f @ "
           "%.1fHz, Noise: %.1f)\n",
           sig_amp, sig_offset, int_amp, int_offset, noise_stddev);
    double sig_freq = NOMINAL_CARRIER_FREQ + sig_offset;
    double int_freq = NOMINAL_CARRIER_FREQ + int_offset;
    for (int f = 0; f < frames_to_wait; f++) {
      for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        current_phase_sig += 2.0 * M_PI * sig_freq * dt;
        current_phase_int += 2.0 * M_PI * int_freq * dt;
        if (current_phase_sig > 2.0 * M_PI)
          current_phase_sig -= 2.0 * M_PI;
        if (current_phase_int > 2.0 * M_PI)
          current_phase_int -= 2.0 * M_PI;
        double sample = (sig_amp * sin(current_phase_sig)) +
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
    double actual_amplitude = sqrt((double)ms);
    format_commas(ms, str_ms);
    format_double_commas(actual_amplitude, str_amp);
    printf("[MEASUREMENT] Final MagSq: %s\n", str_ms);
    printf("[MEASUREMENT] Actual Amplitude: %s\n", str_amp);
    printf("PASS: Measurement complete.\n");
    return 0;
  }

  if (mode == 1) {
    dsp_mixer_init();
    printf("\n[INFO] ISOLATION MODE (Sig Amp: %.1f @ +/-%.1fHz, Int Amp: %.1f "
           "@ %.1fHz, Noise: %.1f)\n",
           sig_amp, sig_offset, int_amp, int_offset, noise_stddev);
    double mark_freq = NOMINAL_CARRIER_FREQ + sig_offset;
    double space_freq = NOMINAL_CARRIER_FREQ - sig_offset;
    double int_freq = NOMINAL_CARRIER_FREQ + int_offset;

    for (int f = 0; f < frames_to_wait; f++) {
      for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        current_phase_sig += 2.0 * M_PI * mark_freq * dt;
        current_phase_int += 2.0 * M_PI * int_freq * dt;
        if (current_phase_sig > 2.0 * M_PI)
          current_phase_sig -= 2.0 * M_PI;
        if (current_phase_int > 2.0 * M_PI)
          current_phase_int -= 2.0 * M_PI;
        double sample = (sig_amp * sin(current_phase_sig)) +
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

    for (int f = 0; f < frames_to_wait; f++) {
      for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        current_phase_sig += 2.0 * M_PI * space_freq * dt;
        current_phase_int += 2.0 * M_PI * int_freq * dt;
        if (current_phase_sig > 2.0 * M_PI)
          current_phase_sig -= 2.0 * M_PI;
        if (current_phase_int > 2.0 * M_PI)
          current_phase_int -= 2.0 * M_PI;
        double sample = (sig_amp * sin(current_phase_sig)) +
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
      printf("FAIL: Filter failed to distinguish Space tone from Mark tone.\n");
      return 1;
    }
    printf(
        "PASS: FIR Low-Pass Filter successfully isolated baseband signals.\n");
    return 0;
  }

  // --- THE SWEEP TABLE GENERATOR ---
  if (mode == 2) {
    printf("\n| Input Offset (Hz) | %18s | %18s | Measured Delta f (Hz) |\n",
           "MagSq (Power)", "Cross Product");
    printf("|-------------------|--------------------|--------------------|----"
           "-------------------|\n");

    for (int step = -10; step <= 10; step++) {
      double current_offset = (double)step;
      double sig_freq = NOMINAL_CARRIER_FREQ + current_offset;

      dsp_mixer_init(); // Flush state
      current_phase_sig = 0.0;

      for (int f = 0; f < frames_to_wait; f++) {
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
          current_phase_sig += 2.0 * M_PI * sig_freq * dt;
          if (current_phase_sig > 2.0 * M_PI)
            current_phase_sig -= 2.0 * M_PI;
          double sample = sig_amp * sin(current_phase_sig);
          if (sample > 32767.0)
            sample = 32767.0;
          if (sample < -32768.0)
            sample = -32768.0;
          frame.adc_buf[i] = (int16_t)sample;
        }
        bit_val = dsp_mixer_process(frame.adc_buf, &cp, &ms);
      }

      format_commas(ms, str_ms);
      format_commas(cp, str_cp);

      // Only calculate Delta f if power is strong enough to not be garbage
      if (ms > 1000000000LL) {
        double ratio = (ms != 0) ? ((double)cp / (double)ms) : 0.0;
        double baseband_fs =
            64.0; // Simulated decimation math to make Hz correct
        double measured_df = ratio * (baseband_fs / (2.0 * M_PI));
        printf("| %17.1f | %18s | %18s | %21.4f |\n", current_offset, str_ms,
               str_cp, measured_df);
      } else {
        printf("| %17.1f | %18s | %18s | %21s |\n", current_offset, str_ms,
               str_cp, "---");
      }
    }
    printf("PASS: Frequency sweep completed successfully.\n");
    return 0;
  }
  return 0;
}
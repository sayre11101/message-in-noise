#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>

#include "adc_buf.h"
#include "dsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DECIMATED_SAMPLE_RATE (SAMPLE_RATE / SAMPLES_PER_FRAME)
#define FRAMES_PER_BIT ((int)(DECIMATED_SAMPLE_RATE / NOMINAL_BAUD_RATE))

int main(void) {
  printf("Initializing DSP Unit Test (Fractional Q14 Math)...\n");
  init_dsp_filter();

  FILE *dump_file = fopen("fsk_test.csv", "w");
  if (dump_file) {
    fprintf(
        dump_file,
        "Frame,Time_s,Input_Freq_Hz,Mag_Sq,Cross_Prod,Est_Freq_Hz,Bit_Val\n");
  }

  double phase = 0.0;
  int16_t test_frame[SAMPLES_PER_FRAME];

  double amplitude = 500.0;
  int frames_to_simulate = 10 * FRAMES_PER_BIT;

  for (int frame = 0; frame < frames_to_simulate; frame++) {
    int current_bit = (frame / FRAMES_PER_BIT) % 2;
    double current_offset = current_bit ? FSK_DEVIATION : -FSK_DEVIATION;
    double current_freq = NOMINAL_CARRIER_FREQ + current_offset;

    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
      phase += 2.0 * M_PI * current_freq / SAMPLE_RATE;
      if (phase > 2.0 * M_PI)
        phase -= 2.0 * M_PI;
      test_frame[i] = (int16_t)(amplitude * sin(phase));
    }

    int64_t cross_prod = 0;
    int64_t mag_sq = 0;

    // Pass the pointers to capture the raw math, and capture the bit!
    int bit_val = process_dsp_frame(test_frame, &cross_prod, &mag_sq);

    double est_freq = 0.0;
    if (mag_sq > 0) {
      est_freq = -((double)cross_prod / (double)mag_sq) *
                 (DECIMATED_SAMPLE_RATE / (2.0 * M_PI));
    }

    if (dump_file) {
      fprintf(dump_file, "%d,%.4f,%.2f,%lld,%lld,%.3f,%d\n", frame,
              (double)frame / DECIMATED_SAMPLE_RATE, current_offset,
              (long long)mag_sq, (long long)cross_prod, est_freq, bit_val);
    }
  }

  if (dump_file)
    fclose(dump_file);
  printf("Test complete. Check fsk_test.csv\n");
  exit(0);
}
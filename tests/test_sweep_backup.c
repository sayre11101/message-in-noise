#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "adc_buf.h"
#include "dsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DECIMATION_FACTOR SAMPLES_PER_FRAME
#define DECIMATED_SAMPLE_RATE (SAMPLE_RATE / DECIMATION_FACTOR)

int main(void) {
  printf("Initializing DSP Unit Test (Frequency Sweep)...\n");
  init_dsp_filter();

  FILE *dump_file = fopen("filter_sweep.csv", "w");
  if (dump_file) {
    fprintf(dump_file, "Offset_Hz,I_Out,Q_Out,Magnitude\n");
  } else {
    printf("Failed to open CSV for writing!\n");
  }

  // Sweep Parameters
  double start_offset = 15.0; // Start at +15 Hz above carrier
  double end_offset = -15.0;  // End at -15 Hz below carrier
  int total_simulated_seconds = 120;
  int total_frames = total_simulated_seconds * DECIMATED_SAMPLE_RATE;

  double phase = 0.0;
  int16_t test_frame[SAMPLES_PER_FRAME];

  for (int frame = 0; frame < total_frames; frame++) {
    // Calculate the instantaneous frequency for this frame
    double progress = (double)frame / total_frames;
    double current_offset =
        start_offset + (end_offset - start_offset) * progress;
    double current_freq = NOMINAL_CARRIER_FREQ + current_offset;

    // Generate 1024 phase-continuous ADC samples
    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
      phase += 2.0 * M_PI * current_freq / SAMPLE_RATE;
      if (phase > 2.0 * M_PI)
        phase -= 2.0 * M_PI;

      // Clean 500-amplitude sine wave
      test_frame[i] = (int16_t)(500.0 * sin(phase));
    }

    // Run the filter
    process_dsp_frame(test_frame);

    // Calculate Magnitude from the exported I/Q debug variables
    double i_val = (double)debug_I;
    double q_val = (double)debug_Q;
    double magnitude = sqrt((i_val * i_val) + (q_val * q_val));

    // Log to CSV
    if (dump_file) {
      fprintf(dump_file, "%.3f,%lld,%lld,%.2f\n", current_offset,
              (long long)debug_I, (long long)debug_Q, magnitude);
    }

    if (frame % (int)DECIMATED_SAMPLE_RATE == 0) {
      printf("[UNIT TEST] Sweeping... Current Offset: %.2f Hz\n",
             current_offset);
    }
  }

  if (dump_file)
    fclose(dump_file);
  printf("Sweep complete. Data written to filter_sweep.csv\n");

  // Kill Zephyr gracefully
  exit(0);
}
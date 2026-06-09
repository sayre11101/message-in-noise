#include "adc_buf.h"
#include "fsk_slicer.h"
#include <stdio.h>
#include <string.h>

int main() {
  printf("\n[INFO] Starting Milestone 2 UART Slicer Evaluation...\n");
  fsk_slicer_init();

  const char *test_message = "DSP";
  int msg_len = strlen(test_message);

  char decoded_buffer[16] = {0};
  int dec_idx = 0;

  // Exact frames-per-bit math from your original main.c
  int decimated_sample_rate = SAMPLE_RATE / SAMPLES_PER_FRAME;
  int frames_per_bit = decimated_sample_rate / NOMINAL_BAUD_RATE;

  char decoded_char;

  for (int i = 0; i < msg_len; i++) {
    char target_char = test_message[i];

    // 1. Send START BIT (0)
    for (int f = 0; f < frames_per_bit; f++) {
      if (fsk_slicer_process(0, &decoded_char))
        decoded_buffer[dec_idx++] = decoded_char;
    }

    // 2. Send 8 DATA BITS (LSB First)
    for (int b = 0; b < 8; b++) {
      int bit_val = (target_char >> b) & 0x01;
      for (int f = 0; f < frames_per_bit; f++) {
        if (fsk_slicer_process(bit_val, &decoded_char))
          decoded_buffer[dec_idx++] = decoded_char;
      }
    }

    // 3. Send STOP BIT (1)
    for (int f = 0; f < frames_per_bit; f++) {
      if (fsk_slicer_process(1, &decoded_char))
        decoded_buffer[dec_idx++] = decoded_char;
    }
  }

  if (strcmp(decoded_buffer, "DSP") == 0) {
    printf("PASS: Successfully decoded string '%s'\n", decoded_buffer);
    return 0;
  } else {
    printf("FAIL: Expected 'DSP', but got '%s'\n", decoded_buffer);
    return 1;
  }
}
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "adc_buf.h"
#include "dsp_mixer.h"
#include "fsk_slicer.h"

#define DECIMATED_SAMPLE_RATE (SAMPLE_RATE / SAMPLES_PER_FRAME)

static adc_buf_t current_frame;
static uint8_t packet_buffer[PACKET_SIZE];
static int packet_index = 0;

int main(void) {
  printf("Initializing Oracle Integer DSP Pipeline...\n");

  dsp_mixer_init();
  fsk_slicer_init();

  int frame_count = 0;
  int simulated_seconds = 0;

  while (1) {
    fill_adc_buf(&current_frame);

    frame_count++;
    simulated_seconds = frame_count / (int)DECIMATED_SAMPLE_RATE;

    if (frame_count % (int)DECIMATED_SAMPLE_RATE == 0) {
      printf("[ORACLE] Processed %d simulated seconds...\n", simulated_seconds);
      fflush(stdout);
    }

    // --- The Clean DSP Call ---
    int64_t cross_product;
    int bit_val =
        dsp_mixer_process(current_frame.adc_buf, &cross_product, NULL);

    if (frame_count % 8 == 0) {
      printf("[ORACLE] Subsecond cross product %lld and bit %d\n",
             (long long)cross_product, bit_val);
      fflush(stdout);
    }

    if (simulated_seconds <
        ((int)(NOMINAL_BAUD_RATE * INITIAL_IDLE_TIME * 0.9))) {
      continue;
    }

    // --- Extracted UART Framing ---
    char new_char;
    if (fsk_slicer_process(bit_val, &new_char)) {

      printf("[ORACLE] Got the stop bit for current char %c, index %d "
             "out of %d\n",
             new_char, packet_index, PACKET_SIZE);

      packet_buffer[packet_index++] = new_char;

      if (packet_index == PACKET_SIZE) {
        printf("[ORACLE] Got a whole packet\n");
        uint8_t sum = 0;
        for (int i = 0; i < PAYLOAD_SIZE; i++) {
          sum += packet_buffer[i];
        }

        const char *hex = "0123456789ABCDEF";
        char expected_upper = hex[(sum >> 4) & 0x0F];
        char expected_lower = hex[sum & 0x0F];

        if (packet_buffer[PAYLOAD_SIZE] == expected_upper &&
            packet_buffer[PAYLOAD_SIZE + 1] == expected_lower) {
          packet_buffer[PAYLOAD_SIZE] = '\0';
          printf("DECODED: %s\n", packet_buffer);
          return 0;
        } else {
          printf("[ORACLE] Checksum mismatch! Retrying...\n");
        }
        packet_index = 0;
      }
    }
  }
}
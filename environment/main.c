#include "adc_buf.h"
#include "dsp_mixer.h"
#include "fsk_slicer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

// Define the maximum number of simulation frames to prevent infinite loops
#define NUMBER_OF_ADC_FRAMES 25000

static adc_buf_t current_frame;

int main(void) {
  /*
   * TODO: 1. Initialize your DSP and FSK modules
   * dsp_mixer_init();
   * fsk_slicer_init();
   *
   * Hint: You will need state variables to track packet buffering
   * char packet_buffer[PACKET_SIZE + 1];
   * int packet_index = 0;
   */

  for (int i = 0; i < NUMBER_OF_ADC_FRAMES; i++) {
    // Fetch a block of raw ADC samples containing the FSK signal,
    // the heavy AWGN, and the +10dB hopping interferer.
    fill_adc_buf(&current_frame);

    /*
     * TODO: 2. Pass the buffer to your DSP mixer to filter the interference
     * int64_t cross_product;
     * int bit_val = dsp_mixer_process(current_frame.adc_buf, &cross_product,
     * NULL);
     *
     * TODO: 3. Pass the sliced bit to your FSK slicer
     * char new_char;
     * if (fsk_slicer_process(bit_val, &new_char)) {
     * * TODO: 4. Buffer the received bytes into the packet structure.
     * Hint: Watch out for the continuous "Mark" idle periods!
     * * TODO: 5. Once a full PACKET_SIZE is received, validate the checksum.
     * Calculate the modulo-256 sum of the payload bytes and compare
     * it against the 2-byte ASCII Hex checksum at the end of the packet.
     * * If the checksum is valid, print the payload and exit!
     * printf("DECODED: %s\n", packet_buffer);
     * break;
     * }
     */
  }

  return 0;
}
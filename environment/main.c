#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "adc_buf.h"

// Note to Agent:
// You are tasked with decoding a weak FSK signal buried in heavy broadband
// noise. There is also a massive interfering carrier (+10dB above our signal)
// sitting at an unknown offset from the nominal 21.5 kHz carrier.
//
// Floating point arithmetic (32-bit float) may lack the precision necessary to
// accumulate large FIR filters without catastrophic cancellation due to the
// noise and interferer. You are strongly encouraged to use fixed-point integer
// math (e.g., int64_t) for your filtering and mixing stages.
//
// The transmitter loops a 16-byte packet:
// - Bytes 0-13: The payload (null-padded).
// - Bytes 14-15: A 2-character ASCII Hex checksum (modulo-256 sum of bytes
// 0-13).
//
// When you successfully decode and validate a packet via its checksum,
// print it strictly as: printf("DECODED: %s\n", payload);

static adc_buf_t current_frame;

int main(void) {
  printf("Starting Sub-Noise FSK Demodulator...\n");

  while (1) {
    // Fetch 8ms of raw 16-bit RF data (128 kHz sample rate)
    fill_adc_buf(&current_frame);

    // ==============================================================================
    // TODO: Implement your Digital Down Converter (DDC) and FSK Demodulator
    // here.
    //
    // 1. Shift the 21.5 kHz carrier to baseband (Mixer).
    // 2. Filter out the noise and the interfering carrier.
    // 3. Determine if the remaining baseband energy is positive (Mark) or
    // negative (Space).
    // 4. Recover the 1-baud 8N1 bitstream.
    // 5. Framer: Extract the 16-byte packet, validate the checksum, and print.
    // ==============================================================================
  }

  return 0;
}
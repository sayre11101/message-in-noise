#include "adc_buf.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Timing & Loop Sequence ---
#define MAX_LOOPS 5
#define INITIAL_IDLE_TIME 5.0 // Seconds of continuous Mark before first packet
#define INTER_PACKET_IDLE_TIME 11.0 // Seconds of continuous Mark between loops

// --- UART Framing (8N1) ---
typedef enum {
  BIT_START = 0,
  // Data bits are indices 1 through 8
  BIT_STOP = 9,
  BITS_PER_BYTE = 10
} uart_frame_bit_t;

/**
 * @brief This code will fill the adc buffer for each frame.
 * @note It is not necessary to generate this code.
 * @note It will be supplied by the verifier
 * @note This is just for testing the build with CMakeLists.txt
 *
 * @param buf
 */
void fill_adc_buf(adc_buf_t *buf) {
  // This file is provided just to test that the code builds
  // The verifier will supply the real code during test
  // Not recommended to spend time writing this code
}
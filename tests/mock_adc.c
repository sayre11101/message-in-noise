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

// --- Word Bank (Max 13 characters to ensure null-termination) ---
#define NUM_WORDS 5
static const char *WORD_BANK[NUM_WORDS] = {"DEEP", "WAVE", "SYNC", "BAUD",
                                           "NULL"};

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

// --- Signal Amplitudes (For 16-bit signed integer output) ---
// Signal: 500, Interferer: 1581 (+10dB), Noise RMS: 8944 (20dB SNR in 1Hz BW)
static const double AMP_SIG = 500.0;
static const double AMP_INT = 1581.0;
static const double NOISE_STDDEV = 8944.0;

// --- Internal State Variables ---
static double global_time = 0.0;
static double current_phase_sig = 0.0;
static double current_phase_int = 0.0;

static const char *secret_word = NULL;
static char tx_buffer[PACKET_SIZE];
static double true_carrier_freq = 0.0;
static double true_interferer_freq = 0.0;
static int initialized = 0;

// Box-Muller transform for AWGN
static double generate_gaussian_noise(double stddev) {
  double u1 = ((double)rand() / RAND_MAX);
  double u2 = ((double)rand() / RAND_MAX);
  if (u1 == 0.0)
    u1 = 1e-10; // Prevent log(0)

  double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
  return z0 * stddev;
}

void fill_adc_buf(adc_buf_t *buf) {
  if (!initialized) {
    srand(time(NULL));
    secret_word = WORD_BANK[rand() % NUM_WORDS];

    // 1. Prepare the buffer (Ensure zero-padded)
    memset(tx_buffer, 0, PACKET_SIZE);
    strncpy(tx_buffer, secret_word, PAYLOAD_SIZE - 1); // Leaves index 13 as \0

    // 2. Calculate Checksum (Modulo-256 sum of bytes 0 to PAYLOAD_SIZE-1)
    uint8_t checksum = 0;
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
      checksum += tx_buffer[i];
    }

    // 3. Append ASCII Hex Checksum to the last 2 bytes
    const char *hex_chars = "0123456789ABCDEF";
    tx_buffer[PAYLOAD_SIZE] = hex_chars[(checksum >> 4) & 0x0F];
    tx_buffer[PAYLOAD_SIZE + 1] = hex_chars[checksum & 0x0F];

    // 4. Carrier: Nominal +/- 10 ppm (+/- 0.215 Hz at 21.5 kHz)
    double ppm_tolerance = NOMINAL_CARRIER_FREQ * 10.0e-6;
    double ppm_error =
        ((double)rand() / RAND_MAX) * (2.0 * ppm_tolerance) - ppm_tolerance;
    true_carrier_freq = NOMINAL_CARRIER_FREQ + ppm_error;

    // 5. Interferer: +/- 10 Hz away from the actual carrier
    int sign = (rand() % 2 == 0) ? 1 : -1;
    true_interferer_freq = true_carrier_freq + (sign * 10.0);

    printf("[MOCK_ENV] SECRET_WORD: %s\n", secret_word);
    printf("[MOCK_ENV] CARRIER_FREQ: %.3f Hz\n", true_carrier_freq);
    printf("[MOCK_ENV] INTERFERER_FREQ: %.3f Hz\n", true_interferer_freq);

    initialized = 1;
  }

  int16_t *buffer = buf->adc_buf;
  double dt = 1.0 / SAMPLE_RATE;

  // Calculate dynamic durations
  double packet_tx_time = (PACKET_SIZE * BITS_PER_BYTE) / BAUD_RATE;
  double loop_duration = packet_tx_time + INTER_PACKET_IDLE_TIME;
  double total_tx_window = MAX_LOOPS * loop_duration;

  for (size_t i = 0; i < SAMPLES_PER_FRAME; i++) {
    global_time += dt;
    double current_freq = true_carrier_freq;

    // Timeline check
    if (global_time >= INITIAL_IDLE_TIME &&
        global_time < (INITIAL_IDLE_TIME + total_tx_window)) {
      double active_time = fmod(global_time - INITIAL_IDLE_TIME, loop_duration);

      if (active_time < packet_tx_time) {
        // Packet Transmission Window
        int total_bits = (int)(active_time / BAUD_RATE);
        int byte_index = total_bits / BITS_PER_BYTE;
        int bit_index = total_bits % BITS_PER_BYTE;

        char target_char = tx_buffer[byte_index];
        int bit_val = 1; // Mark (Idle/Default)

        if (bit_index == BIT_START) {
          bit_val = 0; // Start Bit
        } else if (bit_index > BIT_START && bit_index < BIT_STOP) {
          bit_val =
              (target_char >> (bit_index - 1)) & 0x01; // Data Bits (LSB first)
        } else if (bit_index == BIT_STOP) {
          bit_val = 1; // Stop Bit
        }

        current_freq += (bit_val == 1) ? FSK_DEVIATION : -FSK_DEVIATION;
      } else {
        // Inter-packet gap (Continuous Mark)
        current_freq += FSK_DEVIATION;
      }
    } else {
      // Initial startup gap OR dead-air after max loops (Continuous Mark)
      current_freq += FSK_DEVIATION;
    }

    // Integrate phases
    current_phase_sig += 2.0 * M_PI * current_freq * dt;
    current_phase_int += 2.0 * M_PI * true_interferer_freq * dt;

    // Keep phases bounded
    if (current_phase_sig > 2.0 * M_PI)
      current_phase_sig -= 2.0 * M_PI;
    if (current_phase_int > 2.0 * M_PI)
      current_phase_int -= 2.0 * M_PI;

    // Mix Signal + Interferer + AWGN
    double sample = (AMP_SIG * sin(current_phase_sig)) +
                    (AMP_INT * sin(current_phase_int)) +
                    generate_gaussian_noise(NOISE_STDDEV);

    // Clip to int16_t limits to prevent overflow wrap-around
    if (sample > 32767.0)
      sample = 32767.0;
    if (sample < -32768.0)
      sample = -32768.0;

    buffer[i] = (int16_t)sample;
  }
}
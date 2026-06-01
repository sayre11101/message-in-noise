#ifndef ADC_BUF_H
#define ADC_BUF_H

#include <stdint.h>

// --- RF & Modulation Specifications ---
// --- RF & Modulation Specifications ---
#define SAMPLE_RATE 128000.0 // ADC Sampling Frequency in Hz

#define NOMINAL_CARRIER_FREQ 21500.0 // Target Carrier Frequency in Hz
#define CARRIER_TOLERANCE_PPM 10.0   // Maximum crystal drift (+/- 10 ppm)

#define NOMINAL_BAUD_RATE 1.0      // Symbols per second (8N1 Framing)
#define BAUD_TOLERANCE_PERCENT 2.0 // Maximum baud rate variation (+/- 2%)

#define FSK_DEVIATION 0.50 // +/- 0.50 Hz shift for Mark/Space
#define FSK_PEAK_DEVIATION                                                     \
  FSK_DEVIATION // Peak deviation from center (+/- 0.50 Hz)
#define FSK_TOTAL_SHIFT                                                        \
  (FSK_DEVIATION * 2.0) // Total peak-to-peak frequency shift (1.0 Hz)

// --- Protocol Specifications ---
#define PAYLOAD_SIZE 14
#define CHECKSUM_SIZE 2
#define PACKET_SIZE (PAYLOAD_SIZE + CHECKSUM_SIZE)

// The 16-byte over-the-air packet structure
typedef struct __attribute__((packed)) {
  char payload[PAYLOAD_SIZE];   // Null-padded secret message
  char checksum[CHECKSUM_SIZE]; // ASCII Hex representation of modulo-256 sum of
                                // payload
} fsk_packet_t;

// --- Buffer Specifications ---
#define SAMPLES_PER_FRAME 1024 // Number of 16-bit samples per ADC fetch

// The chunk of RF data returned by the frontend
typedef struct {
  int16_t adc_buf[SAMPLES_PER_FRAME];
} adc_buf_t;

/**
 * @brief Fetches the next frame of ADC data from the simulated RF frontend.
 * * The RF environment contains:
 * - The FSK signal (~20dB SNR in a 1Hz band).
 * - An interfering carrier (+10dB stronger than the signal) at an unknown
 * frequency offset.
 * - Broadband white noise.
 * * @param buf Pointer to the buffer structure to fill.
 */
void fill_adc_buf(adc_buf_t *buf);

#endif // ADC_BUF_H
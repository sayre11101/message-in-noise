#ifndef ADC_BUF_H
#define ADC_BUF_H

#include <stdint.h>

// --- Physical Constants ---
#define SAMPLE_RATE 16384.0         // Hz
#define NOMINAL_CARRIER_FREQ 1200.0 // Hz
#define CARRIER_TOLERANCE_PPM 50.0  // ppm
#define FSK_PEAK_DEVIATION 50.0     // +/- Hz from carrier
#define NOMINAL_BAUD_RATE 100.0     // Baud (bits per second)
#define BAUD_TOLERANCE_PERCENT 2.0  // +/- 2% drift

// --- Hardware Buffer ---
#define SAMPLES_PER_FRAME 256 // Process data in chunks

typedef struct {
  int16_t adc_buf[SAMPLES_PER_FRAME]; // 16-bit signed integer ADC samples
} adc_buf_t;

// --- Packet Structure ---
#define PAYLOAD_SIZE 14
#define CHECKSUM_SIZE 2
#define PACKET_SIZE (PAYLOAD_SIZE + CHECKSUM_SIZE) // 16 bytes total

typedef struct {
  char payload[PAYLOAD_SIZE];
  char checksum[CHECKSUM_SIZE];
} fsk_packet_t;

// --- Hardware Abstraction API ---
void fill_adc_buf(adc_buf_t *buf);

#endif // ADC_BUF_H
#!/bin/bash
set -e

# Overwrite the scaffold with the Oracle reference solution
cat > main.c << 'EOF'
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adc_buf.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Filter Configuration ---
#define FILTER_TAPS 64000
#define DECIMATION_FACTOR SAMPLES_PER_FRAME // 1024
#define DECIMATED_SAMPLE_RATE (SAMPLE_RATE / DECIMATION_FACTOR) // 125 Hz
#define FRAMES_PER_BIT (DECIMATED_SAMPLE_RATE / NOMINAL_BAUD_RATE) // 125 frames per baud

static int32_t fir_coef_I[FILTER_TAPS];
static int32_t fir_coef_Q[FILTER_TAPS];
static int16_t history_buffer[FILTER_TAPS];
static int head = 0;

static adc_buf_t current_frame;

// --- UART State Machine ---
typedef enum { IDLE, START_BIT, DATA_BITS, STOP_BIT } uart_state_t;
static uart_state_t rx_state = IDLE;
static int bit_timer = 0;
static int bit_count = 0;
static char current_char = 0;
static uint8_t packet_buffer[PACKET_SIZE];
static int packet_index = 0;
static int prev_bit_val = 1;

void generate_filter(void) {
    double fc = 3.0 / SAMPLE_RATE; // 3 Hz lowpass
    
    for (int i = 0; i < FILTER_TAPS; i++) {
        int n = i - (FILTER_TAPS / 2);
        double h_n = 0.0;
        
        // Windowed Sinc (Blackman Window for > 70dB stopband)
        if (n == 0) {
            h_n = 2.0 * fc;
        } else {
            h_n = sin(2.0 * M_PI * fc * n) / (M_PI * n);
        }
        double blackman = 0.42 - 0.5 * cos(2.0 * M_PI * i / (FILTER_TAPS - 1)) + 
                          0.08 * cos(4.0 * M_PI * i / (FILTER_TAPS - 1));
        h_n *= blackman;

        // Beat against local oscillator (Superheterodyne)
        double lo_angle = 2.0 * M_PI * NOMINAL_CARRIER_FREQ * n / SAMPLE_RATE;
        double coef_I = h_n * cos(lo_angle);
        double coef_Q = h_n * -sin(lo_angle);

        // Convert to 32-bit fixed point (Q30)
        fir_coef_I[i] = (int32_t)(coef_I * (1 << 30));
        fir_coef_Q[i] = (int32_t)(coef_Q * (1 << 30));
    }
}

int main(void) {
    printf("Initializing Oracle Integer DSP Pipeline...\n");
    generate_filter();
    memset(history_buffer, 0, sizeof(history_buffer));

    int64_t prev_I = 0, prev_Q = 0;

    while (1) {
        fill_adc_buf(&current_frame);

        // 1. Push new 1024 samples into circular buffer
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            history_buffer[head] = current_frame.adc_buf[i];
            head = (head + 1) % FILTER_TAPS;
        }

        // 2. Compute decimated FIR output (Once per frame)
        int64_t acc_I = 0;
        int64_t acc_Q = 0;
        
        int read_idx = head; // Oldest sample aligns with coef[0]
        for (int i = 0; i < FILTER_TAPS; i++) {
            int16_t sample = history_buffer[read_idx];
            // 64-bit Accumulation: 16-bit sample * 32-bit coefficient = 48-bit (Safe!)
            acc_I += (int64_t)sample * fir_coef_I[i];
            acc_Q += (int64_t)sample * fir_coef_Q[i];
            read_idx = (read_idx + 1) % FILTER_TAPS;
        }

        // Scale back down to prevent cross product overflow
        acc_I >>= 30;
        acc_Q >>= 30;

        // 3. Mark/Space Discriminator (Cross Product)
        int64_t cross_prod = (prev_I * acc_Q) - (acc_I * prev_Q);
        int bit_val = (cross_prod > 0) ? 1 : 0;
        
        prev_I = acc_I;
        prev_Q = acc_Q;

        // 4. UART State Machine (Runs at 125 Hz)
        if (rx_state == IDLE && prev_bit_val == 1 && bit_val == 0) {
            rx_state = START_BIT;
            // Center-sampling: wait 1.5 bit periods to sample middle of first data bit
            bit_timer = (FRAMES_PER_BIT * 3) / 2; 
            packet_index = 0;
        }

        if (rx_state != IDLE) {
            bit_timer--;
            if (bit_timer <= 0) {
                bit_timer = FRAMES_PER_BIT;

                if (rx_state == START_BIT) {
                    rx_state = DATA_BITS;
                    bit_count = 0;
                    current_char = 0;
                    // We are now sampling the middle of the first data bit
                    current_char |= (bit_val << bit_count);
                    bit_count++;
                } else if (rx_state == DATA_BITS) {
                    current_char |= (bit_val << bit_count);
                    bit_count++;
                    if (bit_count == 8) rx_state = STOP_BIT;
                } else if (rx_state == STOP_BIT) {
                    if (bit_val == 1) { // Valid Stop Bit
                        packet_buffer[packet_index++] = current_char;
                        
                        // 5. Packet Framing & Checksum Validation
                        if (packet_index == PACKET_SIZE) {
                            uint8_t sum = 0;
                            for (int i = 0; i < PAYLOAD_SIZE; i++) {
                                sum += packet_buffer[i];
                            }
                            
                            const char *hex = "0123456789ABCDEF";
                            char expected_upper = hex[(sum >> 4) & 0x0F];
                            char expected_lower = hex[sum & 0x0F];
                            
                            if (packet_buffer[PAYLOAD_SIZE] == expected_upper && packet_buffer[PAYLOAD_SIZE + 1] == expected_lower) {
                                packet_buffer[PAYLOAD_SIZE] = '\0'; // Ensure null termination
                                printf("DECODED: %s\n", packet_buffer);
                                return 0; // Success!
                            }
                            // If checksum fails, reset and keep listening
                            packet_index = 0; 
                        }
                    } else {
                        // Framing error
                        packet_index = 0;
                    }
                    rx_state = IDLE;
                }
            }
        }
        prev_bit_val = bit_val;
    }
}
EOF

echo "Oracle main.c successfully generated."
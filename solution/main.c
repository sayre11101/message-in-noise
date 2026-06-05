#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "adc_buf.h"
#include "dsp.h"

#define DECIMATED_SAMPLE_RATE (SAMPLE_RATE / SAMPLES_PER_FRAME)
#define NOMINAL_FRAMES_PER_BIT                                                 \
  ((int)(DECIMATED_SAMPLE_RATE / NOMINAL_BAUD_RATE))

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

static int current_frames_per_bit = NOMINAL_FRAMES_PER_BIT;

int main(void) {
  printf("Initializing Oracle Integer DSP Pipeline...\n");

  init_dsp_filter();

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
    // Pass NULL for the optional math pointers because we only care about the
    // sliced bit!
    int64_t cross_product;
    int bit_val =
        process_dsp_frame(current_frame.adc_buf, &cross_product, NULL);

    if (frame_count % 8 == 0) {
      printf("[ORACLE] Subsecond cross product %lld and bit %d\n",
             cross_product, bit_val);
      fflush(stdout);
    }

    if (simulated_seconds < ((int)(NOMINAL_BAUD_RATE * INITIAL_IDLE_TIME * 0.9))) {
      continue;
    }

    // --- UART Framing ---
    if (rx_state == IDLE && prev_bit_val == 1 && bit_val == 0) {
      printf("[ORACLE] Got a start bit\n");
      rx_state = START_BIT;
      current_frames_per_bit = NOMINAL_FRAMES_PER_BIT;
      bit_timer = (current_frames_per_bit * 3) / 2;
    }

    if (rx_state != IDLE) {
      bit_timer--;
      if (bit_timer <= 0) {
        printf("[ORACLE] Got a bit %d\n", bit_val);

        bit_timer = current_frames_per_bit;

        if (rx_state == START_BIT) {
          rx_state = DATA_BITS;
          bit_count = 0;
          current_char = 0;
          current_char |= (bit_val << bit_count);
          bit_count++;
        } else if (rx_state == DATA_BITS) {
          current_char |= (bit_val << bit_count);
          bit_count++;
          if (bit_count == 8)
            rx_state = STOP_BIT;
        } else if (rx_state == STOP_BIT) {
          if (bit_val == 1) {
            printf("[ORACLE] Got the stop bit for current char %c, index %d "
                   "out of %d\n",
                   current_char, packet_index, PACKET_SIZE);
            packet_buffer[packet_index++] = current_char;

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
          } else {
            printf("[ORACLE] UART Framing Error! Retrying...\n");
            packet_index = 0;
          }
          rx_state = IDLE;
        }
      }
    }
    prev_bit_val = bit_val;
  }
}
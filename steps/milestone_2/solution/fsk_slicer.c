#include "fsk_slicer.h"
#include "adc_buf.h"

#define DECIMATED_SAMPLE_RATE (SAMPLE_RATE / SAMPLES_PER_FRAME)
#define NOMINAL_FRAMES_PER_BIT                                                 \
  ((int)(DECIMATED_SAMPLE_RATE / NOMINAL_BAUD_RATE))

// --- UART State Machine Variables ---
typedef enum { IDLE, START_BIT, DATA_BITS, STOP_BIT } uart_state_t;
static uart_state_t rx_state = IDLE;
static int bit_timer = 0;
static int bit_count = 0;
static char current_char = 0;
static int prev_bit_val = 1;
static int current_frames_per_bit = NOMINAL_FRAMES_PER_BIT;

void fsk_slicer_init(void) {
  rx_state = IDLE;
  bit_timer = 0;
  bit_count = 0;
  current_char = 0;
  prev_bit_val = 1;
  current_frames_per_bit = NOMINAL_FRAMES_PER_BIT;
}

int fsk_slicer_process(int bit_val, char *decoded_char) {
  int char_ready = 0;

  // --- Exact Original UART Framing ---
  if (rx_state == IDLE && prev_bit_val == 1 && bit_val == 0) {
    rx_state = START_BIT;
    current_frames_per_bit = NOMINAL_FRAMES_PER_BIT;
    bit_timer = (current_frames_per_bit * 3) / 2;
  }

  if (rx_state != IDLE) {
    bit_timer--;
    if (bit_timer <= 0) {
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
          // Successfully caught the stop bit
          *decoded_char = current_char;
          char_ready = 1;
        }
        rx_state = IDLE;
      }
    }
  }

  prev_bit_val = bit_val;
  return char_ready;
}
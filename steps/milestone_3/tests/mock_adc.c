#include "adc_buf.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zephyr/kernel.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *secret_word = NULL; // TODO: set in initialization
static double true_base_freq = 0.0;
static double true_bit_period = 0.0;

typedef enum {
  SCENARIO_PLL_STEP = 0,
  SCENARIO_MARK_SPACE_PATTERN,
  SCENARIO_UART_PATTERN,
  SCENARIO_FULL_MESSAGE,
} scenario_t;

static scenario_t active_scenario = SCENARIO_FULL_MESSAGE;
static const char *symbol_pattern = "11111001100101101001";
static const char *uart_pattern_message = "OK";

// --- Simulation State ---
static double current_sampling_freq = 122880.0; // Starts at nominal 60Hz * 2048
static double global_time = 0.0;
static double current_phase = 0.0;

static int initialized = 0;

static const double DEVIATION = 0.05; // +/- 0.05 Hz

// --- Hardware Abstraction API ---

void set_adc_sampling_frequency(double target_hz) {
  // The agent calls this to adjust the ADC sampling rate
  current_sampling_freq = target_hz;
}

static scenario_t parse_scenario(void) {
  const char *name = getenv("SCENARIO");
  if (name == NULL || strcmp(name, "") == 0 ||
      strcmp(name, "full_message") == 0) {
    return SCENARIO_FULL_MESSAGE;
  }
  if (strcmp(name, "pll_step") == 0) {
    return SCENARIO_PLL_STEP;
  }
  if (strcmp(name, "mark_space_pattern") == 0) {
    return SCENARIO_MARK_SPACE_PATTERN;
  }
  if (strcmp(name, "uart_pattern") == 0) {
    return SCENARIO_UART_PATTERN;
  }
  return SCENARIO_FULL_MESSAGE;
}

static int symbol_from_uart_byte(char c, int bit_index) {
  if (bit_index == 0) {
    return 0;
  }
  if (bit_index >= 1 && bit_index <= 8) {
    return (c >> (bit_index - 1)) & 0x01;
  }
  return 1;
}

void fill_adc_buf(adc_buf_t *buf) {
  if (!initialized) {
    srand(time(NULL));

    active_scenario = parse_scenario();
    true_base_freq = 58.0 + ((double)rand() / RAND_MAX) * 4.0;
    true_bit_period = 0.98 + ((double)rand() / RAND_MAX) * 0.04;

    // Randomize the word only for the final milestone/full task.
    static const char *WORD_BANK[] = {"ZEPHYR",    "ORACLE", "signal", "System",
                                      "frequency", "FILTER", "widget", "ROUTER",
                                      "Silicon",   "kernel"};
    secret_word = WORD_BANK[rand() % 10];

    if (active_scenario == SCENARIO_PLL_STEP) {
      printf("[MOCK_ENV] SCENARIO: pll_step\n");
      printf("[MOCK_ENV] STEP_FROM_HZ: 59.000\n");
      printf("[MOCK_ENV] STEP_TO_HZ: 60.000\n");
      printf("[MOCK_ENV] STEP_TIME_S: 4.000\n");
    } else if (active_scenario == SCENARIO_MARK_SPACE_PATTERN) {
      true_base_freq = 60.0;
      true_bit_period = 1.0;
      printf("[MOCK_ENV] SCENARIO: mark_space_pattern\n");
      printf("[MOCK_ENV] BASE_FREQ: %.3f\n", true_base_freq);
      printf("[MOCK_ENV] BIT_PERIOD: %.3f\n", true_bit_period);
      printf("[MOCK_ENV] SYMBOL_PATTERN: %s\n", symbol_pattern);
    } else if (active_scenario == SCENARIO_UART_PATTERN) {
      true_base_freq = 60.0;
      true_bit_period = 1.0;
      printf("[MOCK_ENV] SCENARIO: uart_pattern\n");
      printf("[MOCK_ENV] BASE_FREQ: %.3f\n", true_base_freq);
      printf("[MOCK_ENV] BIT_PERIOD: %.3f\n", true_bit_period);
      printf("[MOCK_ENV] UART_MESSAGE: %s\n", uart_pattern_message);
    } else {
      printf("[MOCK_ENV] SCENARIO: full_message\n");
      printf("[MOCK_ENV] SECRET_WORD: %s\n", secret_word);
      printf("[MOCK_ENV] CARRIER_FREQ: %.3f Hz\n", true_base_freq);
      printf("[MOCK_ENV] BIT_PERIOD: %.3f s\n", true_bit_period);
    }

    initialized = 1;
  }

  uint16_t *buffer = buf->adc_buf;

  for (size_t i = 0; i < SAMPLES_PER_CYCLE; i++) {
    double dt = 1.0 / current_sampling_freq;
    global_time += dt;

    double current_freq = true_base_freq;

    if (active_scenario == SCENARIO_PLL_STEP) {
      current_freq = (global_time < 4.0) ? 59.0 : 60.0;
    } else if (active_scenario == SCENARIO_MARK_SPACE_PATTERN) {
      double start_s = 5.0;
      if (global_time >= start_s) {
        int idx = (int)((global_time - start_s) / true_bit_period);
        int pattern_len = (int)strlen(symbol_pattern);
        int bit_val = 1;
        if (idx >= 0 && idx < pattern_len) {
          bit_val = (symbol_pattern[idx] == '1') ? 1 : 0;
        }
        current_freq += (bit_val == 1) ? DEVIATION : -DEVIATION;
      } else {
        current_freq += DEVIATION;
      }
    } else if (active_scenario == SCENARIO_UART_PATTERN) {
      double start_s = 5.0;
      int msg_len = (int)strlen(uart_pattern_message) + 1;
      double total_time = 10.0 * msg_len * true_bit_period;

      if (global_time >= start_s && global_time < start_s + total_time) {
        double msg_time = global_time - start_s;
        int total_bits = (int)(msg_time / true_bit_period);
        int char_index = total_bits / 10;
        int bit_index = total_bits % 10;

        char c = '\0';
        if (char_index < msg_len - 1) {
          c = uart_pattern_message[char_index];
        }

        int bit_val = symbol_from_uart_byte(c, bit_index);
        current_freq += (bit_val == 1) ? DEVIATION : -DEVIATION;
      } else {
        current_freq += DEVIATION;
      }
    } else {
      int msg_len = (int)strlen(secret_word) + 1;
      double start_s = 5.0;
      double total_time = 10.0 * msg_len * true_bit_period;

      if (global_time >= start_s && global_time < start_s + total_time) {
        double msg_time = global_time - start_s;
        int total_bits = (int)(msg_time / true_bit_period);
        int char_index = total_bits / 10;
        int bit_index = total_bits % 10;

        char c = '\0';
        if (char_index < msg_len - 1) {
          c = secret_word[char_index];
        }

        int bit_val = symbol_from_uart_byte(c, bit_index);
        current_freq += (bit_val == 1) ? DEVIATION : -DEVIATION;
      } else {
        current_freq += DEVIATION;
      }
    }

    current_phase += 2.0 * M_PI * current_freq * dt;
    if (current_phase > 2.0 * M_PI) {
      current_phase -= 2.0 * M_PI;
    }

    buffer[i] = (uint16_t)(2047.0 * sin(current_phase) + 2048.0);
  }
}

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Emulator State ---
static uint8_t si5351_regs[256] = {0};
static double current_sampling_freq = 60.0 * 2048.0;
static double global_phase = 0.0;
static double true_grid_freq = 60.0;

// ---------------------------------------------------------------------
// Boot Initialization
// ---------------------------------------------------------------------
static int mock_system_init(void) {
  srand(k_cycle_get_32());
  double random_offset = ((double)rand() / RAND_MAX) * 2.4 - 1.2;
  true_grid_freq = 60.0 + random_offset;

  printf("[MOCK_ENV] True Grid Freq initialized to: %.6f Hz\n", true_grid_freq);
  fflush(stdout);

  return 0;
}
SYS_INIT(mock_system_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

// ---------------------------------------------------------------------
// Direct I2C Driver Stub
// ---------------------------------------------------------------------
static int stub_i2c_transfer(const struct device *dev, struct i2c_msg *msgs,
                             uint8_t num_msgs, uint16_t addr) {
  if (num_msgs < 1)
    return 0;

  for (int i = 0; i < num_msgs; i++) {
    if (msgs[i].flags & I2C_MSG_READ) {
      msgs[i].buf[0] = si5351_regs[si5351_regs[0]];
    } else {
      uint8_t reg_addr = msgs[i].buf[0];
      if (msgs[i].len > 1) {
        for (int j = 1; j < msgs[i].len; j++) {
          si5351_regs[reg_addr + j - 1] = msgs[i].buf[j];
        }

        // If the agent updated the PLLA fractional registers, recalculate the
        // sampling frequency
        if (reg_addr >= 26 && reg_addr <= 33) {
          uint32_t p3 = ((si5351_regs[26] << 8) | si5351_regs[27]) |
                        ((si5351_regs[28] & 0xF0) << 12);
          uint32_t p1 = ((si5351_regs[28] & 0x0F) << 16) |
                        (si5351_regs[29] << 8) | si5351_regs[30];
          uint32_t p2 = ((si5351_regs[31] & 0x0F) << 16) |
                        (si5351_regs[32] << 8) | si5351_regs[33];

          if (p3 > 0) {
            double multiplier = ((double)p1 + 512.0) / 128.0 +
                                ((double)p2) / (128.0 * (double)p3);
            current_sampling_freq = (25000000.0 * multiplier) / 6400.0;
          }
        }
      } else {
        si5351_regs[0] = reg_addr;
      }
    }
  }
  return 0;
}

// Map the stub to Zephyr's internal I2C API struct
static const struct i2c_driver_api dummy_i2c_api = {.transfer =
                                                        stub_i2c_transfer};

// ---------------------------------------------------------------------
// Direct SPI Driver Stub
// ---------------------------------------------------------------------
static int stub_spi_transceive(const struct device *dev,
                               const struct spi_config *config,
                               const struct spi_buf_set *tx_bufs,
                               const struct spi_buf_set *rx_bufs) {
  if (!rx_bufs || rx_bufs->count == 0)
    return 0;

  int16_t *rx_data = (int16_t *)rx_bufs->buffers[0].buf;
  size_t num_samples = rx_bufs->buffers[0].len / sizeof(int16_t);

  double phase_step = (2.0 * M_PI * true_grid_freq) / current_sampling_freq;

  for (size_t i = 0; i < num_samples; i++) {
    double voltage = sin(global_phase) * 2047.0;
    int16_t adc_val = (int16_t)voltage;
    rx_data[i] = adc_val << 4;

    global_phase += phase_step;
    if (global_phase > 2.0 * M_PI) {
      global_phase -= 2.0 * M_PI;
    }
  }

  return 0;
}

// Map the stub to Zephyr's internal SPI API struct
static const struct spi_driver_api dummy_spi_api = {.transceive =
                                                        stub_spi_transceive};

// ---------------------------------------------------------------------
// Hardware Registration
// ---------------------------------------------------------------------
static int dummy_device_init(const struct device *dev) { return 0; }

// Inject our custom APIs directly into the dummy devices
DEVICE_DT_DEFINE(DT_NODELABEL(si5351a), dummy_device_init, NULL, NULL, NULL,
                 POST_KERNEL, 90, &dummy_i2c_api);
DEVICE_DT_DEFINE(DT_NODELABEL(ads8528_a), dummy_device_init, NULL, NULL, NULL,
                 POST_KERNEL, 90, &dummy_spi_api);
# Zephyr RTOS: DSP & FSK Demodulator

## Background

This is a Zephyr RTOS project. The concept is that there is a secret message encoded by FSK at the generator of a power station. The power line frequency is approximately 60Hz and steady over the period of the message, except for a +/- 0.05Hz FSK modulation that encodes an NRZ 8N1 ASCII message at a 1Hz baud rate.

The API provides you with a hardware abstraction layer (HAL) to read the power line voltage. Every time you call `fill_adc_buf`, you receive frames of 2048 samples of right-justified 12-bit unsigned data (ranging from 0 to 4095). You adjust the ADC sampling hardware to track the line by calling `set_adc_sampling_frequency(double target_hz)`.

Your goal is to decode the message. The Oracle solution uses a software Phase-Locked Loop (PLL), but you do not have to use a PLL. Any DSP algorithm is acceptable to pass this challenge.

## Parameters

- **The Carrier:** The base grid frequency is randomized between 58.0 Hz and 62.0 Hz.
- **The Modulation:** The carrier contains a microscopic FSK signal.
  - **Mark (Binary 1 / Idle):** Carrier + 0.05 Hz
  - **Space (Binary 0 / Start):** Carrier - 0.05 Hz
- **The Preamble:** the message begins with a preamble of 5 seconds of "mark" bits before the first start bit
- **The Protocol:** Standard 8N1 serial framing (1 Start bit, 8 Data bits LSB-first, 1 Stop bit).
- **The Clock Recovery:** The baud rate is nominally 1 baud, but the actual transmission rate is randomized by **+/- 2%** (between 0.98 and 1.02 seconds per bit, remaining constant over the length of the message).

## Assignment

- Populate the `main.c` template that is provided in the container.
  - Initialize the sampling frequency.
  - Read blocks of data for a number times defined by `NUMBER_OF_ADC_FRAMES` in the `adc_buf.h` header.
  - Process the data to decode the hidden FSK message up to the maximum of `MAX_MESSAGE_LENGTH` defined in `adc_buf.h`.
  - The message is ascii from 32 to 126 terminated with '\0'
  - Print out the message utilizing the code provided at the bottom of the `main.c` scaffold:
    - `printf("DECODED: %s\n", decoded_message);`
  
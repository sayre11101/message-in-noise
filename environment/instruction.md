# Zephyr RTOS: DSP & FSK Demodulator

## Background

This is a Zephyr RTOS project. You are tasked with decoding a weak FSK signal buried in heavy broadband noise. There is also a massive interfering carrier (+10dB above our signal) sitting at an unknown offset from the nominal 21.5 kHz carrier but at least 10Hz away. Floating point arithmetic (32-bit float) may lack the precision necessary to accumulate large FIR filters without catastrophic cancellation due to the noise and interferer. You are strongly encouraged to use fixed-point integer math (e.g., int64_t) for your DSP implementation. The transmitter transmits a 16-byte packet defined as fsk_packet_t in adc_buf.h. It transmits the packet three times:
- Bytes 0-13: The payload (null-padded).
- Bytes 14-15: A 2-character ASCII Hex checksum (modulo-256 sum of bytes
0-13).
When you successfully decode and validate a packet via its checksum,
print it strictly as: printf("DECODED: %s\n", payload);

The API provides you with a hardware abstraction layer (HAL) to sample the incoming signal. Every time you call `fill_adc_buf`, you receive frames of data in a structure adc_buf_t defined in adc_buf.h The ADC samples are 16 bit signed integers.

Your goal is to decode one of the three copies of the message by verifying the checksum. Any DSP algorithm is acceptable to pass this challenge.

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
  
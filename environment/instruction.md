# Zephyr RTOS: High-Noise FSK Demodulator

## Background

You are tasked with writing a Digital Signal Processing (DSP) pipeline in C for the Zephyr RTOS to decode a weak FSK signal buried in heavy broadband Additive White Gaussian Noise (AWGN). In addition to the noise (~20dB SNR in 1Hz BW), there is a massive unmodulated interfering carrier (+10dB above our signal) that will actively attempt to mask the transmission.

Because of the extreme dynamic range between the +10dB interferer and the weak baseband signal, standard 32-bit floating-point accumulators may suffer from catastrophic cancellation during heavy FIR filtering. **You are strongly encouraged to use fixed-point arithmetic with 64-bit (`int64_t`) or 128-bit (`__int128_t`) integers for your DSP implementation.**

## The API

The environment provides a hardware abstraction layer to sample the incoming RF signal.

* Call `fill_adc_buf(adc_buf_t *buf)` to receive blocks of 16-bit signed integer samples.
* The data structures, buffer lengths, and physical constants are all explicitly defined for you in `adc_buf.h`.

## Signal Specifications

Please refer to the `adc_buf.h` header file for all exact numerical constants.

* **Sampling & Carrier:** The ADC samples at `SAMPLE_RATE`. The target FSK signal is centered near `NOMINAL_CARRIER_FREQ`, subject to a physical crystal drift up to `CARRIER_TOLERANCE_PPM`.
* **Modulation:** Continuous-phase FSK. Mark (idle/1) is the higher frequency, Space (start/0) is the lower frequency. The peak deviation from the center carrier is `FSK_PEAK_DEVIATION`.
* **Baud Rate:** Standard 8N1 serial framing (1 Start bit, 8 Data bits LSB-first, 1 Stop bit) at `NOMINAL_BAUD_RATE`. The actual transmission rate may drift by up to `BAUD_TOLERANCE_PERCENT` and will remain constant for the duration of the simulation.

## The RF Environment Constraints

* **Hopping Interferer:** The +10dB interfering carrier is located at least 10.0 Hz away from the actual carrier frequency (either above or below). To defeat static notch filters, the interferer will randomly hop to a new frequency offset during the dead-air between packet loops.
* **Transmission Timeline:** After an initial idle period of continuous "Mark" tone, the transmitter will repeatedly loop the target packet, separated by continuous "Mark" idle periods.

## Packet Structure

The transmitter sends a `PACKET_SIZE` byte packet, strictly defined as `fsk_packet_t` in the header:

* **Payload (Bytes 0 to `PAYLOAD_SIZE - 1`):** A C-string payload. The secret message is randomly generated but guaranteed to be null-padded (`\0`) so it always forms a valid, printable string.
* **Checksum (The final `CHECKSUM_SIZE` bytes):** A 2-character ASCII Hexadecimal representation of the modulo-256 sum of the payload bytes.
* *Example:* If the modulo-256 sum of the payload bytes is `0x4A`, the first checksum byte will be `'4'` and the second will be `'A'`.

## Output Requirement

Your objective is to ingest the raw ADC samples, filter the interferer, synchronize to the baud rate, slice the bits, frame the UART bytes, and validate the checksum.

Once your code successfully recovers a packet and the checksum matches, print the payload exactly as follows:

* `printf("DECODED: %s\n", packet.payload);`

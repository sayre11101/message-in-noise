# Milestone 2: FSK Slicing & UART Framing

With the heavy interference filtered out by your Physical Layer, you must now implement the Data Link layer to recover the hidden ASCII data. 

Create or modify `/app/fsk_slicer.c` to implement the baud synchronization and UART framing.

## System Parameters & Protocol

* **Modulation:** Continuous-phase FSK. Mark (idle/1) is the higher frequency, Space (start/0) is the lower frequency. The peak deviation from the center carrier is `FSK_PEAK_DEVIATION` (defined in `adc_buf.h`).
* **Baud Rate:** Standard 8N1 serial framing (1 Start bit, 8 Data bits LSB-first, 1 Stop bit) at `NOMINAL_BAUD_RATE`.
* **Baud Drift:** The actual transmission rate may drift by up to `BAUD_TOLERANCE_PERCENT` and will remain constant for the duration of the simulation.

## Your Engineering Task

1. **Frequency Slicing:** Process the cleaned frequency deviations from Milestone 1 and categorize them as Mark or Space.
2. **Clock Recovery:** Implement a synchronization mechanism to lock onto the baud rate and sample the center of each bit period, compensating for the `BAUD_TOLERANCE_PERCENT` drift.
3. **8N1 State Machine:** Reconstruct the bitstream into valid 8-bit bytes. Return `1` when a new, complete character has been successfully decoded, and store it in a provided pointer. Return `0` otherwise.
# Milestone 3: Zephyr Integration and Packet Verification

You must integrate your DSP mixer and FSK slicer into the Zephyr RTOS loop in `main.c`, assemble the incoming bytes into a packet, and verify the checksum.

## The Packet Structure

The transmitter sends a `PACKET_SIZE` byte packet, strictly defined as `fsk_packet_t` in `adc_buf.h`:

* **Payload (Bytes 0 to `PAYLOAD_SIZE - 1`):** A randomly generated, null-padded (`\0`) C-string.
* **Checksum (The final `CHECKSUM_SIZE` bytes):** A 2-character ASCII Hexadecimal representation of the modulo-256 sum of the payload bytes.
  * *Example:* If the modulo-256 sum of the payload bytes is `0x4A`, the first checksum byte will be `'4'` and the second will be `'A'`.

## Integration Steps

1. **Hardware Loop:** Continually call `fill_adc_buf(&buf)` to receive blocks of 16-bit signed integer samples.
2. **Signal Chain:** Pass the buffer through your baseband mixer and LPF (Milestone 1), then pass the extracted frequencies into your UART framer (Milestone 2).
3. **Buffering:** Collect the decoded bytes into an `fsk_packet_t` struct. (Note: The transmission timeline loops the packet continuously, separated by "Mark" idle periods. You must synchronize to the start of a packet).
4. **Validation & Output:** Calculate the modulo-256 sum of your received payload. If it matches the received 2-byte ASCII Hex checksum, you have successfully recovered the transmission.

Upon a successful checksum match, you must break the loop early and print the payload exactly as follows:
`printf("DECODED: %s\n", packet.payload);`
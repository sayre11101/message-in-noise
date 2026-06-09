# Milestone 1: Baseband Mixer and Low-Pass Filter

You are tasked with the Physical Layer DSP for decoding a weak FSK signal buried in heavy broadband Additive White Gaussian Noise (AWGN) and a +10dB hopping interferer.

Before you can decode the FSK bits, you must isolate the target signal. Use sufficient numerical resolution to be able to decode a weak signal in broadband and continuous wave (CW) noise of greater amplitude than the signal.

## Your Engineering Task

You have been provided with `adc_buf.h` containing the physical constants (`SAMPLE_RATE`, `NOMINAL_CARRIER_FREQ`, etc.) and a scaffold at `/app/dsp_mixer.c`.

1. **Implement the Mixer:** Write a function to downconvert the incoming 16-bit ADC samples from the nominal carrier frequency to baseband. The center frequency of the baseband is the carrier frequency of 
2. **Implement the FIR Low-Pass Filter (LPF):** Apply a fixed-point LPF to the mixed signal to reject the out-of-band +10dB hopping interferer and minimize the broadband AWGN.
3. **Extract Instantaneous Frequency:** Calculate and return the filtered frequency deviation (to be used later for Mark/Space discrimination).

## Constraints

* The interferer will randomly hop, but is guaranteed to be at least 10.0 Hz away from the target carrier. Your LPF must be sharp enough to reject it regardless of its current offset.
* You must prevent integer overflow and catastrophic cancellation in your filter accumulators.

#ifndef FSK_SLICER_H
#define FSK_SLICER_H

void fsk_slicer_init(void);

// Takes the sliced bit directly, exactly as your old main.c did
int fsk_slicer_process(int bit_val, char *decoded_char);

#endif
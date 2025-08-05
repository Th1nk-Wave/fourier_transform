#ifndef __DFT_H_
#define __DFT_H_

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159

typedef struct {
    float* real;
    float* imaginary;
    unsigned int size;
} complex_array;

complex_array forward(float sample[], unsigned int samples);
void dft(complex_array ft, float sample[], unsigned int samples);
float* inverse(complex_array transform, unsigned int size);
void reconstruct(float* reconstructed, complex_array transform, unsigned int size);

#endif
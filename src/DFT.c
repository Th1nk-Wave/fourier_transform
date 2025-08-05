#include "DFT.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

complex_array forward(float sample[], unsigned int samples) {
    float* reals = malloc(sizeof(float) * samples);
    float* imaginearies = malloc(sizeof(float) * samples);
    for (unsigned int n = 0; n < samples; n++) {
        imaginearies[n] = 0;
        reals[n] = 0;
        for (unsigned int i = 0; i < samples; i++) {
            imaginearies[n] -= sample[i] * sin(2*PI*n*(float)i/samples);
            reals[n]        += sample[i] * cos(2*PI*n*(float)i/samples);
        }

    }
    complex_array ret = {
        .imaginary = imaginearies,
        .real = reals,
        .size = samples,
    };
    return ret;
}

void dft(complex_array ft, float sample[], unsigned int samples) {
    for (unsigned int n = 0; n < ft.size; n++) {
        ft.imaginary[n] = 0.f;
        ft.real[n] = 0.f;
        for (unsigned int i = 0; i < samples; i++) {
            ft.imaginary[n] -= sample[i] * sin(2*PI*n*(float)i/ft.size);
            ft.real[n]      += sample[i] * cos(2*PI*n*(float)i/ft.size);
        }

    }
}

float* inverse(complex_array transform, unsigned int size) {
    float* reconstructed = malloc(sizeof(float)*size);

    for (unsigned int n = 0; n < size; n++) {
        reconstructed[n] = 0.f;
        for (unsigned int i = 0; i < transform.size; i ++) {
            reconstructed[n] += transform.real[i] * cos(2*PI*i*n/transform.size)
                             + transform.imaginary[i] * sin(2*PI*i*n/transform.size);
        }
        reconstructed[n]/=size;
    }
    return reconstructed;
}

void reconstruct(float* reconstructed, complex_array transform, unsigned int size) {
    //float* reconstructed = malloc(sizeof(float)*size);

    for (unsigned int n = size; n > 0; n--) {
        reconstructed[size-n] = 0.f;
        for (unsigned int i = 0; i < transform.size; i ++) {
            reconstructed[size-n] += transform.real[i] * cos(2*PI*i*n/transform.size)
                             + transform.imaginary[i] * sin(2*PI*i*n/transform.size);
        }
        reconstructed[size-n]/=size;
    }
    //return reconstructed;
}
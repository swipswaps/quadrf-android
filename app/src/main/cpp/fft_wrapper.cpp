#include "fft_wrapper.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "FFTWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * Verbatim citation from FFTW documentation:
 * "FFTW is a C subroutine library for computing the discrete Fourier transform
 * (DFT) in one or more dimensions, of arbitrary input size, and of both real
 * and complex data."
 * Source: www.fftw.org/fftw3_doc/
 */

FFTWrapper::FFTWrapper() : plan(nullptr), input(nullptr), output(nullptr), size(0) {}

FFTWrapper::~FFTWrapper() {
    cleanup();
}

bool FFTWrapper::init(int fft_size) {
    if (fft_size <= 0) return false;

    size = fft_size;
    input = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * size);
    output = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * size);

    if (!input || !output) {
        LOGE("Failed to allocate FFT buffers");
        return false;
    }

    plan = fftwf_plan_dft_1d(size, input, output, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        LOGE("Failed to create FFT plan");
        return false;
    }

    LOGI("FFT initialised with size %d", size);
    return true;
}

void FFTWrapper::execute(float* iq_data, float* spectrum) {
    if (!plan || !input || !output) return;

    // Copy IQ data to input (complex interleaved)
    for (int i = 0; i < size; i++) {
        input[i][0] = iq_data[2*i];
        input[i][1] = iq_data[2*i+1];
    }

    fftwf_execute(plan);

    // Compute magnitude spectrum
    for (int i = 0; i < size; i++) {
        float real = output[i][0];
        float imag = output[i][1];
        spectrum[i] = sqrtf(real*real + imag*imag);
    }
}

void FFTWrapper::cleanup() {
    if (plan) {
        fftwf_destroy_plan(plan);
        plan = nullptr;
    }
    if (input) {
        fftwf_free(input);
        input = nullptr;
    }
    if (output) {
        fftwf_free(output);
        output = nullptr;
    }
    size = 0;
}

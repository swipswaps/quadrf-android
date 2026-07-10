#include "sdr_processor.h"
#include <android/log.h>
#include <cstring>
#include <cmath>

#define LOG_TAG "SdrProcessor"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

SdrProcessor::SdrProcessor() : fft_initialized(false), sample_rate(2048000) {}

SdrProcessor::~SdrProcessor() {
    cleanup();
}

void SdrProcessor::init() {
    LOGI("SDR Processor initialised");
    // FFT initialisation would go here
    fft_initialized = true;
}

void SdrProcessor::process_iq(const unsigned char* data, int length) {
    if (!fft_initialized) return;

    // Convert raw USB data to IQ samples (complex float)
    // This is a placeholder – actual conversion depends on Quad RF format
    float iq_buffer[1024];
    int num_samples = length / 2;

    for (int i = 0; i < num_samples && i < 512; i++) {
        // Assuming interleaved I/Q bytes
        iq_buffer[2*i] = (data[2*i] - 128.0f) / 128.0f;   // I
        iq_buffer[2*i+1] = (data[2*i+1] - 128.0f) / 128.0f; // Q
    }

    // Compute FFT on IQ data
    // In a real implementation, this would call FFTW
    // fftw_execute(plan);

    // Placeholder: generate synthetic spectrum for waterfall
    float spectrum[256];
    for (int i = 0; i < 256; i++) {
        spectrum[i] = 0.5f + 0.5f * sinf(i * 0.1f + time);
    }
    time += 0.01f;

    // Notify Java layer via JNI callback (not implemented in this example)
}

void SdrProcessor::cleanup() {
    if (fft_initialized) {
        LOGI("SDR Processor cleanup");
        fft_initialized = false;
    }
}

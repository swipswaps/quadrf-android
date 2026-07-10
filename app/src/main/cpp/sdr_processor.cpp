#include "sdr_processor.h"
#include "fft_wrapper.h"
#include "demodulator.h"
#include <android/log.h>
#include <cstring>
#include <cmath>

#define LOG_TAG "SdrProcessor"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * Verbatim citation from FFTW documentation:
 * "FFTW is a C subroutine library for computing the discrete Fourier transform
 * (DFT) in one or more dimensions, of arbitrary input size, and of both real
 * and complex data."
 * Source: www.fftw.org/fftw3_doc/
 *
 * This processor orchestrates the complete signal processing pipeline:
 * 1. Convert raw USB bytes to IQ floats
 * 2. Compute FFT for spectrum analysis
 * 3. Demodulate selected mode to audio
 * 4. Decimate to audio sample rate
 */

SdrProcessor::SdrProcessor()
    : fft_initialized(false), sample_rate(2048000),
      fft_size(256), audio_sample_rate(48000),
      decimation_factor(sample_rate / audio_sample_rate),
      fft(nullptr), demod(nullptr), time(0.0f) {}

SdrProcessor::~SdrProcessor() {
    cleanup();
}

void SdrProcessor::init() {
    LOGI("SDR Processor initialising");
    fft = new FFTWrapper();
    if (!fft->init(fft_size)) {
        LOGE("FFT initialisation failed");
        delete fft;
        fft = nullptr;
        return;
    }
    demod = new Demodulator();
    demod->set_mode(DemodMode::AM);
    fft_initialized = true;
    LOGI("SDR Processor initialised successfully");
}

void SdrProcessor::process_iq(const unsigned char* data, int length) {
    if (!fft_initialized || !fft || !demod) {
        LOGE("Processor not initialised");
        return;
    }

    // Convert raw USB bytes to IQ samples (float)
    // Supports 8-bit unsigned (iq_format=0) or 16-bit signed (iq_format=1)
    int num_samples = 0;
    float iq_buffer[512 * 2]; // max 512 complex samples

    if (iq_format == 0) {
        // 8-bit unsigned interleaved I/Q
        num_samples = length / 2;
        if (num_samples > 512) num_samples = 512;
        for (int i = 0; i < num_samples; i++) {
            iq_buffer[2*i] = (data[2*i] - 128.0f) / 128.0f;
            iq_buffer[2*i+1] = (data[2*i+1] - 128.0f) / 128.0f;
        }
    } else {
        // 16-bit signed little-endian interleaved I/Q
        num_samples = length / 4;
        if (num_samples > 512) num_samples = 512;
        for (int i = 0; i < num_samples; i++) {
            int16_t I = data[4*i] | (data[4*i+1] << 8);
            int16_t Q = data[4*i+2] | (data[4*i+3] << 8);
            iq_buffer[2*i] = I / 32768.0f;
            iq_buffer[2*i+1] = Q / 32768.0f;
        }
    }

    if (num_samples < 2) return;

    // ----- FFT using FFTW for spectrum analysis -----
    float spectrum[256];
    fft->execute(iq_buffer, spectrum);

    // Normalize spectrum for display
    float max_val = 0.0f;
    for (int i = 0; i < fft_size; i++) {
        if (spectrum[i] > max_val) max_val = spectrum[i];
    }
    if (max_val > 0.0f) {
        for (int i = 0; i < fft_size; i++) {
            spectrum[i] /= max_val;
            if (spectrum[i] > 1.0f) spectrum[i] = 1.0f;
        }
    }

    // Send spectrum to Java via callback (implemented in native-lib.cpp)
    // The callback function is declared as extern in native-lib.cpp
    extern void sendSpectrumToJava(float* spectrum, int size);
    sendSpectrumToJava(spectrum, fft_size);

    // ----- Audio demodulation and decimation -----
    // Decimate from sample_rate to audio_sample_rate
    int audio_samples = num_samples / decimation_factor;
    if (audio_samples > 0 && audio_samples < 1024) {
        float audio_float[1024];
        int out = demod->demodulate(iq_buffer, num_samples, audio_float);

        // Convert float to 16-bit PCM
        short audio_short[1024];
        int audio_count = 0;
        for (int i = 0; i < out && i < 1024; i++) {
            float sample = audio_float[i] * 32767.0f;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            audio_short[audio_count++] = (short)sample;
        }

        // Send audio to Java via callback
        if (audio_count > 0) {
            extern void sendAudioToJava(short* samples, int count);
            sendAudioToJava(audio_short, audio_count);
        }
    }
}

void SdrProcessor::setFrequency(long long frequencyHz) {
    // This is a placeholder – actual frequency control is implemented
    // via libusb_control_transfer in native-lib.cpp
    LOGI("Frequency set to %lld Hz", frequencyHz);
}

void SdrProcessor::setSampleRate(int new_sample_rate) {
    if (new_sample_rate > 0) {
        sample_rate = new_sample_rate;
        decimation_factor = sample_rate / audio_sample_rate;
        if (decimation_factor < 1) decimation_factor = 1;
        LOGI("Sample rate set to %d Hz, decimation factor %d",
             sample_rate, decimation_factor);
    }
}

void SdrProcessor::setDemodMode(DemodMode mode) {
    if (demod) {
        demod->set_mode(mode);
        LOGI("Demod mode set to %d", static_cast<int>(mode));
    }
}

void SdrProcessor::setIqFormat(int format) {
    iq_format = format;
    LOGI("IQ format set to %d (0=8-bit, 1=16-bit)", format);
}

void SdrProcessor::cleanup() {
    if (fft) {
        fft->cleanup();
        delete fft;
        fft = nullptr;
    }
    if (demod) {
        delete demod;
        demod = nullptr;
    }
    fft_initialized = false;
    LOGI("SDR Processor cleanup complete");
}

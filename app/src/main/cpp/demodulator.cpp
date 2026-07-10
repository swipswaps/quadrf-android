#include "demodulator.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "Demodulator"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/*
 * Verbatim citation from RF Analyzer features:
 * "Audio demodulation: CW, AM, nFM, wFM, LSB, USB"
 * Source: github.com/demantz/RFAnalyzer
 */

Demodulator::Demodulator() : mode(DemodMode::AM) {}

void Demodulator::set_mode(DemodMode mode) {
    this->mode = mode;
    LOGI("Demodulator mode set to %d", static_cast<int>(mode));
}

int Demodulator::demodulate(const float* iq_data, int num_samples, float* audio_out) {
    if (!iq_data || !audio_out || num_samples <= 0) return 0;

    int samples_out = 0;

    switch (mode) {
        case DemodMode::AM:
            samples_out = demodulate_am(iq_data, num_samples, audio_out);
            break;
        case DemodMode::FM:
            samples_out = demodulate_fm(iq_data, num_samples, audio_out);
            break;
        case DemodMode::LSB:
        case DemodMode::USB:
            samples_out = demodulate_ssb(iq_data, num_samples, audio_out, mode);
            break;
        case DemodMode::CW:
            samples_out = demodulate_cw(iq_data, num_samples, audio_out);
            break;
        default:
            break;
    }

    return samples_out;
}

int Demodulator::demodulate_am(const float* iq, int n, float* out) {
    for (int i = 0; i < n; i++) {
        float I = iq[2*i];
        float Q = iq[2*i+1];
        out[i] = sqrtf(I*I + Q*Q);
    }
    return n;
}

int Demodulator::demodulate_fm(const float* iq, int n, float* out) {
    static float phase_prev = 0.0f;
    for (int i = 0; i < n; i++) {
        float I = iq[2*i];
        float Q = iq[2*i+1];
        float phase = atan2f(Q, I);
        float diff = phase - phase_prev;
        // Unwrap phase
        if (diff > M_PI) diff -= 2 * M_PI;
        if (diff < -M_PI) diff += 2 * M_PI;
        out[i] = diff;
        phase_prev = phase;
    }
    return n;
}

int Demodulator::demodulate_ssb(const float* iq, int n, float* out, DemodMode mode) {
    // SSB demodulation: mix with carrier and filter
    // Simplified implementation – real SSB requires Hilbert transform
    for (int i = 0; i < n; i++) {
        float I = iq[2*i];
        float Q = iq[2*i+1];
        if (mode == DemodMode::USB) {
            out[i] = I - Q; // USB approximation
        } else { // LSB
            out[i] = I + Q; // LSB approximation
        }
    }
    return n;
}

int Demodulator::demodulate_cw(const float* iq, int n, float* out) {
    // CW demodulation: mix with BFO and low-pass filter
    for (int i = 0; i < n; i++) {
        float I = iq[2*i];
        float Q = iq[2*i+1];
        // Simple envelope detection
        out[i] = sqrtf(I*I + Q*Q);
    }
    return n;
}

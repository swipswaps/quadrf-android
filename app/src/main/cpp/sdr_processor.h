#ifndef SDR_PROCESSOR_H
#define SDR_PROCESSOR_H

#include "demodulator.h"
#include <atomic>

class FFTWrapper;

class SdrProcessor {
public:
    SdrProcessor();
    ~SdrProcessor();

    void init();
    void process_iq(const unsigned char* data, int length);
    void setFrequency(long long frequencyHz);
    void setSampleRate(int sample_rate);
    void setDemodMode(DemodMode mode);
    void setIqFormat(int format);
    void cleanup();

private:
    bool fft_initialized;
    int sample_rate;
    int fft_size;
    int audio_sample_rate;
    int decimation_factor;
    int iq_format;  // 0 = 8-bit unsigned, 1 = 16-bit signed
    FFTWrapper* fft;
    Demodulator* demod;
    float time;
};

#endif // SDR_PROCESSOR_H

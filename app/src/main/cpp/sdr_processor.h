#ifndef SDR_PROCESSOR_H
#define SDR_PROCESSOR_H

#include <atomic>

class SdrProcessor {
public:
    SdrProcessor();
    ~SdrProcessor();

    void init();
    void process_iq(const unsigned char* data, int length);
    void cleanup();

private:
    bool fft_initialized;
    int sample_rate;
    float time = 0.0f;
};

#endif // SDR_PROCESSOR_H

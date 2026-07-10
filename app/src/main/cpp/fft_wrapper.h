#ifndef FFT_WRAPPER_H
#define FFT_WRAPPER_H

#include <fftw3.h>

class FFTWrapper {
public:
    FFTWrapper();
    ~FFTWrapper();

    bool init(int fft_size);
    void execute(float* iq_data, float* spectrum);
    void cleanup();

private:
    fftwf_plan plan;
    fftwf_complex* input;
    fftwf_complex* output;
    int size;
};

#endif // FFT_WRAPPER_H

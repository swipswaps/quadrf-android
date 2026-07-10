#ifndef DEMODULATOR_H
#define DEMODULATOR_H

enum class DemodMode {
    AM,
    FM,
    LSB,
    USB,
    CW
};

class Demodulator {
public:
    Demodulator();

    void set_mode(DemodMode mode);
    int demodulate(const float* iq_data, int num_samples, float* audio_out);

private:
    DemodMode mode;

    int demodulate_am(const float* iq, int n, float* out);
    int demodulate_fm(const float* iq, int n, float* out);
    int demodulate_ssb(const float* iq, int n, float* out, DemodMode mode);
    int demodulate_cw(const float* iq, int n, float* out);
};

#endif // DEMODULATOR_H

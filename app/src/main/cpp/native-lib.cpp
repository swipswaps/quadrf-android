#include <jni.h>
#include <android/log.h>
#include <libusb-1.0/libusb.h>
#include <fftw3.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <cmath>

#define LOG_TAG "QuadRFNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------- Global state ----------
static JavaVM* gJavaVM = nullptr;
static jobject gCallback = nullptr;
static jclass gCallbackClass = nullptr;
static jmethodID onSpectrumUpdateMID = nullptr;
static jmethodID onAudioSamplesMID = nullptr;
static jmethodID onNativeErrorMID = nullptr;

static libusb_device_handle* gDevHandle = nullptr;
static libusb_context* gCtx = nullptr;
static std::thread sdrThread;
static std::atomic<bool> stopSdr{false};
static std::atomic<bool> callbackReady{false};

// FFTW plans and buffers
static fftwf_plan fft_plan = nullptr;
static fftwf_complex* fft_in = nullptr;
static fftwf_complex* fft_out = nullptr;
static const int FFT_SIZE = 256;  // adjust as needed

// IQ format: 0 = 8-bit unsigned (default), 1 = 16-bit signed little-endian
static int iq_format = 0;

// ---------- JNI_OnLoad ----------
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJavaVM = vm;
    LOGI("JNI_OnLoad: JavaVM cached");
    return JNI_VERSION_1_6;
}

// ---------- Helper: send spectrum to Java ----------
void sendSpectrumToJava(float* spectrum, int size) {
    if (!callbackReady || !gJavaVM || !gCallback) return;
    JNIEnv* env = nullptr;
    int attachResult = gJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (attachResult == JNI_EDETACHED) {
        attachResult = gJavaVM->AttachCurrentThread(&env, nullptr);
        if (attachResult != JNI_OK) return;
    }
    if (!env || !gCallbackClass || !onSpectrumUpdateMID) {
        if (attachResult == JNI_OK) gJavaVM->DetachCurrentThread();
        return;
    }
    jfloatArray jSpectrum = env->NewFloatArray(size);
    env->SetFloatArrayRegion(jSpectrum, 0, size, spectrum);
    env->CallVoidMethod(gCallback, onSpectrumUpdateMID, jSpectrum, size);
    env->DeleteLocalRef(jSpectrum);
    if (attachResult == JNI_OK) gJavaVM->DetachCurrentThread();
}

// ---------- Helper: send audio to Java ----------
void sendAudioToJava(short* samples, int count) {
    if (!callbackReady || !gJavaVM || !gCallback || count <= 0) return;
    JNIEnv* env = nullptr;
    int attachResult = gJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (attachResult == JNI_EDETACHED) {
        attachResult = gJavaVM->AttachCurrentThread(&env, nullptr);
        if (attachResult != JNI_OK) return;
    }
    if (!env || !gCallbackClass || !onAudioSamplesMID) {
        if (attachResult == JNI_OK) gJavaVM->DetachCurrentThread();
        return;
    }
    jshortArray jAudio = env->NewShortArray(count);
    env->SetShortArrayRegion(jAudio, 0, count, samples);
    env->CallVoidMethod(gCallback, onAudioSamplesMID, jAudio, count);
    env->DeleteLocalRef(jAudio);
    if (attachResult == JNI_OK) gJavaVM->DetachCurrentThread();
}

// ---------- Helper: send error to Java ----------
void sendErrorToJava(const char* msg) {
    if (!callbackReady || !gJavaVM || !gCallback) return;
    JNIEnv* env = nullptr;
    int attachResult = gJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (attachResult == JNI_EDETACHED) {
        attachResult = gJavaVM->AttachCurrentThread(&env, nullptr);
        if (attachResult != JNI_OK) return;
    }
    if (!env || !gCallbackClass || !onNativeErrorMID) {
        if (attachResult == JNI_OK) gJavaVM->DetachCurrentThread();
        return;
    }
    jstring jMsg = env->NewStringUTF(msg);
    env->CallVoidMethod(gCallback, onNativeErrorMID, jMsg);
    env->DeleteLocalRef(jMsg);
    if (attachResult == JNI_OK) gJavaVM->DetachCurrentThread();
}

// ---------- JNI methods ----------
extern "C" {

JNIEXPORT void JNICALL
Java_com_quadrf_usbhost_SdrService_startSdrBackend(
        JNIEnv *env, jobject thiz, jint usbFileDescriptor, jobject callback) {
    LOGI("startSdrBackend fd=%d", usbFileDescriptor);
    if (sdrThread.joinable()) { stopSdr = true; sdrThread.join(); stopSdr = false; }
    // Cache callback
    if (gCallback) env->DeleteGlobalRef(gCallback);
    gCallback = env->NewGlobalRef(callback);
    jclass clazz = env->GetObjectClass(callback);
    gCallbackClass = (jclass)env->NewGlobalRef(clazz);
    onSpectrumUpdateMID = env->GetMethodID(clazz, "onSpectrumUpdate", "([FI)V");
    onAudioSamplesMID = env->GetMethodID(clazz, "onAudioSamples", "([SI)V");
    onNativeErrorMID = env->GetMethodID(clazz, "onNativeError", "(Ljava/lang/String;)V");
    env->DeleteLocalRef(clazz);
    callbackReady = true;
    // Init FFTW
    fft_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
    fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
    fft_plan = fftwf_plan_dft_1d(FFT_SIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
    sdrThread = std::thread(sdr_processing_loop, usbFileDescriptor);
}

JNIEXPORT void JNICALL
Java_com_quadrf_usbhost_SdrService_stopSdrBackend(JNIEnv *env, jobject thiz) {
    LOGI("stopSdrBackend");
    stopSdr = true;
    if (sdrThread.joinable()) sdrThread.join();
    callbackReady = false;
    if (gCallback) { env->DeleteGlobalRef(gCallback); gCallback = nullptr; }
    if (gCallbackClass) { env->DeleteGlobalRef(gCallbackClass); gCallbackClass = nullptr; }
    if (fft_plan) { fftwf_destroy_plan(fft_plan); fft_plan = nullptr; }
    if (fft_in) { fftwf_free(fft_in); fft_in = nullptr; }
    if (fft_out) { fftwf_free(fft_out); fft_out = nullptr; }
    if (gDevHandle) { libusb_release_interface(gDevHandle, 0); libusb_close(gDevHandle); gDevHandle = nullptr; }
    if (gCtx) { libusb_exit(gCtx); gCtx = nullptr; }
}

JNIEXPORT void JNICALL
Java_com_quadrf_usbhost_SdrService_setFrequencyNative(JNIEnv *env, jobject thiz, jlong frequencyHz) {
    LOGI("setFrequencyNative %lld Hz", frequencyHz);
    if (!gDevHandle) { sendErrorToJava("Device not connected"); return; }
    // Replace with actual Quad RF control command
    uint8_t bmRequestType = 0x40;
    uint8_t bRequest = 0x01;
    uint16_t wValue = (frequencyHz >> 16) & 0xFFFF;
    uint16_t wIndex = frequencyHz & 0xFFFF;
    int r = libusb_control_transfer(gDevHandle, bmRequestType, bRequest,
                                    wValue, wIndex, nullptr, 0, 1000);
    if (r < 0) {
        LOGE("libusb_control_transfer (freq) failed: %s", libusb_error_name(r));
        sendErrorToJava("Failed to set frequency");
    }
}

JNIEXPORT void JNICALL
Java_com_quadrf_usbhost_SdrService_setGainNative(JNIEnv *env, jobject thiz, jint gainDb) {
    LOGI("setGainNative %d dB", gainDb);
    if (!gDevHandle) { sendErrorToJava("Device not connected"); return; }
    uint8_t bmRequestType = 0x40;
    uint8_t bRequest = 0x02;
    uint16_t wValue = gainDb;
    int r = libusb_control_transfer(gDevHandle, bmRequestType, bRequest,
                                    wValue, 0, nullptr, 0, 1000);
    if (r < 0) {
        LOGE("libusb_control_transfer (gain) failed: %s", libusb_error_name(r));
        sendErrorToJava("Failed to set gain");
    }
}

} // extern "C"

// ---------- Main processing loop ----------
void sdr_processing_loop(int fd) {
    LOGI("Processing thread started");
    int r;
    r = libusb_init(&gCtx);
    if (r < 0) { sendErrorToJava("libusb_init failed"); return; }
#if LIBUSB_API_VERSION >= 0x01000107
    r = libusb_open_device_with_fd(gCtx, fd, &gDevHandle);
#else
    sendErrorToJava("libusb_open_device_with_fd not supported");
    libusb_exit(gCtx); gCtx = nullptr; return;
#endif
    if (r != LIBUSB_SUCCESS || !gDevHandle) {
        sendErrorToJava("Failed to open USB device");
        libusb_exit(gCtx); gCtx = nullptr; return;
    }
    r = libusb_claim_interface(gDevHandle, 0);
    if (r != LIBUSB_SUCCESS) {
        sendErrorToJava("Failed to claim interface");
        libusb_close(gDevHandle); gDevHandle = nullptr;
        libusb_exit(gCtx); gCtx = nullptr; return;
    }

    const int BUFFER_SIZE = 4096;
    unsigned char buffer[BUFFER_SIZE];
    int transferred;
    const int AUDIO_DECIMATION = 16; // 2.048 MHz / 16 = 128 kHz audio
    float iq_float[FFT_SIZE * 2];
    short audio_short[FFT_SIZE / 4];

    while (!stopSdr) {
        r = libusb_bulk_transfer(gDevHandle, 0x81, buffer, BUFFER_SIZE,
                                 &transferred, 1000);
        if (r == LIBUSB_SUCCESS) {
            // Determine number of IQ samples based on format
            int num_samples = 0;
            if (iq_format == 0) { // 8-bit unsigned
                num_samples = transferred / 2;
                if (num_samples > FFT_SIZE) num_samples = FFT_SIZE;
                for (int i = 0; i < num_samples; i++) {
                    float I = (buffer[2*i] - 128.0f) / 128.0f;
                    float Q = (buffer[2*i+1] - 128.0f) / 128.0f;
                    iq_float[2*i] = I;
                    iq_float[2*i+1] = Q;
                }
            } else { // 16-bit signed little-endian (assume)
                num_samples = transferred / 4;
                if (num_samples > FFT_SIZE) num_samples = FFT_SIZE;
                for (int i = 0; i < num_samples; i++) {
                    int16_t I = buffer[4*i] | (buffer[4*i+1] << 8);
                    int16_t Q = buffer[4*i+2] | (buffer[4*i+3] << 8);
                    iq_float[2*i] = I / 32768.0f;
                    iq_float[2*i+1] = Q / 32768.0f;
                }
            }

            // ----- FFT using FFTW -----
            for (int i = 0; i < num_samples && i < FFT_SIZE; i++) {
                fft_in[i][0] = iq_float[2*i];
                fft_in[i][1] = iq_float[2*i+1];
            }
            // Zero-pad if needed
            for (int i = num_samples; i < FFT_SIZE; i++) {
                fft_in[i][0] = 0.0f;
                fft_in[i][1] = 0.0f;
            }
            fftwf_execute(fft_plan);
            float spectrum[FFT_SIZE];
            for (int i = 0; i < FFT_SIZE; i++) {
                float real = fft_out[i][0];
                float imag = fft_out[i][1];
                spectrum[i] = sqrtf(real*real + imag*imag);
            }
            // Normalize and send
            float max_val = 0.0f;
            for (int i = 0; i < FFT_SIZE; i++) if (spectrum[i] > max_val) max_val = spectrum[i];
            if (max_val > 0.0f) {
                for (int i = 0; i < FFT_SIZE; i++) spectrum[i] /= max_val;
            }
            sendSpectrumToJava(spectrum, FFT_SIZE);

            // ----- Audio: AM demodulation -----
            int audio_count = 0;
            for (int i = 0; i < num_samples; i += AUDIO_DECIMATION) {
                if (audio_count >= FFT_SIZE/4) break;
                float I = iq_float[2*i];
                float Q = iq_float[2*i+1];
                float env = sqrtf(I*I + Q*Q);
                short sample = (short)(env * 32767.0f);
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                audio_short[audio_count++] = sample;
            }
            if (audio_count > 0) sendAudioToJava(audio_short, audio_count);

        } else if (r != LIBUSB_ERROR_TIMEOUT) {
            LOGE("Bulk transfer error: %s", libusb_error_name(r));
            sendErrorToJava("USB bulk transfer error");
            break;
        }
    }

    // Cleanup
    if (gDevHandle) { libusb_release_interface(gDevHandle, 0); libusb_close(gDevHandle); gDevHandle = nullptr; }
    if (gCtx) { libusb_exit(gCtx); gCtx = nullptr; }
    callbackReady = false;
    LOGI("Processing thread exiting");
}

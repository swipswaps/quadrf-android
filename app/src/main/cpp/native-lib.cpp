#include <jni.h>
#include <android/log.h>
#include <libusb-1.0/libusb.h>
#include <fftw3.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <cmath>

#include "sdr_processor.h"

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
static std::atomic<bool> deviceOpen{false};

// FFTW plans and buffers
static fftwf_plan fft_plan = nullptr;
static fftwf_complex* fft_in = nullptr;
static fftwf_complex* fft_out = nullptr;
static const int FFT_SIZE = 256;

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

    if (sdrThread.joinable()) {
        stopSdr = true;
        sdrThread.join();
        stopSdr = false;
    }

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

    if (gDevHandle) {
        libusb_release_interface(gDevHandle, 0);
        libusb_close(gDevHandle);
        gDevHandle = nullptr;
    }
    if (gCtx) {
        libusb_exit(gCtx);
        gCtx = nullptr;
    }
    deviceOpen = false;
}

JNIEXPORT void JNICALL
Java_com_quadrf_usbhost_SdrService_setFrequencyNative(JNIEnv *env, jobject thiz, jlong frequencyHz) {
    LOGI("setFrequencyNative %lld Hz", frequencyHz);
    if (!gDevHandle) {
        sendErrorToJava("Device not connected");
        return;
    }

    /*
     * Verbatim citation from HackRF issue:
     * "hackrf_set_freq function internally calls libusb_control_transfer, from the
     * libusb synchronous API, to issue a request and then wait for it to complete."
     * Source: github.com/greatscottgadgets/hackrf/issues/1357
     *
     * Replace these placeholder values with actual Quad RF vendor commands.
     */
    uint8_t bmRequestType = 0x40;  // Vendor, OUT, to device
    uint8_t bRequest = 0x01;       // Vendor-specific command for frequency
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
    if (!gDevHandle) {
        sendErrorToJava("Device not connected");
        return;
    }

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

// ---------- Main processing loop with error recovery ----------
/*
 * Verbatim citation from Stevens & Rago (2013), Advanced Programming in the
 * UNIX Environment, 3rd ed., Addison-Wesley, ISBN 978-0-321-63773-4:
 * "File descriptors are normally small non-negative integers that the kernel
 * uses to identify the files being accessed by a particular process."
 *
 * Verbatim citation from libusb asynchronous API documentation:
 * "The asynchronous API is designed to allow you to execute multiple transfer
 * operations in parallel without blocking your application."
 * Source: libusb.sourceforge.net/api-1.0/group__asyncio.html
 */
void sdr_processing_loop(int fd) {
    LOGI("Processing thread started");
    int r;

    // Initialize libusb
    r = libusb_init(&gCtx);
    if (r < 0) {
        LOGE("libusb_init failed: %s", libusb_error_name(r));
        sendErrorToJava("libusb_init failed");
        return;
    }

    // Open device from file descriptor
#if LIBUSB_API_VERSION >= 0x01000107
    r = libusb_open_device_with_fd(gCtx, fd, &gDevHandle);
#else
    LOGE("libusb_open_device_with_fd not supported");
    sendErrorToJava("libusb_open_device_with_fd not supported");
    libusb_exit(gCtx);
    gCtx = nullptr;
    return;
#endif

    if (r != LIBUSB_SUCCESS || gDevHandle == nullptr) {
        LOGE("Failed to open device with fd: %s", libusb_error_name(r));
        sendErrorToJava("Failed to open USB device");
        libusb_exit(gCtx);
        gCtx = nullptr;
        return;
    }
    deviceOpen = true;
    LOGI("Device opened successfully.");

    // Claim interface 0 (adjust based on Quad RF)
    r = libusb_claim_interface(gDevHandle, 0);
    if (r != LIBUSB_SUCCESS) {
        LOGE("libusb_claim_interface failed: %s", libusb_error_name(r));
        sendErrorToJava("Failed to claim interface");
        libusb_close(gDevHandle);
        gDevHandle = nullptr;
        libusb_exit(gCtx);
        gCtx = nullptr;
        deviceOpen = false;
        return;
    }
    LOGI("Interface claimed.");

    // Initialize SDR processor
    SdrProcessor processor;
    processor.init();
    processor.setIqFormat(iq_format);

    const int BUFFER_SIZE = 4096;
    unsigned char buffer[BUFFER_SIZE];
    int transferred;

    LOGI("Entering main loop");

    while (!stopSdr && deviceOpen) {
        r = libusb_bulk_transfer(gDevHandle, 0x81, buffer, BUFFER_SIZE,
                                 &transferred, 1000);
        if (r == LIBUSB_SUCCESS) {
            // Process IQ data through the SDR pipeline
            processor.process_iq(buffer, transferred);
        } else if (r == LIBUSB_ERROR_TIMEOUT) {
            // Timeout is expected; just continue
        } else if (r == LIBUSB_ERROR_PIPE) {
            // USB pipe error – attempt to recover
            LOGE("USB pipe error, attempting recovery");
            sendErrorToJava("USB pipe error – recovering");
            libusb_clear_halt(gDevHandle, 0x81);
        } else if (r == LIBUSB_ERROR_NO_DEVICE) {
            // Device was disconnected
            LOGE("Device disconnected");
            sendErrorToJava("USB device disconnected");
            deviceOpen = false;
            break;
        } else {
            LOGE("Bulk transfer error: %s", libusb_error_name(r));
            sendErrorToJava("USB bulk transfer error");
            break;
        }
    }

    LOGI("Processing loop exiting.");

    // Cleanup
    processor.cleanup();
    if (gDevHandle) {
        libusb_release_interface(gDevHandle, 0);
        libusb_close(gDevHandle);
        gDevHandle = nullptr;
    }
    if (gCtx) {
        libusb_exit(gCtx);
        gCtx = nullptr;
    }
    deviceOpen = false;
    callbackReady = false;
    LOGI("Processing thread exiting");
}

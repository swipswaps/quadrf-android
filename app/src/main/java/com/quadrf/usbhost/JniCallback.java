package com.quadrf.usbhost;

/*
 * Interface for native-to-Java callbacks.
 * The native library calls these methods to update the UI and play audio.
 *
 * Verbatim citation from JNI specification:
 * "JNI is a native programming interface. It allows Java code that runs inside
 * a Java Virtual Machine (JVM) to interoperate with applications and libraries
 * written in other languages."
 * Source: docs.oracle.com/javase/8/docs/technotes/guides/jni/
 *
 * This interface enables the native C++ thread to communicate with the Java
 * service without blocking the UI thread. It follows the observer pattern,
 * decoupling the signal processing pipeline from the presentation layer.
 */
public interface JniCallback {
    // Called when new spectrum data is ready (for waterfall/spectrum views)
    void onSpectrumUpdate(float[] spectrum, int size);

    // Called when audio samples are ready to be played
    void onAudioSamples(short[] samples, int count);

    // Called when an error occurs in the native thread
    void onNativeError(String message);
}

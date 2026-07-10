# ProGuard rules for Quad RF Android App

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep JNI callback classes
-keep class com.quadrf.usbhost.** { *; }

# Keep libusb and FFTW native calls
-keep class org.libusb.** { *; }

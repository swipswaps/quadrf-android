#!/bin/bash
# Build script for libusb for Android
# Source: libusb.info

set -e

LIBUSB_VERSION="1.0.27"
ANDROID_NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk-bundle}"

if [ ! -d "$ANDROID_NDK" ]; then
    echo "ERROR: Android NDK not found at $ANDROID_NDK"
    echo "Please set ANDROID_NDK environment variable"
    exit 1
fi

# Download libusb
wget https://github.com/libusb/libusb/releases/download/v${LIBUSB_VERSION}/libusb-${LIBUSB_VERSION}.tar.bz2
tar -xjf libusb-${LIBUSB_VERSION}.tar.bz2
cd libusb-${LIBUSB_VERSION}

# Build for arm64-v8a
mkdir -p build-arm64
cd build-arm64
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=21 \
    -DBUILD_SHARED_LIBS=OFF
make -j4
cp lib/libusb-1.0.a ../../app/libs/libusb/lib/
cp ../libusb/*.h ../../app/libs/libusb/include/libusb-1.0/
cd ../..

echo "libusb built successfully for arm64-v8a"

#!/bin/bash
# Build script for FFTW for Android
# Source: www.fftw.org

set -e

FFTW_VERSION="3.3.10"
ANDROID_NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk-bundle}"

if [ ! -d "$ANDROID_NDK" ]; then
    echo "ERROR: Android NDK not found at $ANDROID_NDK"
    echo "Please set ANDROID_NDK environment variable"
    exit 1
fi

# Download FFTW
wget https://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz
tar -xzf fftw-${FFTW_VERSION}.tar.gz
cd fftw-${FFTW_VERSION}

# Build for arm64-v8a
mkdir -p build-arm64
cd build-arm64
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=21 \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_FLOAT=ON
make -j4
cp libfftw3.a ../../app/libs/fftw/lib/
cp ../api/fftw3.h ../../app/libs/fftw/include/
cd ../..

echo "FFTW built successfully for arm64-v8a"

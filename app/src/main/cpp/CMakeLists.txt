cmake_minimum_required(VERSION 3.10.2)

project("quadrfsdr")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(quadrf_sdr_jni SHARED
    native-lib.cpp
    sdr_processor.cpp
    fft_wrapper.cpp
    demodulator.cpp
)

target_include_directories(quadrf_sdr_jni PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/libusb/include/libusb-1.0
    ${CMAKE_SOURCE_DIR}/libs/fftw/include
)

find_library(libusb_location
    NAMES usb-1.0
    PATHS ${CMAKE_SOURCE_DIR}/libs/libusb/lib
    NO_DEFAULT_PATH
)
if(NOT libusb_location)
    message(FATAL_ERROR "libusb not found at ${CMAKE_SOURCE_DIR}/libs/libusb/lib")
endif()

find_library(fftw_location
    NAMES fftw3
    PATHS ${CMAKE_SOURCE_DIR}/libs/fftw/lib
    NO_DEFAULT_PATH
)
if(NOT fftw_location)
    message(FATAL_ERROR "fftw not found at ${CMAKE_SOURCE_DIR}/libs/fftw/lib")
endif()

target_link_libraries(quadrf_sdr_jni
    ${libusb_location}
    ${fftw_location}
    log
)

find_library(log-lib log)
target_link_libraries(quadrf_sdr_jni ${log-lib})

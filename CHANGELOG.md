# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project adheres to
Semantic Versioning.

## [Unreleased]

### Added
- (None – all planned features for 0.2.0 are now released)

## [0.2.0] - 2026-07-10

### Added
- Full JNI callback system (spectrum, audio, error) with proper thread attachment
- Audio playback via AudioTrack (PCM streaming)
- USB control transfers for frequency and gain setting
- Real FFTW integration for spectrum computation
- Gain SeekBar and frequency SeekBar in UI
- Broadcast of spectrum updates from SdrService to MainActivity
- USB detachment handling to prevent crashes
- Configurable IQ sample format (8-bit unsigned / 16-bit signed little-endian)
- Build scripts for libusb and FFTW (scripts/build_*.sh)
- Comprehensive documentation updates

### Changed
- native-lib.cpp now uses FFTW and control transfers
- SdrService now implements JniCallback
- MainActivity now handles gain and frequency controls

### Fixed
- USB permission handling with proper PendingIntent
- Thread lifecycle management with std::atomic and join()
- Graceful shutdown on USB detachment

## [0.1.0] - 2026-07-10

### Added
- Initial release (Proof of Concept)
- USB device detection and permission request
- File descriptor passing to native code via JNI
- libusb initialisation and bulk transfer loop
- Basic APK build and installation
- Initial documentation (README, CONTRIBUTING, CODE_OF_CONDUCT)

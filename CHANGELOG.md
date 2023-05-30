# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.7.0] - Unreleased
### Added
- Add support to ESP-IDF v5.0.
- Add support for `io.edgehog.devicemanager.OTAEvent` interface.
- Add support for update/cancel operation in `io.edgehog.devicemanager.OTARequest` interface.

### Removed
- Remove support for `io.edgehog.devicemanager.OTAResponse` interface.

### Fixed
- Fix GPIO still active issue after execution of LedBehavior routine.

## [0.5.2] - 2022-06-22

## [0.5.1] - 2022-06-01
### Added
- Send `"connected": true` in WiFiScanResults when the device is connected to the AP.

### Fixed
- Minor fixes.

## [0.5.0] - 2022-03-22
### Added
- Initial Edgehog release.

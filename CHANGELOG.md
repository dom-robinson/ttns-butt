# Changelog

All notable TTNS-specific changes to this fork are documented here.
Upstream BUTT release notes remain in `NEWS`.

## [Unreleased] — TTNS macOS build

### Added

- `README.TTNS.md` — fork overview, build instructions, planned customizations
- Homebrew path detection in `configure.ac` (`/opt/homebrew`, `/usr/local`)
- macOS autotools build: Objective-C++ compile mode and `Fl_My_Native_File_Chooser_MAC.mm` in the Darwin makefile
- `AC_PROG_OBJCXX` and `DARWIN` automake conditional

### Fixed

- `port_audio.cpp`: compatible with libsamplerate 0.2.x (`SRC_DATA.data_in` is `const float *`)
- Link failure when `-x objective-c++` was passed to the linker via global `OBJCXXFLAGS`

### Verified

- Clean `./configure && make` on macOS arm64 (Apple Silicon) with Homebrew dependencies
- Binary output: `src/butt`

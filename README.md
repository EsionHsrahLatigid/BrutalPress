# BrutalPress

BrutalPress is a stereo input/output YUP audio effect for extreme three-band upward and downward compression. It is packaged as an independent project with no source or build references to sibling products.

The current project builds a Standalone app, VST3 bundle, and Audio Unit v2 effect on Apple platforms. Plugin identity is stable:

| Field | Value |
| --- | --- |
| App ID | `audio.2bit.brutalpress` |
| Plugin ID | `audio.2bit.BrutalPress` |
| AU subtype | `BrPr` |
| AU type | `aufx` effect |
| Channels | stereo input, stereo output |

## Sound engine

The engine is pure C++20 DSP. It splits stereo input into low, mid, and high bands, applies linked downward compression and optional low-level upward compression per band, blends band gain reduction through a glue control, then enforces a linked lookahead ceiling. The audio processing path uses fixed-size state and performs no allocation, file access, or random-device calls.

## Parameters

| Parameter | Function |
| --- | --- |
| Crush | Links lower thresholds, harder ratios, softer knee, and makeup gain |
| Upward | Raises low-level material before final limiting |
| Attack ms | Fast peak detector response |
| Release ms | Gain recovery and RMS-ish detector response |
| Low Split | Low/mid crossover |
| High Split | Mid/high crossover |
| Glue | Links band gains toward their shared gain reduction |
| Ceiling dB | Linked lookahead limiter ceiling |
| Mix | Dry/wet blend before final limiting |

## Requirements

- macOS 11 or newer for AU/Standalone builds, or Windows 2025 for Windows Standalone/VST3 builds
- Apple Clang, MSVC, or another supported compiler with C++20 support
- CMake 3.31 or newer
- Ninja
- Xcode / macOS SDK for AU builds
- A local YUP checkout at `../yup`, or network access for the pinned fallback checkout

YUP is pinned to commit `9a1c9bc699b6a714f6f52486462d98a140c8bf95` when the adjacent checkout is absent. YUP is ISC-licensed; its own license and fetched dependency licenses remain authoritative.

## Build and test

Fast DSP-only loop:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

Release app and plugins:

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release
```

Artifacts:

- `build/plugin-release/brutalpress_standalone_plugin.app`
- `build/plugin-release/VST3/Release/brutalpress_vst3_plugin.vst3`
- `build/plugin-release/brutalpress_au_plugin.component`

On Windows, CI packages the generated standalone `.exe` and VST3 bundle as `BrutalPress-latest-windows-x64.zip`.

## Continuous integration and releases

GitHub Actions tests and packages macOS 26 arm64 and Windows 2025 x64 builds. Main-branch and pull-request runs publish `latest` ZIP artifacts; a `v*` tag creates or updates one GitHub Release containing versioned ZIPs for both platforms.

The local macOS build ad-hoc signs the standalone app and VST3 bundle. Distribution still requires a Developer ID signing and notarization workflow.

## Verification covered

The engine tests cover crest-factor reduction, upward lift, output ceiling compliance, silence preservation, deterministic rendering, and finite bounded output for non-finite parameter/input cases.

## Current limits

- The editor is functional and intentionally minimal.
- No universal binary has been produced yet; local verification is expected to follow the active compiler architecture.
- Full DAW scanning, AU/VST3 host validation, listening, and calibrated loudness tests remain host-specific follow-up work.

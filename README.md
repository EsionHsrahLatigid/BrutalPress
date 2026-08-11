# BrutalPress

BrutalPress is a stereo input/output YUP audio effect for extreme three-band upward and downward compression. It is packaged as an independent project with no source or build references to sibling products.

The current project builds a Standalone app, VST3 bundle, and Audio Unit v2 effect on Apple platforms. Plugin identity is stable:

| Field | Value |
| --- | --- |
| App ID | `jp.ehl.brutalpress` |
| Plugin ID | `jp.ehl.brutalpress` |
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
cmake --build --preset plugin-release --parallel --target brutalpress_release_bundles brutalpress_engine_tests
ctest --preset plugin-release
```

Artifacts:

- `brutalpress_release_bundles`
- `artifacts/plugin-release/macos-arm64/standalone/brutalpress_standalone_plugin.app`
- `artifacts/plugin-release/macos-arm64/vst3/brutalpress_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/brutalpress_au_plugin.component`

`build/` is CMake's internal workspace. Human-facing products are always staged under `artifacts/plugin-release/<platform-arch>/`; Windows uses `windows-x64` with `standalone/` and `vst3/`.

On Windows, CI packages the generated standalone `.exe` and VST3 bundle as `BrutalPress-latest-windows-x64.zip`.

## Continuous integration and releases

`.github/workflows/ci.yml` is the required CI entrypoint for pushes to `main`, pull requests, and manual runs. A lightweight Linux classifier always runs. Changes limited to `README.md`, `DESIGN.md`, `LICENSE`, `docs/**`, or `.github/ISSUE_TEMPLATE/**` skip the heavy jobs; every other change runs Debug tests and Release bundle builds on macOS 26 arm64 and Windows 2025 x64. Manual dispatches default to forcing both heavy jobs.

Successful heavy runs upload two immutable, 14-day artifacts, each containing a platform ZIP plus a strict single-line SHA-256 manifest:

- `BrutalPress-latest-macos-arm64`, containing `BrutalPress-latest-macos-arm64.zip` and `SHA256SUMS.txt`
- `BrutalPress-latest-windows-x64`, containing `BrutalPress-latest-windows-x64.zip` and `SHA256SUMS.txt`

`.github/workflows/release.yml` is the only `v*` tag workflow. It performs no compilation. The Ubuntu release job resolves lightweight or annotated tags to a commit, requires the tag version to match the CMake project version, requires one successful `CI` push run on `main` for that exact SHA, downloads exactly the two expected unexpired artifacts by ID, verifies their SHA-256 manifests and ZIP integrity, then publishes versioned assets such as `BrutalPress-0.1.1-macos-arm64.zip` and `BrutalPress-0.1.1-windows-x64.zip`. Publication uses a draft release whose asset list is sanitized and rechecked to contain exactly those two ZIPs. A missing, expired, ambiguous, or mismatched provenance chain fails closed.

Release operator sequence: merge or push the version commit to `main`, wait for both platform jobs and `CI Summary` to pass, then create and push the version tag. Never move or reuse a published tag; correct the source and use the next patch version instead.

The local macOS build ad-hoc signs the standalone app and VST3 bundle. Distribution still requires a Developer ID signing and notarization workflow.

## Verification covered

The engine tests cover crest-factor reduction, upward lift, output ceiling compliance, silence preservation, deterministic rendering, and finite bounded output for non-finite parameter/input cases.

## Current limits

- The editor is functional and intentionally minimal.
- No universal binary has been produced yet; local verification is expected to follow the active compiler architecture.
- Full DAW scanning, AU/VST3 host validation, listening, and calibrated loudness tests remain host-specific follow-up work.

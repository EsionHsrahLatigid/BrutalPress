# Design

## Source of truth

- Status: Active
- Last refreshed: 2026-08-08
- Primary product surfaces: macOS Standalone, VST3 editor, AUv2 editor
- Evidence reviewed: extracted BrutalPress engine, YUP wrapper, shared parameter editor, state helper, and tests

## Brand

- Personality: blunt dynamics abuse, pressure control, clinical danger labeling
- Trust signals: explicit ceiling, deterministic presets, stable parameter IDs, stereo effect routing
- Avoid: instrument language, fake metering, decorative distortion imagery, unreadable warning copy

## Product goals

- Goals: make extreme average-level pressure controllable; preserve stereo effect behavior; keep the output ceiling clear
- Non-goals: synth voice behavior, MIDI-triggered identity, vintage compressor emulation, product-suite coupling
- Success signals: stereo input remains the source, parameters automate by stable IDs, limiter ceiling stays enforced, tests remain deterministic

## Personas and jobs

- Primary personas: experimental electronic musicians, noise performers, sound designers, mix-destruction workflows
- User jobs: crush dynamic range quickly; lift quiet detail aggressively; keep a hard ceiling while auditioning destructive settings
- Key contexts of use: DAW inserts, standalone processing checks, loud monitoring with low monitor gain

## Information architecture

- Primary navigation: one-page effect panel
- Core routes/screens: parameter grid only
- Content hierarchy: compression intensity first, timing/splits second, glue/ceiling/mix last

## Design principles

- Name the risk directly.
- Keep controls technical and short.
- Prioritize ceiling and deterministic behavior over ornamental feedback.
- Preserve a quiet, dense, utilitarian editor until real DSP-state metering is added.

## Visual language

- Color: near-black field, red warning accent, neutral labels and numeric values
- Typography: compact system sans for labels and values
- Spacing/layout rhythm: fixed-ratio rotary grid
- Shape/radius/elevation: hard rectangles and circular controls; minimal shadows
- Motion: bounded value updates only
- Imagery/iconography: none until real gain-reduction or band-state telemetry exists

## Components

- Existing components to reuse: YUP `Slider`, `Label`, `AudioProcessorEditor`
- New/changed components: optional future gain-reduction and ceiling telemetry
- Variants and states: four presets, host automation, processor state save/load
- Token/component ownership: editor-local constants until YUP exposes a stable theme/token workflow

## Accessibility

- Target standard: practical desktop accessibility within current YUP capabilities
- Keyboard/focus behavior: host/YUP defaults
- Contrast/readability: warning, labels, and numeric values remain readable against the dark field
- Screen-reader semantics: constrained by current YUP accessibility support; control names must remain explicit
- Reduced motion and sensory considerations: no flashes or decorative flicker

## Responsive behavior

- Supported breakpoints/devices: desktop plugin windows and macOS Standalone
- Layout adaptations: fixed aspect ratio; parameter count determines the grid
- Touch/hover differences: rotary vertical drag remains the primary interaction; hover is nonessential

## Interaction states

- Loading: immediate deterministic initialization
- Empty: silence passes as silence
- Error: invalid/non-finite parameter values clamp safely
- Success: parameter values update visibly and audio changes deterministically
- Disabled: no hidden disabled controls
- Offline/slow network: no runtime network dependency

## Content voice

- Tone: terse, technical, direct about hearing risk
- Terminology: use real dynamics terms such as crush, upward, split, glue, ceiling, and mix
- Microcopy rules: short noun phrases; do not imply classic compressor modeling

## Implementation constraints

- Framework/styling system: C++20 and YUP GUI/audio processor modules
- Performance constraints: no allocation, file access, locks, or non-deterministic calls on the audio thread
- Compatibility constraints: macOS arm64 currently targeted; state version changes require backward-compatible migration
- Test expectations: engine regression tests, three-format release build, and metadata checks for effect identity

## CI and release contract

- `CI Summary` is the stable required check. A Linux classifier always runs; it skips macOS and Windows only for the documented docs-only allowlist and otherwise chooses the conservative heavy path.
- macOS and Windows each build, test, package, and upload one `latest` ZIP plus a strict single-line `SHA256SUMS.txt`. Actions artifacts expire after 14 days.
- Tag pushes never compile. The Release workflow resolves the tag to its commit, requires the normalized tag and CMake project versions to match, locates the unique successful canonical `CI` push run on `main` with the same `head_sha`, requires exactly the two named unexpired platform artifacts, verifies SHA-256 and ZIP integrity, sanitizes the draft asset list, and only then publishes exactly the two versioned release assets.
- Release provenance failures are terminal. Missing, expired, duplicate, or mismatched artifacts must not trigger an automatic rebuild or partial release.
- GitHub actions are pinned to immutable commit SHAs. The release runner requires GitHub CLI 2.x or newer and the minimal `actions: read` / `contents: write` permissions.

## Open questions

- [ ] Which gain-reduction or band telemetry can be exposed without audio-thread synchronization hazards?

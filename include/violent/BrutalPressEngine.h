#pragma once

#include "violent/ViolentDspPrimitives.h"

#include <array>
#include <cstddef>

namespace violent
{

/** Parameters for BrutalPressEngine.

    Public inputs are clamped by setParameters():
    - crush [0, 1] links lower thresholds, harder ratios, softer knee, and makeup.
    - upward [0, 1] enables low-level upward compression before the limiter.
    - attackSeconds [0.00005, 0.1] controls the fast peak detector.
    - releaseSeconds [0.005, 2] controls gain recovery and the RMS-ish detector.
    - lowSplitHz [40, 2000], highSplitHz [1000, 18000] define a 3-band split.
    - glue [0, 1] links bands toward their shared gain reduction.
    - ceilingDb [-24, 0] is always enforced by the linked lookahead limiter.
    - mix [0, 1] blends dry input and compressed signal before final limiting.
*/
struct BrutalPressParameters
{
    float crush = 0.65f;
    float upward = 0.0f;
    float attackSeconds = 0.002f;
    float releaseSeconds = 0.12f;
    float lowSplitHz = 180.0f;
    float highSplitHz = 3500.0f;
    float glue = 0.35f;
    float ceilingDb = -1.0f;
    float mix = 1.0f;
};

/** Stereo input compressor/limiter effect for extreme average-level pressure.

    The engine is YUP-independent C++20 DSP. It performs no heap allocation in
    prepare/process paths: all filters, envelopes, and lookahead buffers are
    fixed-size members.
*/
class BrutalPressEngine
{
public:
    BrutalPressEngine();

    /** Sets the sample rate, rebuilds coefficients, and clears state. */
    void prepare (double sampleRate) noexcept;

    /** Clears filters, envelopes, and limiter delay. */
    void reset() noexcept;

    /** Clamps and applies all public parameters. */
    void setParameters (const BrutalPressParameters& parameters) noexcept;

    /** Processes one stereo input frame and returns the delayed, ceiling-limited output. */
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;

    /** Processes interleaved stereo buffers in-place. Null buffers and non-positive sizes are ignored. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    static constexpr std::size_t numBands = 3;
    static constexpr std::size_t numChannels = 2;
    static constexpr std::size_t maxLookaheadSamples = 128;

    using BandArray = std::array<float, numBands>;
    using ChannelBandFilters = std::array<Biquad, numBands - 1>;

    struct ClampedParameters
    {
        float crush = 0.65f;
        float upward = 0.0f;
        float attackSeconds = 0.002f;
        float releaseSeconds = 0.12f;
        float lowSplitHz = 180.0f;
        float highSplitHz = 3500.0f;
        float glue = 0.35f;
        float ceilingDb = -1.0f;
        float mix = 1.0f;
    };

    void updateFilters() noexcept;
    void updateEnvelopeCoefficients() noexcept;
    [[nodiscard]] BandArray splitBands (float input, std::size_t channel) noexcept;
    [[nodiscard]] BandArray computeLinkedBandGains (const BandArray& leftBands, const BandArray& rightBands) noexcept;
    [[nodiscard]] float computeBandGain (float detector) const noexcept;
    [[nodiscard]] StereoFrame limitLinked (StereoFrame input) noexcept;
    [[nodiscard]] float scanLookaheadPeak() const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    float peakAttackCoefficient = 0.5f;
    float peakReleaseCoefficient = 0.999f;
    float rmsCoefficient = 0.999f;
    float limiterReleaseCoefficient = 0.9995f;
    float limiterGain = 1.0f;
    float ceilingGain = 0.8912509f;

    std::array<ChannelBandFilters, numChannels> crossoverFilters {};
    BandArray peakEnvelope {};
    BandArray rmsEnvelope {};
    std::array<float, maxLookaheadSamples> lookaheadLeft {};
    std::array<float, maxLookaheadSamples> lookaheadRight {};
    std::size_t lookaheadWrite = 0;
};

} // namespace violent

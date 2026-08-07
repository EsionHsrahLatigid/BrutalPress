#include "violent/BrutalPressEngine.h"

#include <algorithm>
#include <cmath>

namespace violent
{

namespace
{

[[nodiscard]] float onePoleCoefficient (double sampleRate, float seconds) noexcept
{
    const auto safeRate = std::isfinite (sampleRate) && sampleRate > 1.0 ? sampleRate : 44100.0;
    const auto safeSeconds = clampFinite (seconds, 0.00001f, 10.0f, 0.1f);
    return std::exp (-1.0f / static_cast<float> (safeRate * safeSeconds));
}

[[nodiscard]] float sanitizeAudio (float value) noexcept
{
    return clampFinite (value, -64.0f, 64.0f, 0.0f);
}

[[nodiscard]] float envelopeFollower (float previous, float target, float attackCoefficient, float releaseCoefficient) noexcept
{
    const auto coefficient = target > previous ? attackCoefficient : releaseCoefficient;
    const auto value = target + coefficient * (previous - target);
    return std::isfinite (value) ? std::max (0.0f, value) : 0.0f;
}

} // namespace

BrutalPressEngine::BrutalPressEngine()
{
    prepare (44100.0);
}

void BrutalPressEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    updateFilters();
    updateEnvelopeCoefficients();
    reset();
}

void BrutalPressEngine::reset() noexcept
{
    for (auto& channel : crossoverFilters)
        for (auto& filter : channel)
            filter.reset();

    peakEnvelope.fill (0.0f);
    rmsEnvelope.fill (0.0f);
    lookaheadLeft.fill (0.0f);
    lookaheadRight.fill (0.0f);
    lookaheadWrite = 0;
    limiterGain = 1.0f;
}

void BrutalPressEngine::setParameters (const BrutalPressParameters& parameters) noexcept
{
    params.crush = clampFinite (parameters.crush, 0.0f, 1.0f, 0.65f);
    params.upward = clampFinite (parameters.upward, 0.0f, 1.0f, 0.0f);
    params.attackSeconds = clampFinite (parameters.attackSeconds, 0.00005f, 0.1f, 0.002f);
    params.releaseSeconds = clampFinite (parameters.releaseSeconds, 0.005f, 2.0f, 0.12f);
    params.lowSplitHz = clampFinite (parameters.lowSplitHz, 40.0f, 2000.0f, 180.0f);
    params.highSplitHz = clampFinite (parameters.highSplitHz, 1000.0f, 18000.0f, 3500.0f);
    if (params.highSplitHz <= params.lowSplitHz * 1.5f)
        params.highSplitHz = std::min (18000.0f, params.lowSplitHz * 1.5f);
    params.glue = clampFinite (parameters.glue, 0.0f, 1.0f, 0.35f);
    params.ceilingDb = clampFinite (parameters.ceilingDb, -24.0f, 0.0f, -1.0f);
    params.mix = clampFinite (parameters.mix, 0.0f, 1.0f, 1.0f);

    ceilingGain = decibelsToGain (params.ceilingDb);
    updateFilters();
    updateEnvelopeCoefficients();
}

StereoFrame BrutalPressEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto dryLeft = sanitizeAudio (inputLeft);
    const auto dryRight = sanitizeAudio (inputRight);

    const auto leftBands = splitBands (dryLeft, 0);
    const auto rightBands = splitBands (dryRight, 1);
    const auto gains = computeLinkedBandGains (leftBands, rightBands);

    auto wetLeft = 0.0f;
    auto wetRight = 0.0f;
    for (std::size_t band = 0; band < numBands; ++band)
    {
        wetLeft += leftBands[band] * gains[band];
        wetRight += rightBands[band] * gains[band];
    }

    const auto dry = 1.0f - params.mix;
    const StereoFrame mixed {
        sanitizeAudio (dryLeft * dry + wetLeft * params.mix),
        sanitizeAudio (dryRight * dry + wetRight * params.mix)
    };

    return limitLinked (mixed);
}

void BrutalPressEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample (left[i], right[i]);
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

void BrutalPressEngine::updateFilters() noexcept
{
    for (auto& channel : crossoverFilters)
    {
        channel[0].setLowPass (sampleRate, params.lowSplitHz, 0.70710678f);
        channel[1].setHighPass (sampleRate, params.highSplitHz, 0.70710678f);
    }
}

void BrutalPressEngine::updateEnvelopeCoefficients() noexcept
{
    peakAttackCoefficient = onePoleCoefficient (sampleRate, params.attackSeconds);
    peakReleaseCoefficient = onePoleCoefficient (sampleRate, params.releaseSeconds);
    rmsCoefficient = onePoleCoefficient (sampleRate, std::max (params.releaseSeconds * 1.8f, 0.02f));
    limiterReleaseCoefficient = onePoleCoefficient (sampleRate, 0.04f + params.releaseSeconds * 0.25f);
}

BrutalPressEngine::BandArray BrutalPressEngine::splitBands (float input, std::size_t channel) noexcept
{
    const auto safeChannel = std::min (channel, numChannels - 1);
    const auto low = crossoverFilters[safeChannel][0].process (input);
    const auto high = crossoverFilters[safeChannel][1].process (input);
    const auto mid = input - low - high;
    return { sanitizeAudio (low), sanitizeAudio (mid), sanitizeAudio (high) };
}

BrutalPressEngine::BandArray BrutalPressEngine::computeLinkedBandGains (const BandArray& leftBands,
                                                                        const BandArray& rightBands) noexcept
{
    BandArray rawGains {};
    float sharedGain = 0.0f;

    for (std::size_t band = 0; band < numBands; ++band)
    {
        const auto peakTarget = std::max (std::fabs (leftBands[band]), std::fabs (rightBands[band]));
        const auto rmsTarget = 0.5f * (leftBands[band] * leftBands[band] + rightBands[band] * rightBands[band]);
        peakEnvelope[band] = envelopeFollower (peakEnvelope[band], peakTarget, peakAttackCoefficient, peakReleaseCoefficient);
        rmsEnvelope[band] = rmsTarget + rmsCoefficient * (rmsEnvelope[band] - rmsTarget);

        const auto rms = std::sqrt (std::max (0.0f, rmsEnvelope[band]));
        const auto detector = std::max (peakEnvelope[band] * 0.72f, rms * 1.18f);
        rawGains[band] = computeBandGain (detector);
        sharedGain += rawGains[band];
    }

    sharedGain /= static_cast<float> (numBands);

    BandArray gains {};
    for (std::size_t band = 0; band < numBands; ++band)
        gains[band] = rawGains[band] * (1.0f - params.glue) + sharedGain * params.glue;

    return gains;
}

float BrutalPressEngine::computeBandGain (float detector) const noexcept
{
    const auto level = std::max (detector, 1.0e-7f);
    const auto downThreshold = 0.42f - 0.34f * params.crush;
    const auto upThreshold = downThreshold * (0.58f + 0.24f * (1.0f - params.crush));
    const auto ratio = 1.6f + params.crush * 18.0f;
    const auto makeupDb = params.crush * 22.0f;

    auto gain = decibelsToGain (makeupDb);

    if (level > downThreshold)
    {
        const auto compressedLevel = downThreshold * std::pow (level / downThreshold, 1.0f / ratio);
        gain *= compressedLevel / level;
    }
    else if (params.upward > 0.0f && level > 1.0e-6f && level < upThreshold)
    {
        const auto distance = std::log (upThreshold / level);
        const auto liftDb = std::min (18.0f, distance * 8.6858896f * 0.55f) * params.upward;
        gain *= decibelsToGain (liftDb);
    }

    return clampFinite (gain, 0.0f, 32.0f, 1.0f);
}

StereoFrame BrutalPressEngine::limitLinked (StereoFrame input) noexcept
{
    lookaheadLeft[lookaheadWrite] = sanitizeAudio (input.left);
    lookaheadRight[lookaheadWrite] = sanitizeAudio (input.right);

    const auto readIndex = (lookaheadWrite + 1) % maxLookaheadSamples;
    const auto peak = scanLookaheadPeak();
    const auto targetGain = peak > ceilingGain ? ceilingGain / peak : 1.0f;
    limiterGain = targetGain < limiterGain
        ? targetGain
        : targetGain + limiterReleaseCoefficient * (limiterGain - targetGain);
    limiterGain = clampFinite (limiterGain, 0.0f, 1.0f, 1.0f);

    StereoFrame output {
        lookaheadLeft[readIndex] * limiterGain,
        lookaheadRight[readIndex] * limiterGain
    };

    output.left = clampFinite (output.left, -ceilingGain, ceilingGain, 0.0f);
    output.right = clampFinite (output.right, -ceilingGain, ceilingGain, 0.0f);

    lookaheadWrite = readIndex;
    return output;
}

float BrutalPressEngine::scanLookaheadPeak() const noexcept
{
    auto peak = 0.0f;
    for (std::size_t i = 0; i < maxLookaheadSamples; ++i)
    {
        peak = std::max (peak, std::fabs (lookaheadLeft[i]));
        peak = std::max (peak, std::fabs (lookaheadRight[i]));
    }
    return peak;
}

} // namespace violent

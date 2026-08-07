#include "violent/BrutalPressEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

using violent::BrutalPressEngine;
using violent::BrutalPressParameters;

namespace
{

constexpr int sampleRate = 48000;
constexpr int latencySamples = 128;

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

float peakOf (const std::vector<float>& samples, int start = 0) noexcept
{
    auto peak = 0.0f;
    for (std::size_t i = static_cast<std::size_t> (std::max (0, start)); i < samples.size(); ++i)
        peak = std::max (peak, std::fabs (samples[i]));
    return peak;
}

float rmsOf (const std::vector<float>& samples, int start = 0) noexcept
{
    auto sum = 0.0;
    auto count = 0;
    for (std::size_t i = static_cast<std::size_t> (std::max (0, start)); i < samples.size(); ++i)
    {
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
        ++count;
    }
    return count > 0 ? static_cast<float> (std::sqrt (sum / static_cast<double> (count))) : 0.0f;
}

float crestFactor (const std::vector<float>& samples, int start = 0) noexcept
{
    const auto rms = rmsOf (samples, start);
    return rms > 0.0f ? peakOf (samples, start) / rms : 0.0f;
}

Rendered render (const std::vector<float>& inputLeft,
                 const std::vector<float>& inputRight,
                 const BrutalPressParameters& params)
{
    assert (inputLeft.size() == inputRight.size());

    BrutalPressEngine engine;
    engine.prepare (sampleRate);
    engine.setParameters (params);

    Rendered output;
    output.left.reserve (inputLeft.size() + latencySamples);
    output.right.reserve (inputRight.size() + latencySamples);

    for (std::size_t i = 0; i < inputLeft.size(); ++i)
    {
        const auto frame = engine.processSample (inputLeft[i], inputRight[i]);
        output.left.push_back (frame.left);
        output.right.push_back (frame.right);
    }

    for (int i = 0; i < latencySamples; ++i)
    {
        const auto frame = engine.processSample (0.0f, 0.0f);
        output.left.push_back (frame.left);
        output.right.push_back (frame.right);
    }

    return output;
}

void testCrestFactorReductionOnTransientFixture()
{
    std::vector<float> left (4096, 0.08f);
    std::vector<float> right (4096, 0.08f);
    for (std::size_t i = 256; i < left.size(); i += 512)
    {
        left[i] = 1.0f;
        right[i] = -1.0f;
    }

    BrutalPressParameters params;
    params.crush = 1.0f;
    params.upward = 0.0f;
    params.attackSeconds = 0.0002f;
    params.releaseSeconds = 0.06f;
    params.ceilingDb = -1.0f;
    params.mix = 1.0f;

    const auto output = render (left, right, params);
    assert (crestFactor (output.left, latencySamples) < crestFactor (left) * 0.72f);
}

void testUpwardLiftOfLowLevelFixture()
{
    std::vector<float> left (4096, 0.012f);
    std::vector<float> right (4096, -0.012f);

    BrutalPressParameters noUpward;
    noUpward.crush = 0.25f;
    noUpward.upward = 0.0f;
    noUpward.ceilingDb = -1.0f;

    BrutalPressParameters upward = noUpward;
    upward.upward = 1.0f;

    const auto base = render (left, right, noUpward);
    const auto lifted = render (left, right, upward);

    assert (rmsOf (lifted.left, latencySamples + 512) > rmsOf (base.left, latencySamples + 512) * 1.8f);
}

void testCeilingCompliance()
{
    std::vector<float> left (8192, 2.0f);
    std::vector<float> right (8192, -1.5f);

    BrutalPressParameters params;
    params.crush = 1.0f;
    params.upward = 1.0f;
    params.ceilingDb = -9.0f;
    params.mix = 0.35f;

    const auto output = render (left, right, params);
    const auto ceiling = violent::decibelsToGain (params.ceilingDb);
    assert (peakOf (output.left) <= ceiling + 0.0001f);
    assert (peakOf (output.right) <= ceiling + 0.0001f);
}

void testSilencePreservation()
{
    std::vector<float> left (1024, 0.0f);
    std::vector<float> right (1024, 0.0f);

    BrutalPressParameters params;
    params.crush = 1.0f;
    params.upward = 1.0f;

    const auto output = render (left, right, params);
    assert (peakOf (output.left) == 0.0f);
    assert (peakOf (output.right) == 0.0f);
}

void testDeterministic()
{
    std::vector<float> left (2048, 0.0f);
    std::vector<float> right (2048, 0.0f);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        left[i] = std::sin (static_cast<float> (i) * 0.071f) * 0.2f;
        right[i] = std::cos (static_cast<float> (i) * 0.047f) * 0.17f;
    }

    BrutalPressParameters params;
    params.crush = 0.72f;
    params.upward = 0.4f;
    params.glue = 0.8f;

    const auto a = render (left, right, params);
    const auto b = render (left, right, params);
    assert (a.left == b.left);
    assert (a.right == b.right);
}

void testExtremeNonFiniteSafety()
{
    BrutalPressEngine engine;
    engine.prepare (std::numeric_limits<double>::infinity());

    BrutalPressParameters params;
    params.crush = std::numeric_limits<float>::infinity();
    params.upward = std::numeric_limits<float>::quiet_NaN();
    params.attackSeconds = -std::numeric_limits<float>::infinity();
    params.releaseSeconds = std::numeric_limits<float>::infinity();
    params.lowSplitHz = std::numeric_limits<float>::quiet_NaN();
    params.highSplitHz = -1000.0f;
    params.glue = std::numeric_limits<float>::infinity();
    params.ceilingDb = -120.0f;
    params.mix = std::numeric_limits<float>::quiet_NaN();
    engine.setParameters (params);

    for (int i = 0; i < 4096; ++i)
    {
        const auto input = (i & 1) == 0 ? std::numeric_limits<float>::infinity()
                                       : -std::numeric_limits<float>::quiet_NaN();
        const auto frame = engine.processSample (input, -input);
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -1.0001f && frame.left <= 1.0001f);
        assert (frame.right >= -1.0001f && frame.right <= 1.0001f);
    }
}

} // namespace

int main()
{
    testCrestFactorReductionOnTransientFixture();
    testUpwardLiftOfLowLevelFixture();
    testCeilingCompliance();
    testSilencePreservation();
    testDeterministic();
    testExtremeNonFiniteSafety();

    std::cout << "BrutalPressEngineTests passed\n";
    return 0;
}

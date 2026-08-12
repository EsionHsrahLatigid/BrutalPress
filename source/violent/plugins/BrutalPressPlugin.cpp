#include "violent/plugins/BrutalPressPlugin.h"

#include "violent/ParameterGridEditor.h"
#include "violent/ProductState.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace violent::plugin
{
namespace
{

constexpr std::array<char, 4> stateMagic { 'B', 'P', 'R', '1' };
constexpr int stateVersion = 1;
constexpr std::size_t presetParameterCount = 9;

constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ 0.72f, 0.00f, 1.8f, 105.0f, 160.0f, 3600.0f, 0.40f, -1.0f, 1.00f }},
    {{ 0.58f, 0.82f, 4.5f, 240.0f, 220.0f, 5200.0f, 0.55f, -1.8f, 0.88f }},
    {{ 0.88f, 0.26f, 0.7f, 70.0f, 95.0f, 2100.0f, 0.18f, -2.4f, 0.96f }},
    {{ 0.64f, 0.38f, 6.0f, 420.0f, 420.0f, 8600.0f, 0.86f, -0.8f, 0.74f }}
}};

yup::NormalisableRange<float> makeAttackRange()
{
    auto range = yup::NormalisableRange<float> (0.05f, 100.0f);
    range.setSkewForCentre (2.0f);
    return range;
}

yup::NormalisableRange<float> makeReleaseRange()
{
    auto range = yup::NormalisableRange<float> (5.0f, 2000.0f);
    range.setSkewForCentre (120.0f);
    return range;
}

yup::NormalisableRange<float> makeLowSplitRange()
{
    auto range = yup::NormalisableRange<float> (40.0f, 2000.0f);
    range.setSkewForCentre (180.0f);
    return range;
}

yup::NormalisableRange<float> makeHighSplitRange()
{
    auto range = yup::NormalisableRange<float> (1000.0f, 18000.0f);
    range.setSkewForCentre (3500.0f);
    return range;
}

yup::AudioParameter::Ptr makeParameter (const char* id,
                                        const char* name,
                                        int hostID,
                                        yup::NormalisableRange<float> range,
                                        float defaultValue,
                                        yup::AudioParameter::ParameterUnit unit,
                                        float smoothingMs)
{
    return yup::AudioParameterBuilder()
        .withID (id)
        .withName (name)
        .withHostID (static_cast<yup::uint32> (hostID))
        .withRange (range)
        .withDefault (defaultValue)
        .withSmoothing (smoothingMs)
        .withModulatable (true)
        .withUnit (unit)
        .build();
}

yup::AudioParameter::Ptr makeParameter (const char* id,
                                        const char* name,
                                        int hostID,
                                        float minValue,
                                        float maxValue,
                                        float defaultValue,
                                        yup::AudioParameter::ParameterUnit unit,
                                        float smoothingMs)
{
    return makeParameter (id,
                          name,
                          hostID,
                          yup::NormalisableRange<float> (minValue, maxValue),
                          defaultValue,
                          unit,
                          smoothingMs);
}

} // namespace

BrutalPressPlugin::BrutalPressPlugin()
    : yup::AudioProcessor ("BrutalPress",
                           yup::AudioBusLayout ({
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2),
                                                },
                                                {
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2),
                                                }))
{
    parameters[crush] = makeParameter ("crush", "Crush", crush, 0.0f, 1.0f, 0.65f, yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[upward] = makeParameter ("upward", "Upward", upward, 0.0f, 1.0f, 0.0f, yup::AudioParameter::ParameterUnit::Percent, 22.0f);
    parameters[attackMs] = makeParameter ("attack_ms", "Attack ms", attackMs, makeAttackRange(), 2.0f, yup::AudioParameter::ParameterUnit::Milliseconds, 8.0f);
    parameters[releaseMs] = makeParameter ("release_ms", "Release ms", releaseMs, makeReleaseRange(), 120.0f, yup::AudioParameter::ParameterUnit::Milliseconds, 24.0f);
    parameters[lowSplit] = makeParameter ("low_split", "Low Split", lowSplit, makeLowSplitRange(), 180.0f, yup::AudioParameter::ParameterUnit::Hertz, 35.0f);
    parameters[highSplit] = makeParameter ("high_split", "High Split", highSplit, makeHighSplitRange(), 3500.0f, yup::AudioParameter::ParameterUnit::Hertz, 35.0f);
    parameters[glue] = makeParameter ("glue", "Glue", glue, 0.0f, 1.0f, 0.35f, yup::AudioParameter::ParameterUnit::Percent, 20.0f);
    parameters[ceilingDb] = makeParameter ("ceiling_db", "Ceiling dB", ceilingDb, -24.0f, 0.0f, -1.0f, yup::AudioParameter::ParameterUnit::Decibels, 12.0f);
    parameters[mix] = makeParameter ("mix", "Mix", mix, 0.0f, 1.0f, 1.0f, yup::AudioParameter::ParameterUnit::Percent, 20.0f);

    for (const auto& parameter : parameters)
        addParameter (parameter);

    syncParameterValuesFromParameters();
    updateEngineParameters();
    setLatencySamples (lookaheadLatencySamples);
}

void BrutalPressPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    engine.reset();
    setLatencySamples (lookaheadLatencySamples);

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);

    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void BrutalPressPlugin::releaseResources()
{
}

void BrutalPressPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (auto& handle : parameterHandles)
            handle.advanceToSample (sample);

        for (std::size_t i = 0; i < parameterHandles.size(); ++i)
            currentParameterValues[i] = parameterHandles[i].getNextValue();

        if ((sample % parameterUpdateCadenceSamples) == 0)
            updateEngineParameters();

        if (left != nullptr && right != nullptr)
        {
            const auto frame = engine.processSample (left[sample], right[sample]);
            left[sample] = frame.left;
            right[sample] = frame.right;
        }
        else if (left != nullptr)
        {
            const auto frame = engine.processSample (left[sample], left[sample]);
            left[sample] = frame.left;
        }

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;
    }

    context.midi.clear();
}

void BrutalPressPlugin::flush()
{
    engine.reset();
}

bool BrutalPressPlugin::acceptsMidi() const noexcept
{
    return false;
}

bool BrutalPressPlugin::producesMidi() const noexcept
{
    return false;
}

int BrutalPressPlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void BrutalPressPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}

int BrutalPressPlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String BrutalPressPlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void BrutalPressPlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result BrutalPressPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0;
    const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
    if (result.failed())
        return result;

    currentPreset.store (loadedPreset, std::memory_order_relaxed);
    return yup::Result::ok();
}

yup::Result BrutalPressPlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed));
}

bool BrutalPressPlugin::hasEditor() const
{
    return true;
}

yup::AudioProcessorEditor* BrutalPressPlugin::createEditor()
{
    return new ParameterGridEditor (*this,
                                    "BrutalPress",
                                    "Hearing risk: this processor can create extreme average level. Keep monitoring low.",
                                    0xfff2f2f0u);
}

void BrutalPressPlugin::syncParameterValuesFromParameters() noexcept
{
    for (std::size_t i = 0; i < parameters.size(); ++i)
        currentParameterValues[i] = parameters[i]->getValue();
}

void BrutalPressPlugin::updateEngineParameters()
{
    violent::BrutalPressParameters engineParameters;
    engineParameters.crush = currentParameterValues[crush];
    engineParameters.upward = currentParameterValues[upward];
    engineParameters.attackSeconds = currentParameterValues[attackMs] * 0.001f;
    engineParameters.releaseSeconds = currentParameterValues[releaseMs] * 0.001f;
    engineParameters.lowSplitHz = currentParameterValues[lowSplit];
    engineParameters.highSplitHz = currentParameterValues[highSplit];
    engineParameters.glue = currentParameterValues[glue];
    engineParameters.ceilingDb = currentParameterValues[ceilingDb];
    engineParameters.mix = currentParameterValues[mix];

    engine.setParameters (engineParameters);
}

} // namespace violent::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new violent::plugin::BrutalPressPlugin();
}

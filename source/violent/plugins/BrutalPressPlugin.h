#pragma once

#include "violent/BrutalPressEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <array>
#include <atomic>

namespace violent::plugin
{

class BrutalPressPlugin final : public yup::AudioProcessor
{
public:
    BrutalPressPlugin();

    void prepareToPlay (const yup::AudioSpec& spec) override;
    void releaseResources() override;
    void processBlock (yup::AudioProcessContext<float>& context) override;
    void flush() override;

    bool acceptsMidi() const noexcept override;
    bool producesMidi() const noexcept override;

    int getCurrentPreset() const noexcept override;
    void setCurrentPreset (int index) noexcept override;
    int getNumPresets() const override;
    yup::String getPresetName (int index) const override;
    void setPresetName (int index, yup::StringRef newName) override;

    yup::Result loadStateFromMemory (const yup::MemoryBlock& data) override;
    yup::Result saveStateIntoMemory (yup::MemoryBlock& data) override;

    bool hasEditor() const override;
    yup::AudioProcessorEditor* createEditor() override;

private:
    enum ParameterIndex
    {
        crush,
        upward,
        attackMs,
        releaseMs,
        lowSplit,
        highSplit,
        glue,
        ceilingDb,
        mix,
        parameterCount
    };

    static constexpr int lookaheadLatencySamples = 128;
    static constexpr int parameterUpdateCadenceSamples = 16;

    void syncParameterValuesFromParameters() noexcept;
    void updateEngineParameters();

    std::array<yup::AudioParameter::Ptr, parameterCount> parameters;
    std::array<yup::AudioParameterHandle, parameterCount> parameterHandles;
    std::array<float, parameterCount> currentParameterValues {};
    violent::BrutalPressEngine engine;

    std::atomic<int> currentPreset { 0 };
    std::array<yup::String, 4> presetNames {
        "Concrete Wall",
        "Upward Burn",
        "Split Hammer",
        "Glue Ceiling"
    };
};

} // namespace violent::plugin

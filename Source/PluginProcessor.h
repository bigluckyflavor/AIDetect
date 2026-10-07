#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Analysis/AnalysisEngine.h"

//==============================================================================
// Parameter IDs (APVTS). Version hint 1 for all Phase 1 parameters.
namespace ParamIDs
{
    inline constexpr const char* analysisSensitivity         = "analysisSensitivity";
    inline constexpr const char* decisionThreshold           = "decisionThreshold";
    inline constexpr const char* minimumConfidence           = "minimumConfidence";
    inline constexpr const char* shortWindowSeconds          = "shortWindowSeconds";
    inline constexpr const char* mediumWindowSeconds         = "mediumWindowSeconds";
    inline constexpr const char* longWindowSeconds           = "longWindowSeconds";
    inline constexpr const char* analysisOverlap             = "analysisOverlap";
    inline constexpr const char* analysisSampleRateChoice    = "analysisSampleRateChoice";
    inline constexpr const char* enableSpectralGroup         = "enableSpectralGroup";
    inline constexpr const char* enableTemporalGroup         = "enableTemporalGroup";
    inline constexpr const char* enableStereoGroup           = "enableStereoGroup";
    inline constexpr const char* enableDynamicsGroup         = "enableDynamicsGroup";
    inline constexpr const char* enableNoiseGroup            = "enableNoiseGroup";
    inline constexpr const char* enableRepetitionGroup       = "enableRepetitionGroup";
    inline constexpr const char* enableVocalGroup            = "enableVocalGroup";
    inline constexpr const char* enableKnownFingerprintGroup = "enableKnownFingerprintGroup";
    inline constexpr const char* autoAnalyze                 = "autoAnalyze";
    inline constexpr const char* captureEnabled              = "captureEnabled";
}

//==============================================================================
/**
    DAAT AI Audio Inspector - Phase 1 shell.

    Transparent pass-through processor. Audio is never modified. Capture and
    analysis arrive in later phases; processBlock currently only counts samples
    through a relaxed atomic so the UI can show that audio is flowing.
*/
class DaatInspectorAudioProcessor : public juce::AudioProcessor,
                                    private juce::Timer
{
public:
    DaatInspectorAudioProcessor();
    ~DaatInspectorAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                   { return true; }

    //==========================================================================
    const juce::String getName() const override       { return JucePlugin_Name; }
    bool acceptsMidi() const override                 { return false; }
    bool producesMidi() const override                { return false; }
    bool isMidiEffect() const override                { return false; }
    double getTailLengthSeconds() const override      { return 0.0; }

    //==========================================================================
    int getNumPrograms() override                     { return 1; }
    int getCurrentProgram() override                  { return 0; }
    void setCurrentProgram (int) override             {}
    const juce::String getProgramName (int) override  { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    /** Non-parameter session state (advanced-mode flag, last export directory,
        selected timeline position, ...). Lives as a child of the APVTS state
        tree so it is saved/restored with the session. Message thread only. */
    juce::ValueTree getUiState() const;
    void setUiStateProperty (const juce::Identifier& id, const juce::var& value);

    /** Analysis engine (owns capture FIFO + worker thread). Editor uses this
        for commands and status; ownership stays with the processor. */
    AnalysisEngine& getEngine() noexcept { return engine; }

    /** Copies the Level-A (APVTS) controls into the analysis engine. Called on
        the message thread by a low-rate timer and before analysis commands, so
        parameter and automation changes are always reflected. */
    void syncRuntimeControls();

    /** Remembers a profile file as the last one used, so new plugin instances
        (not restored from a session) start with it. Message thread only. */
    void rememberLastUsedProfile (const juce::File& file);

    /** Total samples seen since construction (relaxed; UI/diagnostic use only). */
    std::atomic<juce::uint64> samplesProcessed { 0 };

    /** Host sample rate from the last prepareToPlay (relaxed; diagnostic use). */
    std::atomic<double> currentSampleRate { 0.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void ensureUiStateExists();
    void timerCallback() override;
    void loadLastUsedProfile();
    void pushProfileDecisionToParameters (const DetectionProfile& profile);

    static const juce::Identifier uiStateId;

    AnalysisEngine engine;

    // Machine-wide preferences (last-used profile), shared by all instances.
    std::unique_ptr<juce::PropertiesFile> appProperties;

    // Last controls pushed to the engine, to rescore only on a real change.
    daat::detect::RuntimeControls lastSyncedControls;
    bool haveSyncedControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaatInspectorAudioProcessor)
};

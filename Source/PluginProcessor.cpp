#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::Identifier DaatInspectorAudioProcessor::uiStateId { "uiState" };

//==============================================================================
DaatInspectorAudioProcessor::DaatInspectorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DAAT_AI_INSPECTOR", createParameterLayout())
{
    ensureUiStateExists();

    // New instances start with the last profile the user applied from a file.
    // A session restore (setStateInformation) replaces this afterwards.
    loadLastUsedProfile();

    syncRuntimeControls();

    // Low-rate sync so parameter/automation changes reach the engine without
    // touching the audio thread. Message thread only.
    startTimer (250);
}

void DaatInspectorAudioProcessor::loadLastUsedProfile()
{
    juce::PropertiesFile::Options options;
    options.applicationName     = "AI Audio Inspector";
    options.folderName          = "DAAT";
    options.filenameSuffix      = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties = std::make_unique<juce::PropertiesFile> (options);

    const juce::File file (appProperties->getValue ("lastUsedProfile"));
    if (! file.existsAsFile())
        return;

    ProfileValidation v;
    const auto profile = DetectionProfile::loadFromFile (file, v);
    if (! v.ok)
        return; // keep the factory profile; a broken file must not block startup

    engine.setProfile (profile);
    pushProfileDecisionToParameters (profile);

    auto ui = apvts.state.getChildWithName (uiStateId);
    ui.setProperty ("activeProfileName", profile.profileName, nullptr);
    ui.setProperty ("activeProfilePath", file.getFullPathName(), nullptr);
}

void DaatInspectorAudioProcessor::rememberLastUsedProfile (const juce::File& file)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (appProperties != nullptr)
    {
        appProperties->setValue ("lastUsedProfile", file.getFullPathName());
        appProperties->saveIfNeeded();
    }
}

void DaatInspectorAudioProcessor::pushProfileDecisionToParameters (const DetectionProfile& profile)
{
    // These two profile values are backed by plugin parameters (the runtime truth).
    const auto set = [this] (const char* id, double value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
    };
    set (ParamIDs::decisionThreshold, profile.decision.likelyThreshold);
    set (ParamIDs::minimumConfidence, profile.decision.minimumConfidence);
}

void DaatInspectorAudioProcessor::ensureUiStateExists()
{
    // Seed the non-parameter session-state child so later phases can rely on it.
    if (! apvts.state.getChildWithName (uiStateId).isValid())
    {
        juce::ValueTree ui (uiStateId);
        ui.setProperty ("advancedMode", false, nullptr);
        ui.setProperty ("lastExportDirectory", juce::String(), nullptr);
        ui.setProperty ("selectedTimelineSeconds", 0.0, nullptr);
        ui.setProperty ("activeProfileName", "Balanced Research (factory)", nullptr);
        apvts.state.appendChild (ui, nullptr);
    }
}

DaatInspectorAudioProcessor::~DaatInspectorAudioProcessor()
{
    stopTimer();
}

//==============================================================================
void DaatInspectorAudioProcessor::timerCallback()
{
    syncRuntimeControls();
}

void DaatInspectorAudioProcessor::syncRuntimeControls()
{
    using namespace daat::detect;
    RuntimeControls c;

    const auto boolParam = [this] (const char* id)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load() >= 0.5f;
        return true;
    };
    const auto floatParam = [this] (const char* id, float fallback)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return fallback;
    };

    juce::uint32 mask = 0;
    if (boolParam (ParamIDs::enableSpectralGroup))         mask |= groupBit (FeatureGroup::spectral);
    if (boolParam (ParamIDs::enableTemporalGroup))         mask |= groupBit (FeatureGroup::temporal);
    if (boolParam (ParamIDs::enableDynamicsGroup))         mask |= groupBit (FeatureGroup::dynamics);
    if (boolParam (ParamIDs::enableStereoGroup))           mask |= groupBit (FeatureGroup::stereo);
    if (boolParam (ParamIDs::enableNoiseGroup))            mask |= groupBit (FeatureGroup::noise);
    if (boolParam (ParamIDs::enableRepetitionGroup))       mask |= groupBit (FeatureGroup::repetition);
    if (boolParam (ParamIDs::enableVocalGroup))            mask |= groupBit (FeatureGroup::vocal);
    if (boolParam (ParamIDs::enableKnownFingerprintGroup)) mask |= groupBit (FeatureGroup::fingerprint);
    // Model group stays off until ONNX support lands (Phase 8).

    c.groupEnableMask   = mask;
    c.sensitivity       = floatParam (ParamIDs::analysisSensitivity, 0.5f);
    c.decisionThreshold = floatParam (ParamIDs::decisionThreshold, 0.72f);
    c.minimumConfidence = floatParam (ParamIDs::minimumConfidence, 0.55f);
    c.applyOverrides    = true;

    const bool changed = ! haveSyncedControls
                      || c.groupEnableMask   != lastSyncedControls.groupEnableMask
                      || c.sensitivity       != lastSyncedControls.sensitivity
                      || c.decisionThreshold != lastSyncedControls.decisionThreshold
                      || c.minimumConfidence != lastSyncedControls.minimumConfidence;

    engine.setRuntimeControls (c);

    // A parameter or automation change re-scores the existing result from its
    // stored measurements, so the knobs act immediately without re-analysing.
    if (changed && haveSyncedControls && engine.getResult() != nullptr)
        engine.requestRescore();

    lastSyncedControls = c;
    haveSyncedControls = true;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
DaatInspectorAudioProcessor::createParameterLayout()
{
    using P  = juce::ParameterID;
    namespace ID = ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::analysisSensitivity, 1), "Analysis Sensitivity", pct, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::decisionThreshold, 1), "Decision Threshold",
        juce::NormalisableRange<float> (0.5f, 0.95f, 0.001f), 0.72f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::minimumConfidence, 1), "Minimum Confidence", pct, 0.55f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::shortWindowSeconds, 1), "Short Window (s)",
        juce::NormalisableRange<float> (0.5f, 5.0f, 0.1f), 2.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::mediumWindowSeconds, 1), "Medium Window (s)",
        juce::NormalisableRange<float> (4.0f, 15.0f, 0.1f), 8.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::longWindowSeconds, 1), "Long Window (s)",
        juce::NormalisableRange<float> (15.0f, 60.0f, 0.5f), 30.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        P (ID::analysisOverlap, 1), "Analysis Overlap",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        P (ID::analysisSampleRateChoice, 1), "Analysis Sample Rate",
        juce::StringArray { "44100 Hz", "48000 Hz" }, 1));

    auto addGroupSwitch = [&layout] (const char* id, const juce::String& name, bool defaultOn)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (P (id, 1), name, defaultOn));
    };

    addGroupSwitch (ID::enableSpectralGroup,         "Spectral Features",    true);
    addGroupSwitch (ID::enableTemporalGroup,         "Temporal Features",    true);
    addGroupSwitch (ID::enableStereoGroup,           "Stereo Features",      true);
    addGroupSwitch (ID::enableDynamicsGroup,         "Dynamics Features",    true);
    addGroupSwitch (ID::enableNoiseGroup,            "Noise Features",       true);
    addGroupSwitch (ID::enableRepetitionGroup,       "Repetition Features",  true);
    addGroupSwitch (ID::enableVocalGroup,            "Vocal Features",       false); // not implemented yet
    addGroupSwitch (ID::enableKnownFingerprintGroup, "Known Fingerprints",   false); // not implemented yet
    addGroupSwitch (ID::autoAnalyze,                 "Auto Analyze",         false);
    addGroupSwitch (ID::captureEnabled,              "Capture Enabled",      false);

    return layout;
}

//==============================================================================
void DaatInspectorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store (sampleRate, std::memory_order_relaxed);

    // Allocate the capture FIFO and accumulation buffer before any audio
    // callbacks run. jmax keeps mono hosts (0/1 inputs) valid.
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumInputChannels()));
}

void DaatInspectorAudioProcessor::releaseResources()
{
    engine.release();
}

bool DaatInspectorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void DaatInspectorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Transparent pass-through: the host buffer is never modified. Only clear
    // output channels that have no corresponding input (none in the supported
    // layouts, but this keeps hosts with odd bus configs safe).
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Real-time-safe bookkeeping only. No locks, allocations, file I/O, FFTs,
    // strings, or GUI access on this thread.
    samplesProcessed.fetch_add ((juce::uint64) buffer.getNumSamples(),
                                std::memory_order_relaxed);

    // Wait-free hand-off to the analysis worker. Does nothing unless a capture
    // is active; the buffer itself is never modified (transparent pass-through).
    engine.pushAudioBlock (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* DaatInspectorAudioProcessor::createEditor()
{
    return new DaatInspectorAudioProcessorEditor (*this);
}

//==============================================================================
void DaatInspectorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Parameters + the uiState child are serialized together. The active
    // detection profile and the source-file path go into a *copy* of the state
    // (safe from any thread). Captured audio is never written into the session.
    auto state = apvts.copyState();

    auto ui = state.getChildWithName (uiStateId);
    if (ui.isValid())
    {
        ui.setProperty ("activeProfileJson", engine.getProfile().toJsonString(), nullptr);
        ui.setProperty ("sourceFilePath", engine.getSourceFile().getFullPathName(), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DaatInspectorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    ensureUiStateExists(); // sessions saved by other/older versions may lack it

    auto ui = apvts.state.getChildWithName (uiStateId);
    const auto json = ui.getProperty ("activeProfileJson").toString();
    ui.removeProperty ("activeProfileJson", nullptr); // regenerated on every save

    if (json.isNotEmpty())
    {
        ProfileValidation v;
        const auto profile = DetectionProfile::fromJsonString (json, v);
        if (v.ok)
        {
            engine.setProfile (profile);
            ui.setProperty ("activeProfileName", profile.profileName, nullptr);
        }
        else
        {
            engine.setProfile (DetectionProfile::getFactoryDefault());
            ui.setProperty ("activeProfileName", "Balanced Research (factory)", nullptr);
            ui.setProperty ("profileRestoreWarning",
                            "The session's detection profile could not be restored ("
                                + v.errors[0] + "). Using the factory profile.", nullptr);
        }
    }

    syncRuntimeControls();
}

//==============================================================================
juce::ValueTree DaatInspectorAudioProcessor::getUiState() const
{
    return apvts.state.getChildWithName (uiStateId);
}

void DaatInspectorAudioProcessor::setUiStateProperty (const juce::Identifier& id,
                                                      const juce::var& value)
{
    JUCE_ASSERT_MESSAGE_THREAD
    auto ui = apvts.state.getChildWithName (uiStateId);

    if (ui.isValid())
        ui.setProperty (id, value, nullptr);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DaatInspectorAudioProcessor();
}

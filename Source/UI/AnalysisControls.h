#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
    Bottom control strip: Capture / Stop / Analyze / Clear / Load File /
    Export / Settings, plus a sensitivity slider and auto-analyze toggle
    bound to the APVTS.

    Phase 1: buttons report through onAction so the editor can show status;
    the actions themselves are wired to the engine in later phases.
*/
class AnalysisControls : public juce::Component
{
public:
    explicit AnalysisControls (juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

    /** Called on the message thread with the button's name when clicked. */
    std::function<void (const juce::String&)> onAction;

private:
    juce::TextButton captureButton  { "Capture" };
    juce::TextButton stopButton     { "Stop" };
    juce::TextButton analyzeButton  { "Analyze" };
    juce::TextButton clearButton    { "Clear" };
    juce::TextButton loadFileButton { "Load File" };
    juce::TextButton exportButton   { "Export" };
    juce::TextButton settingsButton { "Settings" };

    juce::Label sensitivityLabel { {}, "Sensitivity" };
    juce::Slider sensitivitySlider;
    juce::ToggleButton autoAnalyzeToggle { "Auto" };

    juce::AudioProcessorValueTreeState::SliderAttachment sensitivityAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment autoAnalyzeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisControls)
};

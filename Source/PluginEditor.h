#pragma once

#include "PluginProcessor.h"
#include "UI/DaatLookAndFeel.h"
#include "UI/TimelineView.h"
#include "UI/ResultPanel.h"
#include "UI/EvidencePanel.h"
#include "UI/FeatureScorePanel.h"
#include "UI/DetectionSettingsPanel.h"
#include "UI/ReportPanel.h"
#include "UI/AnalysisControls.h"

//==============================================================================
/**
    Main editor: header, result + evidence row, timeline heat map with its
    toolbar, selection details, status line, and the control strip. The
    detection settings editor opens as an overlay over the content area.

    A 10 Hz timer pulls the engine snapshot and the immutable result pointer;
    views are only updated when the result actually changes. The editor owns no
    analysis state and may be closed and reopened while analysis continues.
*/
class DaatInspectorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit DaatInspectorAudioProcessorEditor (DaatInspectorAudioProcessor&);
    ~DaatInspectorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleAction (const juce::String& actionName);
    void launchFileChooser();
    void showSettings (bool shouldShow);
    void showReport (bool shouldShow);
    void onResultChanged();
    void onTimelineSelectionChanged();
    void updateToolbar (const EngineSnapshot& snap);
    void showMessage (const juce::String& message, int holdMs = 6000);
    void showSessionHints();
    static juce::String formatClock (double seconds);

    DaatInspectorAudioProcessor& processorRef;

    std::unique_ptr<juce::FileChooser> fileChooser;

    DaatLookAndFeel lookAndFeel;

    juce::Label titleLabel   { {}, "DAAT AI Audio Inspector" };
    juce::Label profileLabel { {}, "Profile: Balanced Research (factory)" };
    juce::Label stateLabel   { {}, "Idle" };
    juce::Label statusLabel  { {}, "" };

    ResultPanel resultPanel;
    EvidencePanel evidencePanel;

    juce::Label      timelineTitle   { {}, "Timeline" };
    juce::TextButton zoomOutButton   { "-" };
    juce::TextButton zoomInButton    { "+" };
    juce::TextButton zoomFitButton   { "Fit" };
    juce::TextButton analyzeSelectionButton { "Analyze selection" };
    juce::TextButton excludeSelectionButton { "Exclude selection" };
    juce::TextButton clearExclusionsButton  { "Clear exclusions" };
    juce::TextButton fullAnalysisButton     { "Full analysis" };

    TimelineView timelineView;
    FeatureScorePanel featureScorePanel;
    AnalysisControls controls;
    DetectionSettingsPanel settingsPanel;
    ReportPanel reportPanel;

    juce::TooltipWindow tooltipWindow { this, 600 };

    std::shared_ptr<const AnalysisResult> shownResult;
    bool selectionRestored = false;

    juce::String heldMessage;
    juce::uint32 heldUntilMs = 0;

    juce::uint64 lastSampleCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaatInspectorAudioProcessorEditor)
};

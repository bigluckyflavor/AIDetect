#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analysis/ReportExporter.h"

#include <functional>
#include <memory>

class DaatInspectorAudioProcessor;

//==============================================================================
/**
    Export overlay (spec §17 / §19): JSON report and CSV feature export with
    user-supplied provenance labels. Exports never contain audio.

    Labels and the last export folder persist in the session UI state, so
    labelling a batch of files keeps the previous choices.
*/
class ReportPanel : public juce::Component
{
public:
    explicit ReportPanel (DaatInspectorAudioProcessor& processor);

    /** Call when shown: refreshes the summary from the current result. */
    void panelShown();

    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ReportLabels currentLabels() const;
    void storeLabels();
    void exportReport (bool asCsv);
    void status (const juce::String& message);

    DaatInspectorAudioProcessor& processorRef;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label titleLabel   { {}, "Export report" };
    juce::Label summaryLabel;
    juce::Label privacyLabel;

    juce::Label      provenanceLabel { {}, "Provenance label" };
    juce::ComboBox   provenanceBox;
    juce::Label      generatorLabel  { {}, "Generator (if known)" };
    juce::TextEditor generatorEditor;
    juce::Label      processingLabel { {}, "Processing history" };
    juce::TextEditor processingEditor;
    juce::Label      notesLabel      { {}, "Notes" };
    juce::TextEditor notesEditor;

    juce::ToggleButton includeWindowsToggle { "Include per-window detail in the JSON report" };
    juce::TextButton jsonButton  { "Export JSON report..." };
    juce::TextButton csvButton   { "Export CSV features..." };
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReportPanel)
};

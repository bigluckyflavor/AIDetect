#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analysis/DetectionProfile.h"
#include "../Analysis/AnalysisResult.h"

#include <functional>
#include <memory>

class DaatInspectorAudioProcessor;

//==============================================================================
/**
    Detection settings editor (spec §8 / §15).

    Edits a private *working copy* of the detection profile; nothing reaches the
    engine until Apply, which validates first. Settings that only change scoring
    trigger a cheap rescore of stored measurements; settings that change the
    measurements themselves (analysis block, HF cutoff, repetition lags) trigger
    a full reanalysis.

    Basic mode shows enable / weight / description / current values. Advanced
    mode adds thresholds, frequency and lag ranges, window assignment, and the
    analysis, decision, and group blocks.

    The plugin's Decision Threshold and Minimum Confidence parameters are the
    runtime source of truth for those two values: the editor reads them when
    opened and writes them on Apply, so the profile, the parameters, and saved
    JSON always agree.
*/
class DetectionSettingsPanel : public juce::Component,
                               private juce::ListBoxModel
{
public:
    explicit DetectionSettingsPanel (DaatInspectorAudioProcessor& processor);
    ~DetectionSettingsPanel() override;

    /** Call when the panel becomes visible. Reloads the working copy from the
        engine unless there are unapplied edits, which are kept. */
    void panelShown();

    bool hasUnappliedChanges() const noexcept { return dirty; }

    /** Status messages for the editor's status line. */
    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onClose;

    static juce::File getProfilesDirectory();

    void paint (juce::Graphics& g) override;
    void resized() override;

    //==========================================================================
    // Used by the row and property components.
    DetectionProfile& working() noexcept { return workingProfile; }
    void edited();                                   // any value changed
    void selectFeature (const juce::String& featureId);
    void restoreFeatureDefaults (const juce::String& featureId);
    const FeatureResult* currentAverageFor (const juce::String& featureId) const;
    bool isSelected (const juce::String& featureId) const { return featureId == selectedFeatureId; }

private:
    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void rebuildVisibleList();
    void rebuildDetail();
    void revalidate();
    void refreshPresetList();
    void refreshHeader();
    void setWorking (const DetectionProfile& p, const juce::File& file, bool markDirty);

    void loadWithChooser();
    void loadFromFile (const juce::File& file);
    void save();
    void saveAs();
    void duplicate();
    void resetToFactory();
    void apply();
    void status (const juce::String& message);

    DaatInspectorAudioProcessor& processorRef;

    DetectionProfile workingProfile;
    juce::File workingFile;
    bool dirty = false;
    ProfileValidation validation;
    juce::String selectedFeatureId;
    juce::StringArray visibleIds;
    std::shared_ptr<const AnalysisResult> lastResult;

    juce::Label      titleLabel   { {}, "Detection settings" };
    juce::ComboBox   presetBox;
    juce::Label      nameLabel    { {}, "Name" };
    juce::TextEditor nameEditor;
    juce::TextButton loadButton   { "Load..." };
    juce::TextButton saveButton   { "Save" };
    juce::TextButton saveAsButton { "Save As..." };
    juce::TextButton duplicateButton { "Duplicate" };
    juce::TextButton resetButton  { "Reset" };
    juce::ToggleButton advancedToggle { "Advanced" };
    juce::TextButton applyButton  { "Apply" };
    juce::TextButton closeButton  { "Close" };

    juce::TextEditor searchBox;
    juce::ComboBox   groupFilter;
    juce::Label      dirtyLabel;

    juce::ListBox       featureList { "features", this };
    juce::PropertyPanel detailPanel;
    juce::Label         validationLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> presetFiles;         // preset box ids 2.. map to these
    juce::Rectangle<int> listHeaderArea;
    bool updatingPresetBox = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DetectionSettingsPanel)
};

#include "ReportPanel.h"
#include "DaatLookAndFeel.h"
#include "../PluginProcessor.h"

namespace
{
    juce::String pct (float v)
    {
        return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 100.0f)) + "%";
    }
}

//==============================================================================
ReportPanel::ReportPanel (DaatInspectorAudioProcessor& p)
    : processorRef (p)
{
    setOpaque (true);

    titleLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    summaryLabel.setJustificationType (juce::Justification::topLeft);
    summaryLabel.setFont (juce::Font (juce::FontOptions (12.5f)));
    summaryLabel.setColour (juce::Label::backgroundColourId, DaatColours::panel);
    summaryLabel.setColour (juce::Label::outlineColourId, DaatColours::panelOutline);
    addAndMakeVisible (summaryLabel);

    privacyLabel.setText ("Exports contain measurements, scores, settings, provenance and your labels - "
                          "never audio. Labels are your own statements; the plugin does not infer them. "
                          "Screening estimates are not proof of how a recording was made.",
                          juce::dontSendNotification);
    privacyLabel.setJustificationType (juce::Justification::topLeft);
    privacyLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    privacyLabel.setColour (juce::Label::textColourId, DaatColours::textSecondary);
    addAndMakeVisible (privacyLabel);

    for (auto* l : { &provenanceLabel, &generatorLabel, &processingLabel, &notesLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        l->setColour (juce::Label::textColourId, DaatColours::textSecondary);
        addAndMakeVisible (*l);
    }

    const auto& labels = daat::report::provenanceLabels();
    for (int i = 0; i < labels.size(); ++i)
        provenanceBox.addItem (labels[i], i + 1);
    provenanceBox.onChange = [this] { storeLabels(); };
    addAndMakeVisible (provenanceBox);

    generatorEditor.setTextToShowWhenEmpty ("e.g. the generator or service, if you know it", DaatColours::textDisabled);
    processingEditor.setTextToShowWhenEmpty ("e.g. mastered, MP3 128 kbps, stem-separated", DaatColours::textDisabled);
    notesEditor.setMultiLine (true, true);
    notesEditor.setReturnKeyStartsNewLine (true);
    for (auto* e : { &generatorEditor, &processingEditor, &notesEditor })
    {
        e->onTextChange = [this] { storeLabels(); };
        addAndMakeVisible (*e);
    }

    includeWindowsToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (includeWindowsToggle);

    jsonButton.onClick  = [this] { exportReport (false); };
    csvButton.onClick   = [this] { exportReport (true); };
    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
    jsonButton.setTooltip ("Full report: verdict, confidence, evidence, caveats, settings, labels");
    csvButton.setTooltip ("One row per analysis window - append many files into one training table");
    for (auto* b : { &jsonButton, &csvButton, &closeButton })
        addAndMakeVisible (*b);
}

//==============================================================================
void ReportPanel::panelShown()
{
    auto ui = processorRef.getUiState();

    // Restore the labels used for the previous export.
    const auto provenance = ui.getProperty ("labelProvenance", "Unknown").toString();
    const int idx = daat::report::provenanceLabels().indexOf (provenance);
    provenanceBox.setSelectedId (idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);
    generatorEditor.setText (ui.getProperty ("labelGenerator", {}).toString(), false);
    processingEditor.setText (ui.getProperty ("labelProcessing", {}).toString(), false);
    notesEditor.setText (ui.getProperty ("labelNotes", {}).toString(), false);

    const auto result = processorRef.getEngine().getResult();
    juce::String summary;
    if (result == nullptr)
    {
        summary = "Nothing to export yet - capture or load audio and let the analysis finish.";
    }
    else
    {
        summary << "Source: " << result->sourceName
                << (result->sourceFileHash.isNotEmpty() ? "   |   SHA-256 " + result->sourceFileHash.substring (0, 16) + "..."
                                                        : juce::String ("   |   no file hash (DAW capture)"))
                << "\nVerdict: " << toString (result->verdict)
                << "   |   indicator score " << pct (result->overallLikelihood)
                << "   |   confidence " << pct (result->confidence)
                << "\nAnalysed " << juce::String (result->effectiveSeconds, 1) << " s"
                << (result->isRangeAnalysis ? "  (selected range only)" : "")
                << (result->excludedRanges.empty() ? juce::String()
                                                    : "   |   " + juce::String ((int) result->excludedRanges.size()) + " section(s) excluded")
                << "   |   " << result->totalWindows() << " windows   |   "
                << result->numValidFeatures << " features";
    }
    summaryLabel.setText (summary, juce::dontSendNotification);

    jsonButton.setEnabled (result != nullptr);
    csvButton.setEnabled (result != nullptr && result->totalWindows() > 0);
}

ReportLabels ReportPanel::currentLabels() const
{
    ReportLabels l;
    l.provenance        = provenanceBox.getText().isNotEmpty() ? provenanceBox.getText() : juce::String ("Unknown");
    l.generator         = generatorEditor.getText().trim();
    l.processingHistory = processingEditor.getText().trim();
    l.notes             = notesEditor.getText().trim();
    return l;
}

void ReportPanel::storeLabels()
{
    const auto l = currentLabels();
    processorRef.setUiStateProperty ("labelProvenance", l.provenance);
    processorRef.setUiStateProperty ("labelGenerator", l.generator);
    processorRef.setUiStateProperty ("labelProcessing", l.processingHistory);
    processorRef.setUiStateProperty ("labelNotes", l.notes);
}

void ReportPanel::status (const juce::String& message)
{
    if (onStatus != nullptr)
        onStatus (message);
}

//==============================================================================
void ReportPanel::exportReport (bool asCsv)
{
    const auto result = processorRef.getEngine().getResult();
    if (result == nullptr)
    {
        status ("Nothing to export - analyse audio first.");
        return;
    }

    // Snapshot everything now: the result is immutable, and profile/controls
    // are copied, so the export reflects exactly what is on screen.
    const auto profile  = processorRef.getEngine().getProfile();
    const auto controls = processorRef.getEngine().getRuntimeControls();
    const auto labels   = currentLabels();
    const bool withWindows = includeWindowsToggle.getToggleState();

    auto dir = juce::File (processorRef.getUiState().getProperty ("lastExportDirectory", {}).toString());
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const auto base = juce::File::createLegalFileName (
        juce::File (result->sourceName).getFileNameWithoutExtension().isNotEmpty()
            ? juce::File (result->sourceName).getFileNameWithoutExtension() : juce::String ("capture"));
    const auto suggested = dir.getChildFile (base + (asCsv ? "_daat_features.csv" : "_daat_report.json"));

    chooser = std::make_unique<juce::FileChooser> (asCsv ? "Export CSV features" : "Export JSON report",
                                                   suggested, asCsv ? "*.csv" : "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, result, profile, controls, labels, withWindows, asCsv] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              const auto ext = asCsv ? "csv" : "json";
                              if (! file.hasFileExtension (ext))
                                  file = file.withFileExtension (ext);

                              const auto text = asCsv
                                  ? daat::report::buildFeatureCsv (*result, profile, labels, true)
                                  : daat::report::toJsonString (daat::report::buildJsonReport (*result, profile, controls,
                                                                                               labels, withWindows));
                              juce::String error;
                              if (daat::report::writeTextFile (file, text, error))
                              {
                                  processorRef.setUiStateProperty ("lastExportDirectory",
                                                                   file.getParentDirectory().getFullPathName());
                                  status ("Exported " + file.getFileName() + ".");
                              }
                              else
                              {
                                  status ("Export failed: " + error);
                              }
                          });
}

//==============================================================================
void ReportPanel::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::background);
    g.setColour (DaatColours::panelOutline);
    g.drawRect (getLocalBounds());
}

void ReportPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (64));
    titleLabel.setBounds (top);

    area.removeFromTop (8);
    summaryLabel.setBounds (area.removeFromTop (64));
    area.removeFromTop (12);

    const auto row = [&area] (juce::Label& label, juce::Component& editor, int height)
    {
        auto r = area.removeFromTop (height);
        label.setBounds (r.removeFromLeft (170).withHeight (26));
        r.removeFromLeft (8);
        editor.setBounds (r.withWidth (juce::jmin (r.getWidth(), 620)));
        area.removeFromTop (8);
    };
    row (provenanceLabel, provenanceBox, 26);
    row (generatorLabel, generatorEditor, 26);
    row (processingLabel, processingEditor, 26);
    row (notesLabel, notesEditor, 80);

    auto buttons = area.removeFromTop (28).withTrimmedLeft (178);
    jsonButton.setBounds (buttons.removeFromLeft (170));
    buttons.removeFromLeft (8);
    csvButton.setBounds (buttons.removeFromLeft (170));
    area.removeFromTop (6);
    includeWindowsToggle.setBounds (area.removeFromTop (24).withTrimmedLeft (178).withWidth (420));

    area.removeFromTop (12);
    privacyLabel.setBounds (area.removeFromTop (48).withTrimmedLeft (178).withWidth (juce::jmin (area.getWidth() - 178, 620)));
}

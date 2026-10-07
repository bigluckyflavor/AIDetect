#include "PluginEditor.h"

namespace
{
    juce::String pct (float v)
    {
        return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 100.0f)) + "%";
    }
}

//==============================================================================
DaatInspectorAudioProcessorEditor::DaatInspectorAudioProcessorEditor (DaatInspectorAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      controls (p.apvts),
      settingsPanel (p),
      reportPanel (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, DaatColours::textPrimary);
    addAndMakeVisible (titleLabel);

    profileLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    profileLabel.setColour (juce::Label::textColourId, DaatColours::textSecondary);
    addAndMakeVisible (profileLabel);

    stateLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    stateLabel.setColour (juce::Label::textColourId, DaatColours::accent);
    stateLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (stateLabel);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, DaatColours::textSecondary);
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (resultPanel);
    addAndMakeVisible (evidencePanel);

    //--- Timeline toolbar ---------------------------------------------------
    timelineTitle.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (timelineTitle);

    zoomOutButton.onClick = [this] { timelineView.zoomOut(); };
    zoomInButton.onClick  = [this] { timelineView.zoomIn(); };
    zoomFitButton.onClick = [this] { timelineView.zoomToFit(); };
    zoomOutButton.setTooltip ("Zoom out (or Ctrl + mouse wheel)");
    zoomInButton.setTooltip ("Zoom in (or Ctrl + mouse wheel)");
    zoomFitButton.setTooltip ("Show the whole recording");

    analyzeSelectionButton.setTooltip ("Run a dedicated analysis of the selected range only");
    analyzeSelectionButton.onClick = [this]
    {
        const auto range = timelineView.getSelectedRange();
        if (range.isEmpty())
            return;
        processorRef.syncRuntimeControls();
        processorRef.getEngine().requestAnalyzeRange (range.getStart(), range.getEnd());
        showMessage ("Analysing " + juce::String (range.getStart(), 1) + "-"
                     + juce::String (range.getEnd(), 1) + " s only.");
    };

    excludeSelectionButton.setTooltip ("Leave the selected range out of the overall score");
    excludeSelectionButton.onClick = [this]
    {
        const auto range = timelineView.getSelectedRange();
        if (range.isEmpty())
            return;
        auto& engine = processorRef.getEngine();
        auto ranges = engine.getExcludedRanges();
        ranges.push_back (range);
        engine.setExcludedRanges (ranges);
        engine.requestRescore();
        timelineView.clearSelectedRange();
        showMessage ("Excluded " + juce::String (range.getLength(), 1) + " s from the overall score.");
    };

    clearExclusionsButton.setTooltip ("Include every section in the overall score again");
    clearExclusionsButton.onClick = [this]
    {
        auto& engine = processorRef.getEngine();
        engine.setExcludedRanges ({});
        engine.requestRescore();
        showMessage ("Exclusions cleared.");
    };

    fullAnalysisButton.setTooltip ("Analyse the whole recording again");
    fullAnalysisButton.onClick = [this]
    {
        processorRef.syncRuntimeControls();
        processorRef.getEngine().requestAnalyze();
    };

    for (auto* b : { &zoomOutButton, &zoomInButton, &zoomFitButton, &analyzeSelectionButton,
                     &excludeSelectionButton, &clearExclusionsButton, &fullAnalysisButton })
        addAndMakeVisible (*b);

    timelineView.onSelectionChanged = [this] { onTimelineSelectionChanged(); };
    addAndMakeVisible (timelineView);
    addAndMakeVisible (featureScorePanel);

    controls.onAction = [this] (const juce::String& name) { handleAction (name); };
    addAndMakeVisible (controls);

    //--- Settings overlay ---------------------------------------------------
    settingsPanel.onClose  = [this] { showSettings (false); };
    settingsPanel.onStatus = [this] (const juce::String& m) { showMessage (m); };
    addChildComponent (settingsPanel);

    reportPanel.onClose  = [this] { showReport (false); };
    reportPanel.onStatus = [this] (const juce::String& m) { showMessage (m); };
    addChildComponent (reportPanel);

    setResizable (true, true);
    setResizeLimits (900, 660, 1920, 1400);
    setSize (1080, 760);

    showSessionHints();
    startTimerHz (10); // lightweight status/progress refresh only
}

DaatInspectorAudioProcessorEditor::~DaatInspectorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void DaatInspectorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::background);
    g.setColour (DaatColours::header);
    g.fillRect (getLocalBounds().removeFromTop (44));
}

void DaatInspectorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto headerArea = area.removeFromTop (44).reduced (12, 6);
    stateLabel.setBounds (headerArea.removeFromRight (280));
    titleLabel.setBounds (headerArea.removeFromLeft (260));
    profileLabel.setBounds (headerArea);

    controls.setBounds (area.removeFromBottom (44));
    statusLabel.setBounds (area.removeFromBottom (22).reduced (12, 0));

    area.reduce (12, 8);
    settingsPanel.setBounds (area); // overlays cover the whole content area
    reportPanel.setBounds (area);

    auto topRow = area.removeFromTop (200);
    resultPanel.setBounds (topRow.removeFromLeft ((int) (topRow.getWidth() * 0.44f)));
    topRow.removeFromLeft (8);
    evidencePanel.setBounds (topRow);

    area.removeFromTop (8);
    auto toolbar = area.removeFromTop (26);
    timelineTitle.setBounds (toolbar.removeFromLeft (72));
    const auto place = [&toolbar] (juce::Component& c, int w)
    {
        c.setBounds (toolbar.removeFromLeft (w));
        toolbar.removeFromLeft (4);
    };
    place (zoomOutButton, 28);
    place (zoomInButton, 28);
    place (zoomFitButton, 40);
    toolbar.removeFromLeft (12);
    place (analyzeSelectionButton, 128);
    place (excludeSelectionButton, 128);
    place (clearExclusionsButton, 122);
    place (fullAnalysisButton, 100);

    area.removeFromTop (6);
    // The lanes read fine at ~40 px each; give the rest to the selection details.
    const int timelineHeight = juce::jlimit (120, 200, (int) (area.getHeight() * 0.42f));
    timelineView.setBounds (area.removeFromTop (timelineHeight));
    area.removeFromTop (8);
    featureScorePanel.setBounds (area);
}

//==============================================================================
juce::String DaatInspectorAudioProcessorEditor::formatClock (double seconds)
{
    if (seconds < 0.0 || std::isnan (seconds))
        seconds = 0.0;

    const int total = (int) seconds;
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}

void DaatInspectorAudioProcessorEditor::showMessage (const juce::String& message, int holdMs)
{
    heldMessage = message;
    heldUntilMs = juce::Time::getMillisecondCounter() + (juce::uint32) holdMs;
    statusLabel.setText (message, juce::dontSendNotification);
}

void DaatInspectorAudioProcessorEditor::showSessionHints()
{
    auto ui = processorRef.getUiState();
    if (! ui.isValid())
        return;

    // A session profile that failed to restore.
    const auto warning = ui.getProperty ("profileRestoreWarning").toString();
    if (warning.isNotEmpty())
    {
        processorRef.setUiStateProperty ("profileRestoreWarning", juce::String());
        showMessage (warning, 15000);
        return;
    }

    // Audio is never stored in the session (spec §20); say where it came from.
    const juce::File source (ui.getProperty ("sourceFilePath", {}).toString());
    if (source != juce::File() && processorRef.getEngine().getResult() == nullptr)
    {
        if (source.existsAsFile())
            showMessage ("This session previously analysed " + source.getFileName()
                         + ". Audio is not stored in the session - use Load File to analyse it again.", 15000);
        else
            showMessage ("The previously analysed file is no longer available: "
                         + source.getFullPathName(), 15000);
    }
}

//==============================================================================
void DaatInspectorAudioProcessorEditor::timerCallback()
{
    auto& engine = processorRef.getEngine();
    const auto snap = engine.getSnapshot();

    // Header state line.
    const auto samples = processorRef.samplesProcessed.load (std::memory_order_relaxed);
    const bool audioFlowing = samples != lastSampleCount;
    lastSampleCount = samples;

    juce::String header;
    switch (snap.state)
    {
        case EngineState::Idle:        header = audioFlowing ? "Audio flowing" : "Idle"; break;
        case EngineState::Capturing:   header = "Capturing"; break;
        case EngineState::Live:        header = "Live"; break;
        case EngineState::LoadingFile: header = "Loading " + juce::String (juce::roundToInt (snap.progress * 100.0f)) + "%"; break;
        case EngineState::Analyzing:   header = "Analyzing " + juce::String (juce::roundToInt (snap.progress * 100.0f)) + "%"; break;
        case EngineState::Complete:    header = "Complete"; break;
        case EngineState::Failed:      header = "Failed"; break;
    }
    if (snap.sampleRate > 0.0)
        header << "  |  " << juce::String (snap.sampleRate / 1000.0, 1) << " kHz";
    if (snap.channels > 0)
        header << "  |  " << (snap.channels == 1 ? "mono" : "stereo");
    stateLabel.setText (header, juce::dontSendNotification);

    profileLabel.setText ("Profile: " + processorRef.getUiState().getProperty ("activeProfileName", "-").toString(),
                          juce::dontSendNotification);

    // UI messages stay up for a while before the engine status takes over again.
    if (heldMessage.isNotEmpty() && juce::Time::getMillisecondCounter() < heldUntilMs)
        statusLabel.setText (heldMessage, juce::dontSendNotification);
    else
    {
        heldMessage.clear();
        statusLabel.setText (snap.message, juce::dontSendNotification);
    }

    //--- Result panel -------------------------------------------------------
    juce::String verdict, likelihood = "--", confidence = "--", info;
    const juce::String duration = formatClock (snap.capturedSeconds);

    switch (snap.state)
    {
        case EngineState::Capturing:   verdict = "Capturing..."; break;
        case EngineState::Live:        verdict = "Live capture..."; break;
        case EngineState::LoadingFile: verdict = "Loading file..."; break;
        case EngineState::Analyzing:   verdict = "Analyzing..."; break;
        case EngineState::Failed:      verdict = "Analysis failed"; break;
        case EngineState::Idle:
            verdict = snap.capturedSeconds > 0.0 ? "Captured - not analyzed" : "No analysis yet";
            break;
        case EngineState::Complete:
            verdict = snap.verdictText.isNotEmpty() ? snap.verdictText : juce::String ("Inconclusive");
            if (snap.hasScore)
            {
                likelihood = pct (snap.likelihood);
                confidence = pct (snap.scoreConfidence)
                           + "   (" + juce::String (snap.agreeingGroups) + "/"
                           + juce::String (snap.activeGroups) + " groups agree)";
            }
            break;
    }

    const juce::String durationShown =
        (snap.state == EngineState::Complete && snap.analyzedSeconds > 0.0)
            ? formatClock (snap.analyzedSeconds) : duration;

    if (snap.hasSummary)
    {
        juce::StringArray parts;
        parts.add (snap.sourceName.isNotEmpty() ? snap.sourceName : juce::String ("Audio"));
        parts.add (juce::String (snap.channels) + " ch");
        parts.add (juce::String (snap.sampleRate / 1000.0, 1) + " kHz");
        parts.add ("peak " + juce::String (juce::Decibels::gainToDecibels (snap.peakLevel), 1) + " dB");
        parts.add ("RMS " + juce::String (snap.rmsDb, 1) + " dB");
        if (snap.clipped)   parts.add ("CLIPPING");
        if (snap.truncated) parts.add ("truncated at cap");
        info = parts.joinIntoString ("   |   ");

        if (snap.verdictDetail.isNotEmpty())
            info << "\n" << snap.verdictDetail;
    }

    resultPanel.setResult (verdict, likelihood, confidence, durationShown, info);

    //--- Result-driven views: only when the immutable result changes ---------
    const auto result = engine.getResult();
    if (result != shownResult)
    {
        shownResult = result;
        onResultChanged();
    }
    if (shownResult == nullptr)
        timelineView.setTimelineLength (snap.capturedSeconds);

    updateToolbar (snap);
}

void DaatInspectorAudioProcessorEditor::onResultChanged()
{
    evidencePanel.setResult (shownResult);
    timelineView.setResult (shownResult);

    // Restore the session's selected window once, after the first result -
    // only if the user actually selected one (the scale key is written on selection).
    if (! selectionRestored && shownResult != nullptr)
    {
        selectionRestored = true;
        auto ui = processorRef.getUiState();
        if (ui.hasProperty ("selectedTimelineScale"))
        {
            const double t = (double) ui.getProperty ("selectedTimelineSeconds", -1.0);
            const int scale = (int) ui.getProperty ("selectedTimelineScale", 0);
            if (t >= 0.0 && scale >= 0 && scale <= 2)
                timelineView.selectWindowAt ((AnalysisScale) scale, t);
        }
    }

    featureScorePanel.setContent (shownResult, timelineView.getSelectedWindow(),
                                  timelineView.getSelectedRange());
}

void DaatInspectorAudioProcessorEditor::onTimelineSelectionChanged()
{
    featureScorePanel.setContent (shownResult, timelineView.getSelectedWindow(),
                                  timelineView.getSelectedRange());

    if (const auto* w = timelineView.getSelectedWindowResult())
    {
        // Store the start: it lies inside the window's own drawn segment, so
        // selectWindowAt() restores this window (a midpoint would pick the next).
        processorRef.setUiStateProperty ("selectedTimelineSeconds", w->startSeconds);
        processorRef.setUiStateProperty ("selectedTimelineScale", (int) w->scale);
    }
}

void DaatInspectorAudioProcessorEditor::updateToolbar (const EngineSnapshot& snap)
{
    const bool busy = snap.state == EngineState::Capturing || snap.state == EngineState::Live
                   || snap.state == EngineState::LoadingFile || snap.state == EngineState::Analyzing;
    const bool hasAudio  = snap.capturedSeconds > 0.0 && ! busy;
    const bool hasResult = shownResult != nullptr && ! busy;
    const bool hasRange  = ! timelineView.getSelectedRange().isEmpty();
    const bool hasLength = snap.capturedSeconds > 0.0 || shownResult != nullptr;

    analyzeSelectionButton.setEnabled (hasAudio && hasRange);
    excludeSelectionButton.setEnabled (hasResult && hasRange);
    clearExclusionsButton.setEnabled (hasResult && ! shownResult->excludedRanges.empty());
    fullAnalysisButton.setEnabled (hasAudio);
    for (auto* b : { &zoomOutButton, &zoomInButton, &zoomFitButton })
        b->setEnabled (hasLength);
}

//==============================================================================
void DaatInspectorAudioProcessorEditor::showSettings (bool shouldShow)
{
    if (shouldShow)
    {
        reportPanel.setVisible (false);
        settingsPanel.panelShown();
        settingsPanel.setVisible (true);
        settingsPanel.toFront (true);
    }
    else
    {
        settingsPanel.setVisible (false);
        if (settingsPanel.hasUnappliedChanges())
            showMessage ("Settings closed with unapplied changes - they are kept until you Apply or reopen and Reset.");
    }
}

void DaatInspectorAudioProcessorEditor::showReport (bool shouldShow)
{
    if (shouldShow)
    {
        if (settingsPanel.isVisible())
            showSettings (false);
        reportPanel.panelShown();
        reportPanel.setVisible (true);
        reportPanel.toFront (true);
    }
    else
    {
        reportPanel.setVisible (false);
    }
}

void DaatInspectorAudioProcessorEditor::handleAction (const juce::String& actionName)
{
    auto& engine = processorRef.getEngine();

    // Make sure the engine sees the current parameter values before it scores.
    processorRef.syncRuntimeControls();

    if (actionName == "Capture")        engine.startCapture (false);
    else if (actionName == "Stop")      engine.stop();
    else if (actionName == "Analyze")   engine.requestAnalyze();
    else if (actionName == "Clear")     { engine.clear(); timelineView.clearSelectedRange(); }
    else if (actionName == "Load File") launchFileChooser();
    else if (actionName == "Settings")  showSettings (! settingsPanel.isVisible());
    else if (actionName == "Export")    showReport (! reportPanel.isVisible());
}

void DaatInspectorAudioProcessorEditor::launchFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select an audio file to analyze",
        juce::File(),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file.existsAsFile())
        {
            processorRef.syncRuntimeControls();
            processorRef.getEngine().loadFile (file);
        }
    });
}

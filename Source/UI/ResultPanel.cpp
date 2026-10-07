#include "ResultPanel.h"
#include "DaatLookAndFeel.h"

ResultPanel::ResultPanel()
{
    setOpaque (true);
}

void ResultPanel::setResult (const juce::String& verdict,
                             const juce::String& likelihood,
                             const juce::String& confidence,
                             const juce::String& duration,
                             const juce::String& info)
{
    if (verdict == verdictText && likelihood == likelihoodText
        && confidence == confidenceText && duration == durationText && info == infoText)
        return; // nothing changed - avoid needless repaint

    verdictText    = verdict;
    likelihoodText = likelihood;
    confidenceText = confidence;
    durationText   = duration;
    infoText       = info;
    repaint();
}

void ResultPanel::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::panel);
    g.setColour (DaatColours::panelOutline);
    g.drawRect (getLocalBounds());

    auto area = getLocalBounds().reduced (14, 10);

    auto valueRow = [&g, &area] (const juce::String& label, const juce::String& value)
    {
        auto row = area.removeFromTop (24);
        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (label, row.removeFromLeft (150), juce::Justification::centredLeft);
        g.setColour (DaatColours::textPrimary);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText (value, row, juce::Justification::centredLeft);
    };

    valueRow ("Verdict",           verdictText);
    valueRow ("Likelihood",        likelihoodText);
    valueRow ("Confidence",        confidenceText);
    valueRow ("Duration analyzed", durationText);

    if (infoText.isNotEmpty())
    {
        area.removeFromTop (6);
        auto infoArea = area.removeFromTop (52);
        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (infoText, infoArea, juce::Justification::topLeft, 3);
    }

    area.removeFromTop (6);
    g.setColour (DaatColours::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText (
        "This result is a statistical screening estimate. Audio processing, synthesis, "
        "editing, compression, mastering, and source separation may produce similar "
        "characteristics. The result does not prove how the recording was created.",
        area, juce::Justification::bottomLeft, 4);
}

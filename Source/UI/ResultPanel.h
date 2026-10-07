#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Main result panel.

    Displays Verdict / Likelihood / Confidence / Duration plus an info line and
    the standing scientific-limitations disclaimer. Values are pushed in from
    the editor's timer via setResult(); the panel holds no analysis state and
    only repaints when a field actually changes.
*/
class ResultPanel : public juce::Component
{
public:
    ResultPanel();

    void setResult (const juce::String& verdict,
                    const juce::String& likelihood,
                    const juce::String& confidence,
                    const juce::String& duration,
                    const juce::String& info);

    void paint (juce::Graphics& g) override;

private:
    juce::String verdictText    { "No analysis yet" };
    juce::String likelihoodText { "--" };
    juce::String confidenceText { "--" };
    juce::String durationText   { "0:00" };
    juce::String infoText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResultPanel)
};

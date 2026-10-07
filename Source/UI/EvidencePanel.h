#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analysis/AnalysisResult.h"

#include <memory>

//==============================================================================
/**
    Shows per-group scores, the strongest supporting and contradictory
    indicators, and the caveat list (spec §19 "Evidence panel").

    Holds the result as shared_ptr<const AnalysisResult>, so the worker thread
    can publish a new one at any time without the UI reading torn data.
*/
class EvidencePanel : public juce::Component
{
public:
    EvidencePanel();

    /** Swaps in a new result snapshot; repaints only if it actually changed. */
    void setResult (std::shared_ptr<const AnalysisResult> newResult);

    void paint (juce::Graphics& g) override;

private:
    std::shared_ptr<const AnalysisResult> result;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvidencePanel)
};

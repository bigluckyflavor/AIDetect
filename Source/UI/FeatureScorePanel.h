#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analysis/AnalysisResult.h"
#include "TimelineView.h"

#include <memory>

//==============================================================================
/**
    Explains one timeline selection (spec §14 / §19):
      - the selected window's per-feature results, ranked by contribution,
        with the strongest contributors marked
      - the same moment compared across short / medium / long windows
      - a summary of a selected time range
      - the full description of whichever feature the pointer is over

    Paint-only; reads the immutable result snapshot it is given.
*/
class FeatureScorePanel : public juce::Component
{
public:
    FeatureScorePanel();

    void setContent (std::shared_ptr<const AnalysisResult> result,
                     TimelineView::Selection selection,
                     juce::Range<double> range);

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    struct Row
    {
        const FeatureResult* feature = nullptr;
        float contribution = 0.0f;
    };

    const WindowResult* selectedWindow() const;
    std::vector<Row> rankedRows (const WindowResult& w) const;

    std::shared_ptr<const AnalysisResult> result;
    TimelineView::Selection selection;
    juce::Range<double> range;

    // Row hit-testing for the hover description.
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> rowHitAreas;
    juce::String hoveredFeatureId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeatureScorePanel)
};

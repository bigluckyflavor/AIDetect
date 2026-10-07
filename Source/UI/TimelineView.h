#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analysis/AnalysisResult.h"

#include <functional>
#include <memory>

//==============================================================================
/**
    Timeline heat map (spec §14).

    Three lanes - short, medium, long windows - each cell coloured by that
    window's indicator score and faded by its confidence. Cool = few indicators,
    warm = many; deliberately never an alarm red.

    Interaction:
      - click a cell               select that window
      - drag                       select a time range
      - mouse wheel                scroll
      - Ctrl/Cmd + wheel           zoom around the pointer
      - hover                      tooltip with the window's score

    The view only ever reads an immutable shared_ptr<const AnalysisResult>
    handed to it on the message thread; it never talks to the worker.
*/
class TimelineView : public juce::Component,
                     public juce::TooltipClient,
                     private juce::ScrollBar::Listener
{
public:
    struct Selection
    {
        bool valid = false;
        AnalysisScale scale = AnalysisScale::shortScale;
        int index = -1;
    };

    TimelineView();
    ~TimelineView() override;

    /** New result snapshot. Keeps the window selection by time where possible. */
    void setResult (std::shared_ptr<const AnalysisResult> newResult);

    /** Timeline length to show while no result exists yet (e.g. capturing). */
    void setTimelineLength (double seconds);

    Selection getSelectedWindow() const noexcept { return selected; }
    const WindowResult* getSelectedWindowResult() const;
    void selectWindowAt (AnalysisScale scale, double seconds);
    void clearWindowSelection();

    juce::Range<double> getSelectedRange() const noexcept { return selectedRange; }
    void clearSelectedRange();

    void zoomIn();
    void zoomOut();
    void zoomToFit();

    /** Called on the message thread whenever the window or range selection changes. */
    std::function<void()> onSelectionChanged;

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    juce::String getTooltip() override;

    /** Heat colour for a window score, faded by confidence. Shared with the
        feature panel so both use one visual language. */
    static juce::Colour heatColour (float likelihood, float confidence);

private:
    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;

    juce::Rectangle<int> getPlotArea() const;
    juce::Rectangle<int> getLaneArea (int lane) const;
    juce::Rectangle<int> getRulerArea() const;
    int laneAtY (int y) const;

    double timeToX (double seconds) const;
    double xToTime (double x) const;
    void   setView (double start, double length);
    void   updateScrollBar();

    /** Index of the window in `windows` whose hop segment contains `t`, or -1. */
    static int windowIndexAt (const std::vector<WindowResult>& windows, double t);
    /** Right edge of window i's drawn segment (next start, so overlaps tile). */
    static double segmentEnd (const std::vector<WindowResult>& windows, size_t i);

    std::shared_ptr<const AnalysisResult> result;
    double timelineSeconds = 0.0;
    double viewStart  = 0.0;
    double viewLength = 0.0;

    juce::ScrollBar scrollBar { false };

    Selection selected;
    double selectedTime  = -1.0; // midpoint of the selected window (fallback for reselection)
    double selectedStart = -1.0; // exact start of the selected window, to reselect after rescores

    void setSelection (AnalysisScale scale, int index);
    juce::Range<double> selectedRange;

    bool   dragging = false;
    double dragAnchorTime = 0.0;

    static constexpr int labelWidth   = 72;
    static constexpr int rulerHeight  = 18;
    static constexpr int legendHeight = 16;
    static constexpr int scrollHeight = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineView)
};

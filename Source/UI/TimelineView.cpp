#include "TimelineView.h"
#include "DaatLookAndFeel.h"

namespace
{
    constexpr double kMinViewSeconds = 0.5;

    const char* laneName (int lane)
    {
        switch (lane)
        {
            case 0:  return "Short";
            case 1:  return "Medium";
            default: return "Long";
        }
    }

    juce::String formatTime (double t, double step)
    {
        const int total = (int) std::floor (t + 1.0e-6);
        juce::String s = juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
        if (step < 1.0)
        {
            const int tenths = (int) std::round ((t - std::floor (t)) * 10.0) % 10;
            s << "." << tenths;
        }
        return s;
    }

    juce::String pct (float v)
    {
        return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 100.0f)) + "%";
    }
}

//==============================================================================
TimelineView::TimelineView()
{
    setOpaque (true);
    scrollBar.addListener (this);
    scrollBar.setAutoHide (false);
    addChildComponent (scrollBar);
}

TimelineView::~TimelineView()
{
    scrollBar.removeListener (this);
}

//==============================================================================
juce::Colour TimelineView::heatColour (float likelihood, float confidence)
{
    const juce::Colour cool (0xff2e5a7a), mid (0xff6b6f76), warm (0xffc7a44f);
    const float l = juce::jlimit (0.0f, 1.0f, likelihood);
    const auto c  = l < 0.5f ? cool.interpolatedWith (mid, l * 2.0f)
                             : mid.interpolatedWith (warm, (l - 0.5f) * 2.0f);
    return c.withAlpha (0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, confidence));
}

//==============================================================================
void TimelineView::setResult (std::shared_ptr<const AnalysisResult> newResult)
{
    const double newLength = newResult != nullptr ? newResult->timelineSeconds : timelineSeconds;
    const bool lengthChanged = std::abs (newLength - timelineSeconds) > 1.0e-6 || viewLength <= 0.0;

    result = std::move (newResult);
    timelineSeconds = newLength;

    if (lengthChanged)
        zoomToFit();

    // Keep the same window selected across rescores. Match its exact start
    // first: its midpoint lies in the *next* window's drawn hop segment, so
    // reselecting by midpoint would walk one window right on every rescore.
    if (result == nullptr)
    {
        selected = {};
        selectedTime = selectedStart = -1.0;
    }
    else if (selected.valid)
    {
        const auto& ws = result->windowsFor (selected.scale);
        int idx = -1;
        for (size_t i = 0; i < ws.size(); ++i)
            if (std::abs (ws[i].startSeconds - selectedStart) < 1.0e-6)
                idx = (int) i;
        if (idx < 0 && selectedTime >= 0.0)
            idx = windowIndexAt (ws, selectedTime); // windows changed (e.g. reanalysis)

        if (idx >= 0) setSelection (selected.scale, idx);
        else          selected = {};
    }

    if (! selectedRange.isEmpty())
        selectedRange = selectedRange.getIntersectionWith ({ 0.0, timelineSeconds });

    updateScrollBar();
    repaint();
}

void TimelineView::setTimelineLength (double seconds)
{
    if (result != nullptr || std::abs (seconds - timelineSeconds) < 1.0e-6)
        return;

    timelineSeconds = juce::jmax (0.0, seconds);
    zoomToFit();
}

const WindowResult* TimelineView::getSelectedWindowResult() const
{
    if (! selected.valid || result == nullptr)
        return nullptr;

    const auto& ws = result->windowsFor (selected.scale);
    if (selected.index < 0 || selected.index >= (int) ws.size())
        return nullptr;
    return &ws[(size_t) selected.index];
}

void TimelineView::selectWindowAt (AnalysisScale scale, double seconds)
{
    if (result == nullptr)
        return;

    const int idx = windowIndexAt (result->windowsFor (scale), seconds);
    if (idx < 0)
        return;

    setSelection (scale, idx);
    repaint();
}

void TimelineView::setSelection (AnalysisScale scale, int index)
{
    const auto& w = result->windowsFor (scale)[(size_t) index];
    selected      = { true, scale, index };
    selectedStart = w.startSeconds;
    selectedTime  = 0.5 * (w.startSeconds + w.endSeconds);
}

void TimelineView::clearWindowSelection()
{
    selected = {};
    selectedTime = selectedStart = -1.0;
    repaint();
}

void TimelineView::clearSelectedRange()
{
    selectedRange = {};
    repaint();
    if (onSelectionChanged != nullptr)
        onSelectionChanged();
}

//==============================================================================
void TimelineView::zoomIn()
{
    const double newLen = viewLength * 0.6;
    setView (viewStart + 0.5 * (viewLength - newLen), newLen);
}

void TimelineView::zoomOut()
{
    const double newLen = viewLength / 0.6;
    setView (viewStart - 0.5 * (newLen - viewLength), newLen);
}

void TimelineView::zoomToFit()
{
    setView (0.0, timelineSeconds);
}

void TimelineView::setView (double start, double length)
{
    if (timelineSeconds <= 0.0)
    {
        viewStart = viewLength = 0.0;
        updateScrollBar();
        repaint();
        return;
    }

    viewLength = juce::jlimit (juce::jmin (kMinViewSeconds, timelineSeconds), timelineSeconds, length);
    viewStart  = juce::jlimit (0.0, timelineSeconds - viewLength, start);
    updateScrollBar();
    repaint();
}

void TimelineView::updateScrollBar()
{
    const bool zoomed = timelineSeconds > 0.0 && viewLength < timelineSeconds - 1.0e-6;
    scrollBar.setVisible (zoomed);
    if (zoomed)
    {
        scrollBar.setRangeLimits (0.0, timelineSeconds, juce::dontSendNotification);
        scrollBar.setCurrentRange (viewStart, viewLength, juce::dontSendNotification);
    }
}

void TimelineView::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    setView (newRangeStart, viewLength);
}

//==============================================================================
juce::Rectangle<int> TimelineView::getRulerArea() const
{
    auto area = getLocalBounds().reduced (1);
    return area.removeFromTop (rulerHeight).withTrimmedLeft (labelWidth);
}

juce::Rectangle<int> TimelineView::getPlotArea() const
{
    auto area = getLocalBounds().reduced (1);
    area.removeFromTop (rulerHeight);
    area.removeFromBottom (scrollHeight + legendHeight);
    return area.withTrimmedLeft (labelWidth);
}

juce::Rectangle<int> TimelineView::getLaneArea (int lane) const
{
    const auto plot = getPlotArea();
    const int gap = 2;
    const int h = (plot.getHeight() - 2 * gap) / 3;
    return { plot.getX(), plot.getY() + lane * (h + gap), plot.getWidth(), h };
}

int TimelineView::laneAtY (int y) const
{
    for (int lane = 0; lane < 3; ++lane)
    {
        const auto a = getLaneArea (lane);
        if (y >= a.getY() && y < a.getBottom())
            return lane;
    }
    return -1;
}

double TimelineView::timeToX (double seconds) const
{
    const auto plot = getPlotArea();
    if (viewLength <= 0.0)
        return (double) plot.getX();
    return (double) plot.getX() + (seconds - viewStart) / viewLength * (double) plot.getWidth();
}

double TimelineView::xToTime (double x) const
{
    const auto plot = getPlotArea();
    if (plot.getWidth() <= 0)
        return viewStart;
    return viewStart + (x - (double) plot.getX()) / (double) plot.getWidth() * viewLength;
}

int TimelineView::windowIndexAt (const std::vector<WindowResult>& windows, double t)
{
    for (size_t i = 0; i < windows.size(); ++i)
        if (t >= windows[i].startSeconds && t < segmentEnd (windows, i))
            return (int) i;
    return -1;
}

double TimelineView::segmentEnd (const std::vector<WindowResult>& windows, size_t i)
{
    // Overlapping windows are drawn as their hop segment so the lane tiles.
    if (i + 1 < windows.size())
        return juce::jmin (windows[i].endSeconds, windows[i + 1].startSeconds);
    return windows[i].endSeconds;
}

//==============================================================================
void TimelineView::resized()
{
    auto area = getLocalBounds().reduced (1);
    scrollBar.setBounds (area.removeFromBottom (scrollHeight).withTrimmedLeft (labelWidth));
    updateScrollBar();
}

void TimelineView::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::panel);
    g.setColour (DaatColours::panelOutline);
    g.drawRect (getLocalBounds());

    const auto plot = getPlotArea();

    if (timelineSeconds <= 0.0 || plot.getWidth() <= 0)
    {
        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText ("Timeline heat map - capture or load audio to begin",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    //--- Ruler --------------------------------------------------------------
    {
        const auto ruler = getRulerArea();
        const double pxPerSec = (double) plot.getWidth() / juce::jmax (1.0e-6, viewLength);
        double step = 300.0;
        for (double candidate : { 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0 })
            if (candidate * pxPerSec >= 70.0) { step = candidate; break; }

        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        for (double t = std::ceil (viewStart / step) * step; t <= viewStart + viewLength + 1.0e-9; t += step)
        {
            const int x = juce::roundToInt (timeToX (t));
            g.setColour (DaatColours::panelOutline);
            g.drawVerticalLine (x, (float) ruler.getBottom() - 5.0f, (float) plot.getBottom());
            g.setColour (DaatColours::textSecondary);
            g.drawText (formatTime (t, step), x + 3, ruler.getY(), 60, ruler.getHeight(),
                        juce::Justification::centredLeft);
        }
    }

    //--- Lanes --------------------------------------------------------------
    for (int lane = 0; lane < 3; ++lane)
    {
        const auto laneArea = getLaneArea (lane);
        g.setColour (DaatColours::background);
        g.fillRect (laneArea);

        juce::String label = laneName (lane);
        if (result != nullptr)
        {
            const auto& ws = result->windowsFor ((AnalysisScale) lane);
            if (! ws.empty())
                label << "  " << juce::String (ws.front().endSeconds - ws.front().startSeconds, 0) << " s";
        }
        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (label, 6, laneArea.getY(), labelWidth - 8, laneArea.getHeight(),
                    juce::Justification::centredLeft);
    }

    if (result == nullptr)
    {
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Not analysed yet", plot, juce::Justification::centred);
        return;
    }

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (plot);

        for (int lane = 0; lane < 3; ++lane)
        {
            const auto laneArea = getLaneArea (lane);
            const auto& ws = result->windowsFor ((AnalysisScale) lane);

            for (size_t i = 0; i < ws.size(); ++i)
            {
                const double x0 = timeToX (ws[i].startSeconds);
                const double x1 = timeToX (segmentEnd (ws, i));
                if (x1 < plot.getX() || x0 > plot.getRight())
                    continue;

                g.setColour (heatColour (ws[i].likelihood, ws[i].confidence));
                g.fillRect (juce::Rectangle<double> (x0, laneArea.getY(),
                                                     juce::jmax (1.0, x1 - x0), laneArea.getHeight()).toFloat());
            }
        }

        const int lanesTop    = getLaneArea (0).getY();
        const int lanesBottom = getLaneArea (2).getBottom();

        const auto shadeSpan = [&] (double t0, double t1, juce::Colour c)
        {
            const double x0 = juce::jmax ((double) plot.getX(), timeToX (t0));
            const double x1 = juce::jmin ((double) plot.getRight(), timeToX (t1));
            if (x1 <= x0) return juce::Rectangle<float>();
            juce::Rectangle<float> r ((float) x0, (float) lanesTop, (float) (x1 - x0), (float) (lanesBottom - lanesTop));
            g.setColour (c);
            g.fillRect (r);
            return r;
        };

        //--- Regions outside a range analysis ---
        if (result->isRangeAnalysis)
        {
            for (const auto& span : { juce::Range<double> (0.0, result->rangeStartSeconds),
                                      juce::Range<double> (result->rangeEndSeconds, timelineSeconds) })
            {
                const auto r = shadeSpan (span.getStart(), span.getEnd(), juce::Colours::black.withAlpha (0.55f));
                if (r.getWidth() > 70.0f)
                {
                    g.setColour (DaatColours::textDisabled);
                    g.setFont (juce::Font (juce::FontOptions (11.0f)));
                    g.drawText ("not analysed", r, juce::Justification::centred);
                }
            }
        }

        //--- Excluded sections: dark wash + hatching ---
        for (const auto& ex : result->excludedRanges)
        {
            const auto r = shadeSpan (ex.getStart(), ex.getEnd(), juce::Colours::black.withAlpha (0.45f));
            if (r.isEmpty()) continue;

            juce::Graphics::ScopedSaveState hatchClip (g);
            g.reduceClipRegion (r.toNearestInt());
            g.setColour (DaatColours::textDisabled.withAlpha (0.5f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 7.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);

            if (r.getWidth() > 60.0f)
            {
                g.setColour (DaatColours::textPrimary);
                g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
                g.drawText ("excluded", r, juce::Justification::centred);
            }
        }

        //--- Selected range ---
        if (! selectedRange.isEmpty())
        {
            const auto r = shadeSpan (selectedRange.getStart(), selectedRange.getEnd(),
                                      DaatColours::accent.withAlpha (0.18f));
            g.setColour (DaatColours::accent);
            g.drawVerticalLine (juce::roundToInt (r.getX()), r.getY(), r.getBottom());
            g.drawVerticalLine (juce::roundToInt (r.getRight()) - 1, r.getY(), r.getBottom());
        }

        //--- Selected window: its true extent, not just the drawn segment ---
        if (const auto* w = getSelectedWindowResult())
        {
            const auto laneArea = getLaneArea ((int) selected.scale);
            const double x0 = timeToX (w->startSeconds);
            const double x1 = timeToX (w->endSeconds);
            g.setColour (DaatColours::textPrimary);
            g.drawRect (juce::Rectangle<double> (x0, laneArea.getY(), juce::jmax (2.0, x1 - x0),
                                                 laneArea.getHeight()).toFloat(), 2.0f);
        }
    }

    //--- Selected range length in the ruler ---
    if (! selectedRange.isEmpty())
    {
        const auto ruler = getRulerArea();
        const int x = juce::roundToInt (timeToX (selectedRange.getStart()));
        g.setColour (DaatColours::accent);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("selection " + juce::String (selectedRange.getLength(), 1) + " s",
                    x + 3, ruler.getY(), 140, ruler.getHeight(), juce::Justification::centredLeft);
    }

    //--- Legend ---------------------------------------------------------------
    {
        auto legend = getLocalBounds().reduced (1);
        legend.removeFromBottom (scrollHeight);
        legend = legend.removeFromBottom (legendHeight).withTrimmedLeft (labelWidth).reduced (0, 3);

        auto bar = legend.removeFromLeft (90);
        for (int x = 0; x < bar.getWidth(); ++x)
        {
            g.setColour (heatColour ((float) x / (float) juce::jmax (1, bar.getWidth() - 1), 1.0f));
            g.drawVerticalLine (bar.getX() + x, (float) bar.getY(), (float) bar.getBottom());
        }

        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText ("  fewer -> more indicators  |  faded = low confidence  |  "
                    "drag to select | Ctrl+wheel to zoom",
                    legend, juce::Justification::centredLeft);
    }
}

//==============================================================================
void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    dragAnchorTime = juce::jlimit (0.0, timelineSeconds, xToTime ((double) e.x));
}

void TimelineView::mouseDrag (const juce::MouseEvent& e)
{
    if (timelineSeconds <= 0.0 || ! getPlotArea().contains (e.getMouseDownPosition()))
        return;

    if (! dragging && std::abs (e.getDistanceFromDragStartX()) > 4)
        dragging = true;

    if (dragging)
    {
        const double t = juce::jlimit (0.0, timelineSeconds, xToTime ((double) e.x));
        selectedRange = juce::Range<double> (juce::jmin (dragAnchorTime, t), juce::jmax (dragAnchorTime, t));
        repaint();
    }
}

void TimelineView::mouseUp (const juce::MouseEvent& e)
{
    if (timelineSeconds <= 0.0 || ! getPlotArea().contains (e.getMouseDownPosition()))
        return;

    if (dragging)
    {
        if (selectedRange.getLength() < 0.05)
            selectedRange = {};
    }
    else
    {
        // A click selects the window under the pointer and drops any range.
        selectedRange = {};
        const int lane = laneAtY (e.y);
        if (lane >= 0 && result != nullptr)
        {
            const auto scale = (AnalysisScale) lane;
            const int idx = windowIndexAt (result->windowsFor (scale), xToTime ((double) e.x));
            if (idx >= 0)
                setSelection (scale, idx);
            else
            {
                selected = {};
                selectedTime = selectedStart = -1.0;
            }
        }
    }

    dragging = false;
    repaint();
    if (onSelectionChanged != nullptr)
        onSelectionChanged();
}

void TimelineView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (timelineSeconds <= 0.0 || viewLength <= 0.0)
        return;

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double factor = wheel.deltaY > 0.0f ? 0.8 : 1.25;
        const double anchor = xToTime ((double) e.x);
        setView (anchor - (anchor - viewStart) * factor, viewLength * factor);
    }
    else
    {
        const double delta = (wheel.deltaX != 0.0f ? -wheel.deltaX : -wheel.deltaY) * viewLength * 0.5;
        setView (viewStart + delta, viewLength);
    }
}

juce::String TimelineView::getTooltip()
{
    if (result == nullptr)
        return {};

    const auto p = getMouseXYRelative();
    if (! getPlotArea().contains (p))
        return {};

    const int lane = laneAtY (p.y);
    if (lane < 0)
        return {};

    const auto& ws = result->windowsFor ((AnalysisScale) lane);
    const int idx = windowIndexAt (ws, xToTime ((double) p.x));
    if (idx < 0)
        return {};

    const auto& w = ws[(size_t) idx];
    juce::String tip;
    tip << laneName (lane) << " window  " << juce::String (w.startSeconds, 1) << "-"
        << juce::String (w.endSeconds, 1) << " s\n"
        << "Indicator score " << pct (w.likelihood) << "  |  confidence " << pct (w.confidence);
    if (result->isExcluded (w.startSeconds, w.endSeconds))
        tip << "\nExcluded from the overall score";
    return tip;
}

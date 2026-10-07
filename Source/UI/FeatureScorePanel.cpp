#include "FeatureScorePanel.h"
#include "DaatLookAndFeel.h"

#include <algorithm>

namespace
{
    juce::String pct (float v)
    {
        return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 100.0f)) + "%";
    }

    juce::String formatRaw (float v)
    {
        const float a = std::abs (v);
        if (a >= 100.0f) return juce::String (v, 0);
        if (a >= 10.0f)  return juce::String (v, 1);
        return juce::String (v, 3);
    }

    const char* scaleName (AnalysisScale s)
    {
        switch (s)
        {
            case AnalysisScale::shortScale:  return "Short";
            case AnalysisScale::mediumScale: return "Medium";
            case AnalysisScale::longScale:   return "Long";
        }
        return "Short";
    }

    /** Window of a scale whose centre is nearest `t` among those containing it. */
    const WindowResult* windowAt (const std::vector<WindowResult>& ws, double t)
    {
        const WindowResult* best = nullptr;
        double bestDist = 1.0e9;
        for (const auto& w : ws)
        {
            if (t < w.startSeconds || t >= w.endSeconds)
                continue;
            const double d = std::abs (0.5 * (w.startSeconds + w.endSeconds) - t);
            if (d < bestDist) { bestDist = d; best = &w; }
        }
        return best;
    }
}

//==============================================================================
FeatureScorePanel::FeatureScorePanel()
{
    setOpaque (true);
}

void FeatureScorePanel::setContent (std::shared_ptr<const AnalysisResult> newResult,
                                    TimelineView::Selection newSelection,
                                    juce::Range<double> newRange)
{
    result    = std::move (newResult);
    selection = newSelection;
    range     = newRange;
    repaint();
}

const WindowResult* FeatureScorePanel::selectedWindow() const
{
    if (result == nullptr || ! selection.valid)
        return nullptr;
    const auto& ws = result->windowsFor (selection.scale);
    if (selection.index < 0 || selection.index >= (int) ws.size())
        return nullptr;
    return &ws[(size_t) selection.index];
}

std::vector<FeatureScorePanel::Row> FeatureScorePanel::rankedRows (const WindowResult& w) const
{
    std::vector<Row> rows;
    for (const auto& f : w.features)
    {
        Row r;
        r.feature = &f;
        r.contribution = f.valid ? f.suspicionScore * f.confidence * f.effectiveWeight : -1.0f;
        rows.push_back (r);
    }
    std::sort (rows.begin(), rows.end(),
               [] (const Row& a, const Row& b) { return a.contribution > b.contribution; });
    return rows;
}

//==============================================================================
void FeatureScorePanel::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::panel);
    g.setColour (DaatColours::panelOutline);
    g.drawRect (getLocalBounds());

    rowHitAreas.clear();
    auto area = getLocalBounds().reduced (12, 8);

    g.setColour (DaatColours::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Selection details", area.removeFromTop (18), juce::Justification::topLeft);

    // Bottom strip: full description of the hovered (or strongest) feature.
    auto descArea = area.removeFromBottom (32);

    const auto hint = [&g] (juce::Rectangle<int> r, const juce::String& text)
    {
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (text, r, juce::Justification::topLeft, 3);
    };

    if (result == nullptr)
    {
        hint (area, "Analyse audio, then click a timeline section to see which features drove its score.");
        return;
    }

    //--- Range summary ------------------------------------------------------
    if (! range.isEmpty())
    {
        juce::String line;
        line << "Selected range " << juce::String (range.getStart(), 1) << "-"
             << juce::String (range.getEnd(), 1) << " s (" << juce::String (range.getLength(), 1) << " s):  ";

        for (auto scale : { AnalysisScale::shortScale, AnalysisScale::mediumScale, AnalysisScale::longScale })
        {
            double sum = 0.0;
            int n = 0;
            for (const auto& w : result->windowsFor (scale))
            {
                const double mid = 0.5 * (w.startSeconds + w.endSeconds);
                if (range.contains (mid)) { sum += w.likelihood; ++n; }
            }
            line << scaleName (scale) << " " << (n > 0 ? pct ((float) (sum / n)) : juce::String ("-"))
                 << " (" << n << ")   ";
        }

        g.setColour (DaatColours::accent);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (line + "\nMean window scores only - use Analyze Selection for a dedicated estimate.",
                          area.removeFromTop (32), juce::Justification::topLeft, 2);
        area.removeFromTop (4);
    }

    const auto* w = selectedWindow();
    if (w == nullptr)
    {
        if (range.isEmpty())
            hint (area, "Click a timeline section to see which features drove its score. "
                        "Drag across the timeline to select a range.");
        return;
    }

    //--- Window header -------------------------------------------------------
    {
        juce::String header;
        header << scaleName (w->scale) << " window " << juce::String (w->startSeconds, 1) << "-"
               << juce::String (w->endSeconds, 1) << " s   |   indicator score " << pct (w->likelihood)
               << "   |   confidence " << pct (w->confidence);
        if (result->isExcluded (w->startSeconds, w->endSeconds))
            header << "   |   excluded from overall";

        g.setColour (DaatColours::textPrimary);
        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        g.drawText (header, area.removeFromTop (17), juce::Justification::centredLeft, true);

        // The same moment at every scale (spec: compare short/medium/long).
        const double mid = 0.5 * (w->startSeconds + w->endSeconds);
        juce::String compare ("Same moment:   ");
        for (auto scale : { AnalysisScale::shortScale, AnalysisScale::mediumScale, AnalysisScale::longScale })
        {
            const auto* other = windowAt (result->windowsFor (scale), mid);
            compare << scaleName (scale) << " " << (other != nullptr ? pct (other->likelihood) : juce::String ("-"))
                    << "     ";
        }
        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (compare, area.removeFromTop (16), juce::Justification::centredLeft, true);
        area.removeFromTop (4);
    }

    if (w->features.empty())
    {
        hint (area, "No features are assigned to this window scale in the active profile.");
        return;
    }

    //--- Feature table --------------------------------------------------------
    const int colName = 190, colRaw = 70, colBar = 120, colConf = 52, colWeight = 56;
    {
        auto head = area.removeFromTop (15);
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("FEATURE",   head.removeFromLeft (colName),   juce::Justification::centredLeft);
        g.drawText ("RAW",       head.removeFromLeft (colRaw),    juce::Justification::centredLeft);
        g.drawText ("INDICATOR", head.removeFromLeft (colBar),    juce::Justification::centredLeft);
        g.drawText ("CONF.",     head.removeFromLeft (colConf),   juce::Justification::centredLeft);
        g.drawText ("WEIGHT",    head.removeFromLeft (colWeight), juce::Justification::centredLeft);
        g.drawText ("DETAIL",    head,                            juce::Justification::centredLeft);
    }

    const auto rows = rankedRows (*w);
    const int rowH = 16;
    int shown = 0, strongestMarked = 0;
    juce::String strongestId;

    for (const auto& row : rows)
    {
        if (area.getHeight() < rowH)
            break;

        const auto& f = *row.feature;
        const auto* desc = findFeatureDescriptor (f.featureId);
        auto r = area.removeFromTop (rowH);
        rowHitAreas.emplace_back (r, f.featureId);

        if (f.featureId == hoveredFeatureId)
        {
            g.setColour (DaatColours::buttonHover);
            g.fillRect (r);
        }

        const bool strongest = f.valid && strongestMarked < 3 && f.suspicionScore > 0.05f && row.contribution > 0.0f;
        if (strongest)
        {
            if (strongestMarked == 0) strongestId = f.featureId;
            ++strongestMarked;
        }

        g.setFont (juce::Font (juce::FontOptions (11.5f, strongest ? juce::Font::bold : juce::Font::plain)));
        g.setColour (f.valid ? DaatColours::textPrimary : DaatColours::textDisabled);
        g.drawText ((strongest ? "> " : "  ") + (desc != nullptr ? desc->displayName : f.featureId),
                    r.removeFromLeft (colName), juce::Justification::centredLeft, true);

        g.setColour (DaatColours::textSecondary);
        g.drawText (f.valid ? formatRaw (f.rawValue) : juce::String ("n/a"),
                    r.removeFromLeft (colRaw), juce::Justification::centredLeft);

        auto bar = r.removeFromLeft (colBar).reduced (0, 4).withTrimmedRight (12);
        g.setColour (DaatColours::panelOutline);
        g.fillRect (bar);
        if (f.valid)
        {
            g.setColour (TimelineView::heatColour (f.suspicionScore, f.confidence));
            g.fillRect (bar.withWidth (juce::roundToInt ((float) bar.getWidth() * juce::jlimit (0.0f, 1.0f, f.suspicionScore))));
        }

        g.setColour (DaatColours::textSecondary);
        g.drawText (f.valid ? pct (f.confidence) : juce::String ("-"), r.removeFromLeft (colConf), juce::Justification::centredLeft);
        g.drawText (juce::String (f.effectiveWeight, 2), r.removeFromLeft (colWeight), juce::Justification::centredLeft);
        g.drawText (f.explanation, r, juce::Justification::centredLeft, true);
        ++shown;
    }

    if (shown < (int) rows.size())
    {
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText ("+" + juce::String ((int) rows.size() - shown) + " more (enlarge the window)",
                    area.removeFromTop (14), juce::Justification::centredLeft);
    }

    //--- Description of the hovered feature, else the strongest contributor ---
    const juce::String describeId = hoveredFeatureId.isNotEmpty() ? hoveredFeatureId : strongestId;
    if (const auto* desc = findFeatureDescriptor (describeId))
    {
        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawFittedText (desc->displayName + ": " + desc->description, descArea,
                          juce::Justification::topLeft, 2);
    }
    else
    {
        hint (descArea, "Hover a feature to read what it measures.");
    }
}

void FeatureScorePanel::mouseMove (const juce::MouseEvent& e)
{
    juce::String id;
    for (const auto& [area, featureId] : rowHitAreas)
        if (area.contains (e.getPosition()))
            id = featureId;

    if (id != hoveredFeatureId)
    {
        hoveredFeatureId = id;
        repaint();
    }
}

void FeatureScorePanel::mouseExit (const juce::MouseEvent&)
{
    if (hoveredFeatureId.isNotEmpty())
    {
        hoveredFeatureId.clear();
        repaint();
    }
}

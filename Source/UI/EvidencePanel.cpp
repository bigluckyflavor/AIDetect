#include "EvidencePanel.h"
#include "DaatLookAndFeel.h"

EvidencePanel::EvidencePanel()
{
    setOpaque (true);
}

void EvidencePanel::setResult (std::shared_ptr<const AnalysisResult> newResult)
{
    if (result == newResult)
        return;

    result = std::move (newResult);
    repaint();
}

void EvidencePanel::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::panel);
    g.setColour (DaatColours::panelOutline);
    g.drawRect (getLocalBounds());

    auto area = getLocalBounds().reduced (12, 8);

    g.setColour (DaatColours::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Evidence", area.removeFromTop (18), juce::Justification::topLeft);

    if (result == nullptr)
    {
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("No analysis yet.", area, juce::Justification::centredLeft);
        return;
    }

    // Two columns: group scores on the left, evidence lists on the right.
    auto left  = area.removeFromLeft (juce::jmax (150, area.getWidth() / 3));
    auto right = area.withTrimmedLeft (12);

    //--- Group scores -------------------------------------------------------
    g.setColour (DaatColours::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("Feature groups", left.removeFromTop (16), juce::Justification::topLeft);

    for (const auto& grp : result->groupScores)
    {
        if (left.getHeight() < 16)
            break;

        auto row = left.removeFromTop (16);
        auto nameArea = row.removeFromLeft (86);

        g.setColour (grp.valid ? DaatColours::textPrimary : DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (grp.displayName, nameArea, juce::Justification::centredLeft);

        if (! grp.valid)
        {
            g.setColour (DaatColours::textDisabled);
            g.drawText ("n/a", row, juce::Justification::centredLeft);
            continue;
        }

        // Score bar. Deliberately a neutral blue - not a red "AI" alarm.
        auto bar = row.removeFromLeft (juce::jmax (0, row.getWidth() - 34)).reduced (0, 4);
        g.setColour (DaatColours::panelOutline);
        g.fillRect (bar);
        auto fill = bar.withWidth (juce::roundToInt (bar.getWidth() * juce::jlimit (0.0f, 1.0f, grp.score)));
        g.setColour (DaatColours::accent.withAlpha (0.45f + 0.45f * grp.score));
        g.fillRect (fill);

        g.setColour (DaatColours::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (juce::String (grp.score, 2), row, juce::Justification::centredRight);
    }

    //--- Evidence lists ----------------------------------------------------
    const auto drawList = [&g, &right] (const juce::String& title,
                                        const std::vector<EvidenceItem>& items,
                                        juce::Colour titleColour,
                                        int maxRows)
    {
        if (right.getHeight() < 16)
            return;

        g.setColour (titleColour);
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (title, right.removeFromTop (15), juce::Justification::topLeft);

        if (items.empty())
        {
            g.setColour (DaatColours::textDisabled);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText ("none", right.removeFromTop (14), juce::Justification::topLeft);
            return;
        }

        int drawn = 0;
        for (const auto& item : items)
        {
            if (drawn >= maxRows || right.getHeight() < 14)
                break;

            auto row = right.removeFromTop (14);
            g.setColour (DaatColours::textSecondary);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (item.displayName + " - " + item.detail, row,
                        juce::Justification::centredLeft, true);
            ++drawn;
        }
    };

    drawList ("Strongest indicators", result->strongestEvidence, DaatColours::accentWarm, 3);
    right.removeFromTop (4);
    drawList ("Contradictory indicators", result->contradictoryEvidence, DaatColours::accent, 2);

    //--- Caveats ------------------------------------------------------------
    if (right.getHeight() > 26 && ! result->caveats.isEmpty())
    {
        right.removeFromTop (4);
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("Caveats (" + juce::String (result->caveats.size()) + ")",
                    right.removeFromTop (13), juce::Justification::topLeft);

        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawFittedText (result->caveats[0], right, juce::Justification::topLeft, 2);
    }
}

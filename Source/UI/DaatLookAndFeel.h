#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// DAAT dark palette. Kept in one place so every panel stays consistent.
namespace DaatColours
{
    const juce::Colour background     { 0xff14161a };
    const juce::Colour panel          { 0xff1c1f25 };
    const juce::Colour panelOutline   { 0xff2a2e36 };
    const juce::Colour header         { 0xff101114 };
    const juce::Colour textPrimary    { 0xffd8dbe0 };
    const juce::Colour textSecondary  { 0xff8b919c };
    const juce::Colour textDisabled   { 0xff565b64 };
    const juce::Colour accent         { 0xff4fa3c7 };  // calm blue - deliberately not a red "AI" alarm
    const juce::Colour accentWarm     { 0xffc7a44f };
    const juce::Colour buttonFill     { 0xff262a32 };
    const juce::Colour buttonHover    { 0xff313641 };
}

//==============================================================================
class DaatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DaatLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, DaatColours::background);
        setColour (juce::Label::textColourId,                 DaatColours::textPrimary);

        setColour (juce::TextButton::buttonColourId,   DaatColours::buttonFill);
        setColour (juce::TextButton::buttonOnColourId, DaatColours::accent.withAlpha (0.35f));
        setColour (juce::TextButton::textColourOffId,  DaatColours::textPrimary);
        setColour (juce::TextButton::textColourOnId,   DaatColours::textPrimary);
        setColour (juce::ComboBox::outlineColourId,    DaatColours::panelOutline);

        setColour (juce::Slider::backgroundColourId,         DaatColours::panelOutline);
        setColour (juce::Slider::trackColourId,              DaatColours::accent.withAlpha (0.6f));
        setColour (juce::Slider::thumbColourId,              DaatColours::accent);
        setColour (juce::Slider::textBoxTextColourId,        DaatColours::textPrimary);
        setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);

        setColour (juce::ToggleButton::textColourId,     DaatColours::textPrimary);
        setColour (juce::ToggleButton::tickColourId,     DaatColours::accent);
        setColour (juce::ToggleButton::tickDisabledColourId, DaatColours::textDisabled);

        // Settings editor widgets.
        setColour (juce::TextEditor::backgroundColourId,     DaatColours::background);
        setColour (juce::TextEditor::textColourId,           DaatColours::textPrimary);
        setColour (juce::TextEditor::outlineColourId,        DaatColours::panelOutline);
        setColour (juce::TextEditor::focusedOutlineColourId, DaatColours::accent);
        setColour (juce::TextEditor::highlightColourId,      DaatColours::accent.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId,      DaatColours::textPrimary);

        setColour (juce::ComboBox::backgroundColourId, DaatColours::buttonFill);
        setColour (juce::ComboBox::textColourId,       DaatColours::textPrimary);
        setColour (juce::ComboBox::arrowColourId,      DaatColours::textSecondary);

        setColour (juce::PopupMenu::backgroundColourId,            DaatColours::panel);
        setColour (juce::PopupMenu::textColourId,                  DaatColours::textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, DaatColours::accent.withAlpha (0.4f));
        setColour (juce::PopupMenu::highlightedTextColourId,       DaatColours::textPrimary);

        setColour (juce::PropertyComponent::backgroundColourId, DaatColours::panel);
        setColour (juce::PropertyComponent::labelTextColourId,  DaatColours::textSecondary);
        setColour (juce::ListBox::backgroundColourId,           DaatColours::background);
        setColour (juce::ScrollBar::thumbColourId,              DaatColours::buttonHover);
        setColour (juce::Label::textWhenEditingColourId,        DaatColours::textPrimary);
        setColour (juce::Label::backgroundWhenEditingColourId,  DaatColours::background);

        setColour (juce::TooltipWindow::backgroundColourId, DaatColours::header);
        setColour (juce::TooltipWindow::textColourId,       DaatColours::textPrimary);
        setColour (juce::TooltipWindow::outlineColourId,    DaatColours::panelOutline);

        setColour (juce::AlertWindow::backgroundColourId, DaatColours::panel);
        setColour (juce::AlertWindow::textColourId,       DaatColours::textPrimary);
        setColour (juce::AlertWindow::outlineColourId,    DaatColours::panelOutline);
    }
};

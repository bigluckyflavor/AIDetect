#include "AnalysisControls.h"
#include "../PluginProcessor.h"

AnalysisControls::AnalysisControls (juce::AudioProcessorValueTreeState& apvts)
    : sensitivityAttachment (apvts, ParamIDs::analysisSensitivity, sensitivitySlider),
      autoAnalyzeAttachment (apvts, ParamIDs::autoAnalyze, autoAnalyzeToggle)
{
    auto setupButton = [this] (juce::TextButton& b)
    {
        addAndMakeVisible (b);
        b.onClick = [this, &b]
        {
            if (onAction != nullptr)
                onAction (b.getButtonText());
        };
    };

    setupButton (captureButton);
    setupButton (stopButton);
    setupButton (analyzeButton);
    setupButton (clearButton);
    setupButton (loadFileButton);
    setupButton (exportButton);
    setupButton (settingsButton);

    sensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sensitivitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    addAndMakeVisible (sensitivitySlider);

    sensitivityLabel.setJustificationType (juce::Justification::centredRight);
    sensitivityLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (sensitivityLabel);

    addAndMakeVisible (autoAnalyzeToggle);
}

void AnalysisControls::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    const int gap = 6;
    auto placeButton = [&area, gap] (juce::TextButton& b, int width)
    {
        b.setBounds (area.removeFromLeft (width));
        area.removeFromLeft (gap);
    };

    placeButton (captureButton,  74);
    placeButton (stopButton,     56);
    placeButton (analyzeButton,  72);
    placeButton (clearButton,    58);
    placeButton (loadFileButton, 78);
    placeButton (exportButton,   64);
    placeButton (settingsButton, 74);

    autoAnalyzeToggle.setBounds (area.removeFromRight (60));
    sensitivitySlider.setBounds (area.removeFromRight (juce::jmin (200, area.getWidth() - 70)));
    sensitivityLabel.setBounds (area.removeFromRight (70));
}

#include "DetectionSettingsPanel.h"
#include "DaatLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Utility/JsonUtilities.h"

#include <iterator>

namespace
{
    //==========================================================================
    // Property components bound to getter/setter lambdas. The lambdas look the
    // value up in the working profile on every call, so they never hold a
    // pointer into a map that a later load or reset may have replaced.

    class NumberProperty : public juce::PropertyComponent
    {
    public:
        NumberProperty (const juce::String& name,
                        std::function<double()> getterIn,
                        std::function<void (double)> setterIn,
                        int decimalPlaces)
            : juce::PropertyComponent (name),
              getter (std::move (getterIn)), setter (std::move (setterIn)), decimals (decimalPlaces)
        {
            editor.setEditable (true, true, false);
            editor.setColour (juce::Label::backgroundColourId, DaatColours::background);
            editor.setColour (juce::Label::outlineColourId, DaatColours::panelOutline);
            editor.setColour (juce::Label::textColourId, DaatColours::textPrimary);
            editor.onTextChange = [this]
            {
                const auto text = editor.getText().trim();
                if (text.isNotEmpty() && text.containsOnly ("0123456789.-+eE"))
                    setter (text.getDoubleValue());
                refresh(); // reverts unparsable input
            };
            addAndMakeVisible (editor);
            refresh();
        }

        void refresh() override
        {
            editor.setText (juce::String (getter(), decimals), juce::dontSendNotification);
        }

    private:
        juce::Label editor;
        std::function<double()> getter;
        std::function<void (double)> setter;
        int decimals;
    };

    class BoolProperty : public juce::BooleanPropertyComponent
    {
    public:
        BoolProperty (const juce::String& name, std::function<bool()> getterIn,
                      std::function<void (bool)> setterIn)
            : juce::BooleanPropertyComponent (name, "On", "Off"),
              getter (std::move (getterIn)), setter (std::move (setterIn))
        {
            refresh();
        }

        void setState (bool newState) override { setter (newState); refresh(); }
        bool getState() const override         { return getter(); }

    private:
        std::function<bool()> getter;
        std::function<void (bool)> setter;
    };

    class SliderProperty : public juce::PropertyComponent
    {
    public:
        SliderProperty (const juce::String& name, double min, double max, double step,
                        std::function<double()> getterIn, std::function<void (double)> setterIn)
            : juce::PropertyComponent (name),
              getter (std::move (getterIn)), setter (std::move (setterIn))
        {
            slider.setSliderStyle (juce::Slider::LinearBar);
            slider.setRange (min, max, step);
            slider.onValueChange = [this] { setter (slider.getValue()); };
            addAndMakeVisible (slider);
            refresh();
        }

        void refresh() override { slider.setValue (getter(), juce::dontSendNotification); }

    private:
        juce::Slider slider;
        std::function<double()> getter;
        std::function<void (double)> setter;
    };

    class InfoProperty : public juce::PropertyComponent
    {
    public:
        InfoProperty (const juce::String& name, const juce::String& text, int height)
            : juce::PropertyComponent (name, height)
        {
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::topLeft);
            label.setColour (juce::Label::textColourId, DaatColours::textSecondary);
            label.setFont (juce::Font (juce::FontOptions (12.0f)));
            label.setMinimumHorizontalScale (1.0f);
            addAndMakeVisible (label);
        }

        void refresh() override {}

    private:
        juce::Label label;
    };

    class ActionProperty : public juce::ButtonPropertyComponent
    {
    public:
        ActionProperty (const juce::String& name, const juce::String& text, std::function<void()> actionIn)
            : juce::ButtonPropertyComponent (name, false), buttonText (text), action (std::move (actionIn))
        {
            refresh();
        }

        void buttonClicked() override             { if (action != nullptr) action(); }
        juce::String getButtonText() const override { return buttonText; }

    private:
        juce::String buttonText;
        std::function<void()> action;
    };

    //==========================================================================
    juce::String humanise (const juce::String& key)
    {
        juce::String out;
        for (int i = 0; i < key.length(); ++i)
        {
            const auto c = key[i];
            if (i > 0 && juce::CharacterFunctions::isUpperCase (c))
                out << ' ';
            out << juce::String::charToString (c);
        }
        return out.substring (0, 1).toUpperCase() + out.substring (1);
    }

    int decimalsForKey (const juce::String& key)
    {
        if (key.endsWith ("Hz") || key.endsWith ("Ms")) return 0;
        if (key.endsWith ("Db"))                         return 1;
        return 3;
    }

    juce::String pct (float v)
    {
        return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 100.0f)) + "%";
    }

    const FeatureGroup filterGroups[] = { FeatureGroup::spectral, FeatureGroup::temporal,
                                          FeatureGroup::dynamics, FeatureGroup::stereo,
                                          FeatureGroup::noise,    FeatureGroup::repetition };

    //==========================================================================
    /** One row of the feature list: enable, name, group, weight, current score. */
    class FeatureRow : public juce::Component
    {
    public:
        explicit FeatureRow (DetectionSettingsPanel& ownerIn) : owner (ownerIn)
        {
            enabledToggle.onClick = [this]
            {
                if (auto* fs = setting())
                {
                    fs->enabled = enabledToggle.getToggleState();
                    owner.edited();
                }
            };
            addAndMakeVisible (enabledToggle);

            for (auto* l : { &nameLabel, &groupLabel, &scoreLabel })
            {
                l->setInterceptsMouseClicks (false, false); // clicks select the row
                l->setFont (juce::Font (juce::FontOptions (12.0f)));
                addAndMakeVisible (*l);
            }
            groupLabel.setColour (juce::Label::textColourId, DaatColours::textSecondary);
            scoreLabel.setColour (juce::Label::textColourId, DaatColours::textSecondary);

            weightSlider.setSliderStyle (juce::Slider::LinearBar);
            weightSlider.setRange (0.0, 0.5, 0.005);
            weightSlider.setTooltip ("Feature weight within its group");
            weightSlider.onValueChange = [this]
            {
                if (auto* fs = setting())
                {
                    fs->weight = weightSlider.getValue();
                    owner.edited();
                }
            };
            addAndMakeVisible (weightSlider);
        }

        void setFeature (const juce::String& newId)
        {
            id = newId;
            const auto* d = findFeatureDescriptor (id);
            nameLabel.setText (d != nullptr ? d->displayName : id, juce::dontSendNotification);
            groupLabel.setText (d != nullptr ? toString (d->group) : juce::String(), juce::dontSendNotification);

            if (auto* fs = setting())
            {
                enabledToggle.setToggleState (fs->enabled, juce::dontSendNotification);
                weightSlider.setValue (fs->weight, juce::dontSendNotification);
                weightSlider.setEnabled (fs->enabled);
                nameLabel.setColour (juce::Label::textColourId,
                                     fs->enabled ? DaatColours::textPrimary : DaatColours::textDisabled);
            }

            const auto* avg = owner.currentAverageFor (id);
            scoreLabel.setText (avg != nullptr ? pct (avg->suspicionScore) : juce::String ("-"),
                                juce::dontSendNotification);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (owner.isSelected (id))
                g.fillAll (DaatColours::accent.withAlpha (0.22f));
            g.setColour (DaatColours::panelOutline);
            g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 2);
            enabledToggle.setBounds (r.removeFromLeft (26));
            scoreLabel.setBounds (r.removeFromRight (54));
            weightSlider.setBounds (r.removeFromRight (110).reduced (0, 2));
            groupLabel.setBounds (r.removeFromRight (78));
            nameLabel.setBounds (r);
        }

        void mouseDown (const juce::MouseEvent&) override { owner.selectFeature (id); }

    private:
        FeatureSetting* setting()
        {
            auto& features = owner.working().features;
            const auto it = features.find (id);
            return it != features.end() ? &it->second : nullptr;
        }

        DetectionSettingsPanel& owner;
        juce::String id;
        juce::ToggleButton enabledToggle;
        juce::Label nameLabel, groupLabel, scoreLabel;
        juce::Slider weightSlider;
    };
}

//==============================================================================
juce::File DetectionSettingsPanel::getProfilesDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DAAT").getChildFile ("AI Audio Inspector").getChildFile ("Profiles");
}

DetectionSettingsPanel::DetectionSettingsPanel (DaatInspectorAudioProcessor& p)
    : processorRef (p)
{
    setOpaque (true);

    titleLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    presetBox.setTextWhenNothingSelected ("(unsaved profile)");
    presetBox.setTooltip ("Saved profiles in " + getProfilesDirectory().getFullPathName());
    presetBox.onChange = [this]
    {
        if (updatingPresetBox)
            return;
        const int id = presetBox.getSelectedId();
        if (id == 1)
        {
            setWorking (DetectionProfile::getFactoryDefault(), juce::File(), true);
            status ("Factory profile loaded into the editor. Apply to use it.");
        }
        else if (id >= 2 && id - 2 < presetFiles.size())
        {
            loadFromFile (presetFiles[id - 2]);
        }
    };
    addAndMakeVisible (presetBox);

    nameLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (nameLabel);
    nameEditor.onTextChange = [this]
    {
        workingProfile.profileName = nameEditor.getText().trim();
        edited();
    };
    addAndMakeVisible (nameEditor);

    loadButton.onClick      = [this] { loadWithChooser(); };
    saveButton.onClick      = [this] { save(); };
    saveAsButton.onClick    = [this] { saveAs(); };
    duplicateButton.onClick = [this] { duplicate(); };
    resetButton.onClick     = [this] { resetToFactory(); };
    applyButton.onClick     = [this] { apply(); };
    closeButton.onClick     = [this] { if (onClose != nullptr) onClose(); };

    loadButton.setTooltip ("Load a profile from a JSON file into the editor");
    saveButton.setTooltip ("Save the editor's profile to its file");
    resetButton.setTooltip ("Replace the editor's profile with the factory default");
    applyButton.setTooltip ("Validate and make this the active profile");

    for (auto* b : { &loadButton, &saveButton, &saveAsButton, &duplicateButton, &resetButton,
                     &applyButton, &closeButton })
        addAndMakeVisible (*b);

    advancedToggle.setTooltip ("Show thresholds, frequency ranges, window assignment, and the "
                               "analysis, decision, and group settings");
    advancedToggle.onClick = [this]
    {
        processorRef.setUiStateProperty ("advancedMode", advancedToggle.getToggleState());
        rebuildDetail();
    };
    addAndMakeVisible (advancedToggle);

    searchBox.setTextToShowWhenEmpty ("Search features", DaatColours::textDisabled);
    searchBox.onTextChange = [this] { rebuildVisibleList(); };
    addAndMakeVisible (searchBox);

    groupFilter.addItem ("All groups", 1);
    for (int i = 0; i < (int) std::size (filterGroups); ++i)
        groupFilter.addItem (humanise (toString (filterGroups[i])), i + 2);
    groupFilter.setSelectedId (1, juce::dontSendNotification);
    groupFilter.onChange = [this] { rebuildVisibleList(); };
    addAndMakeVisible (groupFilter);

    dirtyLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (dirtyLabel);

    featureList.setRowHeight (26);
    featureList.setColour (juce::ListBox::backgroundColourId, DaatColours::background);
    featureList.setColour (juce::ListBox::outlineColourId, DaatColours::panelOutline);
    featureList.setOutlineThickness (1);
    addAndMakeVisible (featureList);

    addAndMakeVisible (detailPanel);

    validationLabel.setJustificationType (juce::Justification::topLeft);
    validationLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    validationLabel.setColour (juce::Label::backgroundColourId, DaatColours::background);
    validationLabel.setColour (juce::Label::outlineColourId, DaatColours::panelOutline);
    addAndMakeVisible (validationLabel);
}

DetectionSettingsPanel::~DetectionSettingsPanel()
{
    featureList.setModel (nullptr);
}

//==============================================================================
void DetectionSettingsPanel::panelShown()
{
    lastResult = processorRef.getEngine().getResult();

    if (! dirty)
    {
        auto p = processorRef.getEngine().getProfile();

        // The plugin parameters are the runtime truth for these two values.
        if (auto* v = processorRef.apvts.getRawParameterValue (ParamIDs::decisionThreshold))
            p.decision.likelyThreshold = (double) v->load();
        if (auto* v = processorRef.apvts.getRawParameterValue (ParamIDs::minimumConfidence))
            p.decision.minimumConfidence = (double) v->load();

        const juce::File path (processorRef.getUiState().getProperty ("activeProfilePath", {}).toString());
        setWorking (p, path.existsAsFile() ? path : juce::File(), false);
    }

    advancedToggle.setToggleState ((bool) processorRef.getUiState().getProperty ("advancedMode", false),
                                   juce::dontSendNotification);
    refreshPresetList();
    rebuildVisibleList();
    rebuildDetail();
    revalidate();
}

void DetectionSettingsPanel::setWorking (const DetectionProfile& p, const juce::File& file, bool markDirty)
{
    workingProfile = p;
    workingFile = file;
    dirty = markDirty;
    nameEditor.setText (workingProfile.profileName, false);
    refreshPresetList();
    rebuildVisibleList();
    rebuildDetail();
    revalidate();
}

void DetectionSettingsPanel::edited()
{
    dirty = true;
    revalidate();
    featureList.updateContent();
    featureList.repaint();
    detailPanel.refreshAll();
}

void DetectionSettingsPanel::status (const juce::String& message)
{
    if (onStatus != nullptr)
        onStatus (message);
}

const FeatureResult* DetectionSettingsPanel::currentAverageFor (const juce::String& featureId) const
{
    if (lastResult == nullptr)
        return nullptr;
    for (const auto& f : lastResult->featureAverages)
        if (f.featureId == featureId)
            return &f;
    return nullptr;
}

//==============================================================================
int DetectionSettingsPanel::getNumRows()
{
    return visibleIds.size();
}

juce::Component* DetectionSettingsPanel::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    if (row < 0 || row >= visibleIds.size())
    {
        delete existing;
        return nullptr;
    }

    auto* r = dynamic_cast<FeatureRow*> (existing);
    if (r == nullptr)
    {
        delete existing;
        r = new FeatureRow (*this);
    }
    r->setFeature (visibleIds[row]);
    return r;
}

void DetectionSettingsPanel::selectedRowsChanged (int lastRowSelected)
{
    if (lastRowSelected < 0 || lastRowSelected >= visibleIds.size())
        return;

    const auto id = visibleIds[lastRowSelected];
    if (id != selectedFeatureId)
    {
        selectedFeatureId = id;
        rebuildDetail();
    }
    featureList.repaint();
}

void DetectionSettingsPanel::selectFeature (const juce::String& featureId)
{
    const int row = visibleIds.indexOf (featureId);
    if (row >= 0)
        featureList.selectRow (row);
}

void DetectionSettingsPanel::rebuildVisibleList()
{
    const auto search = searchBox.getText().trim();
    const int groupSel = groupFilter.getSelectedId();

    visibleIds.clear();
    for (const auto& d : getFeatureRegistry())
    {
        if (groupSel >= 2 && d.group != filterGroups[groupSel - 2])
            continue;
        if (search.isNotEmpty()
            && ! d.displayName.containsIgnoreCase (search)
            && ! d.id.containsIgnoreCase (search)
            && ! d.description.containsIgnoreCase (search))
            continue;
        visibleIds.add (d.id);
    }

    featureList.updateContent();

    if (visibleIds.contains (selectedFeatureId))
        featureList.selectRow (visibleIds.indexOf (selectedFeatureId));
    else if (selectedFeatureId.isEmpty() && ! visibleIds.isEmpty())
        featureList.selectRow (0);
    else
        featureList.repaint();
}

//==============================================================================
void DetectionSettingsPanel::rebuildDetail()
{
    const auto openness = detailPanel.getOpennessState();
    const int scrollY = detailPanel.getViewport().getViewPositionY();
    detailPanel.clear();

    const bool advanced = advancedToggle.getToggleState();
    const auto id = selectedFeatureId;
    const auto* desc = findFeatureDescriptor (id);

    if (desc != nullptr && workingProfile.features.count (id) > 0)
    {
        juce::Array<juce::PropertyComponent*> props;
        const auto feature = [this, id]() -> FeatureSetting& { return workingProfile.features[id]; };

        props.add (new InfoProperty ("Measures", desc->description, 62));
        props.add (new InfoProperty ("Group", humanise (toString (desc->group))
                                        + "   |   normalization: " + toString (desc->normalization)
                                        + " (fixed by the feature's implementation)", 40));

        props.add (new BoolProperty ("Enabled",
                                     [feature] { return feature().enabled; },
                                     [this, feature] (bool v) { feature().enabled = v; edited(); }));
        props.add (new SliderProperty ("Weight", 0.0, 0.5, 0.005,
                                       [feature] { return feature().weight; },
                                       [this, feature] (double v) { feature().weight = v; edited(); }));

        juce::String current ("Not analysed yet.");
        if (const auto* avg = currentAverageFor (id))
            current = "raw " + juce::String (avg->rawValue, 4) + "   |   indicator " + pct (avg->suspicionScore)
                    + "   |   confidence " + pct (avg->confidence);
        else if (lastResult != nullptr)
            current = "No valid measurement in the last analysis (disabled, masked, or not applicable).";
        props.add (new InfoProperty ("Current value", current, 26));

        if (advanced)
        {
            const auto scaleProp = [this, feature] (const char* name, juce::uint32 bit)
            {
                return new BoolProperty (name,
                                         [feature, bit] { return (feature().scales & bit) != 0; },
                                         [this, feature, bit] (bool v)
                                         {
                                             auto& s = feature().scales;
                                             s = v ? (s | bit) : (s & ~bit);
                                             edited();
                                         });
            };
            props.add (scaleProp ("Short windows",  ScaleMask::shortWindows));
            props.add (scaleProp ("Medium windows", ScaleMask::mediumWindows));
            props.add (scaleProp ("Long windows",   ScaleMask::longWindows));

            // Every numeric setting the feature carries (thresholds, ranges, lags).
            if (auto* obj = feature().settings.getDynamicObject())
            {
                for (const auto& prop : obj->getProperties())
                {
                    const auto key = prop.name.toString();
                    if (prop.value.isString())
                    {
                        props.add (new InfoProperty (humanise (key), prop.value.toString(), 26));
                        continue;
                    }

                    props.add (new NumberProperty (
                        humanise (key),
                        [feature, key]
                        {
                            return daat::json::getDouble (feature().settings, juce::Identifier (key), 0.0);
                        },
                        [this, feature, key] (double v)
                        {
                            if (auto* o = feature().settings.getDynamicObject())
                                o->setProperty (juce::Identifier (key), v);
                            edited();
                        },
                        decimalsForKey (key)));
                }
            }
        }

        props.add (new ActionProperty ("Defaults", "Restore this feature's defaults",
                                       [this, id] { restoreFeatureDefaults (id); }));

        detailPanel.addSection (desc->displayName, props, true);
    }
    else
    {
        juce::Array<juce::PropertyComponent*> props;
        props.add (new InfoProperty ("Feature", "Select a feature on the left to edit it.", 26));
        detailPanel.addSection ("Feature", props, true);
    }

    if (advanced)
    {
        auto& a = workingProfile.analysis;
        juce::Array<juce::PropertyComponent*> analysis;
        const auto num = [this] (const char* name, double& ref, int decimals)
        {
            return new NumberProperty (name, [&ref] { return ref; },
                                       [this, &ref] (double v) { ref = v; edited(); }, decimals);
        };
        const auto integer = [this] (const char* name, int& ref)
        {
            return new NumberProperty (name, [&ref] { return (double) ref; },
                                       [this, &ref] (double v) { ref = juce::roundToInt (v); edited(); }, 0);
        };

        analysis.add (num ("Sample rate (Hz)", a.sampleRate, 0));
        analysis.add (integer ("FFT size", a.fftSize));
        analysis.add (integer ("Hop size", a.hopSize));
        analysis.add (num ("Short window (s)", a.shortWindowSeconds, 2));
        analysis.add (num ("Medium window (s)", a.mediumWindowSeconds, 2));
        analysis.add (num ("Long window (s)", a.longWindowSeconds, 2));
        analysis.add (num ("Short overlap", a.shortOverlap, 2));
        analysis.add (num ("Medium overlap", a.mediumOverlap, 2));
        analysis.add (num ("Long overlap", a.longOverlap, 2));
        analysis.add (num ("Minimum analysed (s)", a.minimumAnalyzedSeconds, 1));
        detailPanel.addSection ("Analysis (changes trigger reanalysis)", analysis, false);

        auto& d = workingProfile.decision;
        juce::Array<juce::PropertyComponent*> decision;
        decision.add (new InfoProperty ("Note", "Likely threshold and minimum confidence are written to the "
                                                "plugin's Decision Threshold and Minimum Confidence "
                                                "parameters on Apply.", 40));
        decision.add (num ("Likely threshold", d.likelyThreshold, 3));
        decision.add (num ("Unlikely threshold", d.unlikelyThreshold, 3));
        decision.add (num ("Minimum confidence", d.minimumConfidence, 3));
        decision.add (num ("Min. active feature weight", d.minimumActiveFeatureWeight, 3));
        decision.add (new BoolProperty ("Require multiple groups",
                                        [&d] { return d.requireMultipleFeatureGroups; },
                                        [this, &d] (bool v) { d.requireMultipleFeatureGroups = v; edited(); }));
        decision.add (integer ("Min. agreeing groups", d.minimumAgreeingGroups));
        detailPanel.addSection ("Decision", decision, false);

        juce::Array<juce::PropertyComponent*> groupsProps;
        groupsProps.add (new InfoProperty ("Note", "A group contributes only if it is enabled here AND its "
                                                   "switch is on in the plugin parameters.", 40));
        for (const auto g : filterGroups)
        {
            const auto name = toString (g);
            const auto group = [this, name]() -> GroupSetting& { return workingProfile.groups[name]; };
            groupsProps.add (new BoolProperty (humanise (name) + " enabled",
                                               [group] { return group().enabled; },
                                               [this, group] (bool v) { group().enabled = v; edited(); }));
            groupsProps.add (new NumberProperty (humanise (name) + " weight",
                                                 [group] { return group().weight; },
                                                 [this, group] (double v) { group().weight = v; edited(); }, 2));
        }
        detailPanel.addSection ("Groups", groupsProps, false);
    }

    if (openness != nullptr)
        detailPanel.restoreOpennessState (*openness);
    detailPanel.getViewport().setViewPosition (0, scrollY);
}

void DetectionSettingsPanel::restoreFeatureDefaults (const juce::String& featureId)
{
    const auto factory = DetectionProfile::getFactoryDefault();
    const auto it = factory.features.find (featureId);
    if (it == factory.features.end())
        return;

    workingProfile.features[featureId] = it->second;
    edited();
    if (const auto* d = findFeatureDescriptor (featureId))
        status (d->displayName + ": defaults restored in the editor. Apply to use them.");
}

//==============================================================================
void DetectionSettingsPanel::revalidate()
{
    validation = workingProfile.validate();

    // Values the plugin parameters must be able to represent.
    const auto& d = workingProfile.decision;
    if (d.likelyThreshold < 0.5 || d.likelyThreshold > 0.95)
        validation.addError ("Likely threshold must be within 0.50-0.95 (the Decision Threshold parameter range).");
    if (workingProfile.profileName.isEmpty())
        validation.addError ("Profile name must not be empty.");

    juce::String text;
    if (validation.ok && validation.warnings.isEmpty())
    {
        text = "Profile valid.";
        validationLabel.setColour (juce::Label::textColourId, DaatColours::textSecondary);
    }
    else
    {
        for (const auto& e : validation.errors)   text << "Error: " << e << "\n";
        for (const auto& w : validation.warnings) text << "Warning: " << w << "\n";
        validationLabel.setColour (juce::Label::textColourId,
                                   validation.ok ? DaatColours::textSecondary : DaatColours::accentWarm);
    }
    validationLabel.setText (text.trimEnd(), juce::dontSendNotification);

    applyButton.setEnabled (validation.ok);
    saveButton.setEnabled (validation.ok);
    saveAsButton.setEnabled (validation.ok);
    refreshHeader();
}

void DetectionSettingsPanel::refreshHeader()
{
    juce::String text = dirty ? "Unapplied changes" : "Matches the active profile";
    text << "   |   " << (workingFile.existsAsFile() ? workingFile.getFileName()
                                                      : juce::String ("not saved to a file"));
    dirtyLabel.setText (text, juce::dontSendNotification);
    dirtyLabel.setColour (juce::Label::textColourId, dirty ? DaatColours::accentWarm : DaatColours::textSecondary);
}

void DetectionSettingsPanel::refreshPresetList()
{
    const juce::ScopedValueSetter<bool> guard (updatingPresetBox, true);

    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("Factory: Balanced Research", 1);

    const auto dir = getProfilesDirectory();
    presetFiles = dir.isDirectory() ? dir.findChildFiles (juce::File::findFiles, false, "*.json")
                                    : juce::Array<juce::File>();
    presetFiles.sort();

    for (int i = 0; i < presetFiles.size(); ++i)
        presetBox.addItem (presetFiles[i].getFileNameWithoutExtension(), i + 2);

    const int idx = presetFiles.indexOf (workingFile);
    presetBox.setSelectedId (idx >= 0 ? idx + 2 : 0, juce::dontSendNotification);
}

//==============================================================================
void DetectionSettingsPanel::loadWithChooser()
{
    auto startDir = juce::File (processorRef.getUiState().getProperty ("lastProfileDirectory", {}).toString());
    if (! startDir.isDirectory())
        startDir = getProfilesDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Load detection profile", startDir, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  loadFromFile (file);
                          });
}

void DetectionSettingsPanel::loadFromFile (const juce::File& file)
{
    ProfileValidation v;
    const auto loaded = DetectionProfile::loadFromFile (file, v);

    processorRef.setUiStateProperty ("lastProfileDirectory", file.getParentDirectory().getFullPathName());
    processorRef.setUiStateProperty ("lastImportedProfile", file.getFullPathName());

    if (! v.ok)
    {
        // Reject malformed profiles outright rather than half-loading them.
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("Profile rejected")
                                          .withMessage (file.getFileName() + " was not loaded:\n\n"
                                                        + v.errors.joinIntoString ("\n"))
                                          .withButton ("OK"),
                                      nullptr);
        status ("Profile rejected: " + v.errors[0]);
        refreshPresetList();
        return;
    }

    setWorking (loaded, file, true);

    if (v.migrated || ! v.warnings.isEmpty())
        status ("Loaded " + file.getFileName() + " with warnings: " + v.warnings.joinIntoString ("; "));
    else
        status ("Loaded " + file.getFileName() + " into the editor. Apply to use it.");
}

void DetectionSettingsPanel::save()
{
    revalidate();
    if (! validation.ok)
    {
        status ("Fix the validation errors before saving.");
        return;
    }

    if (workingFile == juce::File())
    {
        saveAs();
        return;
    }

    juce::String error;
    if (workingProfile.saveToFile (workingFile, error))
    {
        status ("Saved " + workingFile.getFileName() + ".");
        refreshPresetList();
        refreshHeader();
    }
    else
    {
        status ("Save failed: " + error);
    }
}

void DetectionSettingsPanel::saveAs()
{
    revalidate();
    if (! validation.ok)
    {
        status ("Fix the validation errors before saving.");
        return;
    }

    const auto dir = getProfilesDirectory();
    dir.createDirectory();
    const auto suggested = dir.getChildFile (juce::File::createLegalFileName (workingProfile.profileName) + ".json");

    chooser = std::make_unique<juce::FileChooser> ("Save detection profile", suggested, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (! file.hasFileExtension ("json"))
                                  file = file.withFileExtension ("json");

                              juce::String error;
                              if (workingProfile.saveToFile (file, error))
                              {
                                  workingFile = file;
                                  processorRef.setUiStateProperty ("lastProfileDirectory",
                                                                   file.getParentDirectory().getFullPathName());
                                  status ("Saved " + file.getFileName() + ".");
                                  refreshPresetList();
                                  refreshHeader();
                              }
                              else
                              {
                                  status ("Save failed: " + error);
                              }
                          });
}

void DetectionSettingsPanel::duplicate()
{
    workingProfile.profileName = workingProfile.profileName + " (copy)";
    workingFile = juce::File();
    nameEditor.setText (workingProfile.profileName, false);
    refreshPresetList();
    edited();
    status ("Duplicated in the editor - use Save As to keep it as a new file.");
}

void DetectionSettingsPanel::resetToFactory()
{
    setWorking (DetectionProfile::getFactoryDefault(), juce::File(), true);
    status ("Factory profile loaded into the editor. Apply to use it.");
}

void DetectionSettingsPanel::apply()
{
    revalidate();
    if (! validation.ok)
    {
        status ("Fix the validation errors before applying.");
        return;
    }

    // Write the two parameter-backed values through the parameters (with a
    // gesture, so hosts record it), then push everything to the engine.
    const auto setParam = [this] (const char* id, double value)
    {
        if (auto* p = processorRef.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
            p->endChangeGesture();
        }
    };
    setParam (ParamIDs::decisionThreshold, workingProfile.decision.likelyThreshold);
    setParam (ParamIDs::minimumConfidence, workingProfile.decision.minimumConfidence);
    processorRef.syncRuntimeControls();

    const bool reanalysing = processorRef.getEngine().applyProfile (workingProfile);

    processorRef.setUiStateProperty ("activeProfileName", workingProfile.profileName);
    processorRef.setUiStateProperty ("activeProfilePath",
                                     workingFile.existsAsFile() ? workingFile.getFullPathName() : juce::String());
    if (workingFile.existsAsFile())
        processorRef.rememberLastUsedProfile (workingFile);

    dirty = false;
    refreshHeader();

    const bool haveResult = processorRef.getEngine().getResult() != nullptr;
    status ("Applied \"" + workingProfile.profileName + "\""
            + (! haveResult ? juce::String (" - it will be used for the next analysis.")
               : reanalysing ? juce::String (" - re-analysing (analysis settings changed).")
                             : juce::String (" - rescored from the stored measurements.")));
}

//==============================================================================
void DetectionSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (DaatColours::background);
    g.setColour (DaatColours::panelOutline);
    g.drawRect (getLocalBounds());

    if (! listHeaderArea.isEmpty())
    {
        auto r = listHeaderArea.reduced (4, 0);
        g.setColour (DaatColours::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("ON", r.removeFromLeft (26), juce::Justification::centredLeft);
        g.drawText ("CURRENT", r.removeFromRight (54), juce::Justification::centredLeft);
        g.drawText ("WEIGHT", r.removeFromRight (110), juce::Justification::centredLeft);
        g.drawText ("GROUP", r.removeFromRight (78), juce::Justification::centredLeft);
        g.drawText ("FEATURE", r, juce::Justification::centredLeft);
    }
}

void DetectionSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto row1 = area.removeFromTop (28);
    titleLabel.setBounds (row1.removeFromLeft (170));
    closeButton.setBounds (row1.removeFromRight (64));
    row1.removeFromRight (6);
    applyButton.setBounds (row1.removeFromRight (72));
    row1.removeFromRight (10);
    advancedToggle.setBounds (row1.removeFromRight (96));
    row1.removeFromRight (10);
    presetBox.setBounds (row1.removeFromLeft (230));
    nameLabel.setBounds (row1.removeFromLeft (50));
    row1.removeFromLeft (4);
    nameEditor.setBounds (row1.removeFromLeft (juce::jmin (240, row1.getWidth())));

    area.removeFromTop (6);
    auto row2 = area.removeFromTop (26);
    const auto place = [&row2] (juce::Component& c, int w)
    {
        c.setBounds (row2.removeFromLeft (w));
        row2.removeFromLeft (6);
    };
    place (loadButton, 70);
    place (saveButton, 56);
    place (saveAsButton, 80);
    place (duplicateButton, 80);
    place (resetButton, 60);
    row2.removeFromLeft (10);
    place (searchBox, 190);
    place (groupFilter, 140);
    dirtyLabel.setBounds (row2);

    area.removeFromTop (8);
    validationLabel.setBounds (area.removeFromBottom (62));
    area.removeFromBottom (8);

    auto left = area.removeFromLeft ((int) (area.getWidth() * 0.47f));
    listHeaderArea = left.removeFromTop (16);
    featureList.setBounds (left);
    area.removeFromLeft (10);
    detailPanel.setBounds (area);
}

#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class DriveKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setKnobImage(juce::Image image) { knobImage = std::move(image); }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
private:
    juce::Image knobImage;
};

class SwitchLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setImages(juce::Image offImage, juce::Image onImage);
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
private:
    juce::Image switchOffImage;
    juce::Image switchOnImage;
};

class EqSliderLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setImage(juce::Image image);
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;
private:
    juce::Image sliderImage;
};

class DriveKnobInputOverlay final : public juce::Component
{
public:
    explicit DriveKnobInputOverlay(juce::Slider& target) : slider(target)
    {
        setInterceptsMouseClicks(true, false);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.position.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        const auto range = slider.getMaximum() - slider.getMinimum();
        const auto deltaY = (double)(dragStartY - event.position.y);
        slider.setValue(dragStartValue + deltaY * range / 150.0, juce::sendNotificationSync);
    }

private:
    juce::Slider& slider;
    float dragStartY { 0.0f };
    double dragStartValue { 0.0 };
};

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void configureSlider(juce::Slider& slider, const juce::String& name,
                         double min, double max, double initial);
    void chooseNam();
    void chooseIr();
    void refreshStatus();
    void savePreset();
    void overwritePreset();
    void savePresetToFile(const juce::File& file);
    void refreshPresetList();
    void loadSelectedPreset();
    void toggleCurrentPresetFavorite();
    bool isCurrentPresetFavorite() const;
    juce::File getPresetDirectory() const;
    void markPresetDirty();
    void saveSession();
    void loadSession();
    void loadPreset();
    void applyPreset(const juce::var& data);
    void timerCallback() override;
    void drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& label, float levelDb, bool clipping);

    AudioEngine engine;
    DriveKnobLookAndFeel driveKnobLookAndFeel;
    SwitchLookAndFeel switchLookAndFeel;
    EqSliderLookAndFeel eqSliderLookAndFeel;
    DriveKnobInputOverlay driveKnobInputOverlay { inputGain };
    juce::Image faceplateBackground;
    juce::Image pedalboardBackground;
    juce::Image effectsControlStripBackground;
    float displayedInputDb { -60.0f }, displayedOutputDb { -60.0f };
    float displayedTunerHz { 0.0f };
    bool inputClipping { false }, outputClipping { false };
    int inputClipHoldFrames { 0 }, outputClipHoldFrames { 0 };
    bool restoringSession { false };
    juce::Label title, status;
    juce::TextButton loadNamButton { "LOAD NAM PROFILE" }, loadIrButton { "LOAD CAB IR" };
    juce::TextButton audioButton { "SETTINGS [+]" };
    juce::TextButton tunerToggleButton { "TUNER ON" };
    juce::TextButton transposeDownButton { "−" }, transposeUpButton { "+" };
    juce::Label transposeValueLabel;
    juce::TextButton ampPageButton { "AMP" }, effectsPageButton { "EFFECTS" };
    juce::ToggleButton noiseGateButton { "GATE ON" };
    juce::Slider gateThreshold;
    juce::Label gateThresholdLabel;
    juce::ToggleButton ts9Button { "TS9 ON" };
    juce::Slider ts9DriveSlider, ts9ToneSlider, ts9LevelSlider;
    juce::Label ts9DriveLabel, ts9ToneLabel, ts9LevelLabel;
    juce::ToggleButton compressorButton { "COMP ON" };
    juce::Slider compressorSustainSlider;
    juce::Label compressorSustainLabel;
    juce::ToggleButton delayButton { "DELAY ON" };
    juce::Slider delayTimeSlider, delayFeedbackSlider, delayMixSlider;
    juce::Label delayTimeLabel, delayFeedbackLabel, delayMixLabel;
    juce::TextButton delayTapButton { "TAP TEMPO" };
    double lastDelayTapMs { 0.0 };
    juce::ToggleButton chorusButton { "CHORUS ON" };
    juce::Slider chorusRateSlider, chorusDepthSlider, chorusMixSlider;
    juce::Label chorusRateLabel, chorusDepthLabel, chorusMixLabel;
    juce::ToggleButton reverbButton { "REVERB ON" };
    juce::Slider reverbDecaySlider, reverbPreDelaySlider, reverbSizeSlider, reverbMixSlider;
    juce::Label reverbDecayLabel, reverbPreDelayLabel, reverbSizeLabel, reverbMixLabel;
    juce::ComboBox reverbTypeCombo;
    int delayTapCount { 0 };
    bool effectsPageOpen { false };
    juce::TextButton presetMenuButton { "PRESETS [+]" };
    juce::TextButton deletePresetButton { "DELETE PRESET" };
    bool presetPanelOpen { false };
    juce::TextButton savePresetButton { "SAVE AS..." }, loadPresetButton { "LOAD FILE..." }, overwritePresetButton { "OVERWRITE" };
    juce::ComboBox presetCombo, categoryCombo, presetFilterCombo;
    juce::TextButton favoriteButton { "☆ FAVORITE" };
    juce::Array<juce::File> displayedPresetFiles;
    juce::Label presetLabel;
    juce::File currentPresetFile;
    bool updatingPresetList { false };
    bool presetDirty { false };
    juce::ToggleButton leadModeButton { "LEAD" }, brightButton { "BRIGHT" }, deepButton { "DEEP" }, trebleShiftButton { "SHIFT" }, bassShiftButton { "SHIFT" };
    juce::Label preampHeader, eqHeader, modelHeader;
    juce::Slider inputGain, leadMaster, outputGain;
    std::array<juce::Slider, 5> eq;
    std::array<juce::Slider, 4> tone;
    std::array<juce::Label, 4> toneLabels;
    juce::Label inputLabel, leadMasterLabel, outputLabel;
    std::array<juce::Label, 5> eqLabels;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

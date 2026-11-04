#include "MainComponent.h"
#include "FaceplateImageData.h"

void DriveKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider&)
{
    if (!knobImage.isValid())
    {
        g.setColour(juce::Colour(0xffc8a56a));
        g.fillEllipse((float)x + 5.0f, (float)y + 5.0f, (float)width - 10.0f, (float)height - 10.0f);
        return;
    }

    // Slight counter-clockwise artwork correction so the pointer visually centres on the faceplate scale.
    const float artworkAngleOffset = -0.075f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle) + artworkAngleOffset;
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::rotation(angle, (float)x + width * 0.5f, (float)y + height * 0.5f));
    // Nudge the artwork upward a couple of display pixels without moving the hit area/readout.
    const float artworkYOffset = -2.0f;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageWithin(knobImage, x - 2, y + (int) artworkYOffset - 2, width + 4, height + 4, juce::RectanglePlacement::centred);
}


void SwitchLookAndFeel::setImages(juce::Image offImage, juce::Image onImage)
{
    switchOffImage = std::move(offImage);
    switchOnImage = std::move(onImage);
}

void SwitchLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    const auto& image = button.getToggleState() ? switchOnImage : switchOffImage;
    if (image.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImageWithin(image, 0, 0, button.getWidth(), button.getHeight(),
                          juce::RectanglePlacement::centred);
    }
}

void EqSliderLookAndFeel::setImage(juce::Image image)
{
    sliderImage = std::move(image);
}

void EqSliderLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, style, slider);
    if (!sliderImage.isValid())
        return;

    // The complete supplied slider artwork (housing AND gold handle) moves as one image.
    // Keep its aspect ratio and centre it horizontally in the slider's drawing area.
    const float imageAspect = (float)sliderImage.getWidth() / (float)sliderImage.getHeight();
    const float imageWidth = juce::jmin((float)width, (float)height * imageAspect * 0.40f);
    const float imageHeight = imageWidth / imageAspect;
    const float travel = juce::jmax(0.0f, (float)height - imageHeight + 12.0f);
    const float extraTravel = 6.0f;
    const float imageY = juce::jlimit((float)y - extraTravel,
        (float)y + travel + extraTravel,
        sliderPos - imageHeight * 0.5f);
    const float imageX = (float)x + ((float)width - imageWidth) * 0.5f;

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(sliderImage, juce::Rectangle<float>(imageX, imageY, imageWidth, imageHeight));
}

MainComponent::MainComponent()
{
    setSize(1080, 600);

    // Load the approved faceplate from embedded data. This avoids runtime path issues
    // when Visual Studio launches the executable from a build/configuration folder.
    juce::MemoryBlock faceplateBytes;
    juce::MemoryOutputStream faceplateStream(faceplateBytes, false);
    if (juce::Base64::convertFromBase64(faceplateStream, juce::String(IICPlusFaceplateData::base64)))
        faceplateBackground = juce::ImageFileFormat::loadFrom(faceplateBytes.getData(), faceplateBytes.getSize());

    // DRIVE knob prototype: image is loaded from assets beside the executable.
    const auto knobFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                              .getParentDirectory().getChildFile("assets").getChildFile("knob.png");
    driveKnobLookAndFeel.setKnobImage(juce::ImageFileFormat::loadFrom(knobFile));

    // BRIGHT switch prototype: use the supplied ON/OFF toggle artwork.
    const auto assetsDirectory = knobFile.getParentDirectory();
    pedalboardBackground = juce::ImageFileFormat::loadFrom(assetsDirectory.getChildFile("pedalboard_effects_background.png"));
    effectsControlStripBackground = juce::ImageFileFormat::loadFrom(assetsDirectory.getChildFile("effects_control_strip_background.png"));
    switchLookAndFeel.setImages(
        juce::ImageFileFormat::loadFrom(assetsDirectory.getChildFile("switch_off.png")),
        juce::ImageFileFormat::loadFrom(assetsDirectory.getChildFile("switch_on.png")));
    brightButton.setLookAndFeel(&switchLookAndFeel);
    bassShiftButton.setLookAndFeel(&switchLookAndFeel);
    trebleShiftButton.setLookAndFeel(&switchLookAndFeel);
    leadModeButton.setLookAndFeel(&switchLookAndFeel);
    deepButton.setLookAndFeel(&switchLookAndFeel);
    inputGain.setLookAndFeel(&driveKnobLookAndFeel);
    // BASS uses the same approved knob artwork and rotary rendering as DRIVE.
    tone[0].setLookAndFeel(&driveKnobLookAndFeel);
    // MID uses the same approved knob artwork and rotary rendering.
    tone[1].setLookAndFeel(&driveKnobLookAndFeel);
    tone[2].setLookAndFeel(&driveKnobLookAndFeel);
    // VOLUME uses the same approved knob artwork and rotary rendering.
    leadMaster.setLookAndFeel(&driveKnobLookAndFeel);
    // PRESENCE uses the same approved knob artwork and rotary rendering.
    tone[3].setLookAndFeel(&driveKnobLookAndFeel);
    // MASTER uses the same approved knob artwork and rotary rendering.
    outputGain.setLookAndFeel(&driveKnobLookAndFeel);

    // EQ slider prototype: the complete supplied slider image moves as one piece.
    eqSliderLookAndFeel.setImage(
        juce::ImageFileFormat::loadFrom(assetsDirectory.getChildFile("slider.png")));
    title.setText("MARK IIC+  //  NAM STANDALONE", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0e7d5));
    addAndMakeVisible(title);

    status.setText("Audio engine starting…", juce::dontSendNotification);
    status.setJustificationType(juce::Justification::centredLeft);
    status.setColour(juce::Label::textColourId, juce::Colour(0xffe5d6b8));
    addAndMakeVisible(status);

    // Compact tuner enable control lives in the AMP-page utility strip.
    addAndMakeVisible(tunerToggleButton);
    tunerToggleButton.setClickingTogglesState(true);
    tunerToggleButton.setToggleState(true, juce::dontSendNotification);
    tunerToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff29251e));
    tunerToggleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff66502f));
    tunerToggleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd4b27a));
    tunerToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffe2a8));
    tunerToggleButton.onClick = [this]
    {
        const bool enabled = tunerToggleButton.getToggleState();
        engine.setTunerEnabled(enabled);
        tunerToggleButton.setButtonText(enabled ? "TUNER ON" : "TUNER OFF");
        displayedTunerHz = 0.0f;
        repaint();
    };


    // Always-in-path transpose controls. The tuner remains fed from the original input.
    auto styleTransposeButton = [](juce::TextButton& button)
    {
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff20272b));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff24465b));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd9e5eb));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff8ed4ff));
        button.setWantsKeyboardFocus(false);
    };
    addAndMakeVisible(transposeDownButton);
    addAndMakeVisible(transposeUpButton);
    addAndMakeVisible(transposeValueLabel);
    styleTransposeButton(transposeDownButton);
    styleTransposeButton(transposeUpButton);
    transposeValueLabel.setJustificationType(juce::Justification::centred);
    transposeValueLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    transposeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e5eb));
    auto updateTransposeDisplay = [this]
    {
        const int value = engine.getTransposeSemitones();
        transposeValueLabel.setText((value > 0 ? "+" : "") + juce::String(value) + " st", juce::dontSendNotification);
        transposeValueLabel.setColour(juce::Label::textColourId,
            value == 0 ? juce::Colour(0xffd9e5eb) : juce::Colour(0xff70c8ff));
    };
    transposeDownButton.onClick = [this, updateTransposeDisplay]
    {
        engine.setTransposeSemitones(juce::jmax(-12, engine.getTransposeSemitones() - 1));
        updateTransposeDisplay(); saveSession();
    };
    transposeUpButton.onClick = [this, updateTransposeDisplay]
    {
        engine.setTransposeSemitones(juce::jmin(12, engine.getTransposeSemitones() + 1));
        updateTransposeDisplay(); saveSession();
    };
    updateTransposeDisplay();

    // Noise Gate controls live inside the first PRE-effects tile.
    addAndMakeVisible(noiseGateButton);
    noiseGateButton.setClickingTogglesState(true);
    noiseGateButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeee5d5));
    noiseGateButton.onClick = [this] { engine.setNoiseGateEnabled(noiseGateButton.getToggleState()); saveSession(); markPresetDirty(); repaint(); };
    auto setupGateSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text,
                                  double minimum, double maximum, double initial)
    {
        // Effects use the same photo-realistic rotary knob artwork as the amp controls.
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
        slider.setLookAndFeel(&driveKnobLookAndFeel);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                   juce::MathConstants<float>::pi * 2.8f, true);
        slider.setRange(minimum, maximum, 0.1);
        slider.setValue(initial, juce::dontSendNotification);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffc8a56a));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff514637));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe5d6b8));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffeee5d5));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff171b1f));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff80633e));
        addAndMakeVisible(slider);
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffd4b27a));
        label.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        addAndMakeVisible(label);
    };
    setupGateSlider(gateThreshold, gateThresholdLabel, "THRESHOLD", -80.0, -20.0, -55.0);
    gateThreshold.onValueChange = [this] {
        engine.setNoiseGateThreshold((float)gateThreshold.getValue());
        saveSession();
        markPresetDirty();
    };

    addAndMakeVisible(ts9Button);
    ts9Button.setClickingTogglesState(true);
    ts9Button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeee5d5));
    ts9Button.onClick = [this] { engine.setTs9Enabled(ts9Button.getToggleState()); saveSession(); markPresetDirty(); repaint(); };
    setupGateSlider(ts9DriveSlider, ts9DriveLabel, "DRIVE", 0.0, 10.0, 2.0);
    setupGateSlider(ts9ToneSlider, ts9ToneLabel, "TONE", 0.0, 10.0, 5.0);
    setupGateSlider(ts9LevelSlider, ts9LevelLabel, "LEVEL", 0.0, 10.0, 5.0);
    ts9DriveSlider.onValueChange = [this] { engine.setTs9Drive((float)ts9DriveSlider.getValue()); saveSession(); markPresetDirty(); };
    ts9ToneSlider.onValueChange = [this] { engine.setTs9Tone((float)ts9ToneSlider.getValue()); saveSession(); markPresetDirty(); };
    ts9LevelSlider.onValueChange = [this] { engine.setTs9Level((float)ts9LevelSlider.getValue()); saveSession(); markPresetDirty(); };

    addAndMakeVisible(compressorButton);
    compressorButton.setClickingTogglesState(true);
    compressorButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeee5d5));
    compressorButton.onClick = [this] { engine.setCompressorEnabled(compressorButton.getToggleState()); saveSession(); markPresetDirty(); repaint(); };
    setupGateSlider(compressorSustainSlider, compressorSustainLabel, "SUSTAIN", 0.0, 10.0, 3.0);
    compressorSustainSlider.onValueChange = [this] { engine.setCompressorSustain((float)compressorSustainSlider.getValue()); saveSession(); markPresetDirty(); };

    addAndMakeVisible(delayButton);
    delayButton.setClickingTogglesState(true);
    delayButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeee5d5));
    delayButton.onClick = [this] { engine.setDelayEnabled(delayButton.getToggleState()); saveSession(); markPresetDirty(); repaint(); };
    setupGateSlider(delayTimeSlider, delayTimeLabel, "TIME", 50.0, 1000.0, 350.0);
    delayTimeSlider.setTextValueSuffix(" ms");
    setupGateSlider(delayFeedbackSlider, delayFeedbackLabel, "FEEDBACK", 0.0, 0.85, 0.32);
    setupGateSlider(delayMixSlider, delayMixLabel, "MIX", 0.0, 0.5, 0.22);
    delayTimeSlider.onValueChange = [this] { engine.setDelayTimeMs((float)delayTimeSlider.getValue()); saveSession(); markPresetDirty(); };
    delayFeedbackSlider.onValueChange = [this] { engine.setDelayFeedback((float)delayFeedbackSlider.getValue()); saveSession(); markPresetDirty(); };
    delayMixSlider.onValueChange = [this] { engine.setDelayMix((float)delayMixSlider.getValue()); saveSession(); markPresetDirty(); };
    addAndMakeVisible(delayTapButton);
    delayTapButton.onClick = [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (lastDelayTapMs > 0.0)
        {
            const double interval = now - lastDelayTapMs;
            if (interval >= 250.0 && interval <= 2000.0)
            {
                const double bpm = 60000.0 / interval;
                delayTimeSlider.setValue(juce::jlimit(50.0, 1000.0, 60000.0 / bpm), juce::sendNotificationSync);
                delayTapButton.setButtonText("TAP " + juce::String((int)std::round(bpm)) + " BPM");
            }
        }
        lastDelayTapMs = now;
    };

    addAndMakeVisible(chorusButton);
    chorusButton.setClickingTogglesState(true);
    chorusButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeee5d5));
    chorusButton.onClick = [this] { engine.setChorusEnabled(chorusButton.getToggleState()); saveSession(); markPresetDirty(); repaint(); };
    setupGateSlider(chorusRateSlider, chorusRateLabel, "RATE", 0.1, 5.0, 0.8);
    chorusRateSlider.setTextValueSuffix(" Hz");
    setupGateSlider(chorusDepthSlider, chorusDepthLabel, "DEPTH", 0.0, 10.0, 5.0);
    setupGateSlider(chorusMixSlider, chorusMixLabel, "MIX", 0.0, 0.5, 0.22);
    chorusRateSlider.onValueChange = [this] { engine.setChorusRate((float)chorusRateSlider.getValue()); saveSession(); markPresetDirty(); };
    chorusDepthSlider.onValueChange = [this] { engine.setChorusDepth((float)chorusDepthSlider.getValue()); saveSession(); markPresetDirty(); };
    chorusMixSlider.onValueChange = [this] { engine.setChorusMix((float)chorusMixSlider.getValue()); saveSession(); markPresetDirty(); };

    addAndMakeVisible(reverbButton);
    reverbButton.setClickingTogglesState(true);
    reverbButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeee5d5));
    reverbButton.onClick = [this] { engine.setReverbEnabled(reverbButton.getToggleState()); saveSession(); markPresetDirty(); repaint(); };
    setupGateSlider(reverbDecaySlider, reverbDecayLabel, "DECAY", 0.3, 8.0, 2.2);
    setupGateSlider(reverbPreDelaySlider, reverbPreDelayLabel, "PRE", 0.0, 120.0, 18.0);
    setupGateSlider(reverbSizeSlider, reverbSizeLabel, "SIZE", 0.0, 10.0, 5.0);
    setupGateSlider(reverbMixSlider, reverbMixLabel, "MIX", 0.0, 100.0, 20.0);
    reverbMixSlider.setTextValueSuffix("%");
    reverbDecaySlider.onValueChange = [this] { engine.setReverbDecay((float)reverbDecaySlider.getValue()); saveSession(); markPresetDirty(); };
    reverbPreDelaySlider.onValueChange = [this] { engine.setReverbPreDelay((float)reverbPreDelaySlider.getValue()); saveSession(); markPresetDirty(); };
    reverbSizeSlider.onValueChange = [this] { engine.setReverbSize((float)reverbSizeSlider.getValue()); saveSession(); markPresetDirty(); };
    reverbMixSlider.onValueChange = [this] { engine.setReverbMix((float)reverbMixSlider.getValue()); saveSession(); markPresetDirty(); };

    addAndMakeVisible(reverbTypeCombo);
    reverbTypeCombo.addItem("ROOM", 1);
    reverbTypeCombo.addItem("STUDIO", 2);
    reverbTypeCombo.addItem("PLATE", 3);
    reverbTypeCombo.addItem("HALL", 4);
    reverbTypeCombo.setSelectedId(2, juce::dontSendNotification);
    reverbTypeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff20262b));
    reverbTypeCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xffeee5d5));
    reverbTypeCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff9a7848));
    reverbTypeCombo.onChange = [this] { engine.setReverbType(reverbTypeCombo.getSelectedId() - 1); saveSession(); markPresetDirty(); };

    addAndMakeVisible(audioButton);
    addAndMakeVisible(ampPageButton);
    addAndMakeVisible(effectsPageButton);
    ampPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff9a7848));
    effectsPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34302a));
    for (auto* nav : { &ampPageButton, &effectsPageButton })
    {
        nav->setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0e7d5));
        nav->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff9a7848));
    }
    ampPageButton.onClick = [this]
    {
        effectsPageOpen = false;
        ampPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff9a7848));
        effectsPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34302a));
        resized(); repaint();
    };
    effectsPageButton.onClick = [this]
    {
        effectsPageOpen = true;
        ampPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34302a));
        effectsPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff9a7848));
        resized(); repaint();
    };
    addAndMakeVisible(presetMenuButton);
    addAndMakeVisible(savePresetButton);
    addAndMakeVisible(loadPresetButton);
    addAndMakeVisible(overwritePresetButton);
    addAndMakeVisible(deletePresetButton);
    addAndMakeVisible(presetCombo);
    presetCombo.setTextWhenNothingSelected("Quick presets...");
    presetFilterCombo.addItem("All Presets", 1);
    presetFilterCombo.addItem("Favorites", 2);
    presetFilterCombo.addItem("Clean", 3);
    presetFilterCombo.addItem("Crunch", 4);
    presetFilterCombo.addItem("Rhythm", 5);
    presetFilterCombo.addItem("Lead", 6);
    presetFilterCombo.addItem("Custom", 7);
    presetFilterCombo.setSelectedId(1, juce::dontSendNotification);
    presetFilterCombo.setTextWhenNothingSelected("Filter presets");
    presetFilterCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252525));
    presetFilterCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0e7d5));
    presetFilterCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff9a7848));
    addAndMakeVisible(presetFilterCombo);
    favoriteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34302a));
    favoriteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0e7d5));
    addAndMakeVisible(favoriteButton);
    categoryCombo.addItem("Clean", 1);
    categoryCombo.addItem("Crunch", 2);
    categoryCombo.addItem("Rhythm", 3);
    categoryCombo.addItem("Lead", 4);
    categoryCombo.addItem("Custom", 5);
    categoryCombo.setSelectedId(5, juce::dontSendNotification);
    categoryCombo.setTextWhenNothingSelected("Category");
    categoryCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252525));
    categoryCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0e7d5));
    categoryCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff9a7848));
    addAndMakeVisible(categoryCombo);
    presetCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252525));
    presetCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0e7d5));
    presetCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff9a7848));
    overwritePresetButton.setEnabled(false);
    for (auto* control : { static_cast<juce::Component*>(&savePresetButton),
                           static_cast<juce::Component*>(&categoryCombo),
                           static_cast<juce::Component*>(&presetFilterCombo),
                           static_cast<juce::Component*>(&favoriteButton),
                           static_cast<juce::Component*>(&overwritePresetButton),
                           static_cast<juce::Component*>(&loadPresetButton),
                           static_cast<juce::Component*>(&deletePresetButton) })
        control->setVisible(false);
    deletePresetButton.setEnabled(false);
    presetLabel.setText("Preset: (unsaved)", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe5d6b8));
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetLabel);

    // Ensure the three non-tone amp control labels are actually visible.
    

    for (auto* button : { &audioButton, &presetMenuButton, &savePresetButton, &loadPresetButton, &overwritePresetButton, &deletePresetButton })
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34302a));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0e7d5));
        button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff9a7848));
    }
    for (auto* button : { &leadModeButton, &brightButton, &deepButton, &trebleShiftButton, &bassShiftButton })
    {
        button->setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff0e7d5));
        button->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffd7b77d));
        addAndMakeVisible(*button);
    }
    leadModeButton.setToggleState(true, juce::dontSendNotification);
    leadModeButton.onClick = [this] { engine.setLeadMode(leadModeButton.getToggleState()); saveSession(); markPresetDirty(); refreshStatus(); };
    brightButton.onClick = [this] { engine.setBright(brightButton.getToggleState()); saveSession(); markPresetDirty(); };
    deepButton.onClick = [this] { engine.setDeep(deepButton.getToggleState()); saveSession(); markPresetDirty(); };
    trebleShiftButton.onClick = [this] { engine.setTrebleShift(trebleShiftButton.getToggleState()); saveSession(); markPresetDirty(); };
    bassShiftButton.onClick = [this] { engine.setBassShift(bassShiftButton.getToggleState()); saveSession(); markPresetDirty(); };

    auto styleHeader = [](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffd4b27a));
    };
    styleHeader(preampHeader, "");
    styleHeader(eqHeader, "");
    styleHeader(modelHeader, "");
    addAndMakeVisible(preampHeader);
    addAndMakeVisible(eqHeader);
    addAndMakeVisible(modelHeader);
    savePresetButton.setButtonText("EXPORT / SAVE AS");
    loadPresetButton.setButtonText("IMPORT FILE");
    savePresetButton.onClick = [this] { savePreset(); };
    loadPresetButton.onClick = [this] { loadPreset(); };
    overwritePresetButton.onClick = [this] { overwritePreset(); };
    presetMenuButton.onClick = [this]
    {
        presetPanelOpen = !presetPanelOpen;
        presetMenuButton.setButtonText(presetPanelOpen ? "PRESETS [-]" : "PRESETS [+]");
        for (auto* control : { static_cast<juce::Component*>(&savePresetButton),
                               static_cast<juce::Component*>(&categoryCombo),
                               static_cast<juce::Component*>(&presetFilterCombo),
                               static_cast<juce::Component*>(&favoriteButton),
                               static_cast<juce::Component*>(&overwritePresetButton),
                               static_cast<juce::Component*>(&loadPresetButton),
                               static_cast<juce::Component*>(&deletePresetButton) })
            control->setVisible(presetPanelOpen);
        resized();
    };
    deletePresetButton.onClick = [this]
    {
        if (!currentPresetFile.existsAsFile()) return;
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,
            "Delete preset?", "Permanently delete '" + currentPresetFile.getFileNameWithoutExtension() + "'?",
            "Delete", "Cancel", this, juce::ModalCallbackFunction::create([this](int result) { if (result == 1 && currentPresetFile.existsAsFile()) { currentPresetFile.deleteFile(); currentPresetFile = {}; presetDirty = false; presetLabel.setText("Preset: (unsaved)", juce::dontSendNotification); overwritePresetButton.setEnabled(false); favoriteButton.setEnabled(false); deletePresetButton.setEnabled(false); refreshPresetList(); } }));
    };
    presetCombo.onChange = [this] { if (!updatingPresetList) loadSelectedPreset(); };
    presetFilterCombo.onChange = [this] { refreshPresetList(); };
    favoriteButton.onClick = [this] { toggleCurrentPresetFavorite(); };
    categoryCombo.onChange = [this] { markPresetDirty(); };
    refreshPresetList();
    audioButton.onClick = [this] {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Audio Settings";
        options.dialogBackgroundColour = juce::Colour(0xff252525);
        options.resizable = true;
        options.useNativeTitleBar = true;
        options.content.setOwned(new juce::AudioDeviceSelectorComponent(
            engine.getDeviceManager(), 0, 2, 1, 2, false, false, true, false));
        options.launchAsync();
    };

    configureSlider(inputGain, "", 0.0, 10.0, 5.0);
    configureSlider(leadMaster, "", -24.0, 12.0, 0.0);
    configureSlider(outputGain, "", -36.0, 6.0, -15.0);
    const std::array<juce::String, 4> toneNames { "", "", "", "" };
    for (int i = 0; i < 4; ++i)
    {
        auto& slider = tone[static_cast<size_t>(i)];
        configureSlider(slider, toneNames[static_cast<size_t>(i)], -6.0, 6.0, 0.0);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        toneLabels[static_cast<size_t>(i)].setText(toneNames[static_cast<size_t>(i)], juce::dontSendNotification);
        toneLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        toneLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId, juce::Colour(0xffe5d6b8));
        addAndMakeVisible(toneLabels[static_cast<size_t>(i)]);
        slider.onValueChange = [this, i] { engine.setToneControl(i, (float)tone[static_cast<size_t>(i)].getValue()); saveSession(); markPresetDirty(); };
    }
    inputGain.onValueChange = [this] { engine.setVolume1((float)inputGain.getValue()); saveSession(); markPresetDirty(); };
    leadMaster.onValueChange = [this] { engine.setLeadMasterDb((float)leadMaster.getValue()); saveSession(); markPresetDirty(); };
    outputGain.onValueChange = [this] { engine.setOutputGainDb((float)outputGain.getValue()); saveSession(); markPresetDirty(); };

    // Rotary values are shown by the printed faceplate markings; no live readout overlays.
    const std::array<juce::Slider*, 7> rotaryControls {
        &inputGain, &tone[0], &tone[1], &tone[2], &leadMaster, &tone[3], &outputGain
    };
    for (size_t i = 0; i < rotaryControls.size(); ++i)
    {
        rotaryControls[i]->onValueChange = [this, i, rotaryControls]
        {
            saveSession();
            markPresetDirty();
            if (i == 0) engine.setVolume1((float)inputGain.getValue());
            else if (i == 4) engine.setLeadMasterDb((float)leadMaster.getValue());
            else if (i == 6) engine.setOutputGainDb((float)outputGain.getValue());
            else engine.setToneControl((int)(i == 1 ? 0 : i == 2 ? 1 : i == 3 ? 2 : 3), (float)rotaryControls[i]->getValue());
        };
    }

    const std::array<juce::String, 5> names { "", "", "", "", "" };
    for (int i = 0; i < 5; ++i)
    {
        auto& slider = eq[static_cast<size_t>(i)];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(-12.0, 12.0, 0.5);
        slider.setValue(0.0);
        slider.setLookAndFeel(&eqSliderLookAndFeel);
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd4b27a));
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xffb6a58a));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff0e7d5));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff252525));
        addAndMakeVisible(slider);
        eqLabels[static_cast<size_t>(i)].setText(names[static_cast<size_t>(i)], juce::dontSendNotification);
        eqLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        eqLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId, juce::Colour(0xffe5d6b8));
        addAndMakeVisible(eqLabels[static_cast<size_t>(i)]);
        slider.onValueChange = [this, i] { engine.setEqGainDb(i, (float)eq[static_cast<size_t>(i)].getValue()); saveSession(); markPresetDirty(); };
    }

    // Fixed profiles are shipped in a sibling "profiles" folder beside the app executable.
    // CMake copies this folder after each build, so the app can start fully configured.
    const auto appDirectory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const auto profileDirectory = appDirectory.getChildFile("profiles");
    const auto mainNam = profileDirectory.getChildFile("IIC.nam");
    const auto cleanNam = profileDirectory.getChildFile("IIC_clean.nam");
    const auto cabinetIr = profileDirectory.getChildFile("Cab.wav");
    const bool mainOk = engine.loadNamFile(mainNam);
    const bool cleanOk = engine.loadCleanNamFile(cleanNam);
    const bool irOk = engine.loadIrFile(cabinetIr);
    if (mainOk && cleanOk && irOk)
        status.setText("Fixed profiles loaded: IIC.nam / IIC_clean.nam / Cab.wav", juce::dontSendNotification);
    else
        status.setText("Profile startup issue — check the profiles folder. " + engine.getStatus(), juce::dontSendNotification);

    loadSession();
    engine.start();
    startTimerHz(30);
    if (!mainOk || !cleanOk || !irOk) refreshStatus();
}

MainComponent::~MainComponent()
{
    inputGain.setLookAndFeel(nullptr);
    tone[0].setLookAndFeel(nullptr);
    tone[1].setLookAndFeel(nullptr);
    tone[2].setLookAndFeel(nullptr);
    leadMaster.setLookAndFeel(nullptr);
    tone[3].setLookAndFeel(nullptr);
    outputGain.setLookAndFeel(nullptr);
    for (auto& eqSlider : eq)
        eqSlider.setLookAndFeel(nullptr);
    brightButton.setLookAndFeel(nullptr);
    bassShiftButton.setLookAndFeel(nullptr);
    trebleShiftButton.setLookAndFeel(nullptr);
    leadModeButton.setLookAndFeel(nullptr);
    deepButton.setLookAndFeel(nullptr);
    saveSession();
}

void MainComponent::saveSession()
{
    if (restoringSession) return;
    auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("IICPlusNAM");
    if (!folder.createDirectory()) return;

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("version", 3);
    obj->setProperty("transposeSemitones", engine.getTransposeSemitones());
    obj->setProperty("inputGainScale", 2);
    obj->setProperty("inputGain", inputGain.getValue());
    obj->setProperty("leadMaster", leadMaster.getValue());
    obj->setProperty("outputGain", outputGain.getValue());
    for (int i = 0; i < 4; ++i) obj->setProperty("tone" + juce::String(i), tone[static_cast<size_t>(i)].getValue());
    for (int i = 0; i < 5; ++i) obj->setProperty("eq" + juce::String(i), eq[static_cast<size_t>(i)].getValue());
    obj->setProperty("leadMode", leadModeButton.getToggleState());
    obj->setProperty("bright", brightButton.getToggleState());
    obj->setProperty("deep", deepButton.getToggleState());
    obj->setProperty("trebleShift", trebleShiftButton.getToggleState());
    obj->setProperty("bassShift", bassShiftButton.getToggleState());
    obj->setProperty("noiseGateEnabled", noiseGateButton.getToggleState());
    obj->setProperty("noiseGateThreshold", gateThreshold.getValue());
    obj->setProperty("ts9Enabled", ts9Button.getToggleState());
    obj->setProperty("ts9Drive", ts9DriveSlider.getValue());
    obj->setProperty("ts9Tone", ts9ToneSlider.getValue());
    obj->setProperty("ts9Level", ts9LevelSlider.getValue());
    obj->setProperty("compressorEnabled", compressorButton.getToggleState());
    obj->setProperty("compressorSustain", compressorSustainSlider.getValue());
    obj->setProperty("delayEnabled", delayButton.getToggleState());
    obj->setProperty("delayTimeMs", delayTimeSlider.getValue());
    obj->setProperty("delayFeedback", delayFeedbackSlider.getValue());
    obj->setProperty("delayMix", delayMixSlider.getValue());
    obj->setProperty("chorusEnabled", chorusButton.getToggleState());
    obj->setProperty("chorusRate", chorusRateSlider.getValue());
    obj->setProperty("chorusDepth", chorusDepthSlider.getValue());
    obj->setProperty("chorusMix", chorusMixSlider.getValue());
    obj->setProperty("reverbEnabled", reverbButton.getToggleState());
    obj->setProperty("reverbDecay", reverbDecaySlider.getValue());
    obj->setProperty("reverbPreDelay", reverbPreDelaySlider.getValue());
    obj->setProperty("reverbSize", reverbSizeSlider.getValue());
    obj->setProperty("reverbMix", reverbMixSlider.getValue());
    obj->setProperty("reverbType", reverbTypeCombo.getSelectedId());
    const auto file = folder.getChildFile("session.json");
    file.replaceWithText(juce::JSON::toString(juce::var(obj.get()), true));

    // Persist the selected audio device, driver, sample rate, buffer size and channel setup.
    if (auto xml = engine.getDeviceManager().createStateXml())
        xml->writeTo(folder.getChildFile("audio-device.xml"));
}

void MainComponent::loadSession()
{
    const auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("IICPlusNAM");
    const auto file = folder.getChildFile("session.json");
    if (!file.existsAsFile()) return;

    juce::var data;
    if (juce::JSON::parse(file.loadFileAsString(), data).failed() || !data.isObject()) return;

    restoringSession = true;
    engine.setTransposeSemitones(juce::jlimit(-12, 12, (int)data.getProperty("transposeSemitones", 0)));
    auto getNumber = [&data](const juce::String& key, double fallback)
    {
        return (double) data.getProperty(key, fallback);
    };
    const double savedInputGain = getNumber("inputGain", 0.0);
    const double volume1Value = (int)data.getProperty("inputGainScale", 1) >= 2
        ? savedInputGain : juce::jlimit(0.0, 10.0, (savedInputGain + 24.0) / 4.8);
    inputGain.setValue(volume1Value, juce::sendNotificationSync);
    leadMaster.setValue(getNumber("leadMaster", 0.0), juce::sendNotificationSync);
    outputGain.setValue(getNumber("outputGain", -15.0), juce::sendNotificationSync);
    for (int i = 0; i < 4; ++i)
        tone[static_cast<size_t>(i)].setValue(getNumber("tone" + juce::String(i), 0.0), juce::sendNotificationSync);
    for (int i = 0; i < 5; ++i)
        eq[static_cast<size_t>(i)].setValue(getNumber("eq" + juce::String(i), 0.0), juce::sendNotificationSync);
    leadModeButton.setToggleState((bool)data.getProperty("leadMode", true), juce::sendNotificationSync);
    brightButton.setToggleState((bool)data.getProperty("bright", false), juce::sendNotificationSync);
    deepButton.setToggleState((bool)data.getProperty("deep", false), juce::sendNotificationSync);
    trebleShiftButton.setToggleState((bool)data.getProperty("trebleShift", false), juce::sendNotificationSync);
    bassShiftButton.setToggleState((bool)data.getProperty("bassShift", false), juce::sendNotificationSync);
    noiseGateButton.setToggleState((bool)data.getProperty("noiseGateEnabled", false), juce::sendNotificationSync);
    gateThreshold.setValue(getNumber("noiseGateThreshold", -55.0), juce::sendNotificationSync);
    engine.setNoiseGateEnabled(noiseGateButton.getToggleState());
    engine.setNoiseGateThreshold((float)gateThreshold.getValue());
    ts9Button.setToggleState((bool)data.getProperty("ts9Enabled", false), juce::sendNotificationSync);
    ts9DriveSlider.setValue((double)data.getProperty("ts9Drive", 2.0), juce::sendNotificationSync);
    ts9ToneSlider.setValue((double)data.getProperty("ts9Tone", 5.0), juce::sendNotificationSync);
    ts9LevelSlider.setValue((double)data.getProperty("ts9Level", 5.0), juce::sendNotificationSync);
    engine.setTs9Enabled(ts9Button.getToggleState());
    engine.setTs9Drive((float)ts9DriveSlider.getValue());
    engine.setTs9Tone((float)ts9ToneSlider.getValue());
    engine.setTs9Level((float)ts9LevelSlider.getValue());
    compressorButton.setToggleState((bool)data.getProperty("compressorEnabled", false), juce::sendNotificationSync);
    compressorSustainSlider.setValue(getNumber("compressorSustain", 3.0), juce::sendNotificationSync);
    engine.setCompressorEnabled(compressorButton.getToggleState());
    engine.setCompressorSustain((float)compressorSustainSlider.getValue());
    delayButton.setToggleState((bool)data.getProperty("delayEnabled", false), juce::dontSendNotification);
    delayTimeSlider.setValue(getNumber("delayTimeMs", 350.0), juce::dontSendNotification);
    delayFeedbackSlider.setValue(getNumber("delayFeedback", 0.32), juce::dontSendNotification);
    delayMixSlider.setValue(getNumber("delayMix", 0.22), juce::dontSendNotification);
    engine.setDelayEnabled(delayButton.getToggleState());
    engine.setDelayTimeMs((float)delayTimeSlider.getValue());
    engine.setDelayFeedback((float)delayFeedbackSlider.getValue());
    engine.setDelayMix((float)delayMixSlider.getValue());
    chorusButton.setToggleState((bool)data.getProperty("chorusEnabled", false), juce::dontSendNotification);
    chorusRateSlider.setValue(getNumber("chorusRate", 0.8), juce::dontSendNotification);
    chorusDepthSlider.setValue(getNumber("chorusDepth", 5.0), juce::dontSendNotification);
    chorusMixSlider.setValue(getNumber("chorusMix", 0.22), juce::dontSendNotification);
    engine.setChorusEnabled(chorusButton.getToggleState());
    engine.setChorusRate((float)chorusRateSlider.getValue());
    engine.setChorusDepth((float)chorusDepthSlider.getValue());
    engine.setChorusMix((float)chorusMixSlider.getValue());
    reverbButton.setToggleState((bool)data.getProperty("reverbEnabled", false), juce::dontSendNotification);
    reverbDecaySlider.setValue(getNumber("reverbDecay", 2.2), juce::dontSendNotification);
    reverbPreDelaySlider.setValue(getNumber("reverbPreDelay", 18.0), juce::dontSendNotification);
    reverbSizeSlider.setValue(getNumber("reverbSize", 5.0), juce::dontSendNotification);
    { const double savedMix = getNumber("reverbMix", 20.0);
      reverbMixSlider.setValue(savedMix <= 0.5 ? savedMix * 100.0 : savedMix, juce::dontSendNotification); }
    reverbTypeCombo.setSelectedId((int)data.getProperty("reverbType", 2), juce::dontSendNotification);
    engine.setReverbEnabled(reverbButton.getToggleState());
    engine.setReverbDecay((float)reverbDecaySlider.getValue());
    engine.setReverbPreDelay((float)reverbPreDelaySlider.getValue());
    engine.setReverbSize((float)reverbSizeSlider.getValue());
    engine.setReverbMix((float)reverbMixSlider.getValue());
    engine.setReverbType(reverbTypeCombo.getSelectedId() - 1);
    engine.setTrebleShift(trebleShiftButton.getToggleState());
    engine.setBassShift(bassShiftButton.getToggleState());
    restoringSession = false;
    const int transposeValue = engine.getTransposeSemitones();
    transposeValueLabel.setText((transposeValue > 0 ? "+" : "") + juce::String(transposeValue) + " st", juce::dontSendNotification);
    transposeValueLabel.setColour(juce::Label::textColourId, transposeValue == 0 ? juce::Colour(0xffd9e5eb) : juce::Colour(0xff70c8ff));
    presetLabel.setText("Preset: Last session", juce::dontSendNotification);
}

void MainComponent::configureSlider(juce::Slider& slider, const juce::String& name,
                                    double min, double max, double initial)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(min, max, 0.5);
    slider.setValue(initial);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffc8a56a));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff514637));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe7d4ae));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff0e7d5));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff252525));
    slider.setName(name);
    addAndMakeVisible(slider);
}

void MainComponent::chooseNam()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select a NAM amp profile", juce::File{}, "*.nam");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                const bool ok = engine.loadNamFile(file);
                status.setText(engine.getStatus(), juce::dontSendNotification);
                if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NAM load error", engine.getStatus());
            }
        });
}

void MainComponent::chooseIr()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select a cabinet IR WAV", juce::File{}, "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                const bool ok = engine.loadIrFile(file);
                status.setText(engine.getStatus(), juce::dontSendNotification);
                if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "IR load error", engine.getStatus());
            }
        });
}

juce::File MainComponent::getPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("IICPlusNAM").getChildFile("Presets");
}

void MainComponent::refreshPresetList()
{
    updatingPresetList = true;
    const auto previous = currentPresetFile;
    presetCombo.clear(juce::dontSendNotification);
    displayedPresetFiles.clear();
    auto folder = getPresetDirectory();
    folder.createDirectory();
    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.iicpreset");
    files.sort();
    const int filterId = presetFilterCombo.getSelectedId();
    int selectedId = 0;
    int id = 1;
    for (const auto& file : files)
    {
        juce::var data;
        juce::String category = "Custom";
        bool favorite = false;
        if (juce::JSON::parse(file.loadFileAsString(), data).wasOk() && data.isObject())
        {
            category = data.getProperty("category", "Custom").toString();
            favorite = (bool)data.getProperty("favorite", false);
        }
        const bool matches = filterId == 1
            || (filterId == 2 && favorite)
            || (filterId >= 3 && category == presetFilterCombo.getItemText(filterId - 1));
        if (!matches) continue;
        displayedPresetFiles.add(file);
        const auto star = favorite ? "[FAV] " : "";
        presetCombo.addItem(juce::String(star) + "[" + category + "] " + file.getFileNameWithoutExtension(), id);
        if (previous == file) selectedId = id;
        ++id;
    }
    if (selectedId != 0) presetCombo.setSelectedId(selectedId, juce::dontSendNotification);
    updatingPresetList = false;
    overwritePresetButton.setEnabled(currentPresetFile.existsAsFile());
    favoriteButton.setEnabled(currentPresetFile.existsAsFile());
    favoriteButton.setButtonText(isCurrentPresetFavorite() ? "FAV: ON" : "FAV: OFF");
}

bool MainComponent::isCurrentPresetFavorite() const
{
    if (!currentPresetFile.existsAsFile()) return false;
    juce::var data;
    return juce::JSON::parse(currentPresetFile.loadFileAsString(), data).wasOk()
        && data.isObject() && (bool)data.getProperty("favorite", false);
}

void MainComponent::toggleCurrentPresetFavorite()
{
    if (!currentPresetFile.existsAsFile()) return;
    juce::var data;
    if (juce::JSON::parse(currentPresetFile.loadFileAsString(), data).failed() || !data.isObject()) return;
    auto* object = data.getDynamicObject();
    if (object == nullptr) return;
    object->setProperty("favorite", !((bool)data.getProperty("favorite", false)));
    if (currentPresetFile.replaceWithText(juce::JSON::toString(data, true)))
        refreshPresetList();
}

void MainComponent::markPresetDirty()
{
    if (restoringSession || currentPresetFile == juce::File{}) return;
    presetDirty = true;
    presetLabel.setText("Preset: " + currentPresetFile.getFileNameWithoutExtension() + " *", juce::dontSendNotification);
}

void MainComponent::savePreset()
{
    auto folder = getPresetDirectory();
    folder.createDirectory();
    auto chooser = std::make_shared<juce::FileChooser>("Save IIC+ preset", folder.getChildFile("My Tone.iicpreset"), "*.iicpreset");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            if (file.getFileExtension().isEmpty()) file = file.withFileExtension("iicpreset");
            savePresetToFile(file);
        });
}

void MainComponent::overwritePreset()
{
    if (!currentPresetFile.existsAsFile())
    {
        savePreset();
        return;
    }
    const auto target = currentPresetFile;
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
        "Overwrite preset?", "Replace the saved settings in '" + target.getFileNameWithoutExtension() + "'?",
        "Overwrite", "Cancel", this,
        juce::ModalCallbackFunction::create([this, target](int result)
        {
            if (result == 1) savePresetToFile(target);
        }));
}

void MainComponent::savePresetToFile(const juce::File& file)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("inputGainScale", 2);
    obj->setProperty("inputGain", inputGain.getValue());
    obj->setProperty("leadMaster", leadMaster.getValue());
    obj->setProperty("outputGain", outputGain.getValue());
    for (int i = 0; i < 4; ++i) obj->setProperty("tone" + juce::String(i), tone[static_cast<size_t>(i)].getValue());
    for (int i = 0; i < 5; ++i) obj->setProperty("eq" + juce::String(i), eq[static_cast<size_t>(i)].getValue());
    obj->setProperty("leadMode", leadModeButton.getToggleState());
    obj->setProperty("bright", brightButton.getToggleState());
    obj->setProperty("deep", deepButton.getToggleState());
    obj->setProperty("trebleShift", trebleShiftButton.getToggleState());
    obj->setProperty("bassShift", bassShiftButton.getToggleState());
    obj->setProperty("noiseGateEnabled", noiseGateButton.getToggleState());
    obj->setProperty("noiseGateThreshold", gateThreshold.getValue());
    obj->setProperty("ts9Enabled", ts9Button.getToggleState());
    obj->setProperty("ts9Drive", ts9DriveSlider.getValue());
    obj->setProperty("ts9Tone", ts9ToneSlider.getValue());
    obj->setProperty("ts9Level", ts9LevelSlider.getValue());
    obj->setProperty("compressorEnabled", compressorButton.getToggleState());
    obj->setProperty("compressorSustain", compressorSustainSlider.getValue());
    obj->setProperty("delayEnabled", delayButton.getToggleState());
    obj->setProperty("delayTimeMs", delayTimeSlider.getValue());
    obj->setProperty("delayFeedback", delayFeedbackSlider.getValue());
    obj->setProperty("delayMix", delayMixSlider.getValue());
    obj->setProperty("chorusEnabled", chorusButton.getToggleState());
    obj->setProperty("chorusRate", chorusRateSlider.getValue());
    obj->setProperty("chorusDepth", chorusDepthSlider.getValue());
    obj->setProperty("chorusMix", chorusMixSlider.getValue());
    obj->setProperty("reverbEnabled", reverbButton.getToggleState());
    obj->setProperty("reverbDecay", reverbDecaySlider.getValue());
    obj->setProperty("reverbPreDelay", reverbPreDelaySlider.getValue());
    obj->setProperty("reverbSize", reverbSizeSlider.getValue());
    obj->setProperty("reverbMix", reverbMixSlider.getValue());
    obj->setProperty("reverbType", reverbTypeCombo.getSelectedId());
    obj->setProperty("category", categoryCombo.getText().isNotEmpty() ? categoryCombo.getText() : "Custom");
    if (file == currentPresetFile && currentPresetFile.existsAsFile())
        obj->setProperty("favorite", isCurrentPresetFavorite());
    else
    {
        juce::var existing;
        if (juce::JSON::parse(file.loadFileAsString(), existing).wasOk() && existing.isObject())
            obj->setProperty("favorite", (bool)existing.getProperty("favorite", false));
        else obj->setProperty("favorite", false);
    }
    const auto json = juce::JSON::toString(juce::var(obj.get()), true);
    if (file.replaceWithText(json))
    {
        currentPresetFile = file;
        presetDirty = false;
        presetLabel.setText("Preset: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
        refreshPresetList();
        overwritePresetButton.setEnabled(true);
        deletePresetButton.setEnabled(true);
    }
    else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset save failed", "Could not write the preset file.");
}

void MainComponent::loadSelectedPreset()
{
    const int selectedId = presetCombo.getSelectedId();
    if (selectedId <= 0) return;
    auto folder = getPresetDirectory();
    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.iicpreset");
    files.sort();
    const int index = selectedId - 1;
    if (index < 0 || index >= displayedPresetFiles.size()) return;
    const auto file = displayedPresetFiles.getReference(index);
    juce::var data;
    if (juce::JSON::parse(file.loadFileAsString(), data).failed() || !data.isObject())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset load failed", "This preset file is not valid JSON.");
        return;
    }
    applyPreset(data);
    {
        const auto category = data.getProperty("category", "Custom").toString();
        int categoryId = 5; // Default to Custom if the category is unknown.
        for (int itemIndex = 0; itemIndex < categoryCombo.getNumItems(); ++itemIndex)
        {
            if (categoryCombo.getItemText(itemIndex) == category)
            {
                categoryId = categoryCombo.getItemId(itemIndex);
                break;
            }
        }
        categoryCombo.setSelectedId(categoryId, juce::dontSendNotification);
    }
    currentPresetFile = file;
    presetDirty = false;
    presetLabel.setText("Preset: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
    overwritePresetButton.setEnabled(true);
    deletePresetButton.setEnabled(true);
    favoriteButton.setEnabled(true);
    favoriteButton.setButtonText(isCurrentPresetFavorite() ? "FAV: ON" : "FAV: OFF");
}

void MainComponent::loadPreset()
{
    auto chooser = std::make_shared<juce::FileChooser>("Load IIC+ preset", juce::File{}, "*.iicpreset;*.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            juce::var data;
            const auto parsed = juce::JSON::parse(file.loadFileAsString(), data);
            if (parsed.failed())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset load failed", "This preset file is not valid JSON.");
                return;
            }
            applyPreset(data);
            {
                const auto category = data.getProperty("category", "Custom").toString();
                int categoryId = 5; // Default to Custom if the category is unknown.
                for (int itemIndex = 0; itemIndex < categoryCombo.getNumItems(); ++itemIndex)
                {
                    if (categoryCombo.getItemText(itemIndex) == category)
                    {
                        categoryId = categoryCombo.getItemId(itemIndex);
                        break;
                    }
                }
                categoryCombo.setSelectedId(categoryId, juce::dontSendNotification);
            }
            currentPresetFile = file;
            presetDirty = false;
            presetLabel.setText("Preset: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
            overwritePresetButton.setEnabled(true);
            deletePresetButton.setEnabled(true);
            favoriteButton.setEnabled(true);
            favoriteButton.setButtonText(isCurrentPresetFavorite() ? "FAV: ON" : "FAV: OFF");
            refreshPresetList();
        });
}

void MainComponent::applyPreset(const juce::var& data)
{
    if (!data.isObject()) return;
    const bool wasRestoring = restoringSession;
    restoringSession = true;
    engine.setTransposeSemitones(juce::jlimit(-12, 12, (int)data.getProperty("transposeSemitones", 0)));
    auto getNumber = [&data](const juce::String& key, double fallback) { return (double)data.getProperty(key, fallback); };
    const double savedInputGain = getNumber("inputGain", 0.0);
    const double volume1Value = (int)data.getProperty("inputGainScale", 1) >= 2
        ? savedInputGain : juce::jlimit(0.0, 10.0, (savedInputGain + 24.0) / 4.8);
    inputGain.setValue(volume1Value, juce::sendNotificationSync);
    leadMaster.setValue(getNumber("leadMaster", 0.0), juce::sendNotificationSync);
    outputGain.setValue(getNumber("outputGain", -15.0), juce::sendNotificationSync);
    for (int i = 0; i < 4; ++i) tone[static_cast<size_t>(i)].setValue(getNumber("tone" + juce::String(i), 0.0), juce::sendNotificationSync);
    for (int i = 0; i < 5; ++i) eq[static_cast<size_t>(i)].setValue(getNumber("eq" + juce::String(i), 0.0), juce::sendNotificationSync);
    leadModeButton.setToggleState((bool)data.getProperty("leadMode", true), juce::sendNotificationSync);
    brightButton.setToggleState((bool)data.getProperty("bright", false), juce::sendNotificationSync);
    deepButton.setToggleState((bool)data.getProperty("deep", false), juce::sendNotificationSync);
    trebleShiftButton.setToggleState((bool)data.getProperty("trebleShift", false), juce::sendNotificationSync);
    bassShiftButton.setToggleState((bool)data.getProperty("bassShift", false), juce::sendNotificationSync);
    noiseGateButton.setToggleState((bool)data.getProperty("noiseGateEnabled", false), juce::sendNotificationSync);
    gateThreshold.setValue(getNumber("noiseGateThreshold", -55.0), juce::sendNotificationSync);
    engine.setNoiseGateEnabled(noiseGateButton.getToggleState());
    engine.setNoiseGateThreshold((float)gateThreshold.getValue());
    ts9Button.setToggleState((bool)data.getProperty("ts9Enabled", false), juce::sendNotificationSync);
    ts9DriveSlider.setValue((double)data.getProperty("ts9Drive", 2.0), juce::sendNotificationSync);
    ts9ToneSlider.setValue((double)data.getProperty("ts9Tone", 5.0), juce::sendNotificationSync);
    ts9LevelSlider.setValue((double)data.getProperty("ts9Level", 5.0), juce::sendNotificationSync);
    engine.setTs9Enabled(ts9Button.getToggleState());
    engine.setTs9Drive((float)ts9DriveSlider.getValue());
    engine.setTs9Tone((float)ts9ToneSlider.getValue());
    engine.setTs9Level((float)ts9LevelSlider.getValue());
    compressorButton.setToggleState((bool)data.getProperty("compressorEnabled", false), juce::sendNotificationSync);
    compressorSustainSlider.setValue(getNumber("compressorSustain", 3.0), juce::sendNotificationSync);
    engine.setCompressorEnabled(compressorButton.getToggleState());
    engine.setCompressorSustain((float)compressorSustainSlider.getValue());
    delayButton.setToggleState((bool)data.getProperty("delayEnabled", false), juce::dontSendNotification);
    delayTimeSlider.setValue(getNumber("delayTimeMs", 350.0), juce::dontSendNotification);
    delayFeedbackSlider.setValue(getNumber("delayFeedback", 0.32), juce::dontSendNotification);
    delayMixSlider.setValue(getNumber("delayMix", 0.22), juce::dontSendNotification);
    engine.setDelayEnabled(delayButton.getToggleState());
    engine.setDelayTimeMs((float)delayTimeSlider.getValue());
    engine.setDelayFeedback((float)delayFeedbackSlider.getValue());
    engine.setDelayMix((float)delayMixSlider.getValue());
    chorusButton.setToggleState((bool)data.getProperty("chorusEnabled", false), juce::dontSendNotification);
    chorusRateSlider.setValue(getNumber("chorusRate", 0.8), juce::dontSendNotification);
    chorusDepthSlider.setValue(getNumber("chorusDepth", 5.0), juce::dontSendNotification);
    chorusMixSlider.setValue(getNumber("chorusMix", 0.22), juce::dontSendNotification);
    engine.setChorusEnabled(chorusButton.getToggleState());
    engine.setChorusRate((float)chorusRateSlider.getValue());
    engine.setChorusDepth((float)chorusDepthSlider.getValue());
    engine.setChorusMix((float)chorusMixSlider.getValue());
    reverbButton.setToggleState((bool)data.getProperty("reverbEnabled", false), juce::dontSendNotification);
    reverbDecaySlider.setValue(getNumber("reverbDecay", 2.2), juce::dontSendNotification);
    reverbPreDelaySlider.setValue(getNumber("reverbPreDelay", 18.0), juce::dontSendNotification);
    reverbSizeSlider.setValue(getNumber("reverbSize", 5.0), juce::dontSendNotification);
    { const double savedMix = getNumber("reverbMix", 20.0);
      reverbMixSlider.setValue(savedMix <= 0.5 ? savedMix * 100.0 : savedMix, juce::dontSendNotification); }
    reverbTypeCombo.setSelectedId((int)data.getProperty("reverbType", 2), juce::dontSendNotification);
    engine.setReverbEnabled(reverbButton.getToggleState());
    engine.setReverbDecay((float)reverbDecaySlider.getValue());
    engine.setReverbPreDelay((float)reverbPreDelaySlider.getValue());
    engine.setReverbSize((float)reverbSizeSlider.getValue());
    engine.setReverbMix((float)reverbMixSlider.getValue());
    engine.setReverbType(reverbTypeCombo.getSelectedId() - 1);
    engine.setTrebleShift(trebleShiftButton.getToggleState());
    engine.setBassShift(bassShiftButton.getToggleState());
    restoringSession = wasRestoring;
    refreshStatus();
}

void MainComponent::refreshStatus()
{
    status.setText(engine.getStatus(), juce::dontSendNotification);
}

void MainComponent::timerCallback()
{
    auto updateLevel = [](float linear, float& displayedDb)
    {
        const float db = linear > 0.000001f ? juce::Decibels::gainToDecibels(linear) : -120.0f;
        // Fast attack, slower release so the meter remains readable between audio blocks.
        if (db > displayedDb) displayedDb = db;
        else displayedDb = juce::jmax(db, displayedDb - 2.5f);
        displayedDb = juce::jlimit(-60.0f, 6.0f, displayedDb);
    };
    updateLevel(engine.getInputPeakLinear(), displayedInputDb);
    updateLevel(engine.getOutputPeakLinear(), displayedOutputDb);
    displayedTunerHz = engine.getTunerFrequencyHz();
    if (engine.consumeInputClip()) inputClipHoldFrames = 30;
    if (engine.consumeOutputClip()) outputClipHoldFrames = 30;
    if (inputClipHoldFrames > 0) --inputClipHoldFrames;
    if (outputClipHoldFrames > 0) --outputClipHoldFrames;
    inputClipping = inputClipHoldFrames > 0;
    outputClipping = outputClipHoldFrames > 0;
    // Clip lights hold for about one second, then clear and re-arm for new overloads.
    repaint();
}

void MainComponent::drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   const juce::String& label, float levelDb, bool clipping)
{
    const float labelWidth = 76.0f;
    auto labelArea = bounds.removeFromLeft(labelWidth);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffd4b27a));
    g.drawText(label, labelArea.toNearestInt(), juce::Justification::centredLeft);
    auto dbArea = bounds.removeFromRight(44.0f);
    g.setColour(juce::Colour(0xffe5d6b8));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText(levelDb <= -59.5f ? "-inf dB" : juce::String(levelDb, 1) + " dB", dbArea.toNearestInt(), juce::Justification::centredRight);
    bounds = bounds.reduced(2.0f, 3.0f);
    g.setColour(juce::Colour(0xff0b0b0b));
    g.fillRoundedRectangle(bounds, 2.0f);
    const float proportion = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    auto fill = bounds.withWidth(bounds.getWidth() * proportion);
    if (fill.getWidth() > 0.0f)
    {
        g.setColour(levelDb > -6.0f ? juce::Colour(0xffd7a34a) : juce::Colour(0xff719b67));
        g.fillRoundedRectangle(fill, 2.0f);
    }
    if (clipping)
    {
        g.setColour(juce::Colour(0xffd83d32));
        g.fillRoundedRectangle(juce::Rectangle<float>(dbArea.getX() - 14.0f, bounds.getY(), 10.0f, bounds.getHeight()), 2.0f);
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff080b0e));

    if (effectsPageOpen)
    {
        const float boardHeight = pedalboardBackground.isValid()
            ? (float)getWidth() * (float)pedalboardBackground.getHeight() / (float)pedalboardBackground.getWidth()
            : 0.0f;
        if (pedalboardBackground.isValid())
        {
            const float sx = (float)getWidth() / (float)pedalboardBackground.getWidth();
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(pedalboardBackground, juce::Rectangle<float>(0.0f, 32.0f, (float)getWidth(), boardHeight));
            const std::array<bool, 6> enabled { noiseGateButton.getToggleState(), ts9Button.getToggleState(),
                compressorButton.getToggleState(), delayButton.getToggleState(), chorusButton.getToggleState(),
                reverbButton.getToggleState() };
            const std::array<float, 6> ledX { 155.0f, 416.0f, 681.0f, 972.0f, 1242.0f, 1514.0f };
            const float ledY = 32.0f + 399.0f * sx;
            for (size_t i = 0; i < enabled.size(); ++i)
            {
                const float cx = ledX[i] * sx;
                g.setColour(juce::Colour(0xff171313));
                g.fillEllipse(cx - 10.0f, ledY - 10.0f, 20.0f, 20.0f);
                if (enabled[i])
                {
                    g.setColour(juce::Colour(0x66ff2114));
                    g.fillEllipse(cx - 13.0f, ledY - 13.0f, 26.0f, 26.0f);
                    g.setColour(juce::Colour(0xffff3428));
                }
                else g.setColour(juce::Colour(0xff4a1714));
                g.fillEllipse(cx - 5.5f, ledY - 5.5f, 11.0f, 11.0f);
                g.setColour(enabled[i] ? juce::Colour(0xffffc3a8) : juce::Colour(0xff74423b));
                g.fillEllipse(cx - 2.0f, ledY - 3.5f, 3.5f, 3.0f);
            }
        }
        else
        {
            g.setColour(juce::Colour(0xff15191d)); g.fillAll();
            g.setColour(juce::Colour(0xffd4b27a));
            g.drawFittedText("PEDALBOARD ARTWORK NOT FOUND\nPlace pedalboard_effects_background.png in the assets folder.",
                             getLocalBounds().reduced(30), juce::Justification::centred, 2);
            return;
        }

        const float stripTop = 32.0f + boardHeight + 30.0f;
        if (effectsControlStripBackground.isValid())
        {
            const float stripHeight = (float)getWidth() * (float)effectsControlStripBackground.getHeight()
                                      / (float)effectsControlStripBackground.getWidth();
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(effectsControlStripBackground,
                        juce::Rectangle<float>(0.0f, stripTop, (float)getWidth(), stripHeight));
        }
        else
        {
            g.setColour(juce::Colour(0xff15191d));
            g.fillRect(0.0f, stripTop, (float)getWidth(), 190.0f);
            g.setColour(juce::Colour(0xffffc06a));
            g.drawText("EFFECT CONTROL STRIP ARTWORK NOT FOUND", getLocalBounds().withY((int)stripTop).withHeight(30),
                       juce::Justification::centred);
        }
        return;
    }

    // Photorealistic amp body only (the artwork excludes the app toolbar/footer).
    // The cleared faceplate field is designed for the actual JUCE controls above it.
    if (faceplateBackground.isValid())
    {
        const float imageHeight = (float)getWidth() * (float)faceplateBackground.getHeight()
                                / (float)faceplateBackground.getWidth();
        g.drawImage(faceplateBackground,
                    juce::Rectangle<float>(0.0f, 32.0f, (float)getWidth(), imageHeight));
    }
    else
    {
        // Visible diagnostic fallback if the embedded PNG cannot be decoded.
        auto panel = juce::Rectangle<float>(12.0f, 180.0f, (float)getWidth()-24.0f, 220.0f);
        g.setColour(juce::Colour(0xff101113));
        g.fillRoundedRectangle(panel, 5.0f);
        g.setColour(juce::Colour(0xffc7a36a));
        g.drawRoundedRectangle(panel, 5.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        g.drawFittedText("FACEPLATE IMAGE FAILED TO DECODE", panel.toNearestInt(), juce::Justification::centred, 1);
    }

    // Dedicated bottom utility strip on the AMP page only. The Effects page
    // returns above, so its artwork and control layout remain untouched.
    const float barHeight = 96.0f;
    const float barY = (float)getHeight() - barHeight - 7.0f;
    juce::Rectangle<float> utilityBar(7.0f, barY, (float)getWidth() - 14.0f, barHeight);
    g.setColour(juce::Colour(0xff111416));
    g.fillRoundedRectangle(utilityBar, 5.0f);
    g.setColour(juce::Colour(0xff8f754f));
    g.drawRoundedRectangle(utilityBar, 5.0f, 1.0f);
    g.setColour(juce::Colour(0xff292b2c));
    g.drawLine(utilityBar.getX() + utilityBar.getWidth() * 0.53f, barY + 10.0f,
               utilityBar.getX() + utilityBar.getWidth() * 0.53f, barY + barHeight - 10.0f, 1.0f);

    const float meterX = utilityBar.getX() + 16.0f;
    const float meterWidth = utilityBar.getWidth() * 0.38f - 24.0f;
    drawLevelMeter(g, { meterX, barY + 23.0f, meterWidth, 16.0f },
                   "INPUT", displayedInputDb, inputClipping);
    drawLevelMeter(g, { meterX, barY + 55.0f, meterWidth, 16.0f },
                   "OUTPUT", displayedOutputDb, outputClipping);

    const float tunerLeft = utilityBar.getX() + utilityBar.getWidth() * 0.58f;
    const float tunerWidth = utilityBar.getRight() - tunerLeft - 16.0f;
    g.setColour(juce::Colour(0xffd4b27a));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("TUNER", (int)tunerLeft, (int)barY + 8, (int)tunerWidth - 100, 14, juce::Justification::centred);

    juce::String noteName = "—";
    float cents = 0.0f;
    const bool tunerActive = engine.isTunerEnabled() && displayedTunerHz > 0.0f && engine.getInputPeakLinear() > 0.008f;
    if (tunerActive)
    {
        const float midiFloat = 69.0f + 12.0f * std::log2(displayedTunerHz / 440.0f);
        const int midiNote = (int)std::round(midiFloat);
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        noteName = juce::String(noteNames[(midiNote % 12 + 12) % 12]) + juce::String(midiNote / 12 - 1);
        const float targetHz = 440.0f * std::pow(2.0f, ((float)midiNote - 69.0f) / 12.0f);
        cents = 1200.0f * std::log2(displayedTunerHz / targetHz);
    }
    g.setColour(tunerActive ? juce::Colour(0xfff0d5a0) : juce::Colour(0xffbdb5a7));
    g.setFont(juce::Font(juce::FontOptions(23.0f, juce::Font::bold)));
    g.drawText(tunerActive ? noteName : "PLAY A NOTE", (int)tunerLeft + 4, (int)barY + 24,
               (int)(tunerWidth * 0.28f), 34, juce::Justification::centred);

    const float scaleLeft = tunerLeft + tunerWidth * 0.31f;
    const float scaleWidth = tunerWidth * 0.49f;
    const float scaleY = barY + 46.0f;
    g.setColour(juce::Colour(0xff24292b));
    g.fillRoundedRectangle(scaleLeft, scaleY - 4.0f, scaleWidth, 8.0f, 3.0f);
    for (int i = 0; i <= 10; ++i)
    {
        const float x = scaleLeft + scaleWidth * (float)i / 10.0f;
        g.setColour(i == 5 ? juce::Colour(0xffd4b27a) : juce::Colour(0xff77736b));
        g.fillRect(x - (i == 5 ? 1.5f : 0.7f), scaleY - (i == 5 ? 14.0f : 7.0f),
                   i == 5 ? 3.0f : 1.4f, i == 5 ? 28.0f : 14.0f);
    }
    if (tunerActive)
    {
        const float markerX = scaleLeft + scaleWidth * (0.5f + juce::jlimit(-50.0f, 50.0f, cents) / 100.0f);
        g.setColour(std::abs(cents) < 5.0f ? juce::Colour(0xff86c98a) : juce::Colour(0xffe0c080));
        g.fillEllipse(markerX - 4.0f, scaleY - 4.0f, 8.0f, 8.0f);
    }
    g.setColour(juce::Colour(0xffbdb5a7));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("FLAT", (int)scaleLeft - 2, (int)barY + 63, 42, 12, juce::Justification::centredLeft);
    g.drawText("SHARP", (int)(scaleLeft + scaleWidth - 40), (int)barY + 63, 42, 12, juce::Justification::centredRight);
    g.setColour(tunerActive && std::abs(cents) < 5.0f ? juce::Colour(0xff86c98a) : juce::Colour(0xffd4b27a));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(tunerActive ? juce::String(cents, 1) + " cents" : "CHROMATIC", (int)(tunerLeft + tunerWidth * 0.82f),
               (int)barY + 36, (int)(tunerWidth * 0.16f), 20, juce::Justification::centred);
}

void MainComponent::resized()
{
    // Utility strip stays compact above the amp faceplate.
    auto area = getLocalBounds().reduced(28);
    // Keep utility controls in a dedicated top toolbar, above the amp artwork.
    // The native window caption provides the app title; hide the duplicate title.
    title.setBounds(0, 0, 0, 0);
    auto fileRow = juce::Rectangle<int>(20, 2, getWidth()-40, 27);
    ampPageButton.setBounds(fileRow.removeFromLeft(58).reduced(1));
    effectsPageButton.setBounds(fileRow.removeFromLeft(82).reduced(1));
    audioButton.setBounds(fileRow.removeFromLeft(112).reduced(1));
    presetMenuButton.setBounds(fileRow.removeFromLeft(112).reduced(1));
    presetCombo.setBounds(fileRow.removeFromLeft(210).reduced(1));
    presetLabel.setBounds(fileRow.reduced(3, 1));

    const int utilityBarY = getHeight() - 96 - 7;
    const int utilityBarWidth = getWidth() - 14;
    const int tunerLeft = 7 + (int)(utilityBarWidth * 0.58f);
    const int tunerRight = 7 + utilityBarWidth - 16;
    tunerToggleButton.setBounds(tunerRight - 91, utilityBarY + 6, 82, 18);
    tunerToggleButton.setVisible(!effectsPageOpen);
    const int transposeCenter = 7 + (int)(utilityBarWidth * 0.485f);
    transposeDownButton.setBounds(transposeCenter - 48, utilityBarY + 34, 28, 28);
    transposeValueLabel.setBounds(transposeCenter - 18, utilityBarY + 34, 52, 28);
    transposeUpButton.setBounds(transposeCenter + 36, utilityBarY + 34, 28, 28);
    transposeDownButton.setVisible(!effectsPageOpen);
    transposeValueLabel.setVisible(!effectsPageOpen);
    transposeUpButton.setVisible(!effectsPageOpen);

    if (presetPanelOpen)
    {
        auto managementRow = juce::Rectangle<int>(20, 29, getWidth()-40, 27);
        savePresetButton.setBounds(managementRow.removeFromLeft(120).reduced(1));
        categoryCombo.setBounds(managementRow.removeFromLeft(88).reduced(1));
        presetFilterCombo.setBounds(managementRow.removeFromLeft(190).reduced(1));
        favoriteButton.setBounds(managementRow.removeFromLeft(78).reduced(1));
        overwritePresetButton.setBounds(managementRow.removeFromLeft(94).reduced(1));
        loadPresetButton.setBounds(managementRow.removeFromLeft(88).reduced(1));
        deletePresetButton.setBounds(managementRow.removeFromLeft(88).reduced(1));
    }
    else
    {
        for (auto* control : { static_cast<juce::Component*>(&savePresetButton),
                               static_cast<juce::Component*>(&categoryCombo),
                               static_cast<juce::Component*>(&presetFilterCombo),
                               static_cast<juce::Component*>(&favoriteButton),
                               static_cast<juce::Component*>(&overwritePresetButton),
                               static_cast<juce::Component*>(&loadPresetButton),
                               static_cast<juce::Component*>(&deletePresetButton) })
            control->setBounds(0, 0, 0, 0);
    }

    // Seven amp controls sit across the left side of the black faceplate.
    // Positions are tied to the artwork's knob centres (at the reference
    // 1074px width) and scale proportionally with the window width.
    const float sx = (float)getWidth() / 1074.0f;
    // Stage 3I: shift the knob row down and right relative to the previous screenshot.
    // All seven rotary components use identical square bounds for consistent sizing.
    const int top = (int)(80.0f * sx);
    // Stage 5A: small artwork-alignment refinements; preserve the overall row.
    // Stage 5G: final tiny corrections from the latest screenshot.
    // Move DRIVE and MASTER slightly right; preserve all other knob centres.
    const std::array<int, 7> knobCentres {
        (int)(123.0f*sx), (int)(197.0f*sx) + 1, (int)(272.0f*sx) + 1,
        (int)(350.0f*sx), (int)(428.0f*sx), (int)(506.5f*sx), (int)(586.72f*sx)
    };
    const int knobW = (int)(64.0f * sx);
    const int knobH = (int)(64.0f * sx);
    const std::array<juce::String, 4> names { "BASS", "MID", "TREBLE", "PRESENCE" };
    inputLabel.setText("DRIVE", juce::dontSendNotification);
    leadMasterLabel.setText("VOLUME", juce::dontSendNotification);
    outputLabel.setText("MASTER", juce::dontSendNotification);
    for (auto* label : { &inputLabel, &leadMasterLabel, &outputLabel })
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xffe8e1d4));
        label->setJustificationType(juce::Justification::centred);
    }
    auto setKnobCell = [top, knobW, knobH, sx](juce::Component& label, juce::Component& knob, int centre)
    {
        // Printed knob labels are already part of the faceplate artwork.
        label.setBounds(0, 0, 0, 0);
        // Uniform square bounds for every knob; keep controls aligned to the printed centres.
        knob.setBounds(centre - knobW/2, top + (int)(40.0f*sx), knobW, knobH);
    };
    setKnobCell(inputLabel, inputGain, knobCentres[0]);
    setKnobCell(toneLabels[0], tone[0], knobCentres[1]);
    setKnobCell(toneLabels[1], tone[1], knobCentres[2]);
    setKnobCell(toneLabels[2], tone[2], knobCentres[3]);
    setKnobCell(leadMasterLabel, leadMaster, knobCentres[4]);
    setKnobCell(toneLabels[3], tone[3], knobCentres[5]);
    setKnobCell(outputLabel, outputGain, knobCentres[6]);
    for (int i=0; i<4; ++i)
    {
        toneLabels[(size_t)i].setColour(juce::Label::textColourId, juce::Colour(0xffe8e1d4));
        toneLabels[(size_t)i].setJustificationType(juce::Justification::centred);
        toneLabels[(size_t)i].setText(names[(size_t)i], juce::dontSendNotification);
    }
    // Dedicated switches aligned with their corresponding amp knobs.
    // DRIVE -> BRIGHT, BASS -> SHIFT, TREBLE -> SHIFT,
    // VOLUME -> LEAD, MASTER -> DEEP. MID and PRESENCE have no switches.
    // Move switch row up to align with the INPUT legend beneath the input jack.
    const int switchY = (int)(180.0f * sx);
    const int switchH = (int)(50.0f * sx);
    // The printed faceplate already carries switch labels. Keep only the
    // interactive hit targets so software text does not duplicate the artwork.
    brightButton.setButtonText("");
    bassShiftButton.setButtonText("");
    trebleShiftButton.setButtonText("");
    leadModeButton.setButtonText("");
    deepButton.setButtonText("");
    auto switchBounds = [switchY, switchH, sx](juce::Component& c, int centre)
    {
        c.setBounds(centre - (int)(27.0f*sx), switchY, (int)(54.0f*sx), switchH);
    };
    // These centres target the small printed toggle hardware, which sits left
    // of each associated knob centre on this faceplate. Keep hit areas centred
    // over the artwork rather than centring them on the knobs themselves.
    const std::array<int, 5> switchCentres {
        (int)(122.0f*sx),   // BRIGHT beneath DRIVE
        (int)(198.0f*sx),  // BASS SHIFT beneath BASS (13 px left of original centre)
        (int)(350.0f*sx),  // TREBLE SHIFT beneath TREBLE (13 px left)
        (int)(506.0f*sx),  // PULL LEAD beneath PRESENCE (14 px left)
        (int)(586.0f*sx)   // DEEP beneath MASTER (14 px left)
    };
    switchBounds(brightButton, switchCentres[0]);
    switchBounds(bassShiftButton, switchCentres[1]);
    switchBounds(trebleShiftButton, switchCentres[2]);
    // Pull Lead remains mapped to the existing clean/lead profile switch.
    switchBounds(leadModeButton, switchCentres[3]);
    switchBounds(deepButton, switchCentres[4]);

    // Silver graphic EQ panel on the right, five vertical faders.
    const int eqX = (int)(650.0f * sx);
    const int eqW = (int)(270.0f * sx);
    eqHeader.setBounds(eqX, (int)(68.0f*sx), eqW, (int)(17.0f*sx));
    // Track centres follow the five printed slider tracks in the artwork.
    const std::array<int, 5> eqCentres {
        (int)(678.0f*sx), (int)(714.0f*sx), (int)(748.0f*sx),
        (int)(785.5f*sx), (int)(822.0f*sx)
    };
    const int eqCellW = (int)(40.0f*sx);
    for (int i=0; i<5; ++i)
    {
        // Tighten the vertical travel to match the printed +12 / -12 scale.
        // The slider image is drawn as one complete piece; these bounds bring
        // both endpoint handle positions upward without changing its size or X position.
        auto band = juce::Rectangle<int>(eqCentres[(size_t)i]-eqCellW/2,
                                         (int)(80.0f*sx), eqCellW, (int)(146.0f*sx));
        eqLabels[(size_t)i].setBounds(0, 0, 0, 0);
        band.removeFromTop((int)(21.0f*sx));
        eq[(size_t)i].setBounds(band.reduced((int)(1.0f*sx), 0));
    }

    // The photo background already contains the amp identity and grille badge.
    // Hide old overlay headings/status so they cannot collide with the artwork.
    preampHeader.setBounds(0, 0, 0, 0);
    modelHeader.setBounds(0, 0, 0, 0);
    status.setBounds(0, 0, 0, 0);

    const bool showAmp = !effectsPageOpen;
    const std::array<juce::Component*, 11> ampComponents {
        &inputGain, &leadMaster, &outputGain,
        &brightButton, &deepButton, &leadModeButton, &trebleShiftButton,
        &bassShiftButton, &preampHeader, &eqHeader, &modelHeader
    };
    for (auto* component : ampComponents)
        component->setVisible(showAmp);
    for (auto& knob : tone) knob.setVisible(showAmp);
    for (auto& label : toneLabels) label.setVisible(showAmp);
    for (auto& slider : eq) slider.setVisible(showAmp);
    for (auto& label : eqLabels) label.setVisible(showAmp);
    inputLabel.setVisible(showAmp); leadMasterLabel.setVisible(showAmp); outputLabel.setVisible(showAmp);
    presetLabel.setVisible(true);

    // The first PRE card is the live Noise Gate module; controls are overlaid
    // within that card and remain hidden on the AMP page.
    const bool showGate = effectsPageOpen;
    noiseGateButton.setVisible(showGate);
    gateThreshold.setVisible(showGate);
    gateThresholdLabel.setVisible(showGate);
    ts9Button.setVisible(showGate);
    ts9DriveSlider.setVisible(showGate); ts9ToneSlider.setVisible(showGate); ts9LevelSlider.setVisible(showGate);
    ts9DriveLabel.setVisible(showGate); ts9ToneLabel.setVisible(showGate); ts9LevelLabel.setVisible(showGate);
    compressorButton.setVisible(showGate);
    compressorSustainSlider.setVisible(showGate);
    compressorSustainLabel.setVisible(showGate);
    delayButton.setVisible(showGate);
    delayTimeSlider.setVisible(showGate); delayFeedbackSlider.setVisible(showGate); delayMixSlider.setVisible(showGate);
    delayTimeLabel.setVisible(showGate); delayFeedbackLabel.setVisible(showGate); delayMixLabel.setVisible(showGate);
    delayTapButton.setVisible(showGate);
    chorusButton.setVisible(showGate);
    chorusRateSlider.setVisible(showGate); chorusDepthSlider.setVisible(showGate); chorusMixSlider.setVisible(showGate);
    chorusRateLabel.setVisible(showGate); chorusDepthLabel.setVisible(showGate); chorusMixLabel.setVisible(showGate);
    reverbButton.setVisible(showGate);
    reverbDecaySlider.setVisible(showGate); reverbPreDelaySlider.setVisible(showGate);
    reverbSizeSlider.setVisible(showGate); reverbMixSlider.setVisible(showGate);
    reverbDecayLabel.setVisible(showGate); reverbPreDelayLabel.setVisible(showGate);
    reverbSizeLabel.setVisible(showGate); reverbMixLabel.setVisible(showGate);
    reverbTypeCombo.setVisible(showGate);
    if (showGate)
    {
        const float boardHeight = pedalboardBackground.isValid()
            ? (float)getWidth() * (float)pedalboardBackground.getHeight() / (float)pedalboardBackground.getWidth()
            : (float)getWidth() * 0.34f;
        const float sx = (float)getWidth() / 1199.0f;
        const int stripTop = 32 + (int)std::round(boardHeight) + 30;

        // The reference photo supplies the printed labels, metal rails and panel texture.
        // These live controls sit directly over the photographed knob positions.
        auto placeRotary = [sx, stripTop](juce::Slider& slider, float imageX, float imageY)
        {
            const int w = juce::jmax(34, (int)std::round(56.0f * sx));
            const int h = juce::jmax(56, (int)std::round(80.0f * sx));
            const int x = (int)std::round(imageX * sx);
            const int y = stripTop + (int)std::round((imageY - 29.0f) * sx);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                   juce::jmax(38, (int)std::round(54.0f * sx)),
                                   juce::jmax(15, (int)std::round(18.0f * sx)));
            slider.setBounds(x - w / 2, y, w, h);
        };

        // Labels are already printed into the photographic control-strip artwork.
        for (auto* label : { &gateThresholdLabel, &ts9DriveLabel, &ts9ToneLabel, &ts9LevelLabel,
                             &compressorSustainLabel, &delayTimeLabel, &delayFeedbackLabel, &delayMixLabel,
                             &chorusRateLabel, &chorusDepthLabel, &chorusMixLabel, &reverbDecayLabel,
                             &reverbPreDelayLabel, &reverbSizeLabel, &reverbMixLabel })
            label->setVisible(false);

        const float knobY = 95.0f;
        placeRotary(gateThreshold, 82.0f, knobY);
        placeRotary(ts9DriveSlider, 208.0f, knobY);
        placeRotary(ts9ToneSlider, 274.0f, knobY);
        placeRotary(ts9LevelSlider, 335.0f, knobY);
        placeRotary(compressorSustainSlider, 470.0f, knobY); // centered in the Compressor panel
        placeRotary(delayTimeSlider, 605.0f, knobY);
        placeRotary(delayFeedbackSlider, 668.0f, knobY);
        placeRotary(delayMixSlider, 728.0f, knobY);
        placeRotary(chorusRateSlider, 813.0f, knobY);
        placeRotary(chorusDepthSlider, 874.0f, knobY);
        placeRotary(chorusMixSlider, 930.0f, knobY);
        placeRotary(reverbDecaySlider, 1004.0f, knobY);
        placeRotary(reverbPreDelaySlider, 1050.0f, knobY);
        placeRotary(reverbSizeSlider, 1101.0f, knobY);
        placeRotary(reverbMixSlider, 1146.0f, knobY);

        // Invisible hit targets sit over the six photographed stomp switches.
        // The artwork is 1672 px wide; coordinates below are in that source-image space.
        // The LED indicators are painted separately in paint() from each button's toggle state.
        const float boardScale = (float)getWidth() / 1672.0f;
        const std::array<juce::ToggleButton*, 6> stompButtons {
            &noiseGateButton, &ts9Button, &compressorButton,
            &delayButton, &chorusButton, &reverbButton
        };
        const std::array<float, 6> stompX { 155.0f, 416.0f, 681.0f, 972.0f, 1242.0f, 1514.0f };
        const float stompY = 451.0f;
        const int hitSize = juce::jmax(42, (int)std::round(76.0f * boardScale));
        for (size_t i = 0; i < stompButtons.size(); ++i)
        {
            const int cx = (int)std::round(stompX[i] * boardScale);
            const int cy = 32 + (int)std::round(stompY * boardScale);
            stompButtons[i]->setButtonText({});
            stompButtons[i]->setBounds(cx - hitSize / 2, cy - hitSize / 2, hitSize, hitSize);
            stompButtons[i]->setAlpha(0.01f); // clickable hit area; pedal artwork remains visible
        }

        // Keep utility controls accessible in the narrow gap above the strip.
        const int utilityY = 32 + (int)std::round(boardHeight) + 5;
        delayTapButton.setBounds(juce::Rectangle<int>(getWidth() / 2 - 58, utilityY, 116, 21));
        reverbTypeCombo.setBounds(juce::Rectangle<int>(getWidth() - 112, utilityY, 104, 21));
    }

}


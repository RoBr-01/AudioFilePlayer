#include "PluginEditor.hpp"

#include "PluginProcessor.hpp"

DemoThumbnailComp::DemoThumbnailComp(AudioFormatManager& formatManager,
                                     Slider& slider,
                                     AudioTransportSource& source)
    : transportSource(source),
      zoomSlider(slider),
      thumbnail(1024,
                formatManager,
                thumbnailCache) {  // Increased from 512 to 1024 for better
                                   // multi-channel support
    thumbnail.addChangeListener(this);

    addAndMakeVisible(scrollbar);
    scrollbar.setRangeLimits(visibleRange);
    scrollbar.setAutoHide(false);
    scrollbar.addListener(this);

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);
}

DemoThumbnailComp::~DemoThumbnailComp() {
    scrollbar.removeListener(this);
    thumbnail.removeChangeListener(this);
}

void DemoThumbnailComp::setURL(const URL& url) {
    InputSource* inputSource = nullptr;

#if !JUCE_IOS
    if (url.isLocalFile()) {
        DBG("DemoThumbnailComp: Loading local file: "
            << url.getLocalFile().getFullPathName());
        inputSource = new FileInputSource(url.getLocalFile());
    } else
#endif
    {
        DBG("DemoThumbnailComp: Loading URL: " << url.toString(false));
        if (inputSource == nullptr)
            inputSource = new URLInputSource(url);
    }

    if (inputSource != nullptr) {
        thumbnail.setSource(inputSource);

        DBG("DemoThumbnailComp: Thumbnail total length: "
            << thumbnail.getTotalLength());
        DBG("DemoThumbnailComp: Thumbnail num channels: "
            << thumbnail.getNumChannels());

        Range<double> newRange(0.0, thumbnail.getTotalLength());
        scrollbar.setRangeLimits(newRange);
        setRange(newRange);

        startTimerHz(40);
    } else {
        DBG("DemoThumbnailComp: Failed to create input source!");
    }
}

URL DemoThumbnailComp::getLastDroppedFile() const noexcept {
    return lastFileDropped;
}

void DemoThumbnailComp::setZoomFactor(double amount) {
    if (thumbnail.getTotalLength() > 0) {
        auto newScale = jmax(
            0.001,
            thumbnail.getTotalLength() * (1.0 - jlimit(0.0, 0.99, amount)));
        auto timeAtCentre = xToTime((float)getWidth() / 2.0f);

        setRange(
            {timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5});
    }
}

void DemoThumbnailComp::setRange(Range<double> newRange) {
    visibleRange = newRange;
    scrollbar.setCurrentRange(visibleRange);
    updateCursorPosition();
    repaint();
}

void DemoThumbnailComp::setFollowsTransport(bool shouldFollow) {
    isFollowingTransport = shouldFollow;
}

void DemoThumbnailComp::paint(Graphics& g) {
    g.fillAll(Colours::darkgrey);
    g.setColour(Colours::lightblue);

    if (thumbnail.getTotalLength() > 0.0) {
        auto thumbArea = getLocalBounds();
        thumbArea.removeFromBottom(scrollbar.getHeight() + 4);

        // Draw all channels
        thumbnail.drawChannels(g,
                               thumbArea.reduced(2),
                               visibleRange.getStart(),
                               visibleRange.getEnd(),
                               1.0f);

        // Display channel count in top-left corner
        g.setColour(Colours::white);
        g.setFont(12.0f);
        auto numChannels = thumbnail.getNumChannels();
        g.drawText(
            String(numChannels) + (numChannels == 1 ? " channel" : " channels"),
            thumbArea.getX() + 5,
            thumbArea.getY() + 5,
            150,
            20,
            Justification::centredLeft);
    } else {
        g.setFont(14.0f);
        g.setColour(Colours::white);
        g.drawFittedText("(No audio file selected)",
                         getLocalBounds(),
                         Justification::centred,
                         2);
    }
}

void DemoThumbnailComp::resized() {
    scrollbar.setBounds(getLocalBounds().removeFromBottom(14).reduced(2));
}

void DemoThumbnailComp::changeListenerCallback(ChangeBroadcaster*) {
    // this method is called by the thumbnail when it has changed, so we should
    // repaint it..
    repaint();
}

bool DemoThumbnailComp::isInterestedInFileDrag(const StringArray& /*files*/) {
    return true;
}

void DemoThumbnailComp::filesDropped(const StringArray& files,
                                     int /*x*/,
                                     int /*y*/) {
    lastFileDropped = URL(File(files[0]));
    DBG("DemoThumbnailComp: File dropped: " << lastFileDropped.toString(false));
    sendChangeMessage();
}

void DemoThumbnailComp::mouseDown(const MouseEvent& e) {
    mouseDrag(e);
}

void DemoThumbnailComp::mouseDrag(const MouseEvent& e) {
    if (canMoveTransport())
        transportSource.setPosition(jmax(0.0, xToTime((float)e.x)));
}

void DemoThumbnailComp::mouseUp(const MouseEvent&) {
    //    transportSource.start();
}

void DemoThumbnailComp::mouseWheelMove(const MouseEvent&,
                                       const MouseWheelDetails& wheel) {
    if (thumbnail.getTotalLength() > 0.0) {
        auto newStart = visibleRange.getStart() -
                        wheel.deltaX * (visibleRange.getLength()) / 10.0;
        newStart = jlimit(
            0.0,
            jmax(0.0, thumbnail.getTotalLength() - (visibleRange.getLength())),
            newStart);

        if (canMoveTransport())
            setRange({newStart, newStart + visibleRange.getLength()});

        if (wheel.deltaY != 0.0f)
            zoomSlider.setValue(zoomSlider.getValue() - wheel.deltaY);

        repaint();
    }
}

float DemoThumbnailComp::timeToX(const double time) const {
    if (visibleRange.getLength() <= 0)
        return 0;

    return (float)getWidth() *
           (float)((time - visibleRange.getStart()) / visibleRange.getLength());
}

double DemoThumbnailComp::xToTime(const float x) const {
    return (x / (float)getWidth()) * (visibleRange.getLength()) +
           visibleRange.getStart();
}

bool DemoThumbnailComp::canMoveTransport() const noexcept {
    return !(isFollowingTransport && transportSource.isPlaying());
}

void DemoThumbnailComp::scrollBarMoved(ScrollBar* scrollBarThatHasMoved,
                                       double newRangeStart) {
    if (scrollBarThatHasMoved == &scrollbar) {
        if (!(isFollowingTransport && transportSource.isPlaying())) {
            setRange(visibleRange.movedToStartAt(newRangeStart));
        }
    }
}

void DemoThumbnailComp::timerCallback() {
    if (canMoveTransport()) {
        updateCursorPosition();
    } else {
        setRange(
            visibleRange.movedToStartAt(transportSource.getCurrentPosition() -
                                        (visibleRange.getLength() / 2.0)));
    }
}

void DemoThumbnailComp::updateCursorPosition() {
    currentPositionMarker.setRectangle(
        Rectangle<float>(timeToX(transportSource.getCurrentPosition()) - 0.75f,
                         0,
                         1.5f,
                         (float)(getHeight() - scrollbar.getHeight())));
}

//==============================================================================
AudioFilePlayerAudioProcessorEditor::AudioFilePlayerAudioProcessorEditor(
    AudioFilePlayerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    addAndMakeVisible(zoomLabel);
    zoomLabel.setFont(Font(FontOptions(15.00f, Font::plain)));
    zoomLabel.setJustificationType(Justification::centredRight);
    zoomLabel.setEditable(false, false, false);
    zoomLabel.setColour(TextEditor::textColourId, Colours::black);
    zoomLabel.setColour(TextEditor::backgroundColourId, Colour(0x00000000));

    addAndMakeVisible(followTransportButton);
    followTransportButton.onClick = [this] { updateFollowTransportState(); };

    addAndMakeVisible(chooseFileButton);
    chooseFileButton.setColour(TextButton::buttonColourId, Colour(0xff7f7fff));
    chooseFileButton.setColour(TextButton::textColourOffId, Colours::white);
    chooseFileButton.onClick = [this] { chooseFile(); };

    addAndMakeVisible(filenameLabel);
    filenameLabel.setFont(Font(FontOptions(14.00f, Font::plain)));
    filenameLabel.setJustificationType(Justification::centredLeft);
    filenameLabel.setEditable(false, false, false);
    filenameLabel.setColour(Label::backgroundColourId,
                            Colours::white.withAlpha(0.8f));
    filenameLabel.setColour(Label::outlineColourId, Colours::grey);
    filenameLabel.setColour(Label::textColourId, Colours::black);

    addAndMakeVisible(zoomSlider);
    zoomSlider.setRange(0, 1, 0);
    zoomSlider.onValueChange = [this] {
        thumbnail->setZoomFactor(zoomSlider.getValue());
    };
    zoomSlider.setSkewFactor(2);

    thumbnail.reset(new DemoThumbnailComp(audioProcessor.formatManager,
                                          zoomSlider,
                                          audioProcessor.transportSource));
    addAndMakeVisible(thumbnail.get());
    thumbnail->addChangeListener(this);  // listen for dragAndDrop activities

    startStopButton.setClickingTogglesState(true);
    addAndMakeVisible(startStopButton);
    startStopButton.setColour(TextButton::buttonColourId, Colour(0xff79ed7f));
    startStopButton.setColour(TextButton::textColourOffId, Colours::black);
    startStopButton.onClick = [this] { startOrStop(); };

    startTimerHz(50);
    setOpaque(true);
    setSize(500, 500);
}

AudioFilePlayerAudioProcessorEditor::~AudioFilePlayerAudioProcessorEditor() {
    thumbnail->removeChangeListener(this);
}

void AudioFilePlayerAudioProcessorEditor::paint(Graphics& g) {
    g.fillAll(getUIColourIfAvailable(
        LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
}

void AudioFilePlayerAudioProcessorEditor::resized() {
    auto r = getLocalBounds().reduced(4);

    auto controls = r.removeFromBottom(120);

    // File selection area
    auto fileArea = controls.removeFromTop(30);
    chooseFileButton.setBounds(fileArea.removeFromLeft(100));
    fileArea.removeFromLeft(4);  // spacing
    filenameLabel.setBounds(fileArea);

    controls.removeFromTop(4);  // spacing

    auto zoom = controls.removeFromTop(25);
    zoomLabel.setBounds(zoom.removeFromLeft(50));
    zoomSlider.setBounds(zoom);

    followTransportButton.setBounds(controls.removeFromTop(25));
    startStopButton.setBounds(controls);

    r.removeFromBottom(6);

    thumbnail->setBounds(r);
}

//==============================================================================
void AudioFilePlayerAudioProcessorEditor::startOrStop() {
    auto shouldPlay = startStopButton.getToggleState();
    if (shouldPlay) {
        audioProcessor.transportSource.start();
    } else {
        audioProcessor.transportSource.stop();
    }
}

void AudioFilePlayerAudioProcessorEditor::updateFollowTransportState() {
    thumbnail->setFollowsTransport(followTransportButton.getToggleState());
}

void AudioFilePlayerAudioProcessorEditor::chooseFile() {
    fileChooser.reset(new FileChooser(
        "Choose an audio file...",
        File::getSpecialLocation(File::userHomeDirectory),
        audioProcessor.formatManager.getWildcardForAllFormats()));

    auto chooserFlags =
        FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            DBG("Editor: File chosen: " << file.getFullPathName());
            filenameLabel.setText(file.getFileName(), dontSendNotification);
            audioProcessor.transportSourceCreator.requestTransportForURL(
                URL(file));
        }
    });
}

void AudioFilePlayerAudioProcessorEditor::changeListenerCallback(
    ChangeBroadcaster* source) {
    if (source == thumbnail.get()) {
        auto droppedFile = thumbnail->getLastDroppedFile();
        DBG("Editor: changeListenerCallback - file dropped");
        if (droppedFile.getLocalFile().existsAsFile()) {
            filenameLabel.setText(droppedFile.getLocalFile().getFileName(),
                                  dontSendNotification);
            DBG("Editor: Requesting transport for dropped file: "
                << droppedFile.toString(false));
        }
        audioProcessor.transportSourceCreator.requestTransportForURL(
            droppedFile);
    }
}

void AudioFilePlayerAudioProcessorEditor::timerCallback() {
    if (audioProcessor.sourceHasChanged.compareAndSetBool(true, false)) {
        auto& src = audioProcessor.activeSource;
        bool hasValidSource = src.get() != nullptr;

        DBG("Editor: timerCallback - sourceHasChanged detected, "
            "hasValidSource: " +
            String(hasValidSource ? "true" : "false"));

        if (hasValidSource) {
            if (src.get() != activeSource.get()) {
                DBG("Editor: New source detected!");

                AudioFilePlayerAudioProcessor::refreshCurrentFileInAPVTS(
                    audioProcessor.apvts, src->currentAudioFile);
                activeSource = src;

                zoomSlider.setValue(0, dontSendNotification);

                DBG("Editor: Setting URL on thumbnail: " +
                    activeSource->currentAudioFile.toString(false));
                thumbnail->setURL(activeSource->currentAudioFile);

                if (activeSource->currentAudioFile.isLocalFile()) {
                    filenameLabel.setText(
                        activeSource->currentAudioFile.getLocalFile()
                            .getFileName(),
                        dontSendNotification);
                }
            }
        }
    }

    // Check transport length and update button state
    bool canPlay = audioProcessor.transportSource.getTotalLength() > 0;
    DBG("Editor: transportSource.getTotalLength() = " +
        String(audioProcessor.transportSource.getTotalLength()));

    startStopButton.setEnabled(canPlay);

    auto isPlaying = audioProcessor.transportSource.isPlaying();
    if (canPlay) {
        startStopButton.setButtonText(!isPlaying ? "Start" : "Stop");
    } else {
        startStopButton.setButtonText("Load an audio file first...");
    }

    startStopButton.setToggleState(isPlaying, dontSendNotification);
}
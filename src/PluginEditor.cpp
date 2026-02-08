#include "PluginEditor.hpp"
#include "PluginProcessor.hpp"

//==============================================================================
// DemoThumbnailComp Implementation
DemoThumbnailComp::DemoThumbnailComp(AudioFormatManager& formatManager,
                                     Slider& slider,
                                     AudioTransportSource& source)
    : transportSource(source),
      zoomSlider(slider),
      thumbnail(1024, formatManager, thumbnailCache)
{
    addAndMakeVisible(scrollbar);
    scrollbar.setRangeLimits(visibleRange);
    scrollbar.setAutoHide(false);
    scrollbar.addListener(this);

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);

    thumbnail.addChangeListener(this);
    startTimerHz(40);

    setOpaque(true);
}

DemoThumbnailComp::~DemoThumbnailComp()
{
    scrollbar.removeListener(this);
    thumbnail.removeChangeListener(this);
}

void DemoThumbnailComp::setURL(const URL& url)
{
    std::unique_ptr<InputSource> inputSource;

#if !JUCE_IOS
    if (url.isLocalFile())
        inputSource.reset(new FileInputSource(url.getLocalFile()));
    else
#endif
        inputSource.reset(new URLInputSource(url));

    if (inputSource)
    {
        thumbnail.setSource(inputSource.release());

        Range<double> newRange(0.0, thumbnail.getTotalLength());
        scrollbar.setRangeLimits(newRange);
        setRange(newRange);

        waveformNeedsUpdate = true;
        repaint();
    }
}

void DemoThumbnailComp::setZoomFactor(double amount)
{
    if (thumbnail.getTotalLength() > 0)
    {
        auto newScale = jmax(
            0.001,
            thumbnail.getTotalLength() * (1.0 - jlimit(0.0, 0.99, amount)));
        auto timeAtCentre = xToTime((float)getWidth() / 2.0f);

        setRange({timeAtCentre - newScale * 0.5,
                  timeAtCentre + newScale * 0.5});
    }
}

void DemoThumbnailComp::setRange(Range<double> newRange)
{
    visibleRange = newRange;
    scrollbar.setCurrentRange(visibleRange);

    waveformNeedsUpdate = true; // mark waveform dirty
    updateCursorPosition();
    repaint();
}

void DemoThumbnailComp::setFollowsTransport(bool shouldFollow)
{
    isFollowingTransport = shouldFollow;
}

void DemoThumbnailComp::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);

    // Draw waveform image if cached
    if (!waveformCache.isValid() || waveformNeedsUpdate)
        updateWaveformImage();

    if (waveformCache.isValid())
        g.drawImageAt(waveformCache, 0, 0);

    // Draw playhead marker
    g.setColour(Colours::white);
    auto x = (int)timeToX(transportSource.getCurrentPosition());
    g.drawLine((float)x, 0.0f, (float)x, (float)getHeight() - scrollbar.getHeight(), 2.0f);
}

void DemoThumbnailComp::updateWaveformImage()
{
    if (thumbnail.getTotalLength() <= 0 || getWidth() <= 0 || getHeight() <= 0)
        return;

    waveformCache = Image(Image::ARGB, getWidth(), getHeight(), true);
    Graphics g(waveformCache);
    g.fillAll(Colours::darkgrey);

    auto thumbArea = getLocalBounds();
    thumbArea.removeFromBottom(scrollbar.getHeight() + 4);

    g.setColour(Colours::lightblue);
    thumbnail.drawChannels(g,
                           thumbArea.reduced(2),
                           visibleRange.getStart(),
                           visibleRange.getEnd(),
                           1.0f);

    waveformNeedsUpdate = false;
}

void DemoThumbnailComp::resized()
{
    scrollbar.setBounds(getLocalBounds().removeFromBottom(14).reduced(2));
    waveformNeedsUpdate = true;
    repaint();
}

void DemoThumbnailComp::changeListenerCallback(ChangeBroadcaster*) { waveformNeedsUpdate = true; repaint(); }
bool DemoThumbnailComp::isInterestedInFileDrag(const StringArray&) { return true; }
void DemoThumbnailComp::filesDropped(const StringArray& files, int, int)
{
    lastFileDropped = URL(File(files[0]));
    sendChangeMessage();
}

void DemoThumbnailComp::mouseDown(const MouseEvent& e) { mouseDrag(e); }
void DemoThumbnailComp::mouseDrag(const MouseEvent& e)
{
    if (canMoveTransport())
        transportSource.setPosition(jmax(0.0, xToTime((float)e.x)));
}
void DemoThumbnailComp::mouseUp(const MouseEvent&) {}
void DemoThumbnailComp::mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (thumbnail.getTotalLength() <= 0)
        return;

    auto newStart = visibleRange.getStart() -
                    wheel.deltaX * visibleRange.getLength() / 10.0;
    newStart = jlimit(0.0,
                      jmax(0.0, thumbnail.getTotalLength() - visibleRange.getLength()),
                      newStart);

    if (canMoveTransport())
        setRange({newStart, newStart + visibleRange.getLength()});

    if (wheel.deltaY != 0.0f)
        zoomSlider.setValue(zoomSlider.getValue() - wheel.deltaY);

    repaint();
}

float DemoThumbnailComp::timeToX(const double time) const
{
    if (visibleRange.getLength() <= 0)
        return 0.0f;
    return (float)getWidth() *
           (float)((time - visibleRange.getStart()) / visibleRange.getLength());
}

double DemoThumbnailComp::xToTime(const float x) const
{
    return (x / (float)getWidth()) * visibleRange.getLength() +
           visibleRange.getStart();
}

bool DemoThumbnailComp::canMoveTransport() const noexcept
{
    return !(isFollowingTransport && transportSource.isPlaying());
}

void DemoThumbnailComp::scrollBarMoved(ScrollBar*, double newRangeStart)
{
    if (!(isFollowingTransport && transportSource.isPlaying()))
        setRange(visibleRange.movedToStartAt(newRangeStart));
}

void DemoThumbnailComp::timerCallback()
{
    if (canMoveTransport())
        updateCursorPosition();
    else
        setRange(
            visibleRange.movedToStartAt(transportSource.getCurrentPosition() -
                                        visibleRange.getLength() / 2.0));
}

void DemoThumbnailComp::updateCursorPosition()
{
    currentPositionMarker.setBounds(
        (int)timeToX(transportSource.getCurrentPosition()) - 1,
        0,
        2,
        getHeight() - scrollbar.getHeight());
}

//==============================================================================
// AudioFilePlayerAudioProcessorEditor Implementation
AudioFilePlayerAudioProcessorEditor::AudioFilePlayerAudioProcessorEditor(
    AudioFilePlayerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setResizeLimits(400, 400, 1200, 1000);

    // Restore size from APVTS if available
    int w = (int)audioProcessor.apvts.getRawParameterValue("windowWidth")->load();
    int h = (int)audioProcessor.apvts.getRawParameterValue("windowHeight")->load();
    setSize(w, h);

    addAndMakeVisible(zoomLabel);
    zoomLabel.setFont(Font(15.0f, Font::plain));
    zoomLabel.setJustificationType(Justification::centredRight);

    addAndMakeVisible(followTransportButton);
    followTransportButton.onClick = [this] { updateFollowTransportState(); };

    addAndMakeVisible(chooseFileButton);
    chooseFileButton.setColour(TextButton::buttonColourId, Colour(0xff7f7fff));
    chooseFileButton.setColour(TextButton::textColourOffId, Colours::white);
    chooseFileButton.onClick = [this] { chooseFile(); };

    addAndMakeVisible(filenameLabel);
    filenameLabel.setColour(Label::backgroundColourId, Colours::white.withAlpha(0.8f));
    filenameLabel.setColour(Label::outlineColourId, Colours::grey);
    filenameLabel.setColour(Label::textColourId, Colours::black);

    addAndMakeVisible(zoomSlider);
    zoomSlider.setRange(0, 1, 0);
    zoomSlider.setSkewFactor(2.0);
    zoomSlider.onValueChange = [this] {
        if (thumbnail)
            thumbnail->setZoomFactor(zoomSlider.getValue());
    };

    thumbnail.reset(new DemoThumbnailComp(audioProcessor.formatManager,
                                          zoomSlider,
                                          audioProcessor.transportSource));
    addAndMakeVisible(thumbnail.get());
    thumbnail->addChangeListener(this);

    startStopButton.setClickingTogglesState(true);
    addAndMakeVisible(startStopButton);
    startStopButton.setColour(TextButton::buttonColourId, Colour(0xff79ed7f));
    startStopButton.setColour(TextButton::textColourOffId, Colours::black);
    startStopButton.onClick = [this] { startOrStop(); };

    initializeWithExistingState();
    startTimerHz(50);
    setOpaque(true);
}

AudioFilePlayerAudioProcessorEditor::~AudioFilePlayerAudioProcessorEditor()
{
    if (thumbnail)
        thumbnail->removeChangeListener(this);

    // Save size to APVTS
    audioProcessor.apvts.getParameterAsValue("windowWidth") = getWidth();
    audioProcessor.apvts.getParameterAsValue("windowHeight") = getHeight();
}

void AudioFilePlayerAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(getUIColourIfAvailable(
        LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
}

void AudioFilePlayerAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(4);
    auto controls = r.removeFromBottom(140);

    auto fileArea = controls.removeFromTop(30);
    chooseFileButton.setBounds(fileArea.removeFromLeft(100));
    fileArea.removeFromLeft(4);
    filenameLabel.setBounds(fileArea);

    controls.removeFromTop(4);
    auto zoomArea = controls.removeFromTop(25);
    zoomLabel.setBounds(zoomArea.removeFromLeft(50));
    zoomSlider.setBounds(zoomArea);

    followTransportButton.setBounds(controls.removeFromTop(25));
    startStopButton.setBounds(controls);

    if (thumbnail) {
        thumbnail->setBounds(r);
        thumbnail->repaint();
    }

    // save size to APVTS
    audioProcessor.apvts.getParameterAsValue("windowWidth") = getWidth();
    audioProcessor.apvts.getParameterAsValue("windowHeight") = getHeight();
}

//==============================================================================
// Button callbacks
void AudioFilePlayerAudioProcessorEditor::startOrStop()
{
    auto shouldPlay = startStopButton.getToggleState();
    if (shouldPlay)
        audioProcessor.transportSource.start();
    else
        audioProcessor.transportSource.stop();
}

void AudioFilePlayerAudioProcessorEditor::updateFollowTransportState()
{
    thumbnail->setFollowsTransport(followTransportButton.getToggleState());
}

void AudioFilePlayerAudioProcessorEditor::chooseFile()
{
    fileChooser.reset(new FileChooser(
        "Choose an audio file...",
        File::getSpecialLocation(File::userHomeDirectory),
        audioProcessor.formatManager.getWildcardForAllFormats()));

    auto chooserFlags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            filenameLabel.setText(file.getFileName(), dontSendNotification);
            audioProcessor.transportSourceCreator.requestTransportForURL(URL(file));
        }
    });
}

//==============================================================================
// ChangeListener
void AudioFilePlayerAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == thumbnail.get()) {
        auto droppedFile = thumbnail->getLastDroppedFile();
        if (droppedFile.getLocalFile().existsAsFile()) {
            filenameLabel.setText(droppedFile.getLocalFile().getFileName(), dontSendNotification);
        }
        audioProcessor.transportSourceCreator.requestTransportForURL(droppedFile);
    }
}

//==============================================================================
// Initialize state
void AudioFilePlayerAudioProcessorEditor::initializeWithExistingState()
{
    if (audioProcessor.activeSource != nullptr) {
        auto& src = audioProcessor.activeSource;
        activeSource = src;

        thumbnail->setURL(src->currentAudioFile);

        if (src->currentAudioFile.isLocalFile()) {
            filenameLabel.setText(src->currentAudioFile.getLocalFile().getFileName(), dontSendNotification);
        }

        bool canPlay = audioProcessor.transportSource.getTotalLength() > 0;
        startStopButton.setEnabled(canPlay);

        auto isPlaying = audioProcessor.transportSource.isPlaying();
        if (canPlay)
            startStopButton.setButtonText(!isPlaying ? "Start" : "Stop");
        else
            startStopButton.setButtonText("Load an audio file first...");

        startStopButton.setToggleState(isPlaying, dontSendNotification);
    }
}

//==============================================================================
// Timer callback
void AudioFilePlayerAudioProcessorEditor::timerCallback()
{
    // Handle any source changes
    if (audioProcessor.sourceHasChanged.exchange(false)) {
        auto& src = audioProcessor.activeSource;
        if (src != nullptr && src.get() != activeSource.get()) {
            activeSource = src;

            AudioFilePlayerAudioProcessor::refreshCurrentFileInAPVTS(
                audioProcessor.apvts, src->currentAudioFile);

            zoomSlider.setValue(0, dontSendNotification);
            thumbnail->setURL(src->currentAudioFile);

            if (src->currentAudioFile.isLocalFile()) {
                filenameLabel.setText(src->currentAudioFile.getLocalFile().getFileName(), dontSendNotification);
            }
        }
    }

    // Update start/stop button
    bool canPlay = audioProcessor.transportSource.getTotalLength() > 0;
    startStopButton.setEnabled(canPlay);

    auto isPlaying = audioProcessor.transportSource.isPlaying();
    if (canPlay)
        startStopButton.setButtonText(!isPlaying ? "Start" : "Stop");
    else
        startStopButton.setButtonText("Load an audio file first...");

    startStopButton.setToggleState(isPlaying, dontSendNotification);
}

URL DemoThumbnailComp::getLastDroppedFile() const noexcept
{
    return lastFileDropped;
}
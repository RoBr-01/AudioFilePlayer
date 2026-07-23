#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.hpp"

// Helper to get consistent UI colours
inline juce::Colour getUIColourIfAvailable(
    juce::LookAndFeel_V4::ColourScheme::UIColour uiColour,
    juce::Colour fallback = juce::Colour(0xff4d4d4d)) noexcept {
    if (auto* v4 = dynamic_cast<juce::LookAndFeel_V4*>(
            &juce::LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour(uiColour);

    return fallback;
}

//==============================================================================
// Thumbnail component for waveform display
class DemoThumbnailComp : public juce::Component,
                          public juce::ChangeListener,
                          public juce::FileDragAndDropTarget,
                          public juce::ChangeBroadcaster,
                          private juce::ScrollBar::Listener,
                          private juce::Timer {
   public:
    DemoThumbnailComp(juce::AudioFormatManager& formatManager,
                      juce::AudioThumbnailCache& cacheToUse,
                      juce::Slider& slider,
                      juce::AudioTransportSource& source);
    ~DemoThumbnailComp() override;

    void setURL(const juce::URL& url);
    juce::URL getLastDroppedFile() const noexcept;
    void setZoomFactor(double amount);
    void setRange(juce::Range<double> newRange);
    void setFollowsTransport(bool shouldFollow);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    bool isInterestedInFileDrag(const juce::StringArray& /*files*/) override;
    void filesDropped(const juce::StringArray& files,
                      int /*x*/,
                      int /*y*/) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails& wheel) override;

   private:
    juce::AudioTransportSource& transportSource;
    juce::Slider& zoomSlider;
    juce::ScrollBar scrollbar{false};

    juce::AudioThumbnailCache thumbnailCache{5};
    juce::AudioThumbnail thumbnail;
    juce::Range<double> visibleRange;
    bool isFollowingTransport = false;
    juce::URL lastFileDropped;

    juce::DrawableRectangle currentPositionMarker;

    juce::Image waveformCache;
    bool waveformNeedsUpdate = true;

    float timeToX(const double time) const;
    double xToTime(const float x) const;
    bool canMoveTransport() const noexcept;

    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved,
                        double newRangeStart) override;
    void timerCallback() override;
    void updateCursorPosition();
    void updateWaveformImage();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DemoThumbnailComp)
};

class AudioFilePlayerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::ChangeListener,
                                            public juce::Timer {
   public:
    AudioFilePlayerAudioProcessorEditor(AudioFilePlayerAudioProcessor& p);
    ~AudioFilePlayerAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

   private:
    AudioFilePlayerAudioProcessor& audioProcessor;

    std::unique_ptr<DemoThumbnailComp> thumbnail;
    juce::Label zoomLabel{{}, "zoom:"};
    juce::Slider zoomSlider{juce::Slider::LinearHorizontal,
                            juce::Slider::NoTextBox};
    juce::ToggleButton followTransportButton{"Follow Transport"};
    juce::TextButton startStopButton{"Load an audio file first..."};
    juce::TextButton chooseFileButton{"Choose File..."};
    juce::Label filenameLabel{{}, "No file selected"};

    ReferencedTransportSourceData::Ptr activeSource;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void startOrStop();
    void updateFollowTransportState();
    void chooseFile();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void initializeWithExistingState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        AudioFilePlayerAudioProcessorEditor)
};
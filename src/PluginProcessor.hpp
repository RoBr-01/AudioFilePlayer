#pragma once

#include <JuceHeader.h>

#include "DataStructures.hpp"

//==============================================================================
struct ReferencedTransportSourceData : juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<ReferencedTransportSourceData>;

    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;
    juce::URL currentAudioFile;
    double audioFileSourceSampleRate{0};
};

struct AudioFormatReaderSourceCreator : juce::Thread {
    AudioFormatReaderSourceCreator(
        LatestValue<ReferencedTransportSourceData::Ptr>& pendingSourceOut,
        AudioFormatManager& afm)
        : juce::Thread("TransportSourceCreator"),
          pendingSource(pendingSourceOut),
          formatManager(afm) {
        startThread();
    }

    ~AudioFormatReaderSourceCreator() override {
        stopThread(500);
    }

    void run() override {
        DBG("AudioFormatReaderSourceCreator thread started!");

        while (!threadShouldExit()) {
            juce::URL audioURL;
            if (pendingURL.getIfNew(audioURL)) {
                DBG("AudioFormatReaderSourceCreator: Loading URL: " +
                    audioURL.toString(false));

                std::unique_ptr<AudioFormatReader> reader;

                if (audioURL.isLocalFile()) {
                    reader.reset(formatManager.createReaderFor(
                        audioURL.getLocalFile()));
                } else {
                    auto options = URL::InputStreamOptions(
                        URL::ParameterHandling::inAddress);
                    reader.reset(formatManager.createReaderFor(
                        audioURL.createInputStream(options)));
                }

                if (reader != nullptr) {
                    DBG("Loaded audio file: " + audioURL.toString(false));

                    using RTS = ReferencedTransportSourceData;
                    RTS::Ptr rts = new ReferencedTransportSourceData();

                    rts->audioFileSourceSampleRate = reader->sampleRate;

                    rts->currentAudioFileSource.reset(
                        new AudioFormatReaderSource(reader.release(), true));

                    rts->currentAudioFile = audioURL;

                    pendingSource.set(rts);
                }
            }

            wait(5);
        }

        DBG("AudioFormatReaderSourceCreator thread exiting!");
    }

    // Safe to call from any thread. If called again before the previous
    // request has been picked up by run(), the previous request is simply
    // superseded and never loaded -- only the latest request matters.
    bool requestTransportForURL(juce::URL url) {
        pendingURL.set(url);
        notify();  // Wake up the thread
        return true;
    }

   private:
    LatestValue<juce::URL> pendingURL;
    LatestValue<ReferencedTransportSourceData::Ptr>& pendingSource;

    AudioFormatManager& formatManager;
};

class AudioFilePlayerAudioProcessor : public juce::AudioProcessor,
                                      private juce::Timer {
   public:
    //==============================================================================
    AudioFilePlayerAudioProcessor();
    ~AudioFilePlayerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();

    template <typename SourceType>
    static void refreshCurrentFileInAPVTS(
        juce::AudioProcessorValueTreeState& apvts,
        SourceType& currentAudioFile) {
        auto file = currentAudioFile.getLocalFile();
        if (file.existsAsFile()) {
            apvts.state.setProperty(
                "CurrentFile", file.getFullPathName(), nullptr);
        }
    }

   public:
    // Set (message thread only, via timerCallback) whenever a new source is
    // swapped in. The editor polls/consumes this to refresh its UI.
    juce::Atomic<bool> sourceHasChanged{false};

    juce::AudioProcessorValueTreeState apvts{
        *this, nullptr, "Properties", createParameterLayout()};

    // Message-thread-owned. Only ever read/written from timerCallback() and
    // from the editor (also message thread). Never touched from processBlock.
    ReferencedTransportSourceData::Ptr activeSource;

   private:
    TimeSliceThread directoryScannerBackgroundThread{"audio file preview"};

    LatestValue<ReferencedTransportSourceData::Ptr> pendingSource;

    // Runs on the message thread. Picks up the latest pending source and
    // performs the (expensive, allocating, locking) transportSource.
    // setSource() call here instead of on the audio thread.
    void timerCallback() override;
    void checkForNewSource();

   public:
    AudioFormatManager formatManager;
    AudioTransportSource transportSource;

    AudioFormatReaderSourceCreator transportSourceCreator{
        pendingSource, formatManager};

    AudioThumbnailCache thumbnailCache{5};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilePlayerAudioProcessor)
};
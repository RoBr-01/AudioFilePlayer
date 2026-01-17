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
        Fifo<ReferencedTransportSourceData::Ptr>& fifo,
        ReleasePool<ReferencedTransportSourceData>& pool,
        AudioFormatManager& afm)
        : juce::Thread("TransportSourceCreator"),
          transportSourceFifo(fifo),
          releasePool(pool),
          formatManager(afm) {
        startThread();
    }

    ~AudioFormatReaderSourceCreator() override {
        stopThread(500);
    }

    void run() override {
        DBG("AudioFormatReaderSourceCreator thread started!");

        while (!threadShouldExit()) {
            // Check if there are URLs waiting to be processed
            if (urlFifo.getNumAvailableForReading() > 0) {
                DBG("AudioFormatReaderSourceCreator: URLs available for "
                    "reading: " +
                    String(urlFifo.getNumAvailableForReading()));

                juce::URL audioURL;
                while (urlFifo.pull(audioURL)) {
                    DBG("AudioFormatReaderSourceCreator: Pulled URL: " +
                        audioURL.toString(false));

                    std::unique_ptr<AudioFormatReader> reader;

                    if (audioURL.isLocalFile()) {
                        DBG("AudioFormatReaderSourceCreator: Creating reader "
                            "for local file...");
                        reader.reset(formatManager.createReaderFor(
                            audioURL.getLocalFile()));
                    } else {
                        DBG("AudioFormatReaderSourceCreator: Creating reader "
                            "for remote URL...");
                        auto options = URL::InputStreamOptions(
                            URL::ParameterHandling::inAddress);
                        reader.reset(formatManager.createReaderFor(
                            audioURL.createInputStream(options)));
                    }

                    if (reader != nullptr) {
                        DBG("Loaded audio file: " + audioURL.toString(false));
                        DBG("Channels: " + String(reader->numChannels));
                        DBG("Sample Rate: " + String(reader->sampleRate));
                        DBG("Length: " + String(reader->lengthInSamples));

                        using RTS = ReferencedTransportSourceData;
                        RTS::Ptr rts = new ReferencedTransportSourceData();

                        rts->audioFileSourceSampleRate = reader->sampleRate;
                        rts->currentAudioFileSource.reset(
                            new AudioFormatReaderSource(reader.release(),
                                                        true));
                        rts->currentAudioFile = audioURL;

                        releasePool.add(rts);
                        DBG("AudioFormatReaderSourceCreator: Pushing to "
                            "transport source FIFO");
                        transportSourceFifo.push(rts);
                    } else {
                        DBG("Failed to create reader for: " +
                            audioURL.toString(false));
                    }
                }
            }

            wait(5);
        }

        DBG("AudioFormatReaderSourceCreator thread exiting!");
    }

    bool requestTransportForURL(juce::URL url) {
        DBG("AudioFormatReaderSourceCreator::requestTransportForURL called "
            "with: " +
            url.toString(false));
        if (urlFifo.push(url)) {
            DBG("AudioFormatReaderSourceCreator: URL pushed to FIFO "
                "successfully");
            notify();  // Wake up the thread
            return true;
        }

        DBG("AudioFormatReaderSourceCreator: Failed to push URL to FIFO!");
        return false;
    }

   private:
    Fifo<juce::URL> urlFifo;
    Fifo<ReferencedTransportSourceData::Ptr>& transportSourceFifo;
    ReleasePool<ReferencedTransportSourceData>& releasePool;

    juce::Atomic<bool> urlNeedsProcessingFlag{false};

    AudioFormatManager& formatManager;
};

class AudioFilePlayerAudioProcessor : public juce::AudioProcessor {
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
    // AudioFormatManager formatManager;
    // AudioTransportSource transportSource;

    // AudioFormatReaderSourceCreator transportSourceCreator{
    //     fifo, pool, formatManager};

    juce::Atomic<bool> sourceHasChanged{false};

    juce::AudioProcessorValueTreeState apvts{
        *this, nullptr, "Properties", createParameterLayout()};

    ReferencedTransportSourceData::Ptr activeSource;

   private:
    juce::Atomic<bool> transportIsPlaying{false};

    TimeSliceThread directoryScannerBackgroundThread{"audio file preview"};

    Fifo<ReferencedTransportSourceData::Ptr> fifo;
    ReleasePool<ReferencedTransportSourceData> pool;

   public:
    AudioFormatManager formatManager;
    AudioTransportSource transportSource;

    AudioFormatReaderSourceCreator transportSourceCreator{
        fifo, pool, formatManager};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilePlayerAudioProcessor)
};
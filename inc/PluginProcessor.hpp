#pragma once

#include <JuceHeader.h>

#include "FIFO.hpp"

template <typename ReferenceCountedType>
struct ReleasePool : juce::Timer {
    ReleasePool() {
        deletionPool.reserve(5000);

        startTimer(1 * 1000);
    }

    ~ReleasePool() override {
        stopTimer();
    }

    using Ptr = typename ReferenceCountedType::Ptr;

    void add(Ptr ptr) {
        if (ptr == nullptr)
            return;

        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            addIfNotAlreadyThere(ptr);
        } else {
            if (fifo.push(ptr)) {
                successfullyAdded.set(true);
            } else {
                jassertfalse;
            }
        }
    }

    void timerCallback() override {
        if (successfullyAdded.compareAndSetBool(false, true)) {
            Ptr ptr;
            while (fifo.pull(ptr)) {
                addIfNotAlreadyThere(ptr);
                ptr = nullptr;
            }
        }

        deletionPool.erase(
            std::remove_if(
                deletionPool.begin(),
                deletionPool.end(),
                [](const auto& ptr) { return ptr->getReferenceCount() <= 1; }),
            deletionPool.end());
    }

   private:
    Fifo<Ptr, 512> fifo;
    std::vector<Ptr> deletionPool;
    juce::Atomic<bool> successfullyAdded{false};

    void addIfNotAlreadyThere(Ptr ptr) {
        auto found = std::find_if(
            deletionPool.begin(), deletionPool.end(), [ptr](const auto& elem) {
                return elem.get() == ptr.get();
            });

        if (found == deletionPool.end())
            deletionPool.push_back(ptr);
    }
};
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
        // create a referenced transport source if there is a new URL to load
        while (!threadShouldExit()) {
            if (urlNeedsProcessingFlag.compareAndSetBool(false, true)) {
                juce::URL audioURL;
                while (urlFifo.getNumAvailableForReading() > 0) {
                    if (urlFifo.pull(audioURL)) {
                        // create a new referenced transport source for this
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
                            using RTS = ReferencedTransportSourceData;
                            RTS::Ptr rts = new ReferencedTransportSourceData();

                            rts->audioFileSourceSampleRate = reader->sampleRate;

                            rts->currentAudioFileSource.reset(
                                new AudioFormatReaderSource(reader.release(),
                                                            true));
                            rts->currentAudioFile = audioURL;

                            // add it to the release pool
                            releasePool.add(rts);
                            // add it to the transportSourceFifo
                            transportSourceFifo.push(rts);
                        }
                    }
                }
            }

            wait(5);
        }
    }

    bool requestTransportForURL(juce::URL url) {
        if (urlFifo.push(url)) {
            urlNeedsProcessingFlag.set(true);
            return true;
        }

        return false;
    }

   private:
    Fifo<juce::URL> urlFifo;
    Fifo<ReferencedTransportSourceData::Ptr>& transportSourceFifo;
    ReleasePool<ReferencedTransportSourceData>& releasePool;

    // TimeSliceThread& directoryScannerBackgroundThread;

    juce::Atomic<bool> urlNeedsProcessingFlag{false};

    AudioFormatManager& formatManager;
};
/**
 */
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
    AudioFormatManager formatManager;
    AudioTransportSource transportSource;

    AudioFormatReaderSourceCreator transportSourceCreator{
        fifo, pool, formatManager};

    juce::Atomic<bool> sourceHasChanged{false};

    juce::AudioProcessorValueTreeState apvts{
        *this, nullptr, "Properties", createParameterLayout()};

    ReferencedTransportSourceData::Ptr activeSource;

   private:
    juce::Atomic<bool> transportIsPlaying{false};

    TimeSliceThread directoryScannerBackgroundThread{"audio file preview"};

    Fifo<ReferencedTransportSourceData::Ptr> fifo;
    ReleasePool<ReferencedTransportSourceData> pool;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilePlayerAudioProcessor)
};

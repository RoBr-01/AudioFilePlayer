#include "PluginProcessor.hpp"

#include "PluginEditor.hpp"

//==============================================================================
AudioFilePlayerAudioProcessor::AudioFilePlayerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
#endif
              .withOutput(
                  "Output", juce::AudioChannelSet::discreteChannels(16), true)
#endif
      )
#endif
{
    formatManager.registerBasicFormats();
    directoryScannerBackgroundThread.startThread(
        juce::Thread::Priority::normal);
}

AudioFilePlayerAudioProcessor::~AudioFilePlayerAudioProcessor() {
    DBG("Destructor: Stopping transport source...");
    transportSource.setSource(nullptr);
    transportSource.releaseResources();

    DBG("Destructor: Stopping background thread...");
    directoryScannerBackgroundThread.stopThread(1000);

    DBG("Destructor: Clearing active source...");
    activeSource = nullptr;

    DBG("Destructor: Complete");
}

//==============================================================================
const juce::String AudioFilePlayerAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool AudioFilePlayerAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioFilePlayerAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioFilePlayerAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioFilePlayerAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int AudioFilePlayerAudioProcessor::getNumPrograms() {
    return 1;  // NB: some hosts don't cope very well if you tell them there are
               // 0 programs, so this should be at least 1, even if you're not
               // really implementing programs.
}

int AudioFilePlayerAudioProcessor::getCurrentProgram() {
    return 0;
}

void AudioFilePlayerAudioProcessor::setCurrentProgram(int index) {}

const juce::String AudioFilePlayerAudioProcessor::getProgramName(int index) {
    return {};
}

void AudioFilePlayerAudioProcessor::changeProgramName(
    int index, const juce::String& newName) {}

void AudioFilePlayerAudioProcessor::prepareToPlay(double sampleRate,
                                                  int samplesPerBlock) {
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);

    DBG("prepareToPlay called:");
    DBG("  Sample Rate: " << sampleRate);
    DBG("  Samples Per Block: " << samplesPerBlock);
    DBG("  Total Num Input Channels: " << getTotalNumInputChannels());
    DBG("  Total Num Output Channels: " << getTotalNumOutputChannels());
}

void AudioFilePlayerAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    transportSource.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioFilePlayerAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // Support 1 to 16 channels
    auto outputChannels = layouts.getMainOutputChannelSet().size();

    if (outputChannels < 1 || outputChannels > 16)
        return false;

#if !JucePlugin_IsSynth
    // Input and output channel counts must match
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void AudioFilePlayerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Check if there's a new source available
    ReferencedTransportSourceData::Ptr ptr = nullptr;

    // Pull all available sources, keeping only the most recent one
    ReferencedTransportSourceData::Ptr temp;
    while (fifo.pull(temp)) {
        ptr = temp;  // Keep the most recent
    }

    if (ptr != nullptr) {
        DBG("processBlock: Got new source from FIFO");

        pool.add(activeSource);
        activeSource = ptr;
        transportSource.stop();

        // Get channel count from reader
        auto* reader =
            activeSource->currentAudioFileSource->getAudioFormatReader();
        jassert(reader != nullptr);

        int numChannels = reader->numChannels;
        DBG("Setting transport source with " + String(numChannels) +
            " channels");

        transportSource.setSource(activeSource->currentAudioFileSource.get(),
                                  32768,
                                  &directoryScannerBackgroundThread,
                                  activeSource->audioFileSourceSampleRate,
                                  numChannels);

        // Restore saved playback position if available
        if (apvts.state.hasProperty("PlaybackPosition")) {
            double savedPosition =
                apvts.state.getProperty("PlaybackPosition", 0.0);
            transportSource.setPosition(savedPosition);
            DBG("Restored playback position: " + String(savedPosition) +
                " seconds");

            // Clear it so we don't restore again if another file is loaded
            apvts.state.removeProperty("PlaybackPosition", nullptr);
        }

        sourceHasChanged.set(true);

        DBG("Active source changed in processBlock");
        DBG("  Transport total length: " +
            String(transportSource.getTotalLength()));
    }

    // Only process if we have an active source
    if (activeSource != nullptr && transportSource.getTotalLength() > 0) {
        AudioSourceChannelInfo asci(&buffer, 0, buffer.getNumSamples());
        transportSource.getNextAudioBlock(asci);

        // Debug logging
        static int processCounter = 0;
        if (++processCounter % 100 == 0 && transportSource.isPlaying()) {
            DBG("=== processBlock Analysis ===");
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                float rms = buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
                if (rms > 0.0001f) {
                    DBG("  Channel " + String(ch) + " RMS: " + String(rms));
                }
            }
        }
    } else {
        buffer.clear();
    }
}

//==============================================================================
bool AudioFilePlayerAudioProcessor::hasEditor() const {
    return true;  // (change this to false if you choose to not supply an
                  // editor)
}

juce::AudioProcessorEditor* AudioFilePlayerAudioProcessor::createEditor() {
    return new AudioFilePlayerAudioProcessorEditor(*this);
}

//==============================================================================
void AudioFilePlayerAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData) {
    if (activeSource != nullptr) {
        refreshCurrentFileInAPVTS(apvts, activeSource->currentAudioFile);

        // Save playback position
        apvts.state.setProperty(
            "PlaybackPosition", transportSource.getCurrentPosition(), nullptr);

        juce::MemoryOutputStream mos(destData, true);
        apvts.state.writeToStream(mos);
    }
}

void AudioFilePlayerAudioProcessor::setStateInformation(const void* data,
                                                        int sizeInBytes) {
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);

        if (auto url = apvts.state.getProperty("CurrentFile", {});
            url != var()) {
            File file(url.toString());
            if (file.existsAsFile()) {
                DBG("State restoration: Loading file: " +
                    file.getFullPathName());
                juce::URL path(file);
                transportSourceCreator.requestTransportForURL(path);
                // Position will be restored after file loads in processBlock
            }
        }
    }
}

AudioProcessorValueTreeState::ParameterLayout
AudioFilePlayerAudioProcessor::createParameterLayout() {
    AudioProcessorValueTreeState::ParameterLayout layout;
    return layout;
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AudioFilePlayerAudioProcessor();
}
#include "PluginProcessor.hpp"

#include "PluginEditor.hpp"

//==============================================================================
AudioFilePlayerAudioProcessor::AudioFilePlayerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::discreteChannels(16), true)) {
    formatManager.registerBasicFormats();
    directoryScannerBackgroundThread.startThread(
        juce::Thread::Priority::normal);

    // Poll for newly-loaded sources on the message thread. 30Hz is plenty
    // responsive for "a file finished loading" without being wasteful.
    startTimerHz(30);
}

AudioFilePlayerAudioProcessor::~AudioFilePlayerAudioProcessor() {
    stopTimer();

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
    return 1;
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
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioFilePlayerAudioProcessor::releaseResources() {
    transportSource.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioFilePlayerAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    auto outputChannels = layouts.getMainOutputChannelSet().size();

    if (outputChannels < 1 || outputChannels > 16)
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

//==============================================================================
// Real-time audio thread. Deliberately does nothing except pull audio from
// whatever source is currently set. It must NEVER allocate, lock, or touch
// `activeSource` / `pendingSource` directly -- that's all handled on the
// message thread in timerCallback()/checkForNewSource(). AudioTransportSource
// is designed to have its source swapped concurrently from another thread
// while getNextAudioBlock() runs here, so this is safe.
void AudioFilePlayerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (transportSource.getTotalLength() > 0) {
        AudioSourceChannelInfo asci(&buffer, 0, buffer.getNumSamples());
        transportSource.getNextAudioBlock(asci);
    } else {
        buffer.clear();
    }
}

//==============================================================================
// Message thread. Picks up the latest pending source and performs the swap,
// including the (allocating, locking) transportSource.setSource() call. This
// is where the "expensive" work now lives, well away from the audio
// callback.
void AudioFilePlayerAudioProcessor::timerCallback() {
    checkForNewSource();
}

void AudioFilePlayerAudioProcessor::checkForNewSource() {
    ReferencedTransportSourceData::Ptr ptr;
    if (!pendingSource.getIfNew(ptr) || ptr == nullptr)
        return;

    DBG("checkForNewSource: swapping in new source (message thread)");
    
    activeSource = ptr;
    transportSource.stop();

    auto* reader = activeSource->currentAudioFileSource->getAudioFormatReader();
    jassert(reader != nullptr);

    int numChannels = reader->numChannels;

    transportSource.setSource(activeSource->currentAudioFileSource.get(),
                              32768,
                              &directoryScannerBackgroundThread,
                              activeSource->audioFileSourceSampleRate,
                              numChannels);

    if (apvts.state.hasProperty("PlaybackPosition")) {
        double savedPosition =
            apvts.state.getProperty("PlaybackPosition", 0.0);
        transportSource.setPosition(savedPosition);
        apvts.state.removeProperty("PlaybackPosition", nullptr);
    }

    sourceHasChanged.set(true);
}

//==============================================================================
bool AudioFilePlayerAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* AudioFilePlayerAudioProcessor::createEditor() {
    return new AudioFilePlayerAudioProcessorEditor(*this);
}

//==============================================================================
void AudioFilePlayerAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData) {
    if (activeSource != nullptr) {
        refreshCurrentFileInAPVTS(apvts, activeSource->currentAudioFile);
        apvts.state.setProperty(
            "PlaybackPosition", transportSource.getCurrentPosition(), nullptr);
    }

    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
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
                juce::URL path(file);
                transportSourceCreator.requestTransportForURL(path);
                // Position will be restored after file loads, in
                // checkForNewSource().
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
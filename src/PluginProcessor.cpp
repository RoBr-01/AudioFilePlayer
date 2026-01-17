#include "PluginProcessor.hpp"

#include "PluginEditor.hpp"

//==============================================================================
AudioFilePlayerAudioProcessor::AudioFilePlayerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput(
                  "Input", juce::AudioChannelSet::discreteChannels(16), true)
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

AudioFilePlayerAudioProcessor::~AudioFilePlayerAudioProcessor() {}

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

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    ReferencedTransportSourceData::Ptr ptr;
    while (fifo.pull(ptr)) {
        ;
    }

    if (ptr != nullptr) {
        pool.add(activeSource);
        activeSource = ptr;
        transportSource.stop();
        transportSource.setSource(activeSource->currentAudioFileSource.get(),
                                  32768,
                                  &directoryScannerBackgroundThread,
                                  activeSource->audioFileSourceSampleRate);
        sourceHasChanged.set(true);

        DBG("Active source changed in processBlock");
        if (activeSource->currentAudioFileSource.get() != nullptr) {
            auto* reader =
                activeSource->currentAudioFileSource->getAudioFormatReader();
            if (reader != nullptr) {
                DBG("  New source has " + String(reader->numChannels) +
                    " channels");
                DBG("  Transport total length after setSource: " +
                    String(transportSource.getTotalLength()));
            }
        }
    }

    // Only process if we have an active source
    if (activeSource != nullptr && transportSource.getTotalLength() > 0) {
        AudioSourceChannelInfo asci(&buffer, 0, buffer.getNumSamples());
        transportSource.getNextAudioBlock(asci);
    } else {
        // No source loaded, output silence
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
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    if (activeSource != nullptr) {
        refreshCurrentFileInAPVTS(apvts, activeSource->currentAudioFile);

        juce::MemoryOutputStream mos(destData, true);
        apvts.state.writeToStream(mos);
    }
}

void AudioFilePlayerAudioProcessor::setStateInformation(const void* data,
                                                        int sizeInBytes) {
    // You should use this method to restore your parameters from this memory
    // block, whose contents will have been created by the getStateInformation()
    // call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
        if (auto url = apvts.state.getProperty("CurrentFile", {});
            url != var()) {
            File file(url.toString());
            jassert(file.existsAsFile());
            if (file.existsAsFile()) {
                juce::URL path(file);
                transportSourceCreator.requestTransportForURL(path);
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
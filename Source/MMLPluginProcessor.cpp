#include "MMLPluginProcessor.h"
#include "MMLPluginEditor.h"
#include "MMLParser/EnhancedMMLParser.h"

namespace MMLPlugin {
//==============================================================================
MMLPluginProcessor::MMLPluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
      )
    , parameters(*this, nullptr, "Parameters", {})
{
    needsMidiUpdate = false;
    lastMidiSendTime = 0;
}

MMLPluginProcessor::~MMLPluginProcessor()
{
}

//==============================================================================
const juce::String MMLPluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MMLPluginProcessor::acceptsMidi() const
{
    return true;
}

bool MMLPluginProcessor::producesMidi() const
{
    return true; // Must return true to output MIDI
}

bool MMLPluginProcessor::isMidiEffect() const
{
    return true; // Operate as a MIDI effect plugin
}

double MMLPluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MMLPluginProcessor::getNumPrograms()
{
    return 1;   // At least one program is required
}

int MMLPluginProcessor::getCurrentProgram()
{
    return 0;
}

void MMLPluginProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String MMLPluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void MMLPluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void MMLPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialization before playback
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    
    // Clear existing MIDI sequence
    currentSequence.clear();
    needsMidiUpdate = false;
}

void MMLPluginProcessor::releaseResources()
{
    // Release resources when playback stops
}

bool MMLPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support only mono or stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Ensure input and output layouts match
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void MMLPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear input MIDI messages
    midiMessages.clear();
    
    // If MIDI data needs to be sent
    if (needsMidiUpdate && currentSequence.getNumEvents() > 0) {
        // Get current playback position info
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        auto playHeadPtr = getPlayHead();
        
        if (playHeadPtr != nullptr) {
#if JUCE_VERSION >= 0x060000
            auto optionalInfo = playHeadPtr->getPosition();
            if (optionalInfo.hasValue()) {
                auto& info = *optionalInfo;
                posInfo.bpm = info.getBpm().orFallback(120.0);
                posInfo.timeInSamples = info.getTimeInSamples().orFallback(0);
                posInfo.timeInSeconds = info.getTimeInSeconds().orFallback(0.0);
                posInfo.ppqPosition = info.getPpqPosition().orFallback(0.0);
            }
#else
            playHeadPtr->getCurrentPosition(posInfo);
#endif
        }
        double samplesPerSecond = getSampleRate();
        double ppqPerSecond = posInfo.bpm / 60.0;
        
        for (int i = 0; i < currentSequence.getNumEvents(); ++i) {
            const auto* event = currentSequence.getEventPointer(i);
            
            // In Cubase, use timestamp to place MIDI events
            // Convert PPQ position to sample position
            double eventPpq = event->message.getTimeStamp();
            double eventOffsetSeconds = eventPpq / ppqPerSecond;
            int eventSamplePosition = static_cast<int>(eventOffsetSeconds * samplesPerSecond);
            
            // Ensure not to exceed buffer size
            eventSamplePosition = juce::jmin(eventSamplePosition, buffer.getNumSamples() - 1);
            eventSamplePosition = juce::jmax(0, eventSamplePosition);
            
            // Add event to buffer
            midiMessages.addEvent(event->message, eventSamplePosition);
        }
        
        // Mark MIDI data as processed
        needsMidiUpdate = false;
        
        // Debug output
        DBG("Added " + juce::String(currentSequence.getNumEvents()) + " MIDI events to buffer");
    }
    
    // Clear audio buffer (process only MIDI)
    buffer.clear();
}

//==============================================================================
bool MMLPluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MMLPluginProcessor::createEditor()
{
    return new MMLPluginEditor(*this);
}

//==============================================================================
void MMLPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save parameters to memory block
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MMLPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore parameters from memory block
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(parameters.state.getType())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

//==============================================================================

bool MMLPluginProcessor::processMML(const juce::String& mmlText)
{
    // Save MML text
    this->mmlText = mmlText;
    
    // Use enhanced MML parser
    EnhancedMMLParser parser;
    bool success = parser.parse(mmlText);
    
    if (!success) {
        // Set error message on parse failure
        errorMessage = "MML ERROR: " + parser.getError();
        return false;
    }
    
    // Clear previous error message
    errorMessage = "";
    
    // Generate MIDI sequence from parsed MML
    currentSequence = parser.generateMidi();
    
    // Debug output
    DBG("Generated MIDI sequence with " + juce::String(currentSequence.getNumEvents()) + " events");
    
    // Ensure at least one note exists (for testing)
    if (currentSequence.getNumEvents() == 0) {
        // Add simple C major scale as fallback
        for (int i = 0; i < 8; ++i) {
            juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60 + i, (juce::uint8)100);
            noteOn.setTimeStamp(i * 0.5);

            juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, 60 + i);
            noteOff.setTimeStamp(i * 0.5 + 0.4);
            
            currentSequence.addEvent(noteOn);
            currentSequence.addEvent(noteOff);
        }
        
        DBG("Added fallback C major scale with " + juce::String(currentSequence.getNumEvents()) + " events");
    }
    
    // Mark that MIDI update is needed
    needsMidiUpdate = true;
    
    // Immediately send MIDI to track
    sendMidiToTrack();
    
    return true;
}

const juce::MidiMessageSequence& MMLPluginProcessor::getMidiSequence() const
{
    return currentSequence;
}

juce::String MMLPluginProcessor::getErrorMessage() const
{
    return errorMessage;
}

void MMLPluginProcessor::sendMidiToTrack()
{
    // Implementation to send MIDI to DAW track
    lastMidiSendTime = juce::Time::currentTimeMillis();
    
    // Force update on next processBlock call
    needsMidiUpdate = true;
}

juce::String MMLPluginProcessor::getMMLText() const
{
    return mmlText;
}

void MMLPluginProcessor::setMMLText(const juce::String& text)
{
    mmlText = text;
}

} // namespace MMLPlugin

//==============================================================================
// Function to create plugin instance
// Important: This function must be placed in the global namespace
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MMLPlugin::MMLPluginProcessor();
}

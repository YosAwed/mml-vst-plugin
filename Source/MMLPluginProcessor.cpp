#include "MMLPluginProcessor.h"
#include "MMLPluginEditor.h"
#include "MMLParser/EnhancedMMLParser.h"

namespace MMLPlugin {
//==============================================================================
MMLPluginProcessor::MMLPluginProcessor()
    : AudioProcessor(BusesProperties())
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
    
    // Don't clear sequence - let it persist between playback sessions
    // Instead, mark that we need to process it
    if (currentSequence.getNumEvents() > 0) {
        needsMidiUpdate = true;
    }
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
    // Create a temporary buffer for our MIDI events
    juce::MidiBuffer tempMidiBuffer;
    
    // If MIDI data needs to be sent or was recently updated
    if (currentSequence.getNumEvents() > 0) {
        // Get current playback position info
        double currentBpm = 120.0;  // Default BPM if none provided
        double ppqPosition = 0.0;   // Default PPQ position
        bool isPlaying = false;     // Playback status
        
        auto playHeadPtr = getPlayHead();
        if (playHeadPtr != nullptr) {
#if JUCE_VERSION >= 0x060000
            if (auto posInfo = playHeadPtr->getPosition()) {
                // u5024u304cu5b58u5728u3059u308bu5834u5408u306eu307fu51e6u7406u3092u884cu3046
                if (auto bpm = posInfo->getBpm())
                    currentBpm = *bpm;
                
                if (auto ppq = posInfo->getPpqPosition())
                    ppqPosition = *ppq;
                
                // getIsPlaying()u306fu76f4u63a5boolu3092u8fd4u3059u53efu80fdu6027u304cu3042u308b
                isPlaying = posInfo->getIsPlaying();
            }
#else
            juce::AudioPlayHead::CurrentPositionInfo posInfo;
            playHeadPtr->getCurrentPosition(posInfo);
            currentBpm = posInfo.bpm;
            ppqPosition = posInfo.ppqPosition;
            isPlaying = posInfo.isPlaying;
#endif
        }
        
        
        // Force process for newly-added MML input
        if (needsMidiUpdate) {
            double samplesPerSecond = getSampleRate();
            double ppqPerSecond = currentBpm / 60.0;
            
            for (int i = 0; i < currentSequence.getNumEvents(); ++i) {
                const auto* event = currentSequence.getEventPointer(i);
                
                // Convert MML timestamp to appropriate buffer position
                double eventPpq = event->message.getTimeStamp();
                double eventOffsetSeconds = eventPpq / ppqPerSecond;
                int eventSamplePosition = static_cast<int>(eventOffsetSeconds * samplesPerSecond);
                
                // Ensure valid sample position within buffer
                eventSamplePosition = juce::jmin(eventSamplePosition, buffer.getNumSamples() - 1);
                eventSamplePosition = juce::jmax(0, eventSamplePosition);
                
                // Add event to our temporary buffer
                tempMidiBuffer.addEvent(event->message, eventSamplePosition);
            }
            
            // Only clear flag when we've successfully processed events
            needsMidiUpdate = false;
            
            // Debug MIDI output
            DBG("Processed " + juce::String(currentSequence.getNumEvents()) + " MIDI events from MML");
        }
    }
    
    // Add our processed MIDI to output buffer
    if (!tempMidiBuffer.isEmpty()) {
        // Add our events to the output MIDI buffer
        for (auto metadata : tempMidiBuffer) {
            midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);
        }
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
    
    // Debug message for tracking
    DBG("MIDI update requested - sequence has " + juce::String(currentSequence.getNumEvents()) + " events");
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

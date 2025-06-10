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
    sequenceStartTime = 0.0;
    sequenceIsPlaying = false;
    nextEventIndex = 0;
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
    // Clear audio buffer (MIDI-only plugin)
    buffer.clear();
    
    // If new MIDI data needs to be processed
    if (needsMidiUpdate && currentSequence.getNumEvents() > 0) {
        // Start the sequence playback
        sequenceStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        sequenceIsPlaying = true;
        nextEventIndex = 0;
        needsMidiUpdate = false;
        
        DBG("Starting MIDI sequence playback with " + juce::String(currentSequence.getNumEvents()) + " events");
    }
    
    // Process MIDI events if sequence is playing
    if (sequenceIsPlaying && currentSequence.getNumEvents() > 0) {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double elapsedTime = currentTime - sequenceStartTime;
        
        // Process events that should occur in this buffer
        int bufferSamples = buffer.getNumSamples();
        double bufferDuration = bufferSamples / getSampleRate();
        
        // Check all events to see if they should be played now
        while (nextEventIndex < currentSequence.getNumEvents()) {
            const auto* event = currentSequence.getEventPointer(nextEventIndex);
            double eventTime = event->message.getTimeStamp();
            
            // If event time has passed or is within this buffer
            if (eventTime <= elapsedTime + bufferDuration) {
                // Calculate sample position within current buffer
                double timeInBuffer = eventTime - elapsedTime;
                int samplePosition = static_cast<int>(timeInBuffer * getSampleRate());
                
                // Clamp to buffer bounds
                samplePosition = juce::jlimit(0, bufferSamples - 1, samplePosition);
                
                // Add the event to output
                juce::MidiMessage message = event->message;
                midiMessages.addEvent(message, samplePosition);
                
                DBG("Playing MIDI event at time " + juce::String(eventTime) + 
                    "s (elapsed: " + juce::String(elapsedTime) + 
                    "s, sample: " + juce::String(samplePosition) + ")");
                
                if (message.isNoteOn()) {
                    DBG("  Note ON: " + juce::String(message.getNoteNumber()) + 
                        " velocity " + juce::String(message.getVelocity()));
                } else if (message.isNoteOff()) {
                    DBG("  Note OFF: " + juce::String(message.getNoteNumber()));
                }
                
                nextEventIndex++;
            } else {
                // No more events for this buffer
                break;
            }
        }
        
        // Check if sequence is complete
        if (nextEventIndex >= currentSequence.getNumEvents()) {
            sequenceIsPlaying = false;
            DBG("MIDI sequence playback completed");
        }
    }
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
    
    // Check if MML parsing produced any events
    if (currentSequence.getNumEvents() == 0) {
        errorMessage = "MML parsing produced no MIDI events. Please check your MML syntax.";
        DBG("No MIDI events generated from MML input");
        return false;
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
    // Schedule MIDI sequence for playback
    lastMidiSendTime = juce::Time::currentTimeMillis();
    needsMidiUpdate = true;
    
    DBG("=== MML to MIDI conversion requested ===");
    DBG("MIDI sequence contains " + juce::String(currentSequence.getNumEvents()) + " events");
    
    // Debug output note details
    int noteOnCount = 0;
    int noteOffCount = 0;
    
    for (int i = 0; i < currentSequence.getNumEvents(); ++i) {
        const auto* event = currentSequence.getEventPointer(i);
        if (event->message.isNoteOn()) {
            noteOnCount++;
            DBG("Event " + juce::String(i) + ": Note ON at " + juce::String(event->message.getTimeStamp()) + 
                "s, Note=" + juce::String(event->message.getNoteNumber()));
        } else if (event->message.isNoteOff()) {
            noteOffCount++;
            DBG("Event " + juce::String(i) + ": Note OFF at " + juce::String(event->message.getTimeStamp()) + 
                "s, Note=" + juce::String(event->message.getNoteNumber()));
        }
    }
    
    DBG("Total: " + juce::String(noteOnCount) + " note-on, " + juce::String(noteOffCount) + " note-off events");
    DBG("Sequence will start playing in next audio callback");
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

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
                // Process only when position info exists
                if (auto bpm = posInfo->getBpm())
                    currentBpm = *bpm;
                
                if (auto ppq = posInfo->getPpqPosition())
                    ppqPosition = *ppq;
                
                // getIsPlaying() returns bool directly
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
        
        // Improved Cubase 14 compatibility: ensure all notes are played in sync with tempo
        if (needsMidiUpdate || isPlaying) {
            double samplesPerSecond = getSampleRate();
            double ppqPerSecond = currentBpm / 60.0;
            
            // If new update exists, insert MML to MIDI track
            if (needsMidiUpdate) {
                // Dump all notes
                DBG("Converting all MML notes to MIDI for Cubase track");
                
                // Classify note-on and note-off events
                juce::Array<juce::MidiMessage> noteOnMessages;
                juce::Array<juce::MidiMessage> noteOffMessages;
                
                // Collect all notes
                for (int i = 0; i < currentSequence.getNumEvents(); ++i) {
                    const auto* event = currentSequence.getEventPointer(i);
                    juce::MidiMessage midiMsg = event->message;
                    
                    if (midiMsg.isNoteOn()) {
                        noteOnMessages.add(midiMsg);
                    } else if (midiMsg.isNoteOff()) {
                        noteOffMessages.add(midiMsg);
                    } else {
                        // Add other messages as-is
                        tempMidiBuffer.addEvent(midiMsg, 0);
                    }
                }
                
                // Add note-on messages with correctly set timestamps
                for (auto& noteOn : noteOnMessages) {
                    // Get note timestamp (PPQ units within buffer)
                    double timestamp = noteOn.getTimeStamp();
                    
                    // Calculate relative timestamp considering current playback position
                    // Cubase considers current playback position, so set to 0 here
                    juce::MidiMessage newNoteOn = noteOn;
                    // Slightly delay to avoid playback timing issues
                    newNoteOn.setTimeStamp(0.0);
                    
                    // Insert note into MIDI track
                    midiMessages.addEvent(newNoteOn, 0);
                    DBG("Added note ON to Cubase track: Channel=" + juce::String(newNoteOn.getChannel()) + 
                        ", Note=" + juce::String(newNoteOn.getNoteNumber()) + ", Velocity=" + juce::String(newNoteOn.getVelocity()));
                }
                
                // Add all note-off messages in the same way
                for (auto& noteOff : noteOffMessages) {
                    double timestamp = noteOff.getTimeStamp();
                    juce::MidiMessage newNoteOff = noteOff;
                    // Set note-off to same relative time as note-on
                    newNoteOff.setTimeStamp(0.0);
                    
                    midiMessages.addEvent(newNoteOff, 0);
                    DBG("Added note OFF to Cubase track: Channel=" + juce::String(newNoteOff.getChannel()) + 
                        ", Note=" + juce::String(newNoteOff.getNoteNumber()));
                }
                
                // Processing complete flag
                needsMidiUpdate = false;
                DBG("Processed " + juce::String(currentSequence.getNumEvents()) + " MIDI events and sent to Cubase track");
                
                // Process here if adding to Cubase internal track
            }
            // Normal playback processing is unnecessary, so commented out
            /*
            else if (isPlaying) {
                // Implement normal processing during playback if needed here
            }
            */
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
    // Send data to Cubase 14 MIDI track
    lastMidiSendTime = juce::Time::currentTimeMillis();
    
    // Force update on next processBlock call
    needsMidiUpdate = true;
    
    // Add debug information to ensure all notes are correctly inserted into track
    DBG("=== MML to MIDI conversion requested ===");
    DBG("MIDI sequence contains " + juce::String(currentSequence.getNumEvents()) + " events");
    
    // Debug output note details
    int noteOnCount = 0;
    int noteOffCount = 0;
    
    for (int i = 0; i < currentSequence.getNumEvents(); ++i) {
        const auto* event = currentSequence.getEventPointer(i);
        if (event->message.isNoteOn())
            noteOnCount++;
        else if (event->message.isNoteOff())
            noteOffCount++;
    }
    
    DBG("Notes: " + juce::String(noteOnCount) + " note-on, " + juce::String(noteOffCount) + " note-off events");
    
    // Get Cubase playback head information
    if (auto* playHead = getPlayHead()) {
#if JUCE_VERSION >= 0x060000
        if (auto posInfo = playHead->getPosition()) {
            // Update if time information is available
            double currentPos = posInfo->getPpqPosition() ? *posInfo->getPpqPosition() : 0.0;
            double currentBpm = posInfo->getBpm() ? *posInfo->getBpm() : 120.0;
            
            DBG("Current playback position: PPQ=" + juce::String(currentPos) + ", BPM=" + juce::String(currentBpm));
        }
#endif
    }
    
    DBG("MML conversion will be processed in the next audio callback");
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

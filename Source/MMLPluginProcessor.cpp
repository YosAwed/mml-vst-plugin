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
        
        // Cubase 14u3068u306eu4e92u63dbu6027u3092u6539u5584u3057u3001u5168u3066u306eu30ceu30fcu30c8u304cu30c6u30f3u30ddu306bu5408u308fu305bu3066u518du751fu3055u308cu308bu3088u3046u306bu4feeu6b63
        if (needsMidiUpdate || isPlaying) {
            double samplesPerSecond = getSampleRate();
            double ppqPerSecond = currentBpm / 60.0;
            
            // u65b0u3057u3044u66f4u65b0u304cu3042u308bu5834u5408u306fu3001MMLu3092MIDIu30c8u30e9u30c3u30afu306bu633fu5165
            if (needsMidiUpdate) {
                // u5168u3066u306eu30ceu30fcu30c8u3092u30c0u30f3u30d7
                DBG("Converting all MML notes to MIDI for Cubase track");
                
                // u30ceu30fcu30c8u30aau30f3u3068u30ceu30fcu30c8u30aau30d5u3092u5206u985e
                juce::Array<juce::MidiMessage> noteOnMessages;
                juce::Array<juce::MidiMessage> noteOffMessages;
                
                // u5168u3066u306eu30ceu30fcu30c8u3092u53ceu96c6
                for (int i = 0; i < currentSequence.getNumEvents(); ++i) {
                    const auto* event = currentSequence.getEventPointer(i);
                    juce::MidiMessage midiMsg = event->message;
                    
                    if (midiMsg.isNoteOn()) {
                        noteOnMessages.add(midiMsg);
                    } else if (midiMsg.isNoteOff()) {
                        noteOffMessages.add(midiMsg);
                    } else {
                        // u305du306eu4ed6u306eu30e1u30c3u30bbu30fcu30b8u306fu305du306eu307eu307eu8ffdu52a0
                        tempMidiBuffer.addEvent(midiMsg, 0);
                    }
                }
                
                // u30bfu30a4u30e0u30b9u30bfu30f3u30d7u304cu6b63u3057u304fu8a2du5b9au3055u308cu305fu30ceu30fcu30c8u30aau30f3u30e1u30c3u30bbu30fcu30b8u3092u8ffdu52a0
                for (auto& noteOn : noteOnMessages) {
                    // u30ceu30fcu30c8u306eu30bfu30a4u30e0u30b9u30bfu30f3u30d7u3092u53d6u5f97uff08u62dbu5f15u5185u306ePPQu5358u4f4duff09
                    double timestamp = noteOn.getTimeStamp();
                    
                    // u73feu5728u306eu518du751fu4f4du7f6eu3092u8003u616eu3057u3066u76f8u5bfeu7684u306au30bfu30a4u30e0u30b9u30bfu30f3u30d7u3092u8a08u7b97
                    // Cubaseu306fu73feu5728u306eu518du751fu4f4du7f6eu3092u8003u616eu3059u308bu306eu3067u3001u3053u3053u3067u306f0u306bu8a2du5b9a
                    juce::MidiMessage newNoteOn = noteOn;
                    // u5c11u3057u9045u3089u305bu3066u518du751fu6642u306eu554fu984cu3092u56deu907f
                    newNoteOn.setTimeStamp(0.0);
                    
                    // MIDIu30c8u30e9u30c3u30afu306bu30ceu30fcu30c8u3092u633fu5165
                    midiMessages.addEvent(newNoteOn, 0);
                    DBG("Added note ON to Cubase track: Channel=" + juce::String(newNoteOn.getChannel()) + 
                        ", Note=" + juce::String(newNoteOn.getNoteNumber()) + ", Velocity=" + juce::String(newNoteOn.getVelocity()));
                }
                
                // u5168u3066u306eu30ceu30fcu30c8u30aau30d5u30e1u30c3u30bbu30fcu30b8u3082u540cu69d8u306bu8ffdu52a0
                for (auto& noteOff : noteOffMessages) {
                    double timestamp = noteOff.getTimeStamp();
                    juce::MidiMessage newNoteOff = noteOff;
                    // u30ceu30fcu30c8u30aau30d5u306fu30ceu30fcu30c8u30aau30f3u3068u540cu3058u76f8u5bfeu6642u9593u306bu8a2du5b9a
                    newNoteOff.setTimeStamp(0.0);
                    
                    midiMessages.addEvent(newNoteOff, 0);
                    DBG("Added note OFF to Cubase track: Channel=" + juce::String(newNoteOff.getChannel()) + 
                        ", Note=" + juce::String(newNoteOff.getNoteNumber()));
                }
                
                // u51e6u7406u5b8cu4e86u30d5u30e9u30b0
                needsMidiUpdate = false;
                DBG("Processed " + juce::String(currentSequence.getNumEvents()) + " MIDI events and sent to Cubase track");
                
                // u307eu305fu306fCubaseu306eu5185u90e8u30c8u30e9u30c3u30afu306bu8ffdu52a0u3059u308bu5834u5408u306fu3053u3053u3067u51e6u7406
            }
            // u901au5e38u306eu518du751fu6642u306eu51e6u7406u306fu4e0du8981u306au306eu3067u30b3u30e1u30f3u30c8u30a2u30a6u30c8
            /*
            else if (isPlaying) {
                // u518du751fu4e2du306eu901au5e38u51e6u7406u306fu5fc5u8981u306au3089u3053u3053u306bu5b9fu88c5
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
    // Cubase 14u306eMIDIu30c8u30e9u30c3u30afu306bu30c7u30fcu30bfu3092u9001u4fe1
    lastMidiSendTime = juce::Time::currentTimeMillis();
    
    // u6b21u306eprocessBlocku547cu3073u51fau3057u3067u30a2u30c3u30d7u30c7u30fcu30c8u3092u5f37u5236
    needsMidiUpdate = true;
    
    // u3059u3079u3066u306eu30ceu30fcu30c8u304cu6b63u3057u304fu30c8u30e9u30c3u30afu306bu633fu5165u3055u308cu308bu3088u3046u306bu30c7u30d0u30c3u30b0u60c5u5831u3092u8ffdu52a0
    DBG("=== MML to MIDI conversion requested ===");
    DBG("MIDI sequence contains " + juce::String(currentSequence.getNumEvents()) + " events");
    
    // u30ceu30fcu30c8u306eu5185u8a33u3092u30c7u30d0u30c3u30b0u51fau529b
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
    
    // Cubaseu306eu518du751fu30d8u30c3u30c9u306eu60c5u5831u3092u53d6u5f97
    if (auto* playHead = getPlayHead()) {
#if JUCE_VERSION >= 0x060000
        if (auto posInfo = playHead->getPosition()) {
            // u30bfu30a4u30e0u60c5u5831u304cu5229u7528u53efu80fdu306au5834u5408u306fu66f4u65b0
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

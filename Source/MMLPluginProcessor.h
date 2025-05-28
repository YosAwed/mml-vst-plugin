#pragma once

#include <JuceHeader.h>
#include "MMLParser/EnhancedMMLParser.h"

namespace MMLPlugin {

//==============================================================================
/**
 * MML Plugin Processor Class - Optimized for Cubase 14
 * 
 * This class parses Music Macro Language (MML) text and converts it to MIDI data for sending to a Cubase track.
 */
class MMLPluginProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MMLPluginProcessor();
    ~MMLPluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    // MML処理メソッド
    
    /**
     * Processes MML text and converts it to a MIDI sequence.
     * @param mmlText MML text to process
     * @return True if processing succeeded, false otherwise.
     */
    bool processMML(const juce::String& mmlText);
    
    /**
     * Gets the current MIDI sequence.
     * @return MIDI message sequence.
     */
    const juce::MidiMessageSequence& getMidiSequence() const;
    
    /**
     * Gets the error message after processing.
     * @return Error message string.
     */
    juce::String getErrorMessage() const;
    
    /**
     * Sends MIDI data to the track.
     */
    void sendMidiToTrack();
    
    /**
     * Gets the current MML text.
     * @return MML text.
     */
    juce::String getMMLText() const;
    
    /**
     * Sets the MML text.
     * @param text MML text to set.
     */
    void setMMLText(const juce::String& text);

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiMessageSequence currentSequence;
    juce::String mmlText;
    juce::String errorMessage;
    bool needsMidiUpdate;
    juce::int64 lastMidiSendTime;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MMLPluginProcessor)
};

} // namespace MMLPlugin

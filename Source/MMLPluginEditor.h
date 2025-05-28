#pragma once

#include <JuceHeader.h>
#include "MMLPluginProcessor.h"

namespace MMLPlugin {
/**
 * MML Plugin Editor Class - Optimized for Cubase 14
 * 
 * This class provides a user interface for entering Music Macro Language (MML) text.
 */
class MMLPluginEditor  : public juce::AudioProcessorEditor,
                         private juce::TextEditor::Listener,
                         private juce::Button::Listener
{
public:
    MMLPluginEditor (MMLPluginProcessor&);
    ~MMLPluginEditor() override;
    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Implementation of TextEditor::Listener
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;
    void textEditorFocusLost (juce::TextEditor&) override;
    
    // Implementation of Button::Listener
    void buttonClicked (juce::Button*) override;
    
    // Method to process MML text
    void processMMLText();

    // Reference to processor
    MMLPluginProcessor& audioProcessor;
    
    // UI components
    juce::TextEditor mmlTextEditor;
    juce::TextButton convertButton;
    juce::Label statusLabel;
    juce::Label titleLabel;
    juce::Label instructionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MMLPluginEditor)
};

} // namespace MMLPlugin

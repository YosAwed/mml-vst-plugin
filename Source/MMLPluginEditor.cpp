#include "MMLPluginEditor.h"
#include "MMLPluginProcessor.h"

namespace MMLPlugin {

//==============================================================================
MMLPluginEditor::MMLPluginEditor (MMLPluginProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText("MML Plugin", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    instructionLabel.setText("Enter MML text below and click 'Convert to MIDI'", juce::dontSendNotification);
    instructionLabel.setFont(juce::Font(14.0f));
    instructionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(instructionLabel);

    mmlTextEditor.setMultiLine(true);
    mmlTextEditor.setReturnKeyStartsNewLine(true);
    mmlTextEditor.setReadOnly(false);
    mmlTextEditor.setScrollbarsShown(true);
    mmlTextEditor.setCaretVisible(true);
    mmlTextEditor.setPopupMenuEnabled(true);
    mmlTextEditor.setText(audioProcessor.getMMLText());
    mmlTextEditor.addListener(this);
    addAndMakeVisible(mmlTextEditor);
    
    convertButton.setButtonText("Convert to MIDI");
    convertButton.addListener(this);
    addAndMakeVisible(convertButton);
    
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(14.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    setSize (500, 400);
}

MMLPluginEditor::~MMLPluginEditor()
{
    mmlTextEditor.removeListener(this);
    convertButton.removeListener(this);
}

//==============================================================================
void MMLPluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MMLPluginEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    
    instructionLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);
    
    mmlTextEditor.setBounds(area.removeFromTop(200));
    area.removeFromTop(10);
    
    convertButton.setBounds(area.removeFromTop(30).withSizeKeepingCentre(150, 30));
    area.removeFromTop(10);
    
    statusLabel.setBounds(area.removeFromTop(30));
}

//==============================================================================
void MMLPluginEditor::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &mmlTextEditor)
    {
        audioProcessor.setMMLText(mmlTextEditor.getText());
    }
}

void MMLPluginEditor::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
}

void MMLPluginEditor::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    editor.giveAwayKeyboardFocus();
}

void MMLPluginEditor::textEditorFocusLost(juce::TextEditor& editor)
{
    if (&editor == &mmlTextEditor)
    {
        audioProcessor.setMMLText(mmlTextEditor.getText());
    }
}

void MMLPluginEditor::buttonClicked(juce::Button* button)
{
    if (button == &convertButton)
    {
        processMMLText();
    }
}

void MMLPluginEditor::processMMLText()
{
    juce::String mmlText = mmlTextEditor.getText();
    
    if (mmlText.isEmpty())
    {
        statusLabel.setText("Error: MML text is empty", juce::dontSendNotification);
        return;
    }
    
    bool success = audioProcessor.processMML(mmlText);
    
    if (success)
    {
        statusLabel.setText("Conversion successful! MIDI data sent to Cubase", juce::dontSendNotification);
        
        int numEvents = audioProcessor.getMidiSequence().getNumEvents();
        statusLabel.setText("Conversion successful! Generated " + juce::String(numEvents) + " MIDI events", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText(audioProcessor.getErrorMessage(), juce::dontSendNotification);
    }
}

} // namespace MMLPlugin

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

/**
 * EnhancedMMLParser - Optimized for Cubase 14
 * 
 * This class parses Music Macro Language (MML) text and converts it into MIDI data.
 * Optimized to maximize compatibility with Cubase 14.
 */
class EnhancedMMLParser
{
public:
    EnhancedMMLParser();
    ~EnhancedMMLParser();
    
    /**
     * Parses the given MML text.
     * @param mmlText The MML text to parse.
     * @return True if parsing succeeded, false otherwise.
     */
    bool parse(const juce::String& mmlText);
    
    /**
     * Generates a MIDI sequence from the parsed MML.
     * @return MIDI message sequence.
     */
    juce::MidiMessageSequence generateMidi();
    
    /**
     * Gets the error message after parsing.
     * @return Error message string.
     */
    juce::String getError() const;

private:
    struct MMLNote {
        MMLNote();
        char noteName;
        int accidental;
        int octave;
        double duration;
        bool isTied;
        double timestamp;
    };
    struct MMLLoop {
        MMLLoop();
        int startPos;
        int count;
    };
    struct ParseResult {
        ParseResult();
        std::vector<MMLNote> notes;
        double totalDuration;
    };
    struct ParseState {
        ParseState();
        int position;
        int octave;
        double defaultDuration;
        int tempo;
        int volume;
        double currentTime;
        std::vector<MMLLoop> loops;
    };

    bool parseNote(ParseState& state, const juce::String& text, ParseResult& result);
    bool parseRest(ParseState& state, const juce::String& text, ParseResult& result);
    bool parseOctave(ParseState& state, const juce::String& text);
    bool parseDuration(ParseState& state, const juce::String& text);
    bool parseTempo(ParseState& state, const juce::String& text);
    bool parseVolume(ParseState& state, const juce::String& text);
    bool parseLoop(ParseState& state, const juce::String& text, ParseResult& result);
    bool parseEndLoop(ParseState& state, const juce::String& text, ParseResult& result);
    double parseDurationValue(ParseState& state, const juce::String& text, int& position);
    int noteNameToMidiNote(char noteName, int accidental, int octave);

    juce::String errorMessage;
    ParseResult parseResult;
    std::map<char, int> noteToMidiMap;

};

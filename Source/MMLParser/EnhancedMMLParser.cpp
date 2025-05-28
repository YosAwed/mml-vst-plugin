#include "EnhancedMMLParser.h"

EnhancedMMLParser::MMLNote::MMLNote()
    : noteName('c'), accidental(0), octave(4), duration(0.25), isTied(false), timestamp(0.0) {}

EnhancedMMLParser::MMLLoop::MMLLoop()
    : startPos(0), count(2) {}

EnhancedMMLParser::ParseResult::ParseResult()
    : totalDuration(0.0) {}

EnhancedMMLParser::ParseState::ParseState()
    : position(0), octave(4), defaultDuration(0.25), tempo(120), volume(100), currentTime(0.0) {}


EnhancedMMLParser::EnhancedMMLParser()
{
    noteToMidiMap['c'] = 0;
    noteToMidiMap['d'] = 2;
    noteToMidiMap['e'] = 4;
    noteToMidiMap['f'] = 5;
    noteToMidiMap['g'] = 7;
    noteToMidiMap['a'] = 9;
    noteToMidiMap['b'] = 11;
}

EnhancedMMLParser::~EnhancedMMLParser()
{
}

bool EnhancedMMLParser::parse(const juce::String& mmlText)
{
    ParseState state;
    parseResult = ParseResult();
    errorMessage = "";
    
    if (mmlText.isEmpty())
    {
        errorMessage = "Empty MML text";
        return false;
    }
    
    while (state.position < mmlText.length())
    {
        char c = mmlText[state.position];
        
        if (juce::CharacterFunctions::isWhitespace(c))
        {
            state.position++;
            continue;
        }
        
        switch (c)
        {
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'a':
            case 'b':
                if (!parseNote(state, mmlText, parseResult))
                    return false;
                break;
                
            case 'r':
                if (!parseRest(state, mmlText, parseResult))
                    return false;
                break;
                
            case 'o':
                if (!parseOctave(state, mmlText))
                    return false;
                break;
                
            case '>':
                state.octave = juce::jmin(state.octave + 1, 8);
                state.position++;
                break;
                
            case '<':
                state.octave = juce::jmax(state.octave - 1, 0);
                state.position++;
                break;
                
            case 'l':
                if (!parseDuration(state, mmlText))
                    return false;
                break;
                
            case 't':
                if (!parseTempo(state, mmlText))
                    return false;
                break;
                
            case 'v':
                if (!parseVolume(state, mmlText))
                    return false;
                break;
                
            case '[':
                if (!parseLoop(state, mmlText, parseResult))
                    return false;
                break;
                
            case ']':
                if (!parseEndLoop(state, mmlText, parseResult))
                    return false;
                break;
                
            default:
                state.position++;
                break;
        }
    }
    
    parseResult.totalDuration = state.currentTime;
    
    return true;
}

juce::MidiMessageSequence EnhancedMMLParser::generateMidi()
{
    juce::MidiMessageSequence sequence;

    for (const auto& note : parseResult.notes)
    {
        if (note.noteName == 'r')
            continue;
        
        int midiNote = noteNameToMidiNote(note.noteName, note.accidental, note.octave);
        
        double onTime = note.timestamp;
        double offTime = note.timestamp + note.duration;
        
        juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, midiNote, (juce::uint8)100);
        noteOn.setTimeStamp(onTime);
        sequence.addEvent(noteOn);
        
        if (!note.isTied)
        {
            juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, midiNote);
            noteOff.setTimeStamp(offTime);
            sequence.addEvent(noteOff);
        }
    }
    
    sequence.sort();
    
    return sequence;
}

juce::String EnhancedMMLParser::getError() const
{
    return errorMessage;
}

bool EnhancedMMLParser::parseNote(ParseState& state, const juce::String& text, ParseResult& result)
{
    char noteName = text[state.position++];
    
    int accidental = 0;
    if (state.position < text.length())
    {
        if (text[state.position] == '+' || text[state.position] == '#')
        {
            accidental = 1;
            state.position++;
        }
        else if (text[state.position] == '-')
        {
            accidental = -1;
            state.position++;
        }
    }
    
    double duration = state.defaultDuration;
    if (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
    {
        duration = parseDurationValue(state, text, state.position);
    }
    
    if (state.position < text.length() && text[state.position] == '.')
    {
        duration *= 1.5;
        state.position++;
    }
    
    bool isTied = false;
    if (state.position < text.length() && (text[state.position] == '&' || text[state.position] == '^'))
    {
        isTied = true;
        state.position++;
    }
    
    MMLNote note;
    note.noteName = noteName;
    note.accidental = accidental;
    note.octave = state.octave;
    note.duration = duration;
    note.isTied = isTied;
    note.timestamp = state.currentTime;
    
    result.notes.push_back(note);
    
    if (!isTied)
    {
        state.currentTime += duration;
    }
    
    return true;
}

bool EnhancedMMLParser::parseRest(ParseState& state, const juce::String& text, ParseResult& result)
{
    // Skip 'r'
    state.position++;
    
    // Parse note duration
    double duration = state.defaultDuration;
    if (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
    {
        duration = parseDurationValue(state, text, state.position);
    }
    
    // Parse dotted note
    if (state.position < text.length() && text[state.position] == '.')
    {
        duration *= 1.5;
        state.position++;
    }
    
    // Create rest note
    MMLNote rest;
    rest.noteName = 'r';
    rest.duration = duration;
    rest.timestamp = state.currentTime;
    
    // Add rest note
    result.notes.push_back(rest);
    
    // Advance time
    state.currentTime += duration;
    
    return true;
}

bool EnhancedMMLParser::parseOctave(ParseState& state, const juce::String& text)
{
    // Skip 'o'
    state.position++;
    
    // Parse octave number
    if (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
    {
        int octave = text[state.position] - '0';
        state.octave = juce::jlimit(0, 8, octave);
        state.position++;
        return true;
    }
    
    errorMessage = "Invalid octave at position " + juce::String(state.position);
    return false;
}

bool EnhancedMMLParser::parseDuration(ParseState& state, const juce::String& text)
{
    // Skip 'l'
    state.position++;
    
    // Parse note duration
    if (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
    {
        state.defaultDuration = parseDurationValue(state, text, state.position);
        
        // Parse dotted note
        if (state.position < text.length() && text[state.position] == '.')
        {
            state.defaultDuration *= 1.5;
            state.position++;
        }
        
        return true;
    }
    
    errorMessage = "Invalid duration at position " + juce::String(state.position);
    return false;
}

bool EnhancedMMLParser::parseTempo(ParseState& state, const juce::String& text)
{
    // Skip 't'
    state.position++;
    
    // Parse tempo value
    if (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
    {
        int tempo = 0;
        while (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
        {
            tempo = tempo * 10 + (text[state.position] - '0');
            state.position++;
        }
        
        // Check tempo range
        if (tempo >= 20 && tempo <= 300)
        {
            state.tempo = tempo;
            return true;
        }
        
        errorMessage = "Tempo out of range (20-300) at position " + juce::String(state.position);
        return false;
    }
    
    errorMessage = "Invalid tempo at position " + juce::String(state.position);
    return false;
}

bool EnhancedMMLParser::parseVolume(ParseState& state, const juce::String& text)
{
    // Skip 'v'
    state.position++;
    
    // Parse volume value
    if (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
    {
        int volume = 0;
        while (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
        {
            volume = volume * 10 + (text[state.position] - '0');
            state.position++;
        }
        
        // Check volume range
        if (volume >= 0 && volume <= 127)
        {
            state.volume = volume;
            return true;
        }
        
        errorMessage = "Volume out of range (0-127) at position " + juce::String(state.position);
        return false;
    }
    
    errorMessage = "Invalid volume at position " + juce::String(state.position);
    return false;
}

bool EnhancedMMLParser::parseLoop(ParseState& state, const juce::String& text, ParseResult& result)
{
    // Skip '['
    state.position++;
    
    // Save loop information
    MMLLoop loop;
    loop.startPos = state.position;
    loop.count = 2; // Default is 2 times
    
    state.loops.push_back(loop);
    
    return true;
}

bool EnhancedMMLParser::parseEndLoop(ParseState& state, const juce::String& text, ParseResult& result)
{
    // Skip ']'
    state.position++;
    
    // Ensure loop stack is not empty
    if (state.loops.empty())
    {
        errorMessage = "Unmatched loop end at position " + juce::String(state.position);
        return false;
    }
    
    // Parse loop count
    int count = 2; // デフォルト
    if (state.position < text.length())
    {
        // If there is a number after '*'
        if (text[state.position] == '*' && state.position + 1 < text.length() && 
            juce::CharacterFunctions::isDigit(text[state.position + 1]))
        {
            state.position++;
            while (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
            {
                count = count * 10 + (text[state.position] - '0');
                state.position++;
            }
        }
        // If there is a number directly
        else if (juce::CharacterFunctions::isDigit(text[state.position]))
        {
            count = 0;
            while (state.position < text.length() && juce::CharacterFunctions::isDigit(text[state.position]))
            {
                count = count * 10 + (text[state.position] - '0');
                state.position++;
            }
        }
    }
    
    // Check loop count range
    if (count < 1 || count > 100)
    {
        errorMessage = "Loop count out of range (1-100) at position " + juce::String(state.position);
        return false;
    }
    
    // Get last loop information
    MMLLoop& loop = state.loops.back();
    loop.count = count;
    
    // Process loop (from the second time onward)
    for (int i = 1; i < count; i++)
    {
        // Return to loop start position
        int savedPosition = state.position;
        state.position = loop.startPos;
        
        // Re-parse loop content
        while (state.position < savedPosition - 1)
        {
            // Get current character
            char c = text[state.position];
            
            // Parse command (except parseEndLoop)
            switch (c)
            {
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                case 'g':
                case 'a':
                case 'b':
                    if (!parseNote(state, text, result))
                        return false;
                    break;
                case 'r':
                    if (!parseRest(state, text, result))
                        return false;
                    break;
                case 'o':
                    if (!parseOctave(state, text))
                        return false;
                    break;
                case '>':
                    state.octave = juce::jmin(state.octave + 1, 8);
                    state.position++;
                    break;
                case '<':
                    state.octave = juce::jmax(state.octave - 1, 0);
                    state.position++;
                    break;
                case 'l':
                    if (!parseDuration(state, text))
                        return false;
                    break;
                case 't':
                    if (!parseTempo(state, text))
                        return false;
                    break;
                case 'v':
                    if (!parseVolume(state, text))
                        return false;
                    break;
                case '[':
                    if (!parseLoop(state, text, result))
                        return false;
                    break;
                default:
                    state.position++;
                    break;
            }
        }
        
        // Restore original position
        state.position = savedPosition;
    }
    
    // Remove from loop stack
    state.loops.pop_back();
    
    return true;
}

double EnhancedMMLParser::parseDurationValue(ParseState& state, const juce::String& text, int& position)
{
    // Parse denominator of note duration
    int denominator = 0;
    while (position < text.length() && juce::CharacterFunctions::isDigit(text[position]))
    {
        denominator = denominator * 10 + (text[position] - '0');
        position++;
    }
    
    // Use default value if denominator is 0
    if (denominator == 0)
        return state.defaultDuration;
    
    // Calculate note duration (quarter note = 1.0)
    return 4.0 / denominator;
}

int EnhancedMMLParser::noteNameToMidiNote(char noteName, int accidental, int octave)
{
    // Get base note number from note name
    int baseNote = noteToMidiMap[noteName];
    
    // Calculate MIDI note number (C4 = 60)
    return baseNote + accidental + (octave + 1) * 12;
}

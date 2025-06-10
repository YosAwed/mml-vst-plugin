# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a VST3 plugin that converts Music Macro Language (MML) text to MIDI data, specifically optimized for Cubase 14. The plugin functions as both a MIDI effect and synthesizer, allowing users to input MML text through a GUI and generate MIDI output that can be recorded to DAW tracks.

## Build System

### Primary Build Method
- **JUCE Framework**: This project uses JUCE 6.0+ with Projucer
- **Main Project File**: `MML.jucer` - open this in Projucer to configure the project
- **Target Platform**: Windows with Visual Studio 2022
- **Output Format**: VST3 plugin only (`pluginFormats="buildVST3"`)

### Build Commands
```bash
# Build the project (run from Builds/VisualStudio2022/ directory)
msbuild MML.sln /p:Configuration=Debug /p:Platform=x64
msbuild MML.sln /p:Configuration=Release /p:Platform=x64

# Alternative: Open MML.sln in Visual Studio 2022 and build
# Or use the shortcut from project root:
cd "Builds/VisualStudio2022" && msbuild MML.sln /p:Configuration=Debug /p:Platform=x64
```

### Development Workflow
- **Project Configuration**: Use Projucer to modify `MML.jucer` and regenerate Visual Studio projects
- **Code Changes**: Edit source files in `Source/` directory, then rebuild
- **Testing**: Load the built VST3 plugin in a DAW (preferably Cubase 14) to test functionality
- **Plugin Location**: Built VST3 files are output to `Builds/VisualStudio2022/x64/Debug/VST3/MML.vst3/`

### Build Targets
- **MML_SharedCode**: Static library containing core plugin logic
- **MML_VST3**: The main VST3 plugin DLL
- **MML_VST3ManifestHelper**: Helper for VST3 manifest generation

## Architecture

### Core Components

**MMLPluginProcessor** (`Source/MMLPluginProcessor.*`)
- Main audio processor inheriting from `juce::AudioProcessor`
- Configured as both MIDI effect and synthesizer (`pluginIsMidiEffectPlugin,pluginIsSynth,pluginProducesMidiOut`)
- Handles MML text processing and MIDI sequence generation
- Key methods: `processMML()`, `getMidiSequence()`, `sendMidiToTrack()`

**MMLPluginEditor** (`Source/MMLPluginEditor.*`)
- GUI component with text input for MML and convert button
- Implements `juce::TextEditor::Listener` and `juce::Button::Listener`
- Real-time MML processing and status feedback

**EnhancedMMLParser** (`Source/MMLParser/EnhancedMMLParser.*`)
- Core MML parsing logic, converts MML text to MIDI sequences
- Supports notes, rests, octaves, duration, tempo, volume, and loops
- Optimized for Cubase 14 compatibility

### Data Flow
1. User inputs MML text in the editor GUI
2. `MMLPluginEditor` calls `processMMLText()` on button click or text change
3. `MMLPluginProcessor::processMML()` uses `EnhancedMMLParser` to convert MML to MIDI
4. Generated MIDI sequence is stored and output during `processBlock()`
5. Cubase receives MIDI data and can record it to tracks

### Plugin Characteristics
- **Type**: MIDI Effect + Synthesizer hybrid
- **MIDI I/O**: Accepts and produces MIDI
- **Audio I/O**: No audio processing (MIDI-only)
- **VST3 Category**: Filter, Fx, Instrument

## Development Notes

### JUCE Module Dependencies
The project requires these JUCE modules (configured in MML.jucer):
- `juce_audio_basics`, `juce_audio_processors`, `juce_audio_plugin_client`
- `juce_core`, `juce_data_structures`, `juce_events`
- `juce_graphics`, `juce_gui_basics`, `juce_gui_extra`
- `juce_audio_devices`, `juce_audio_formats`, `juce_audio_utils`

### Key Configuration
- **Namespace**: Code is wrapped in `MMLPlugin` namespace
- **JUCE Options**: `JUCE_STRICT_REFCOUNTEDPOINTER=1`, `JUCE_VST3_CAN_REPLACE_VST2=0`
- **Company**: "NANKIN"
- **Plugin Name**: "MML"

### Code Organization
- All source files are in `Source/` with parser logic separated into `Source/MMLParser/`
- Build files generated in `Builds/VisualStudio2022/`
- JUCE library code auto-generated in `JuceLibraryCode/`

### Cubase 14 Optimization
The code includes specific optimizations and comments indicating Cubase 14 compatibility focus. When making changes, ensure MIDI timing and output format remain compatible with Cubase's expectations for MIDI effects.

### Thread Safety and State Management
- The plugin uses `std::atomic<bool> needsMidiUpdate` for thread-safe MIDI processing
- State is managed through `juce::AudioProcessorValueTreeState parameters`
- MIDI sequence generation is handled separately from the audio processing thread

### Plugin Installation and Testing
- Built VST3 plugins are located in `Builds/VisualStudio2022/x64/[Debug|Release]/VST3/MML.vst3/`
- For testing, copy the `.vst3` bundle to your DAW's VST3 plugins folder
- The plugin appears in DAWs under categories: Filter, Fx, Instrument due to its hybrid nature
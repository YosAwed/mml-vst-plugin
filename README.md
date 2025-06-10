# MML VST Plugin

A powerful VST3 plugin that converts Music Macro Language (MML) text to MIDI data, specifically optimized for Cubase 14 and other professional DAWs.

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](#building)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](#requirements)
[![VST3](https://img.shields.io/badge/format-VST3-orange.svg)](#features)

## Overview

The MML VST Plugin bridges the gap between text-based music composition and modern DAW workflows. Input music using the intuitive MML syntax, and instantly generate MIDI data that can be recorded directly to your DAW tracks. Perfect for rapid prototyping, chiptune composition, and educational purposes.

## Features

- **🎵 Full MML Support**: Complete Music Macro Language implementation with notes, rests, octaves, durations, tempo, volume, and loops
- **🎛️ Real-time Conversion**: Instant MML-to-MIDI conversion with live preview
- **🎹 MIDI Effect**: Functions as both MIDI effect and synthesizer in your DAW
- **⚡ Cubase 14 Optimized**: Specifically optimized for Cubase 14 compatibility and performance
- **🖥️ Intuitive GUI**: Clean interface with MML text editor, convert button, and status feedback
- **🔄 Loop Support**: Advanced loop constructs with customizable repeat counts
- **📝 Error Reporting**: Detailed error messages with position information for debugging

## Requirements

- **Platform**: Windows 10/11 (x64)
- **DAW**: Any VST3-compatible DAW (optimized for Cubase 14)
- **Framework**: JUCE 6.0+
- **Compiler**: Visual Studio 2022
- **Build Tool**: MSBuild or Visual Studio IDE

## Building

### Prerequisites

1. Install [JUCE Framework](https://juce.com/get-juce) 6.0 or later
2. Install Visual Studio 2022 with C++ development tools
3. Clone this repository

### Build Steps

```bash
# Navigate to the build directory
cd "Builds/VisualStudio2022"

# Build Debug version
msbuild MML.sln /p:Configuration=Debug /p:Platform=x64

# Build Release version
msbuild MML.sln /p:Configuration=Release /p:Platform=x64
```

**Alternative**: Open `MML.jucer` in Projucer to configure and regenerate project files, then build in Visual Studio 2022.

### Build Outputs

- **Debug**: `Builds/VisualStudio2022/x64/Debug/VST3/MML.vst3/`
- **Release**: `Builds/VisualStudio2022/x64/Release/VST3/MML.vst3/`

## Installation

1. Build the plugin using the steps above
2. Copy the `MML.vst3` folder to your VST3 plugins directory:
   - Windows: `C:\Program Files\Common Files\VST3\`
3. Restart your DAW
4. Load "MML" from the Filter, Fx, or Instrument categories

## MML Syntax Reference

### Basic Notes
```
c d e f g a b    # Natural notes
c+ d# f-         # Sharp (+/#) and flat (-) accidentals
```

### Durations
```
c1     # Whole note
c2     # Half note  
c4     # Quarter note (default)
c8     # Eighth note
c16    # Sixteenth note
c32    # Thirty-second note
c4.    # Dotted quarter note (1.5x duration)
c4&    # Tied note
```

### Octaves
```
o4     # Set octave to 4 (middle octave)
>      # Increase octave by 1
<      # Decrease octave by 1
```

### Rests
```
r4     # Quarter rest
r8.    # Dotted eighth rest
```

### Global Settings
```
l8     # Set default length to eighth note
t120   # Set tempo to 120 BPM (range: 20-300)
v100   # Set volume to 100 (range: 0-127)
```

### Loops
```
[cde]        # Loop 2 times (default)
[fgab]*3     # Loop 3 times
[gab]4       # Loop 4 times (alternative syntax)
```

### Complete Example
```
t140 v80 o4 l8 
c d e f [g a b >c]*2 <g4 r4
c+ d# f- g2.
```

## Usage

1. **Load the Plugin**: Add MML as a MIDI effect or instrument in your DAW
2. **Input MML Code**: Type or paste your MML text in the editor
3. **Convert**: Click the "Convert" button or press Enter
4. **Record MIDI**: The generated MIDI will be output to your track
5. **Edit & Iterate**: Modify the MML and convert again as needed

## Architecture

### Core Components

- **MMLPluginProcessor**: Main audio processor handling MIDI generation and output
- **MMLPluginEditor**: GUI component with text editor and controls
- **EnhancedMMLParser**: Core MML parsing engine with comprehensive syntax support

### Data Flow

1. User inputs MML text in the editor
2. Parser validates and converts MML to internal representation
3. MIDI sequence is generated with proper timing and note data
4. MIDI messages are output during the audio processing callback
5. DAW receives and can record the MIDI data

## Development

### Project Structure

```
Source/
├── MMLPluginProcessor.*     # Main processor (MIDI generation)
├── MMLPluginEditor.*        # GUI components and user interaction
└── MMLParser/
    └── EnhancedMMLParser.*  # MML parsing and MIDI conversion
```

### Key Configuration

- **Namespace**: `MMLPlugin`
- **Plugin Type**: MIDI Effect + Synthesizer hybrid
- **JUCE Modules**: Audio processors, GUI basics, core utilities
- **VST3 Categories**: Filter, Fx, Instrument

### Development Workflow

1. Modify source files in `Source/` directory
2. Use Projucer to update `MML.jucer` if needed
3. Rebuild using MSBuild or Visual Studio
4. Test in your DAW of choice

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

### Development Guidelines

- Follow the existing code style and naming conventions
- Ensure Cubase 14 compatibility when making changes
- Add appropriate error handling and validation
- Test thoroughly with various MML inputs

## License

This project is open source. Please see the license file for details.

## Support

- **Issues**: Report bugs and request features via GitHub Issues
- **Documentation**: See `CLAUDE.md` for development guidelines
- **MML Reference**: Comprehensive syntax guide included above

---

**Note**: This plugin is specifically optimized for Cubase 14 but should work with any VST3-compatible DAW.
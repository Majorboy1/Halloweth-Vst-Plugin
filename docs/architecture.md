# Halloweth Architecture

## Baseline

Halloweth is a JUCE 8.0.8, C++17 VST3 and Standalone instrument built with CMake and
Visual Studio 2022. The Release build and the current real-processor regression suite pass.
The plugin product, bundle, CMake targets, and C++ namespaces use the HALLOWETH name.

The current audio path is:

```text
Host MIDI + APVTS parameters
        -> PluginProcessor
        -> AudioEngine
           -> SynthVoice (oscillator synth)
           -> SampleVoice (JSON sample maps and drums)
            -> SfzEngine (sfizz SFZ instruments)
        -> EffectChain
        -> host output
```

Sample files and maps are loaded away from the audio callback. Loaded sample data is
immutable while voices may reference it. The engine supports synth, multisample map, drum
and SFZ modes. JSON maps describe key ranges, root notes, velocity layers, round robin,
looping and one-shot playback. Factory guitar presets use bundled Meta Open Sound
recordings; there is still no bundled piano sample bank.

## Salamander Evaluation

The separately cloned `SalamanderPianoApp` builds a Windows VST3 and Standalone target
against MSVC 14.44, JUCE 8.0.8 and sfizz. Its application code is MIT-licensed; sfizz is
BSD 2-Clause. The Salamander Grand Piano recordings are a separate work by Alexander Holm
under CC BY 3.0 and require attribution when redistributed.

The project is a complete piano application, not a component library. Halloweth now vendors
the pinned sfizz C API for SFZ parsing and rendering; it does not copy Salamander's processor,
editor, sample manager or Android asset-management code. The Halloweth adapter loads files
off-thread, dispatches MIDI with block offsets and renders through the shared output/effects
chain. The upstream wrapper was not reused: it routes MIDI to one channel and its reverb
path copies a buffer in `processBlock`, which can allocate. sfizz's streaming behavior still
needs performance testing with larger banks.

The Salamander source checkout contains Git LFS pointer files rather than the desktop
sample payloads. The Release VST3 and Standalone binaries were built successfully, but
full piano playback and FL Studio behavior cannot be confirmed from that checkout until a
complete, licensed sample bank is installed. A single CC BY 3.0 C4 velocity-layer sample
was fetched to a temporary directory for evaluation; it was not added to Halloweth.

## Module Boundaries

Keep the processor, synth, JSON sample-map, drum and effects paths intact. SFZ is a separate
block-rendered backend rather than a replacement for the existing JUCE sampler:

```text
Source/
  AudioEngine/
    AudioEngine.*              Engine selection and MIDI routing
    SynthVoice.*               Existing oscillator synthesis
    SampleVoice.*              Existing JSON-map and drum playback
    SfzInstrument.*            sfizz-backed SFZ instance and MIDI adapter
  Instruments/
    SampleMap.*                Existing JSON map model
    InstrumentLoader.*         Existing WAV/AIFF sample-map loader
    Piano/                     Piano-specific configuration and presets
  Presets/                     Preset metadata and instrument references
Resources/
  GuitarSamples/               Bundled CC0 guitar sample subset and notices
  SampleMaps/                  User-facing maps and templates
  Licenses/                    Notices for each redistributed library
Tests/                         Audio output, MIDI, load and realtime regressions
```

Each loaded SFZ owns one sfizz synth instance. The adapter sends note, note-off, CC, pitch
wheel and channel pressure events and renders directly into the host buffer. Instances load
off the audio thread and retire only when no callback is using them. The current automated
fixture verifies a generated one-region SFZ and WAV; compatibility with ARIA extensions,
the complete Salamander map and its full sample bank has not yet been verified. No
Salamander piano preset is exposed in the factory browser yet.

## Licensing and Integration Rules

- Preserve the Salamander application's MIT notice if any of its application code is reused.
- Preserve the sfizz BSD 2-Clause notice in Halloweth's third-party notices if sfizz is
  linked into a distributed build.
- Bundle the license texts for sfizz's linked dependencies; these are copied from the pinned
  source tree into `Resources/Licenses/sfizz/` and then into VST3/Standalone resources.
- Include Alexander Holm's CC BY 3.0 attribution and license with any Salamander samples
  redistributed by Halloweth. Do not fetch or bundle the full library implicitly during a
  normal code build.
- Keep sample-library content separate from engine code so users can install, update and
  remove large banks independently.
- Never describe a synth patch as a recorded instrument; label unavailable sampled
  instruments clearly until their licensed assets are installed.

## Build and Verification

Halloweth uses `D:\devtools\halloweth\cmake\bin\cmake.exe`, Visual Studio 2022 x64,
JUCE 8.0.8 and sfizz pinned at `3993b5c`. The local CMake and compiler executables are not on PATH. The Salamander build
uses the same toolchain; its JUCE path was pointed at Halloweth's cached 8.0.8 source for
this local evaluation, so its original pinned JUCE submodule commit was not independently
verified. Re-establish its pinned dependency before claiming a reproducible upstream build.

# HALLOWETH

Cinematic hybrid synth and premium world-instrument platform — VST3 for Windows.

> **Build status: Phase 6 (expanded premium bank).** The plugin shell, MIDI, sample engine,
> multisample maps, drum kit, synth design, and a **rich Halloweth factory bank with guitar,
> piano, saxophone, percussion and drum presets** are implemented and being refined.
>
> **Vocal chops and the remaining effects do not exist yet.** The factory bank includes
> compact, CC0 nylon- and steel-string guitar recordings; other sample-based instruments
> still require audio you supply.
> See [Roadmap](#roadmap).

---

## 1. What this build does today

| Capability | Status |
|---|---|
| VST3 + standalone targets compile and load | implemented |
| Host automation (Master, Tone, ADSR, Loop, Engine, Synth) | implemented |
| MIDI input, 16-voice polyphony, pitch bend, per-voice ADSR | implemented |
| Sample playback: WAV/AIFF/FLAC/OGG, pitch tracking, looping | implemented |
| Multisample maps: key ranges, velocity layers, round robin, one-shot | implemented |
| CC0 sampled upright piano: all 88 keys, four velocity layers, SFZ | implemented |
| SFZ instruments: sfizz, async loading, MIDI events, block rendering, state recall | implemented; tested with a basic SFZ map |
| 16-pad drum kit: per-pad level, pan, tuning, one-shot, kit JSON save/load | implemented |
| Two-oscillator synth: PolyBLEP saw/square, sine, triangle, TPT filter | implemented |
| Synth sound design: filter envelope, LFO, sub oscillator, stereo spread | implemented |
| Engine switching (sample map / synth / drums / SFZ) | implemented |
| Effects: reverb, delay, chorus, LP/HP filter, drive, compressor, stereo width | implemented |
| Preset browser: search, category/genre filters, favourites, save/delete, import/export | implemented |
| Factory presets across guitars, piano, pads, keys, bass, leads, textures, drums | implemented |
| Asynchronous instrument loading, missing-file reporting | implemented |
| Save/restore state, including instrument and drum kit | implemented |
| Dark interface: navigation, preset strip, engine/drum/effect pages, keyboard, meters | implemented |
| Automated engine tests | implemented, 102 checks passing |
| Preset browser, vocal chops, remaining effects (phaser, flanger, EQ, transient) | **not implemented** |

See [Architecture](docs/architecture.md) for the current engine boundaries and SFZ details.
See the [sound library guide](docs/SOUND_LIBRARY_GUIDE.md) for guitar/piano recommendations,
Omnisphere-style alternatives, and Afrobeats kit sources and license notes.

Nothing in the interface is decorative: every control is connected to a real parameter,
and sections that are not built show an explicit "not implemented yet" page instead of
fake knobs.

## 2. Requirements

- Windows 10/11 x64
- Visual Studio 2022 Build Tools with the **Desktop development with C++** workload
  (JUCE 8 does not support MinGW)
- CMake 3.22 or newer
- Git (to fetch JUCE and initialize the pinned sfizz submodule)

This machine is currently set up with:

| Tool | Location |
|---|---|
| VS 2022 Build Tools | `D:\devtools\vs2022bt` |
| CMake 3.31.8 | `D:\devtools\halloweth\cmake\bin\cmake.exe` |
| Ninja 1.13.1 | `D:\devtools\halloweth\ninja\ninja.exe` |
| JUCE 8.0.8 | CMake FetchContent cache at `build-release\_deps\juce-src` |
| sfizz | `ThirdParty\sfizz` submodule, pinned at `3993b5c` |

## 3. Build

```powershell
.\scripts\build.ps1              # Release
.\scripts\build.ps1 -Config Debug
.\scripts\build.ps1 -Clean
```

Output:

```
build-release\HALLOWETH_artefacts\Release\VST3\HALLOWETH.vst3
build-release\HALLOWETH_artefacts\Release\Standalone\HALLOWETH.exe
```

Manual equivalent:

```powershell
cmake -S . -B build-release -G "Visual Studio 17 2022" -A x64
cmake --build build-release --config Release --parallel
```

The standalone app is the fastest way to check the plugin without a DAW.

### Tests

```powershell
.\build-release\HALLOWETH_Tests_artefacts\Release\HALLOWETH_Tests.exe
# or
cd build-release; ctest -C Release --output-on-failure
```

The suite drives the real processor (no mocks) and checks: silence when idle, note-on
produces audio, both channels carry signal, voice/peak readouts, chord polyphony,
release back to silence, patch parameter application, preset wrap-around, favourites,
state save/restore, and correct operation after a sample-rate/buffer-size change.

Latest run: **102/102 passed** (engine, MIDI, presets, preset files, state, synth pitch and
sound design, map JSON, zone selection, round robin, multisample pitch, one-shot/loop,
file I/O, async loads, drum kit, and every effect's behaviour).

## 4. Install in FL Studio

**FL Studio only scans the shared VST3 folder** (`C:\Program Files\Common Files\VST3`),
so the installer keeps the plugin data on `D:` and links it into the shared folder:

```
D:\VST3\HALLOWETH.vst3                                   <- real files
C:\Program Files\Common Files\VST3\HALLOWETH.vst3        <- junction to the above
```

Close FL Studio, then double-click `scripts\install-vst3-system.cmd` and click **Yes**
on the administrator prompt. It:

1. copies the bundle to `D:\VST3\HALLOWETH.vst3`
2. replaces the shared-folder entry with a junction to it
3. verifies the plugin binary resolves through the junction

Then start FL Studio → **Options → Manage plugins → Find installed plugins**.
HALLOWETH appears under **Generators → VST3**.

Because the files live on `D:`, later builds only need `scripts\build.ps1` plus a re-run of
the installer to update.

### Why not a `.dll`?

VST2 (`.dll`) was discontinued by Steinberg in 2018 and its SDK is no longer distributable,
so JUCE 8 cannot build VST2 plugins. FL Studio 2024/2025/2026 supports VST3 natively —
the `.vst3` **bundle** (a folder containing `Contents\x86_64-win\HALLOWETH.vst3`) is the
correct and current format.

The plugin is linked against the **static** MSVC runtime, so it has no dependency on the
VC++ redistributable.

## 5. Project layout

```
CMakeLists.txt              JUCE plugin target (VST3 + Standalone)
Source/
  PluginProcessor.{h,cpp}   Audio/MIDI processing, parameters, state, async sample loads
  PluginEditor.{h,cpp}      Editor shell and page switching
  AudioEngine/
    AudioEngine.{h,cpp}     Voice ownership, instrument slot, engine mode and parameters
    SynthVoice.{h,cpp}      Two-oscillator subtractive voice (PolyBLEP + TPT filter)
    SampleVoice.{h,cpp}     Zone-based sample playback with looping and one-shot
    SampleData.h            Immutable sample buffer + root note + loop points
    SampleLoader.{h,cpp}    WAV/AIFF reading (message/background thread only)
  Instruments/
    SampleMap.{h,cpp}       Zone/map model + JSON read/write
    DrumKit.{h,cpp}         16-pad kit model + JSON read/write
    InstrumentLoader.{h,cpp} Resolves maps and kits into playable instruments
  Effects/
    EffectChain.{h,cpp}     Serial chain: filter, drive, chorus, delay, reverb,
                            compressor, stereo width
  Parameters/
    ParameterIDs.h          Stable parameter identifiers
  Presets/
    PresetManager.{h,cpp}   Factory + user presets, favourites, import/export
    FactoryPresets.{h,cpp}  The factory preset definitions
  UI/
    Theme.h                 Colours, fonts, card drawing
    HallowethLookAndFeel     Custom knobs and buttons
    NavigationBar           Left navigation rail
    PresetBar               Preset strip + MIDI lamp
    MidiActivityLight
    Knobs/LabeledKnob
    Pages/HomePage          Build status, output meter, master/tone
    Pages/InstrumentsPage   Engine switching, sample/map loading, synth + envelope
    Pages/DrumsPage         16-pad grid, per-pad level/pan/tuning, kit load/save
    Pages/EffectsPage       Seven effects with enable switches
    Pages/PresetsPage       Search, filters, favourites, save/delete, import/export
    Pages/PlaceholderPage   Honest "not implemented" page
scripts/
  build.ps1                 Configure + build
  install-vst3.ps1          Copy bundle into a VST3 folder
Tests/
  AudioEngineTests.cpp      Engine, MIDI, preset and state tests
ThirdParty/JUCE/            JUCE 8.0.8 (not redistributed with binaries)
Resources/PianoSamples/     CC0 Meta Piano SFZ and OGG samples
Resources/DrumKits/         External Afrobeats sample-pack sources
```

## 6. Presets

Factory presets live in code; user presets are JSON files in
`%APPDATA%\Halloweth\Presets`. Favourites are stored in the same folder.

- The preset strip at the top always shows the current preset and steps through the
  whole bank; the star toggles a favourite.
- **PRESETS** page: search, category and genre filters, double-click to load,
  save the current state as a user preset, delete user presets, import/export JSON.
- Presets marked **NEEDS SAMPLE** (the drum kits) require audio you supply — the plugin
  never pretends those recordings exist.

## 7. Threading and real-time rules

- `processBlock` performs **no** allocation, file I/O, locking or logging.
- Editor → audio communication uses `std::atomic` values (voice count, output peak).
- Parameter changes flow through `AudioProcessorValueTreeState` smoothed values.
- The on-screen keyboard feeds the same `MidiKeyboardState` used by the audio thread,
  merged with host MIDI each block.
- Sample files are read on a worker thread; the finished sample is published on the message
  thread and then swapped in with an atomic pointer.
- Sample data is immutable and is never freed while any voice could reference it. Replacing
  an instrument only swaps a pointer; resident samples are pruned in `prepareToPlay`,
  which never runs while audio is flowing.

## 8. Roadmap

| Phase | Scope | State |
|---|---|---|
| 1 | Foundation: build system, plugin shell, UI frame, MIDI + audio path | **done** |
| 2 | Sample engine: WAV/AIFF loading, pitch tracking, looping, ADSR, async load | **done** |
| 3 | Multisample mapping, zone/velocity/round-robin selection, synth engine | **done** |
| 3b | Drum engine: 16 pads, per-pad level/pan/tuning, kit JSON | **done** |
| 4 | Effects: reverb, delay, chorus, filter, drive, compressor, width | **done** |
| 4b | Remaining effects (phaser, flanger, EQ, transient, stutter, reverse) | planned |
| 5 | Preset browser, factory presets, favourites, save/import/export | **done** |
| 5b | Vocal chops engine and page | next |
| 6 | Polish, automated tests, FL Studio validation, release build | planned |

## 9. Sample content and licensing

The factory nylon and steel guitars and the Meta Upright Piano use Meta Open Sound
recordings under CC0 1.0. Their sample maps, source notes and license texts are in
`Resources/GuitarSamples/` and `Resources/PianoSamples/`. The SFZ engine uses the
BSD-2-Clause sfizz library; licenses for sfizz and its linked dependencies are included
under `Resources/Licenses/` and copied into plugin/app resources at build time.
Other factory instruments are synthesis patches or require user-supplied audio. No
Omnisphere or other proprietary sample content is copied, extracted or redistributed.

## 10. Known limitations of the current build

- The bundled piano is an upright; sampled grand pianos, drums and other multisample
  instruments can be loaded from user-supplied libraries. `Resources/SampleMaps/example-multisample.json`
  is a documented template.
- Effects run as one serial master chain; there is no per-instrument routing yet.
- Implemented effects: reverb, delay, chorus, low/high-pass filter, drive, compressor,
  stereo width. Phaser, flanger, EQ, transient shaping, stutter and reverse are not built.
- Drum kits save to JSON, but there are no factory kits (no audio content).
- Key ranges and velocity layers are hard-switched; there are no velocity crossfades.
- Zone selection falls back to any readable zone when nothing matches a note.
- Samples longer than 120 seconds are truncated; there is no streaming.
- Synth is two oscillators + sub, one filter, one LFO; no unison voices, FM or wavetables.
- Vocal chops (pitch, reverse, stutter, slicing) are not implemented.
- No arpeggiator or MIDI learn yet.
- UI uses system typefaces; no packaged custom fonts or icons yet.

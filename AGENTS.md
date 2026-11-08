# Agent guidance

HALLOWETH is a JUCE 8 C++17 instrument plugin. Keep changes focused and preserve
the existing product name, parameter IDs, preset compatibility, and Windows VST3
support.

## Project boundaries

- `Source/AudioEngine` owns voice generation and sample playback.
- `Source/Effects` owns the serial audio effect chain.
- `Source/PluginProcessor.*` owns host parameters, state, MIDI, and async loading.
- `Source/UI` contains the editor and message-thread UI only.
- `Source/Presets` owns factory and user preset behavior.
- `Tests/AudioEngineTests.cpp` is the existing processor and engine test suite.

## Audio and threading rules

- `processBlock` and functions it calls must not allocate, perform file I/O, lock,
  log, or access UI objects.
- Keep sample loading and filesystem work off the audio thread. Publish completed
  loads safely to the processor/message thread.
- Preserve existing automation IDs and serialized state compatibility when
  changing parameters.
- Keep UI mutations on JUCE's message thread.

## Build

Use `scripts/build.ps1` on Windows with Visual Studio 2022 and CMake 3.22+.
JUCE is read from `ThirdParty/JUCE` when present and otherwise fetched at the
version pinned in `CMakeLists.txt`. Do not commit generated build directories,
JUCE checkout contents, local environment files, or installed plugin bundles.

## Change discipline

- Prefer small, reviewable changes that follow the surrounding JUCE/C++ style.
- Update `README.md` when user-facing capabilities or build instructions change.
- Do not claim an effect, instrument, or preset capability that is not implemented.
- Only bundle audio with a license that permits redistribution in a plugin; include
  its source, exact license, and required attribution beside the samples.
- Run the existing test executable after audio engine, parameter, state, or preset
  changes when the task asks for verification.

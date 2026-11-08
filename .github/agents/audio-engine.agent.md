---
name: Audio Engine
description: Work on HALLOWETH's real-time audio engine, synth voices, sample playback, and effects.
tools: [read, edit, search]
---

Focus on `Source/AudioEngine` and `Source/Effects`. Preserve the real-time rules in
`AGENTS.md`: no allocations, file I/O, locks, logging, or UI access from
`processBlock` or its audio-thread call chain. Keep voice and effect state prepared
before processing, and preserve sample lifetime guarantees. Keep changes narrowly
scoped and explain any parameter or sound behavior changes.

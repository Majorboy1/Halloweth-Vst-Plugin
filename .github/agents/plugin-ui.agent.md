---
name: Plugin UI
description: Improve HALLOWETH's JUCE editor, controls, navigation, and visual design.
tools: [read, edit, search]
---

Focus on `Source/PluginEditor.*` and `Source/UI`. Keep UI work on JUCE's message
thread, use the shared theme and custom look-and-feel, and bind controls to the
existing processor parameters or state. Do not add decorative controls for
unimplemented audio features. Update user-facing documentation when behavior
changes.

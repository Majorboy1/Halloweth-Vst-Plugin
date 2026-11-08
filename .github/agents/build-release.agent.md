---
name: Build and Release
description: Maintain HALLOWETH's CMake build, Windows packaging scripts, and release documentation.
tools: [read, edit, search]
---

Focus on `CMakeLists.txt`, `scripts/`, `.github/workflows/`, and build-related
documentation. Preserve the MSVC and VST3 requirements, keep JUCE pinned, and do
not commit generated artifacts, local paths outside documented fallbacks, or
credentials. Prefer changes that work on a clean Windows runner as well as the
maintainer's machine.

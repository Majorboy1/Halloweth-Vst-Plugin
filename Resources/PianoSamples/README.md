# Bundled piano

`meta-piano/` contains Meta Open Sound's full 88-key upright piano: 352 OGG
recordings arranged as four velocity layers, the original SFZ map, its design
notes, checksums, and the CC0 1.0 license. The source release is
[Meta Piano v1.0.0](https://github.com/facebookincubator/Meta-open-sound/releases/tag/meta-piano-v1.0.0).

The SFZ is loaded by the **Meta Upright Piano** factory preset. CMake copies this
folder beside the VST3, standalone app, and test executable so the preset can
resolve its SFZ and relative samples after installation.

The sample collection is CC0 1.0 and may be used and redistributed. The complete
license text is included in `meta-piano/LICENSE`.

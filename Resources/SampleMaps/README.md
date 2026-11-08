# Sample map format

HALLOWETH instruments are described by plain JSON so you can author, version and share
them. A map points at WAV/AIFF files that **you** own or have licensed.

## Important

`example-multisample.json` is a **template**. The sample files it references do not exist
and are not shipped with this project. Replace the paths with your own recordings before
loading it — otherwise HALLOWETH reports the missing files and refuses to use that map.

## Zone fields

| Field | Meaning |
|---|---|
| `file` | Path to a WAV/AIFF file. Relative paths resolve against the map file's folder. |
| `keyLow` / `keyHigh` | MIDI note range this zone answers to (0–127, inclusive). |
| `rootNote` | MIDI note at which the recording plays back at its original pitch. |
| `velLow` / `velHigh` | MIDI velocity range (1–127). Zones sharing a key range form layers. |
| `roundRobin` | Ordering hint inside a round-robin group (0, 1, 2 …). |
| `loopStart` / `loopEnd` | Sample frames for looping. `0`/`0` uses the file's embedded loop. |
| `oneShot` | `true` ignores note-off — for drums, hits and vocal chops. |

## How a note is chosen

1. Zones matching both key **and** velocity are preferred.
2. If none match both, zones matching the key range are used.
3. If no zone covers the note, any readable zone is used so the instrument still sounds.
4. Among the chosen group, successive note-ons rotate through the zones (round robin).

## Example

```json
{
  "name": "My Piano",
  "category": "Piano",
  "zones": [
    { "file": "samples/C4_soft.wav", "keyLow": 60, "keyHigh": 71,
      "rootNote": 60, "velLow": 1, "velHigh": 63 },
    { "file": "samples/C4_hard.wav", "keyLow": 60, "keyHigh": 71,
      "rootNote": 60, "velLow": 64, "velHigh": 127 }
  ]
}
```

Load it from **INSTRUMENTS → LOAD MAP**.

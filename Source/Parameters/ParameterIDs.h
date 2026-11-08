#pragma once

namespace halloweth::param
{
    /** Stable parameter identifiers. These strings are part of the saved-state
        format, so they must never change once released. */
    inline constexpr const char* masterGain  = "masterGain";
    inline constexpr const char* tone        = "tone";

    inline constexpr const char* attack      = "attack";
    inline constexpr const char* decay       = "decay";
    inline constexpr const char* sustain     = "sustain";
    inline constexpr const char* release     = "release";

    inline constexpr const char* loopEnabled = "loopEnabled";

    /** 0 = sample map, 1 = synth engine. */
    inline constexpr const char* engineMode  = "engineMode";

    inline constexpr const char* synthWave      = "synthWave";
    inline constexpr const char* synthDetune    = "synthDetune";
    inline constexpr const char* synthMix       = "synthMix";
    inline constexpr const char* synthCutoff    = "synthCutoff";
    inline constexpr const char* synthResonance = "synthResonance";
    inline constexpr const char* synthSub       = "synthSub";
    inline constexpr const char* synthSpread    = "synthSpread";
    inline constexpr const char* synthFilterEnv = "synthFilterEnv";
    inline constexpr const char* synthLfoDepth  = "synthLfoDepth";

    // Effects
    inline constexpr const char* fxReverbOn   = "fxReverbOn";
    inline constexpr const char* fxReverbMix  = "fxReverbMix";
    inline constexpr const char* fxReverbSize = "fxReverbSize";
    inline constexpr const char* fxReverbDamp = "fxReverbDamp";

    inline constexpr const char* fxDelayOn       = "fxDelayOn";
    inline constexpr const char* fxDelayTime     = "fxDelayTime";
    inline constexpr const char* fxDelayFeedback = "fxDelayFeedback";
    inline constexpr const char* fxDelayMix      = "fxDelayMix";

    inline constexpr const char* fxChorusOn    = "fxChorusOn";
    inline constexpr const char* fxChorusRate  = "fxChorusRate";
    inline constexpr const char* fxChorusDepth = "fxChorusDepth";
    inline constexpr const char* fxChorusMix   = "fxChorusMix";

    inline constexpr const char* fxFilterOn     = "fxFilterOn";
    inline constexpr const char* fxFilterCutoff = "fxFilterCutoff";
    inline constexpr const char* fxFilterRes    = "fxFilterRes";
    inline constexpr const char* fxFilterMode   = "fxFilterMode";

    inline constexpr const char* fxDriveOn     = "fxDriveOn";
    inline constexpr const char* fxDriveAmount = "fxDriveAmount";
    inline constexpr const char* fxDriveTone   = "fxDriveTone";

    inline constexpr const char* fxCompOn        = "fxCompOn";
    inline constexpr const char* fxCompThreshold = "fxCompThreshold";
    inline constexpr const char* fxCompRatio     = "fxCompRatio";
    inline constexpr const char* fxCompMakeup    = "fxCompMakeup";

    inline constexpr const char* fxWidthOn     = "fxWidthOn";
    inline constexpr const char* fxWidthAmount = "fxWidthAmount";
}

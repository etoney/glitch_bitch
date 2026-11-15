// GLITCH BITCH — factory presets (parameter id -> real value). Anything not listed is reset to its default.
// Presets never touch THE BUTTON itself, the HOLD/LATCH mode, the capture settings or the output level.
#pragma once
#include <string>
#include <utility>
#include <vector>
#include <cstdint>

namespace gb
{
struct Preset { std::string name; std::vector<std::pair<std::string, float>> values; };

inline void addPattern (Preset& p, uint16_t bits)
{
    for (int i = 0; i < 16; ++i) p.values.push_back ({ "step" + std::to_string (i), (float) ((bits >> i) & 1u) });
}

// note-value indices (see kNoteBeats): 0 1/2, 1 1/4, 2 1/4T, 3 1/8, 4 1/8T, 5 1/16, 6 1/16T, 7 1/32, 8 1/32T, 9 1/64
// snap indices: 0 Off, 1 1/16, 2 1/8, 3 1/4, 4 1/2, 5 1 bar
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> v;
        auto add = [&] (std::string name, std::vector<std::pair<std::string, float>> vals, int pattern = -1)
        {
            Preset p { std::move (name), std::move (vals) };
            if (pattern >= 0) addPattern (p, (uint16_t) pattern);
            v.push_back (std::move (p));
        };
        add ("Init (Stutter 1/16)", {});
        add ("Chopper 1/32",       { { "stutterSpeed", 7 }, { "stutterSmooth", 1.5f } }, 0b1010101010101010);
        add ("Gate Keeper",        { { "stutterSpeed", 6 }, { "stutterDepth", 0.85f }, { "stutterSmooth", 6 } }, 0b0110110101101101);
        add ("Beat Repeat 1/8",    { { "stutterArm", 0 }, { "repeatArm", 1 }, { "repeatLength", 3 }, { "snap", 3 } });
        add ("Machine Gun",        { { "stutterArm", 0 }, { "repeatArm", 1 }, { "repeatLength", 7 }, { "snap", 2 } });
        add ("Sinking Repeat",     { { "stutterArm", 0 }, { "repeatArm", 1 }, { "repeatLength", 5 }, { "repeatDrop", -1.0f }, { "snap", 3 } });
        add ("Rising Panic",       { { "stutterArm", 0 }, { "repeatArm", 1 }, { "repeatLength", 7 }, { "repeatDrop", 1.5f }, { "snap", 3 } });
        add ("Tape Kill",          { { "stutterArm", 0 }, { "tapeStopArm", 1 }, { "tapeStopTime", 0.9f } });
        add ("Spin Cycle",         { { "stutterArm", 0 }, { "tapeStopArm", 1 }, { "tapeStartArm", 1 }, { "tapeStopTime", 0.6f }, { "tapeStartTime", 0.5f } });
        add ("Power Up",           { { "stutterArm", 0 }, { "tapeStartArm", 1 }, { "tapeStartTime", 1.2f }, { "tapeStartShape", 0.7f } });
        add ("Backmask",           { { "stutterArm", 0 }, { "reverseArm", 1 }, { "reverseLength", 1 }, { "snap", 3 } });
        add ("Scatterbrain",       { { "stutterArm", 0 }, { "scatterArm", 1 }, { "scatterSlice", 5 }, { "scatterChaos", 0.7f }, { "scatterRange", 4 }, { "snap", 1 } });
        add ("Broken CD",          { { "stutterArm", 0 }, { "scatterArm", 1 }, { "scatterSlice", 7 }, { "scatterChaos", 0.95f }, { "scatterRange", 2 },
                                     { "crushArm", 1 }, { "crushBits", 11 }, { "crushRate", 14000 } });
        add ("8-Bit Meltdown",     { { "stutterSpeed", 7 }, { "crushArm", 1 }, { "crushBits", 4 }, { "crushRate", 4000 } }, 0b0100100101001001);
        add ("Frozen Scream",      { { "stutterArm", 0 }, { "freezeArm", 1 }, { "freezeMix", 0.85f }, { "freezeSize", 0.6f }, { "diveArm", 1 }, { "diveAmount", -7 }, { "diveTime", 2.5f } });
        add ("Death Dive",         { { "stutterArm", 0 }, { "diveArm", 1 }, { "diveAmount", -24 }, { "diveTime", 1.5f }, { "crushArm", 1 }, { "crushBits", 9 }, { "crushRate", 9000 } });
        add ("Total Destruction",  { { "stutterSpeed", 7 }, { "repeatArm", 1 }, { "repeatLength", 5 }, { "repeatDrop", -0.5f }, { "scatterArm", 1 }, { "scatterChaos", 0.5f },
                                     { "crushArm", 1 }, { "crushBits", 7 }, { "tapeStopArm", 1 }, { "tapeStartArm", 1 }, { "snap", 3 } }, 0b0010010010010010);
        return v;
    }();
    return list;
}
} // namespace gb

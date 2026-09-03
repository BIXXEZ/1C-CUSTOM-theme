#pragma once

// ============================================================
// DARKMOD CORE - COLOR MAPPING
//
// The rule set in darkmod.ini and the matcher that decides what a
// given on-screen color becomes.
//
// FindColorMapping() is the whole reason this library exists.
// Palette Studio has to predict what the injected DLL will do to
// a screenshot; if it used its own "cleaner" matcher the preview
// would be a picture of something that never happens. So the DLL
// and Studio call the same function, and there is no second
// implementation to drift.
// ============================================================

#include "DarkModColor.h"

#include <string>
#include <vector>

namespace dm
{

    struct Mapping
    {
        uint8_t sr = 0;
        uint8_t sg = 0;
        uint8_t sb = 0;

        uint8_t dr = 0;
        uint8_t dg = 0;
        uint8_t db = 0;

        bool enabled = true;
    };


    using MappingList =
        std::vector<Mapping>;


    inline Color MappingSource(
        const Mapping& m)
    {
        return Color{ m.sr, m.sg, m.sb };
    }

    inline Color MappingDestination(
        const Mapping& m)
    {
        return Color{ m.dr, m.dg, m.db };
    }


    // ------------------------------------------------------------
    // THE MATCHER
    //
    // Three levels, in this order:
    //
    //   1. exact source match wins immediately, so a deliberate
    //      rule is never blocked by anything below
    //   2. if the incoming color is one this rule set PRODUCES
    //      (within DESTINATION_MATCH_TOLERANCE_SQ), refuse - this
    //      is what stops a repaint cycle where our own output gets
    //      treated as a source and mapped again
    //   3. otherwise take the nearest source within
    //      COLOR_MATCH_RADIUS, which is what covers anti-aliasing
    //      and the 5-bit quantisation in darkmod_colors.json
    //
    // Rule order matters: on equal distance the FIRST rule wins,
    // because the comparison is strict. Anything rebuilding this
    // list has to preserve the order of the source colors.
    //
    // Returns false when the color must be left alone.
    // ------------------------------------------------------------

    bool FindColorMapping(
        const MappingList& mappings,
        int R,
        int G,
        int B,
        int& outR,
        int& outG,
        int& outB);


    // ------------------------------------------------------------
    // FILE FORMAT:  enabled|#SRC|#DST
    // ------------------------------------------------------------

    bool ParseMappingLine(
        const std::wstring& line,
        Mapping& out);

    std::string FormatMappingLine(
        const Mapping& m);

    MappingList ParseMappingText(
        const std::wstring& text);

    std::string FormatMappingText(
        const MappingList& mappings);


    // ------------------------------------------------------------
    // RULE BUILDING
    //
    // Every source color is classified into one of the 12
    // families and receives that family's replacement color. This
    // is the launcher's palette writer, kept here so Studio builds
    // byte-identical rule sets.
    // ------------------------------------------------------------

    MappingList BuildMappings(
        const std::vector<Color>& sources,
        const Color(&familyDst)[FAMILY_COUNT]);

    //
    // The source colors used before any analysis result exists.
    // Kept here because the collision check, the rule writer and
    // Studio's renderer all have to agree on it.
    //
    const std::vector<Color>& BootstrapSources();

}

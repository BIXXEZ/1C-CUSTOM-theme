#pragma once

// ============================================================
// DARKMOD CORE - COLORS JSON
//
// darkmod_colors.json is the handshake between the DLL, which
// measures what 1C actually draws, and everything that decides
// what to draw instead.
//
// The histogram is 5 bits per channel, and the color written out
// is the CENTER of the bucket - not a color that was necessarily
// on screen. That single fact is why COLOR_MATCH_RADIUS exists,
// and why the writer and the readers have to live next to each
// other: a change to the bucket size here silently changes what
// the matcher over in DarkModMapping.cpp has to tolerate.
// ============================================================

#include "DarkModColor.h"
#include "DarkModCapture.h"

#include <array>
#include <string>
#include <vector>

namespace dm
{

    // ------------------------------------------------------------
    // ONE COLOR AS STORED IN THE FILE
    // ------------------------------------------------------------

    struct LiveColor
    {
        std::string hex;
        double percentage = 0.0;
        uint64_t pixels = 0;
    };


    // ------------------------------------------------------------
    // MINIMAL JSON READING
    //
    // Deliberately not a parser. The file is written by one known
    // producer, so scanning for the keys is enough and keeps a
    // JSON library out of an injected DLL.
    // ------------------------------------------------------------

    std::string ParseJsonString(
        const std::string& json,
        size_t position);

    bool ParseJsonNumberAfter(
        const std::string& json,
        size_t position,
        double& result);

    std::string ParseJsonMode(
        const std::string& json);

    std::vector<LiveColor> ParseLiveColors(
        const std::string& json);


    // ------------------------------------------------------------
    // HISTOGRAM
    //
    // 5 bits/channel: 32 * 32 * 32 = 32768 bins.
    // ------------------------------------------------------------

    constexpr size_t SCREEN_HISTOGRAM_SIZE = 32768;

    struct ScreenHistogram
    {
        std::array<uint64_t,
            SCREEN_HISTOGRAM_SIZE>
            bins{};

        uint64_t pixels = 0;
    };

    uint32_t ScreenBin(
        int r,
        int g,
        int b);

    ScreenHistogram AnalyzeScreen(
        const CapturedScreen& screen);


    struct ScreenColor
    {
        uint64_t pixels = 0;

        int r = 0;
        int g = 0;
        int b = 0;
    };

    //
    // Most-used first. Ties keep bin order, which is why the rule
    // list built from this is reproducible run to run.
    //
    std::vector<ScreenColor> BuildTopScreenColors(
        const ScreenHistogram& histogram,
        size_t limit);


    // ------------------------------------------------------------
    // WRITING
    //
    // Defaults are the values the DLL has always written; they are
    // fields rather than literals only so the caller stays the one
    // that knows its own pid and counters.
    // ------------------------------------------------------------

    struct ColorsJsonInfo
    {
        int version = 1;
        const char* alphaVersion = "0.4";
        DWORD processId = 0;
        const char* mode = "ANALYZE";
        unsigned long long replacementsSinceReport = 0;
        size_t colorLimit = 100;
    };

    std::string FormatColorsJson(
        const ColorsJsonInfo& info,
        const CapturedScreen& screen,
        const ScreenHistogram& histogram);


    // ------------------------------------------------------------
    // ANALYSIS PAYLOAD REJECTION
    //
    // A capture is only "original colors" if MOD was off when it
    // was taken. This asks the set itself: how much of the screen
    // area is made of colors this palette produces?
    //
    // A clean 1C capture is ~86% #FCFCFC plus small accents, so it
    // cannot approach the threshold by accident; a capture of a
    // modded frame is almost entirely palette colors. Studio uses
    // the same check to refuse analysing an already-recolored
    // window instead of quietly producing nonsense.
    // ------------------------------------------------------------

    constexpr double OWN_OUTPUT_COVERAGE_PERCENT = 40.0;

    bool LooksLikeOwnOutput(
        const std::vector<LiveColor>& colors,
        const Color(&destinations)[FAMILY_COUNT]);

    //
    // Same question about a histogram, for callers that hold pixel
    // data rather than a parsed file.
    //
    bool LooksLikeOwnOutput(
        const ScreenHistogram& histogram,
        const Color(&destinations)[FAMILY_COUNT]);

}

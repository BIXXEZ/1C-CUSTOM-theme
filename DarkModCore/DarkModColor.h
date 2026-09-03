#pragma once

// ============================================================
// DARKMOD CORE - COLOR
//
// The single definition of a color, of the 12 semantic families,
// and of the numbers that decide whether two colors count as the
// same one.
//
// This used to exist twice: the launcher carried ClassifyFamily
// and ColorDistanceSq, the DLL carried its own copy of
// COLOR_MATCH_RADIUS with a comment asking the reader to keep the
// two in sync by hand. Palette Studio would have made it three,
// and a preview that disagreed with the DLL by one unit of radius
// is a preview that lies. So the constants live here and nowhere
// else.
// ============================================================

#include <windows.h>

#include <string>
#include <cstdint>

namespace dm
{

    // ------------------------------------------------------------
    // COLOR
    // ------------------------------------------------------------

    struct Color
    {
        BYTE r;
        BYTE g;
        BYTE b;
    };


    inline COLORREF ToColorRef(
        Color c)
    {
        return
            RGB(c.r, c.g, c.b);
    }


    inline bool ColorEquals(
        Color a,
        Color b)
    {
        return
            a.r == b.r &&
            a.g == b.g &&
            a.b == b.b;
    }


    int ColorDistanceSq(
        Color a,
        Color b);


    // ------------------------------------------------------------
    // MATCH TOLERANCES
    //
    // darkmod_colors.json stores 5-bit bucket centers, not exact
    // screen colors, so a source color can be up to 4 units per
    // channel away from what 1C actually drew - sqrt(3 * 8^2) is
    // about 13.8. Radius 18 covers that plus anti-aliasing without
    // swallowing neighbouring families.
    //
    // The destination tolerance is only about double -> byte
    // rounding on our own output, so it must stay far below the
    // match radius or the fuzzy step would start refusing real
    // source colors.
    // ------------------------------------------------------------

    constexpr int COLOR_MATCH_RADIUS = 18;

    constexpr int COLOR_MATCH_RADIUS_SQ =
        COLOR_MATCH_RADIUS *
        COLOR_MATCH_RADIUS;

    constexpr int DESTINATION_MATCH_TOLERANCE_SQ = 4;


    // ------------------------------------------------------------
    // TEXT CONVERSION
    // ------------------------------------------------------------

    std::string WToUtf8(
        const std::wstring& text);

    std::wstring Utf8ToW(
        const std::string& text);


    //
    // Lenient: anything unparsable yields the fallback. The
    // launcher passes its surface color so a corrupt config still
    // paints something sane, which is the behaviour it had before
    // this moved out of DarkModLauncher.cpp.
    //
    Color ParseHexColor(
        const std::string& hex,
        Color fallback = Color{ 0, 0, 0 });

    //
    // Strict: use this when a bad value has to be detected rather
    // than absorbed, e.g. when reading user files.
    //
    bool ParseHexColorStrict(
        const std::string& hex,
        Color& out);

    std::string FormatHex(
        Color c);

    std::wstring FormatHexW(
        Color c);

    //
    // "#RRGGBB" in wide text, as it appears in darkmod.ini.
    //
    bool ParseRGB(
        const std::wstring& text,
        int& r,
        int& g,
        int& b);


    // ------------------------------------------------------------
    // COLOR MATH
    // ------------------------------------------------------------

    struct Hsv
    {
        double h = 0.0;   // 0..360
        double s = 0.0;   // 0..1
        double v = 0.0;   // 0..1
    };

    Hsv ToHsv(
        Color c);

    Color FromHsv(
        double h,
        double s,
        double v);

    //
    // WCAG relative luminance and contrast ratio. Palette Studio
    // scores text readability with these; nothing in the launcher
    // needed them before.
    //
    double Luminance(
        Color c);

    double ContrastRatio(
        Color a,
        Color b);

    //
    // Perceived brightness, 0..1. Cheaper than Luminance and good
    // enough for ordering surfaces by lightness.
    //
    double Brightness(
        Color c);

    Color LerpColor(
        Color a,
        Color b,
        float t);

    Color DarkenColor(
        Color c,
        float amount);

    Color LightenColor(
        Color c,
        float amount);


    // ------------------------------------------------------------
    // 12 COLOR FAMILIES
    //
    // Every source color found on screen is assigned to exactly
    // one of these, and a palette is 12 replacement colors - one
    // per family. Changing the order or the count invalidates
    // every stored palette, so both are fixed.
    // ------------------------------------------------------------

    enum class ColorFamily
    {
        White,
        LightGray,
        Gray,
        DarkGray,
        Black,
        Red,
        Peach,
        Yellow,
        Green,
        Blue,
        Mauve,
        Pink
    };

    constexpr int FAMILY_COUNT = 12;

    const wchar_t* FamilyName(
        ColorFamily f);

    //
    // Stable ASCII key for files. FamilyName is Russian display
    // text and must never be written to disk.
    //
    const char* FamilyKey(
        ColorFamily f);

    ColorFamily ClassifyFamily(
        Color c);

    //
    // Clamped cast, for the many places that carry a family as an
    // int index.
    //
    inline ColorFamily FamilyAt(
        int index)
    {
        if (index < 0)
            index = 0;

        if (index >= FAMILY_COUNT)
            index = FAMILY_COUNT - 1;

        return
            static_cast<ColorFamily>(
                index);
    }

}

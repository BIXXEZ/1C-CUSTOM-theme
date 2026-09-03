// ============================================================
// DARKMOD CORE - COLOR
// ============================================================

#include "DarkModColor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dm
{

    // ============================================================
    // DISTANCE
    // ============================================================

    int ColorDistanceSq(
        Color a,
        Color b)
    {
        const int dr =
            static_cast<int>(a.r) -
            static_cast<int>(b.r);

        const int dg =
            static_cast<int>(a.g) -
            static_cast<int>(b.g);

        const int db =
            static_cast<int>(a.b) -
            static_cast<int>(b.b);

        return
            dr * dr +
            dg * dg +
            db * db;
    }


    // ============================================================
    // TEXT CONVERSION
    // ============================================================

    std::string WToUtf8(
        const std::wstring& text)
    {
        if (text.empty())
            return {};

        const int bytes =
            WideCharToMultiByte(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(
                    text.size()),
                nullptr,
                0,
                nullptr,
                nullptr);

        if (bytes <= 0)
            return {};

        std::string result(
            static_cast<size_t>(bytes),
            '\0');

        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(
                text.size()),
            result.data(),
            bytes,
            nullptr,
            nullptr);

        return result;
    }


    std::wstring Utf8ToW(
        const std::string& text)
    {
        if (text.empty())
            return {};

        const int chars =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(
                    text.size()),
                nullptr,
                0);

        if (chars <= 0)
            return {};

        std::wstring result(
            static_cast<size_t>(chars),
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(
                text.size()),
            result.data(),
            chars);

        return result;
    }


    // ============================================================
    // HEX
    // ============================================================

    static int HexDigit(
        char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';

        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;

        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;

        return -1;
    }


    bool ParseHexColorStrict(
        const std::string& hex,
        Color& out)
    {
        //
        // Trailing whitespace and CR are common in hand-edited
        // files, so measure the payload instead of the string.
        //

        size_t end =
            hex.size();

        while (
            end > 0 &&
            (hex[end - 1] == '\r' ||
                hex[end - 1] == '\n' ||
                hex[end - 1] == ' ' ||
                hex[end - 1] == '\t'))
        {
            --end;
        }

        size_t begin = 0;

        while (
            begin < end &&
            (hex[begin] == ' ' ||
                hex[begin] == '\t'))
        {
            ++begin;
        }

        if (
            end - begin != 7 ||
            hex[begin] != '#')
        {
            return false;
        }

        int digits[6]{};

        for (int i = 0; i < 6; ++i)
        {
            digits[i] =
                HexDigit(
                    hex[begin + 1 + i]);

            if (digits[i] < 0)
                return false;
        }

        out.r =
            static_cast<BYTE>(
                digits[0] * 16 + digits[1]);

        out.g =
            static_cast<BYTE>(
                digits[2] * 16 + digits[3]);

        out.b =
            static_cast<BYTE>(
                digits[4] * 16 + digits[5]);

        return true;
    }


    Color ParseHexColor(
        const std::string& hex,
        Color fallback)
    {
        Color out{};

        if (ParseHexColorStrict(
            hex,
            out))
        {
            return out;
        }

        //
        // The launcher's old parser only required the string to
        // start with "#RRGGBB" and ignored anything after it. Keep
        // that tolerance here so no existing caller changes
        // behaviour; only genuinely invalid digits now fall back.
        //

        if (
            hex.size() > 7 &&
            hex[0] == '#' &&
            ParseHexColorStrict(
                hex.substr(0, 7),
                out))
        {
            return out;
        }

        return fallback;
    }


    std::string FormatHex(
        Color c)
    {
        char text[16]{};

        sprintf_s(
            text,
            sizeof(text),
            "#%02X%02X%02X",
            static_cast<unsigned>(c.r),
            static_cast<unsigned>(c.g),
            static_cast<unsigned>(c.b));

        return text;
    }


    std::wstring FormatHexW(
        Color c)
    {
        wchar_t text[16]{};

        swprintf_s(
            text,
            _countof(text),
            L"#%02X%02X%02X",
            static_cast<unsigned>(c.r),
            static_cast<unsigned>(c.g),
            static_cast<unsigned>(c.b));

        return text;
    }


    bool ParseRGB(
        const std::wstring& text,
        int& r,
        int& g,
        int& b)
    {
        std::wstring value =
            text;

        while (
            !value.empty() &&
            (value.front() == L' ' ||
                value.front() == L'\t'))
        {
            value.erase(
                value.begin());
        }

        while (
            !value.empty() &&
            (value.back() == L' ' ||
                value.back() == L'\t' ||
                value.back() == L'\r' ||
                value.back() == L'\n'))
        {
            value.pop_back();
        }

        if (
            value.size() != 7 ||
            value[0] != L'#')
        {
            return false;
        }

        int digits[6]{};

        for (int i = 0; i < 6; ++i)
        {
            const wchar_t wc =
                value[1 + i];

            if (wc > 127)
                return false;

            digits[i] =
                HexDigit(
                    static_cast<char>(wc));

            if (digits[i] < 0)
                return false;
        }

        r = digits[0] * 16 + digits[1];
        g = digits[2] * 16 + digits[3];
        b = digits[4] * 16 + digits[5];

        return true;
    }


    // ============================================================
    // COLOR MATH
    // ============================================================

    Hsv ToHsv(
        Color c)
    {
        const int r = c.r;
        const int g = c.g;
        const int b = c.b;

        const int mx =
            (std::max)({ r, g, b });

        const int mn =
            (std::min)({ r, g, b });

        const int d =
            mx - mn;

        Hsv out{};

        out.v =
            mx / 255.0;

        out.s =
            mx
            ? d / static_cast<double>(mx)
            : 0.0;

        if (!d)
            return out;

        double h = 0.0;

        if (mx == r)
        {
            h =
                60.0 *
                std::fmod(
                    (g - b) /
                    static_cast<double>(d),
                    6.0);
        }
        else if (mx == g)
        {
            h =
                60.0 *
                (
                    (b - r) /
                    static_cast<double>(d) +
                    2.0
                    );
        }
        else
        {
            h =
                60.0 *
                (
                    (r - g) /
                    static_cast<double>(d) +
                    4.0
                    );
        }

        if (h < 0.0)
            h += 360.0;

        out.h = h;

        return out;
    }


    Color FromHsv(
        double h,
        double s,
        double v)
    {
        h =
            std::fmod(
                h,
                360.0);

        if (h < 0.0)
            h += 360.0;

        s =
            std::clamp(
                s,
                0.0,
                1.0);

        v =
            std::clamp(
                v,
                0.0,
                1.0);

        const double c =
            v * s;

        const double x =
            c *
            (1.0 -
                std::fabs(
                    std::fmod(
                        h / 60.0,
                        2.0) -
                    1.0));

        const double m =
            v - c;

        double r = 0.0;
        double g = 0.0;
        double b = 0.0;

        if (h < 60.0) { r = c; g = x; }
        else if (h < 120.0) { r = x; g = c; }
        else if (h < 180.0) { g = c; b = x; }
        else if (h < 240.0) { g = x; b = c; }
        else if (h < 300.0) { r = x; b = c; }
        else { r = c; b = x; }

        auto toByte =
            [](double value)
            {
                return
                    static_cast<BYTE>(
                        std::clamp(
                            std::lround(
                                value * 255.0),
                            0L,
                            255L));
            };

        return Color{
            toByte(r + m),
            toByte(g + m),
            toByte(b + m)
        };
    }


    static double LinearChannel(
        int value)
    {
        const double c =
            value / 255.0;

        return
            c <= 0.03928
            ? c / 12.92
            : std::pow(
                (c + 0.055) / 1.055,
                2.4);
    }


    double Luminance(
        Color c)
    {
        return
            0.2126 * LinearChannel(c.r) +
            0.7152 * LinearChannel(c.g) +
            0.0722 * LinearChannel(c.b);
    }


    double ContrastRatio(
        Color a,
        Color b)
    {
        const double la =
            Luminance(a);

        const double lb =
            Luminance(b);

        const double hi =
            (std::max)(la, lb);

        const double lo =
            (std::min)(la, lb);

        return
            (hi + 0.05) /
            (lo + 0.05);
    }


    double Brightness(
        Color c)
    {
        return
            (
                0.299 * c.r +
                0.587 * c.g +
                0.114 * c.b
                ) / 255.0;
    }


    Color LerpColor(
        Color a,
        Color b,
        float t)
    {
        t =
            std::clamp(
                t,
                0.0f,
                1.0f);

        auto mix =
            [t](BYTE x, BYTE y)
            {
                return
                    static_cast<BYTE>(
                        static_cast<float>(x) +
                        (static_cast<float>(y) -
                            static_cast<float>(x)) *
                        t +
                        0.5f);
            };

        return Color{
            mix(a.r, b.r),
            mix(a.g, b.g),
            mix(a.b, b.b)
        };
    }


    Color DarkenColor(
        Color c,
        float amount)
    {
        return
            LerpColor(
                c,
                Color{ 0, 0, 0 },
                amount);
    }


    Color LightenColor(
        Color c,
        float amount)
    {
        return
            LerpColor(
                c,
                Color{ 255, 255, 255 },
                amount);
    }


    // ============================================================
    // FAMILIES
    // ============================================================

    const wchar_t* FamilyName(
        ColorFamily f)
    {
        switch (f)
        {
        case ColorFamily::White:
            return L"Белый";

        case ColorFamily::LightGray:
            return L"Светло-серый";

        case ColorFamily::Gray:
            return L"Серый";

        case ColorFamily::DarkGray:
            return L"Тёмно-серый";

        case ColorFamily::Black:
            return L"Чёрный";

        case ColorFamily::Red:
            return L"Красный";

        case ColorFamily::Peach:
            return L"Персиковый";

        case ColorFamily::Yellow:
            return L"Жёлтый";

        case ColorFamily::Green:
            return L"Зелёный";

        case ColorFamily::Blue:
            return L"Синий";

        case ColorFamily::Mauve:
            return L"Фиолетовый";

        case ColorFamily::Pink:
            return L"Розовый";
        }

        return L"Цвет";
    }


    const char* FamilyKey(
        ColorFamily f)
    {
        switch (f)
        {
        case ColorFamily::White:      return "white";
        case ColorFamily::LightGray:  return "lightgray";
        case ColorFamily::Gray:       return "gray";
        case ColorFamily::DarkGray:   return "darkgray";
        case ColorFamily::Black:      return "black";
        case ColorFamily::Red:        return "red";
        case ColorFamily::Peach:      return "peach";
        case ColorFamily::Yellow:     return "yellow";
        case ColorFamily::Green:      return "green";
        case ColorFamily::Blue:       return "blue";
        case ColorFamily::Mauve:      return "mauve";
        case ColorFamily::Pink:       return "pink";
        }

        return "color";
    }


    // ------------------------------------------------------------
    // Moved verbatim from DarkModLauncher.cpp. The thresholds are
    // load-bearing: every palette on disk was tuned against this
    // exact partition of the color space, so changing a boundary
    // silently re-points stored colors at different families.
    // ------------------------------------------------------------

    ColorFamily ClassifyFamily(
        Color c)
    {
        const int r = c.r;
        const int g = c.g;
        const int b = c.b;

        const int mx =
            (std::max)({ r, g, b });

        const int mn =
            (std::min)({ r, g, b });

        const int d =
            mx - mn;

        if (mx >= 245 && mn >= 235)
            return ColorFamily::White;

        if (mx <= 22)
            return ColorFamily::Black;

        if (d <= 14)
        {
            if (mx >= 205)
                return ColorFamily::LightGray;

            if (mx >= 110)
                return ColorFamily::Gray;

            return ColorFamily::DarkGray;
        }

        double h = 0.0;

        if (d)
        {
            if (mx == r)
            {
                h =
                    60.0 *
                    std::fmod(
                        (g - b) /
                        static_cast<double>(d),
                        6.0);
            }
            else if (mx == g)
            {
                h =
                    60.0 *
                    (
                        (b - r) /
                        static_cast<double>(d) +
                        2.0
                        );
            }
            else
            {
                h =
                    60.0 *
                    (
                        (r - g) /
                        static_cast<double>(d) +
                        4.0
                        );
            }

            if (h < 0)
                h += 360.0;
        }

        if (h < 15 || h >= 345)
            return ColorFamily::Red;

        if (h < 45)
            return ColorFamily::Peach;

        if (h < 75)
            return ColorFamily::Yellow;

        if (h < 165)
            return ColorFamily::Green;

        if (h < 255)
            return ColorFamily::Blue;

        if (h < 315)
            return ColorFamily::Mauve;

        return ColorFamily::Pink;
    }

}

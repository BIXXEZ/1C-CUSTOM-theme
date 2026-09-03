// ============================================================
// DARKMOD CORE - COLORS JSON
// ============================================================

#include "DarkModColorsJson.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace dm
{

    // ============================================================
    // MINIMAL JSON READING
    // ============================================================

    std::string ParseJsonString(
        const std::string& json,
        size_t position)
    {
        while (
            position < json.size() &&
            json[position] != '"')
        {
            ++position;
        }

        if (position >= json.size())
            return {};

        ++position;

        const size_t begin =
            position;

        while (
            position < json.size() &&
            json[position] != '"')
        {
            ++position;
        }

        if (position >= json.size())
            return {};

        return
            json.substr(
                begin,
                position - begin);
    }


    bool ParseJsonNumberAfter(
        const std::string& json,
        size_t position,
        double& result)
    {
        while (
            position < json.size() &&
            (
                json[position] == ' ' ||
                json[position] == '\t' ||
                json[position] == '\r' ||
                json[position] == '\n' ||
                json[position] == ':'
                ))
        {
            ++position;
        }

        if (position >= json.size())
            return false;

        char* end = nullptr;

        result =
            strtod(
                json.c_str() +
                position,
                &end);

        return
            end !=
            json.c_str() + position;
    }


    std::string ParseJsonMode(
        const std::string& json)
    {
        const size_t pos =
            json.find(
                "\"mode\"");

        if (pos == std::string::npos)
            return {};

        return
            ParseJsonString(
                json,
                pos + 6);
    }


    std::vector<LiveColor> ParseLiveColors(
        const std::string& json)
    {
        std::vector<LiveColor> result;

        size_t pos = 0;

        while (true)
        {
            pos =
                json.find(
                    "\"color\"",
                    pos);

            if (pos == std::string::npos)
                break;

            const std::string color =
                ParseJsonString(
                    json,
                    pos + 7);

            if (color.empty())
            {
                pos += 7;
                continue;
            }

            const size_t percentagePos =
                json.find(
                    "\"percentage\"",
                    pos);

            if (
                percentagePos ==
                std::string::npos)
            {
                break;
            }

            double percentage = 0.0;

            if (
                !ParseJsonNumberAfter(
                    json,
                    percentagePos + 12,
                    percentage))
            {
                pos =
                    percentagePos + 12;

                continue;
            }

            const size_t pixelsPos =
                json.find(
                    "\"pixels\"",
                    percentagePos);

            uint64_t pixels = 0;

            if (
                pixelsPos !=
                std::string::npos)
            {
                double p = 0.0;

                if (
                    ParseJsonNumberAfter(
                        json,
                        pixelsPos + 8,
                        p))
                {
                    pixels =
                        static_cast<uint64_t>(p);
                }
            }

            result.push_back(
                {
                    color,
                    percentage,
                    pixels
                });

            pos =
                percentagePos + 12;

            //
            // Never truncate here. The launcher's main screen shows
            // families, but the details window and the renderer's
            // source list both need every color in the file - and
            // the rule list must stay in file order.
            //
        }

        return result;
    }


    // ============================================================
    // HISTOGRAM
    // ============================================================

    uint32_t ScreenBin(
        int r,
        int g,
        int b)
    {
        return
            (static_cast<uint32_t>(
                r >> 3)
                << 10) |
            (static_cast<uint32_t>(
                g >> 3)
                << 5) |
            static_cast<uint32_t>(
                b >> 3);
    }


    ScreenHistogram AnalyzeScreen(
        const CapturedScreen& screen)
    {
        ScreenHistogram result{};

        if (screen.pixels.empty())
            return result;

        const size_t count =
            screen.pixels.size() / 4;

        //
        // Full scan. About two million pixels for a maximised
        // window, which is cheap enough to do once per second.
        //

        for (
            size_t i = 0;
            i < count;
            ++i)
        {
            const uint8_t* p =
                &screen.pixels[i * 4];

            const int B = p[0];
            const int G = p[1];
            const int R = p[2];

            //
            // The DIB is opaque for our purposes; alpha is ignored.
            //

            ++result.bins[
                ScreenBin(
                    R,
                    G,
                    B)];

            ++result.pixels;
        }

        return result;
    }


    std::vector<ScreenColor> BuildTopScreenColors(
        const ScreenHistogram& histogram,
        size_t limit)
    {
        std::vector<ScreenColor> result;

        result.reserve(
            SCREEN_HISTOGRAM_SIZE);

        for (
            int r5 = 0;
            r5 < 32;
            ++r5)
        {
            for (
                int g5 = 0;
                g5 < 32;
                ++g5)
            {
                for (
                    int b5 = 0;
                    b5 < 32;
                    ++b5)
                {
                    const uint32_t index =
                        (static_cast<uint32_t>(
                            r5)
                            << 10) |
                        (static_cast<uint32_t>(
                            g5)
                            << 5) |
                        static_cast<uint32_t>(
                            b5);

                    const uint64_t pixels =
                        histogram.bins[index];

                    if (!pixels)
                        continue;

                    ScreenColor c{};

                    //
                    // Representative value = center of bucket.
                    //

                    c.r =
                        (std::min)(
                            255,
                            r5 * 8 + 4);

                    c.g =
                        (std::min)(
                            255,
                            g5 * 8 + 4);

                    c.b =
                        (std::min)(
                            255,
                            b5 * 8 + 4);

                    c.pixels =
                        pixels;

                    result.push_back(c);
                }
            }
        }

        //
        // stable_sort, not sort: equal pixel counts must keep bin
        // order so the same capture always yields the same rule
        // order, and equal-distance matches stay reproducible.
        //

        std::stable_sort(
            result.begin(),
            result.end(),
            [](const ScreenColor& a,
                const ScreenColor& b)
            {
                return
                    a.pixels >
                    b.pixels;
            });

        if (result.size() > limit)
            result.resize(limit);

        return result;
    }


    // ============================================================
    // WRITING
    // ============================================================

    static std::string Hex8(
        int value)
    {
        char buffer[3]{};

        sprintf_s(
            buffer,
            "%02X",
            value);

        return buffer;
    }


    static std::string HexRGB8(
        int r,
        int g,
        int b)
    {
        return
            "#" +
            Hex8(r) +
            Hex8(g) +
            Hex8(b);
    }


    std::string FormatColorsJson(
        const ColorsJsonInfo& info,
        const CapturedScreen& screen,
        const ScreenHistogram& histogram)
    {
        SYSTEMTIME st{};

        GetLocalTime(&st);

        std::ostringstream out;

        out
            << "{\n"
            << "  \"version\": "
            << info.version
            << ",\n"
            << "  \"alpha_version\": \""
            << info.alphaVersion
            << "\",\n"
            << "  \"timestamp\": \""
            << std::setfill('0')
            << std::setw(4)
            << st.wYear
            << "-"
            << std::setw(2)
            << st.wMonth
            << "-"
            << std::setw(2)
            << st.wDay
            << "T"
            << std::setw(2)
            << st.wHour
            << ":"
            << std::setw(2)
            << st.wMinute
            << ":"
            << std::setw(2)
            << st.wSecond
            << "."
            << std::setw(3)
            << st.wMilliseconds
            << "\",\n"

            << "  \"process_id\": "
            << info.processId
            << ",\n"

            << "  \"window\": {\n"
            << "    \"width\": "
            << screen.width
            << ",\n"
            << "    \"height\": "
            << screen.height
            << "\n"
            << "  },\n"

            << "  \"pixels_analyzed\": "
            << histogram.pixels
            << ",\n"

            << "  \"source_replacements_since_report\": "
            << info.replacementsSinceReport
            << ",\n"

            << "  \"mode\": \""
            << info.mode
            << "\",\n"

            << "  \"colors\": [\n";

        const std::vector<ScreenColor> top =
            BuildTopScreenColors(
                histogram,
                info.colorLimit);

        for (
            size_t i = 0;
            i < top.size();
            ++i)
        {
            const ScreenColor& c =
                top[i];

            const double percentage =
                histogram.pixels > 0
                ? static_cast<double>(
                    c.pixels) *
                100.0 /
                static_cast<double>(
                    histogram.pixels)
                : 0.0;

            out
                << "    {\n"
                << "      \"color\": \""
                << HexRGB8(
                    c.r,
                    c.g,
                    c.b)
                << "\",\n"
                << "      \"percentage\": "
                << std::fixed
                << std::setprecision(4)
                << percentage
                << ",\n"
                << "      \"pixels\": "
                << c.pixels
                << "\n"
                << "    }";

            if (
                i + 1 <
                top.size())
            {
                out << ",";
            }

            out << "\n";
        }

        out
            << "  ]\n"
            << "}\n";

        return out.str();
    }


    // ============================================================
    // ANALYSIS PAYLOAD REJECTION
    // ============================================================

    bool LooksLikeOwnOutput(
        const std::vector<LiveColor>& colors,
        const Color(&destinations)[FAMILY_COUNT])
    {
        double matchedCoverage = 0.0;

        for (const LiveColor& lc : colors)
        {
            const Color c =
                ParseHexColor(lc.hex);

            for (int i = 0; i < FAMILY_COUNT; ++i)
            {
                if (
                    ColorEquals(
                        c,
                        destinations[i]))
                {
                    matchedCoverage +=
                        lc.percentage;

                    break;
                }
            }
        }

        return
            matchedCoverage >=
            OWN_OUTPUT_COVERAGE_PERCENT;
    }


    bool LooksLikeOwnOutput(
        const ScreenHistogram& histogram,
        const Color(&destinations)[FAMILY_COUNT])
    {
        if (!histogram.pixels)
            return false;

        //
        // Compare BINS, not colors.
        //
        // The list-based overload above compares hex strings
        // because that is what the launcher has always done, but
        // the strings in the file are bucket centers - a
        // destination only ever matches one exactly when each of
        // its channels happens to be 4 mod 8. Working straight off
        // the histogram there is no reason to accept that: asking
        // whether a destination falls in this bin answers exactly
        // the question "could these pixels have been our output".
        //

        uint64_t matched = 0;

        for (int i = 0; i < FAMILY_COUNT; ++i)
        {
            const uint32_t bin =
                ScreenBin(
                    destinations[i].r,
                    destinations[i].g,
                    destinations[i].b);

            //
            // Two families can share a bin; count each bin once.
            //

            bool alreadyCounted = false;

            for (int j = 0; j < i; ++j)
            {
                if (
                    ScreenBin(
                        destinations[j].r,
                        destinations[j].g,
                        destinations[j].b) ==
                    bin)
                {
                    alreadyCounted = true;
                    break;
                }
            }

            if (alreadyCounted)
                continue;

            matched +=
                histogram.bins[bin];
        }

        const double coverage =
            static_cast<double>(matched) *
            100.0 /
            static_cast<double>(
                histogram.pixels);

        return
            coverage >=
            OWN_OUTPUT_COVERAGE_PERCENT;
    }

}

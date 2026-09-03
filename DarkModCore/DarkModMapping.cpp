// ============================================================
// DARKMOD CORE - COLOR MAPPING
// ============================================================

#include "DarkModMapping.h"

#include <algorithm>
#include <sstream>

namespace dm
{

    // ============================================================
    // THE MATCHER
    //
    // Moved verbatim from DarkModDLL.cpp. It runs once per drawing
    // operation inside 1C, so it stays a flat loop over the rule
    // list with no allocation and no sorting - and the list is
    // taken by reference precisely so the DLL can keep resolving
    // its atomic shared_ptr at the call site instead of handing
    // ownership around.
    // ============================================================

    bool FindColorMapping(
        const MappingList& mappings,
        int R,
        int G,
        int B,
        int& outR,
        int& outG,
        int& outB)
    {
        const Mapping* nearest = nullptr;

        int nearestDistance =
            COLOR_MATCH_RADIUS_SQ + 1;

        bool isOwnOutput = false;

        for (const Mapping& m : mappings)
        {
            if (!m.enabled)
                continue;

            const int dr =
                R - static_cast<int>(m.sr);

            const int dg =
                G - static_cast<int>(m.sg);

            const int db =
                B - static_cast<int>(m.sb);

            const int distance =
                dr * dr + dg * dg + db * db;

            //
            // Level 1: exact match wins immediately, ahead of the
            // destination check, so a deliberate rule always
            // applies.
            //

            if (distance == 0)
            {
                outR = m.dr;
                outG = m.dg;
                outB = m.db;

                return true;
            }

            //
            // Strict less-than: on equal distance the FIRST rule
            // keeps the slot. Rule order is part of the result.
            //

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = &m;
            }

            //
            // Level 2 evidence: is the incoming color a color THIS
            // rule set produces? Keep scanning - an exact source
            // match later in the list still outranks this.
            //

            if (!isOwnOutput)
            {
                const int ddr =
                    R - static_cast<int>(m.dr);

                const int ddg =
                    G - static_cast<int>(m.dg);

                const int ddb =
                    B - static_cast<int>(m.db);

                if (
                    ddr * ddr +
                    ddg * ddg +
                    ddb * ddb <=
                    DESTINATION_MATCH_TOLERANCE_SQ)
                {
                    isOwnOutput = true;
                }
            }
        }

        //
        // Level 2: our own output never feeds back into the
        // matcher.
        //

        if (isOwnOutput)
            return false;

        if (!nearest)
            return false;

        //
        // Level 3: nearest source within the fuzzy radius.
        //

        outR = nearest->dr;
        outG = nearest->dg;
        outB = nearest->db;

        return
            nearestDistance <=
            COLOR_MATCH_RADIUS_SQ;
    }


    // ============================================================
    // FILE FORMAT
    // ============================================================

    static std::wstring TrimW(
        const std::wstring& text)
    {
        size_t begin = 0;

        size_t end =
            text.size();

        auto space =
            [](wchar_t c)
            {
                return
                    c == L' ' ||
                    c == L'\t' ||
                    c == L'\r' ||
                    c == L'\n';
            };

        while (
            begin < end &&
            space(text[begin]))
        {
            ++begin;
        }

        while (
            end > begin &&
            space(text[end - 1]))
        {
            --end;
        }

        return
            text.substr(
                begin,
                end - begin);
    }


    bool ParseMappingLine(
        const std::wstring& line,
        Mapping& out)
    {
        const std::wstring text =
            TrimW(line);

        if (text.empty())
            return false;

        if (text[0] == L';')
            return false;

        const size_t p1 =
            text.find(L'|');

        if (p1 == std::wstring::npos)
            return false;

        const size_t p2 =
            text.find(
                L'|',
                p1 + 1);

        if (p2 == std::wstring::npos)
            return false;

        const std::wstring enabledText =
            TrimW(
                text.substr(
                    0,
                    p1));

        const std::wstring srcText =
            TrimW(
                text.substr(
                    p1 + 1,
                    p2 - p1 - 1));

        const std::wstring dstText =
            TrimW(
                text.substr(
                    p2 + 1));

        int sr = 0;
        int sg = 0;
        int sb = 0;

        int dr = 0;
        int dg = 0;
        int db = 0;

        if (
            !ParseRGB(
                srcText,
                sr,
                sg,
                sb))
        {
            return false;
        }

        if (
            !ParseRGB(
                dstText,
                dr,
                dg,
                db))
        {
            return false;
        }

        out.sr = static_cast<uint8_t>(sr);
        out.sg = static_cast<uint8_t>(sg);
        out.sb = static_cast<uint8_t>(sb);

        out.dr = static_cast<uint8_t>(dr);
        out.dg = static_cast<uint8_t>(dg);
        out.db = static_cast<uint8_t>(db);

        //
        // The accepted spellings are the ones the DLL already
        // accepted. Anything else counts as disabled rather than as
        // a parse error, which is how hand-edited files behave now.
        //

        out.enabled =
            enabledText == L"1" ||
            enabledText == L"true" ||
            enabledText == L"TRUE";

        return true;
    }


    std::string FormatMappingLine(
        const Mapping& m)
    {
        std::string line;

        line += m.enabled ? "1|" : "0|";

        line +=
            FormatHex(
                MappingSource(m));

        line += "|";

        line +=
            FormatHex(
                MappingDestination(m));

        line += "\n";

        return line;
    }


    MappingList ParseMappingText(
        const std::wstring& text)
    {
        MappingList result;

        std::wistringstream stream(
            text);

        std::wstring line;

        while (
            std::getline(
                stream,
                line))
        {
            Mapping m{};

            if (
                ParseMappingLine(
                    line,
                    m))
            {
                result.push_back(m);
            }
        }

        return result;
    }


    std::string FormatMappingText(
        const MappingList& mappings)
    {
        std::string text;

        text.reserve(
            mappings.size() * 24);

        for (const Mapping& m : mappings)
        {
            text +=
                FormatMappingLine(m);
        }

        return text;
    }


    // ============================================================
    // RULE BUILDING
    // ============================================================

    MappingList BuildMappings(
        const std::vector<Color>& sources,
        const Color(&familyDst)[FAMILY_COUNT])
    {
        MappingList result;

        result.reserve(
            sources.size());

        for (const Color& src : sources)
        {
            const Color dst =
                familyDst[
                    static_cast<int>(
                        ClassifyFamily(src))];

            Mapping m{};

            m.sr = src.r;
            m.sg = src.g;
            m.sb = src.b;

            m.dr = dst.r;
            m.dg = dst.g;
            m.db = dst.b;

            m.enabled = true;

            result.push_back(m);
        }

        return result;
    }


    // ------------------------------------------------------------
    // Safe bootstrap sources, used before the first analysis
    // result exists. Moved verbatim from the launcher's
    // BuildSourceList(); the order is significant because equal
    // distances resolve to the first rule.
    // ------------------------------------------------------------

    const std::vector<Color>& BootstrapSources()
    {
        static const std::vector<Color> sources =
        {
            { 255,255,255 },
            { 0,0,0 },
            { 204,204,204 },
            { 153,153,153 },
            { 102,102,102 },
            { 34,34,34 },
            { 68,68,68 }
        };

        return sources;
    }

}

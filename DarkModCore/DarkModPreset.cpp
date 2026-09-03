// ============================================================
// DARKMOD CORE - PRESETS
// ============================================================

#include "DarkModPreset.h"
#include "DarkModPaths.h"

#include <sstream>
#include <cstdlib>

namespace dm
{

    // ============================================================
    // CUSTOM PALETTE
    // ============================================================

    // ------------------------------------------------------------
    // Moved verbatim from the launcher's Custom branch. Two
    // families share the background and two share the surface,
    // which is why four editable colors are enough to fill twelve
    // slots.
    // ------------------------------------------------------------

    Color CustomFamilyColor(
        const CustomPaletteColors& custom,
        ColorFamily f)
    {
        switch (f)
        {
        case ColorFamily::White:
        case ColorFamily::LightGray:
            return custom.background;

        case ColorFamily::Gray:
        case ColorFamily::DarkGray:
            return custom.surface;

        case ColorFamily::Black:
            return custom.text;

        default:
            return custom.accent;
        }
    }


    // ============================================================
    // BUILT-IN TABLES
    //
    // Moved verbatim from PaletteFamilyBaseColor(). Order is
    // White, LightGray, Gray, DarkGray, Black, Red, Peach, Yellow,
    // Green, Blue, Mauve, Pink - the ColorFamily order, which is
    // fixed.
    // ============================================================

    namespace
    {

        struct BuiltinEntry
        {
            const char* id;
            const wchar_t* name;
        };


        const BuiltinEntry BUILTIN_ENTRIES[
            BUILTIN_PRESET_COUNT] =
        {
            { "dark",       L"Dark" },
            { "deepdark",   L"Deep Dark" },
            { "midnight",   L"Midnight" },
            { "mocha",      L"Catppuccin Mocha" },
            { "macchiato",  L"Catppuccin Macchiato" },
            { "frappe",     L"Catppuccin Frappé" },
            { "latte",      L"Catppuccin Latte" },
            { "custom",     L"Custom" }
        };


        const Color BUILTIN_DARK[FAMILY_COUNT] =
        {
            { 17,17,27 },
            { 24,24,37 },
            { 30,30,46 },
            { 49,50,68 },
            { 255,255,255 },
            { 243,139,168 },
            { 250,179,135 },
            { 249,226,175 },
            { 166,227,161 },
            { 137,180,250 },
            { 203,166,247 },
            { 245,194,231 }
        };

        const Color BUILTIN_DEEPDARK[FAMILY_COUNT] =
        {
            { 0,0,0 },
            { 5,5,5 },
            { 10,10,10 },
            { 18,18,18 },
            { 255,255,255 },
            { 245,245,245 },
            { 235,235,235 },
            { 225,225,225 },
            { 215,215,215 },
            { 205,205,205 },
            { 248,248,248 },
            { 255,255,255 }
        };

        const Color BUILTIN_MIDNIGHT[FAMILY_COUNT] =
        {
            { 11,16,32 },
            { 18,24,43 },
            { 26,33,56 },
            { 37,44,70 },
            { 250,249,240 },
            { 239,125,150 },
            { 237,156,126 },
            { 229,205,140 },
            { 160,205,180 },
            { 144,177,245 },
            { 190,170,245 },
            { 228,186,223 }
        };

        const Color BUILTIN_MOCHA[FAMILY_COUNT] =
        {
            { 24,24,37 },
            { 49,50,68 },
            { 69,71,90 },
            { 49,50,68 },
            { 205,214,244 },
            { 243,139,168 },
            { 250,179,135 },
            { 249,226,175 },
            { 166,227,161 },
            { 137,180,250 },
            { 203,166,247 },
            { 245,194,231 }
        };

        const Color BUILTIN_MACCHIATO[FAMILY_COUNT] =
        {
            { 24,25,38 },
            { 36,39,58 },
            { 54,58,79 },
            { 36,39,58 },
            { 202,207,245 },
            { 237,135,150 },
            { 245,169,127 },
            { 238,212,159 },
            { 166,218,149 },
            { 138,173,244 },
            { 198,160,246 },
            { 240,198,230 }
        };

        const Color BUILTIN_FRAPPE[FAMILY_COUNT] =
        {
            { 41,44,60 },
            { 65,69,89 },
            { 81,87,109 },
            { 65,69,89 },
            { 198,208,245 },
            { 231,130,132 },
            { 239,159,118 },
            { 229,200,142 },
            { 161,201,146 },
            { 140,170,238 },
            { 202,158,230 },
            { 234,184,222 }
        };

        const Color BUILTIN_LATTE[FAMILY_COUNT] =
        {
            { 239,241,245 },
            { 230,233,239 },
            { 220,224,232 },
            { 188,192,204 },
            { 76,79,105 },
            { 210,15,57 },
            { 254,100,11 },
            { 223,142,29 },
            { 64,160,43 },
            { 30,102,245 },
            { 136,57,239 },
            { 234,118,203 }
        };


        const Color* BuiltinTable(
            int index)
        {
            switch (index)
            {
            case 0: return BUILTIN_DARK;
            case 1: return BUILTIN_DEEPDARK;
            case 2: return BUILTIN_MIDNIGHT;
            case 3: return BUILTIN_MOCHA;
            case 4: return BUILTIN_MACCHIATO;
            case 5: return BUILTIN_FRAPPE;
            case 6: return BUILTIN_LATTE;
            }

            //
            // 7 is Custom, which has no table.
            //

            return nullptr;
        }

    }


    const char* BuiltinPresetId(
        int index)
    {
        if (
            index < 0 ||
            index >= BUILTIN_PRESET_COUNT)
        {
            return "dark";
        }

        return
            BUILTIN_ENTRIES[index].id;
    }


    const wchar_t* BuiltinPresetName(
        int index)
    {
        if (
            index < 0 ||
            index >= BUILTIN_PRESET_COUNT)
        {
            return L"Dark";
        }

        return
            BUILTIN_ENTRIES[index].name;
    }


    Color BuiltinFamilyColor(
        int index,
        ColorFamily f,
        const CustomPaletteColors& custom)
    {
        const Color* table =
            BuiltinTable(index);

        if (!table)
        {
            return
                CustomFamilyColor(
                    custom,
                    f);
        }

        return
            table[
                static_cast<int>(f)];
    }


    Preset MakeBuiltinPreset(
        int index,
        const CustomPaletteColors& custom)
    {
        Preset preset{};

        preset.id =
            BuiltinPresetId(index);

        preset.name =
            BuiltinPresetName(index);

        preset.builtin = true;

        for (int i = 0; i < FAMILY_COUNT; ++i)
        {
            preset.family[i] =
                BuiltinFamilyColor(
                    index,
                    FamilyAt(i),
                    custom);
        }

        return preset;
    }


    // ============================================================
    // TEXT HELPERS
    // ============================================================

    namespace
    {

        std::vector<std::string> SplitFields(
            const std::string& line,
            size_t limit)
        {
            std::vector<std::string> fields;

            size_t begin = 0;

            while (true)
            {
                if (
                    limit &&
                    fields.size() + 1 == limit)
                {
                    fields.push_back(
                        line.substr(begin));

                    break;
                }

                const size_t pos =
                    line.find(
                        '|',
                        begin);

                if (pos == std::string::npos)
                {
                    fields.push_back(
                        line.substr(begin));

                    break;
                }

                fields.push_back(
                    line.substr(
                        begin,
                        pos - begin));

                begin = pos + 1;
            }

            return fields;
        }


        std::string TrimLine(
            const std::string& text)
        {
            size_t begin = 0;

            size_t end =
                text.size();

            auto space =
                [](char c)
                {
                    return
                        c == ' ' ||
                        c == '\t' ||
                        c == '\r' ||
                        c == '\n';
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


        //
        // Fields are separated by '|', so a name containing one
        // would split into two fields on the way back in.
        //
        std::string SanitizeField(
            const std::string& text)
        {
            std::string out =
                text;

            for (char& c : out)
            {
                if (
                    c == '|' ||
                    c == '\r' ||
                    c == '\n')
                {
                    c = ' ';
                }
            }

            return out;
        }


        bool ParseIndex(
            const std::string& text,
            int& out)
        {
            const std::string value =
                TrimLine(text);

            if (value.empty())
                return false;

            //
            // Decimal first, hex second. The second attempt exists
            // for legacy override files where the writer left
            // std::hex set: indices 10 and 11 arrived as "A"/"B".
            //

            char* end = nullptr;

            long decimal =
                strtol(
                    value.c_str(),
                    &end,
                    10);

            if (
                end &&
                *end == '\0')
            {
                out =
                    static_cast<int>(decimal);

                return true;
            }

            end = nullptr;

            long hex =
                strtol(
                    value.c_str(),
                    &end,
                    16);

            if (
                end &&
                *end == '\0')
            {
                out =
                    static_cast<int>(hex);

                return true;
            }

            return false;
        }

    }


    // ============================================================
    // LEGACY OVERRIDES
    // ============================================================

    bool LoadLegacyOverrides(
        const std::wstring& path,
        LegacyOverrides& out)
    {
        out = {};

        const std::string text =
            ReadTextFile(path);

        if (text.empty())
            return false;

        std::istringstream stream(text);

        std::string line;

        bool first = true;

        while (
            std::getline(
                stream,
                line))
        {
            line =
                TrimLine(line);

            if (line.empty())
                continue;

            const std::vector<std::string> fields =
                SplitFields(
                    line,
                    3);

            if (
                first &&
                fields.size() >= 2 &&
                fields[0] == "version")
            {
                first = false;

                int version = -1;
                int count = -1;

                ParseIndex(
                    fields[1],
                    version);

                if (fields.size() >= 3)
                {
                    ParseIndex(
                        fields[2],
                        count);
                }

                //
                // A different version means the ordinals this file
                // is keyed by moved, so the colors would land on
                // the wrong presets. Report instead of guessing.
                //

                if (
                    version !=
                    OVERRIDE_FILE_VERSION ||
                    count !=
                    OVERRIDE_PALETTE_COUNT)
                {
                    out.versionMismatch = true;

                    return false;
                }

                continue;
            }

            first = false;

            if (fields.size() < 3)
                continue;

            int palette = -1;
            int family = -1;

            if (
                !ParseIndex(
                    fields[0],
                    palette))
            {
                continue;
            }

            bool decimalFamily = true;

            {
                const std::string value =
                    TrimLine(fields[1]);

                char* end = nullptr;

                strtol(
                    value.c_str(),
                    &end,
                    10);

                decimalFamily =
                    end &&
                    *end == '\0';
            }

            if (
                !ParseIndex(
                    fields[1],
                    family))
            {
                continue;
            }

            if (
                palette < 0 ||
                palette >= OVERRIDE_PALETTE_COUNT ||
                family < 0 ||
                family >= FAMILY_COUNT)
            {
                continue;
            }

            Color color{};

            if (
                !ParseHexColorStrict(
                    fields[2],
                    color))
            {
                continue;
            }

            out.color[palette][family] =
                color;

            out.set[palette][family] =
                true;

            ++out.records;

            if (!decimalFamily)
                ++out.recovered;
        }

        return
            out.records > 0;
    }


    // ============================================================
    // PRESET STORE
    // ============================================================

    int PresetStore::IndexOf(
        const std::string& id) const
    {
        for (
            size_t i = 0;
            i < m_presets.size();
            ++i)
        {
            if (m_presets[i].id == id)
            {
                return
                    static_cast<int>(i);
            }
        }

        return -1;
    }


    const Preset* PresetStore::Find(
        const std::string& id) const
    {
        const int index =
            IndexOf(id);

        return
            index < 0
            ? nullptr
            : &m_presets[index];
    }


    Preset* PresetStore::Find(
        const std::string& id)
    {
        const int index =
            IndexOf(id);

        return
            index < 0
            ? nullptr
            : &m_presets[index];
    }


    const Preset& PresetStore::Selected() const
    {
        const int index =
            SelectedIndex();

        if (index >= 0)
            return m_presets[index];

        //
        // Nothing selectable at all. Callers paint with these
        // twelve colors on every frame, so hand them something
        // valid rather than making each one check.
        //

        static const Preset fallback =
            MakeBuiltinPreset(
                0,
                CustomPaletteColors{});

        return fallback;
    }


    int PresetStore::SelectedIndex() const
    {
        if (m_presets.empty())
            return -1;

        const int index =
            IndexOf(m_selectedId);

        if (index >= 0)
            return index;

        //
        // The selected preset was deleted. First in the list is the
        // least surprising replacement.
        //

        return 0;
    }


    void PresetStore::SetSelectedId(
        const std::string& id)
    {
        m_selectedId = id;
    }


    void PresetStore::SeedBuiltins(
        const CustomPaletteColors& custom,
        const LegacyOverrides* overrides)
    {
        for (
            int p = 0;
            p < BUILTIN_PRESET_COUNT;
            ++p)
        {
            Preset preset =
                MakeBuiltinPreset(
                    p,
                    custom);

            //
            // Built-in index and legacy palette ordinal are the
            // same number here, and only here: this is the one
            // moment the ordinal is still meaningful, because the
            // list is still exactly the eight it was written for.
            //

            if (
                overrides &&
                p < OVERRIDE_PALETTE_COUNT)
            {
                for (int i = 0; i < FAMILY_COUNT; ++i)
                {
                    if (overrides->set[p][i])
                    {
                        preset.family[i] =
                            overrides->color[p][i];
                    }
                }
            }

            AddOrReplace(preset);
        }

        if (m_selectedId.empty())
        {
            m_selectedId =
                BuiltinPresetId(0);
        }
    }


    void PresetStore::RestoreBuiltins(
        const CustomPaletteColors& custom)
    {
        //
        // Rebuild the built-ins in their original order, in front
        // of whatever the user generated, and leave generated
        // presets untouched.
        //

        std::vector<Preset> rebuilt;

        rebuilt.reserve(
            m_presets.size() +
            BUILTIN_PRESET_COUNT);

        for (
            int p = 0;
            p < BUILTIN_PRESET_COUNT;
            ++p)
        {
            rebuilt.push_back(
                MakeBuiltinPreset(
                    p,
                    custom));
        }

        for (const Preset& preset : m_presets)
        {
            if (preset.builtin)
                continue;

            rebuilt.push_back(preset);
        }

        m_presets =
            std::move(rebuilt);
    }


    void PresetStore::AddOrReplace(
        const Preset& preset)
    {
        const int index =
            IndexOf(preset.id);

        if (index >= 0)
        {
            m_presets[index] = preset;
            return;
        }

        m_presets.push_back(preset);
    }


    bool PresetStore::Remove(
        const std::string& id)
    {
        const int index =
            IndexOf(id);

        if (index < 0)
            return false;

        m_presets.erase(
            m_presets.begin() +
            index);

        //
        // Selection is by id, so a deleted preset simply stops
        // resolving and SelectedIndex() falls back. Move it
        // explicitly anyway so the saved file names something real.
        //

        if (m_selectedId == id)
        {
            m_selectedId =
                m_presets.empty()
                ? std::string()
                : m_presets[0].id;
        }

        return true;
    }


    bool PresetStore::Rename(
        const std::string& id,
        const std::wstring& name)
    {
        Preset* preset =
            Find(id);

        if (!preset)
            return false;

        preset->name = name;

        return true;
    }


    void PresetStore::SyncCustomPreset(
        const CustomPaletteColors& custom)
    {
        Preset* preset =
            Find("custom");

        if (!preset)
            return;

        for (int i = 0; i < FAMILY_COUNT; ++i)
        {
            preset->family[i] =
                CustomFamilyColor(
                    custom,
                    FamilyAt(i));
        }
    }


    std::string PresetStore::MakeUniqueId(
        const std::string& base) const
    {
        std::string clean;

        for (char c : base)
        {
            if (
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '-')
            {
                clean += c;
            }
            else if (c >= 'A' && c <= 'Z')
            {
                clean +=
                    static_cast<char>(
                        c - 'A' + 'a');
            }
            else if (
                !clean.empty() &&
                clean.back() != '-')
            {
                clean += '-';
            }
        }

        while (
            !clean.empty() &&
            clean.back() == '-')
        {
            clean.pop_back();
        }

        if (clean.empty())
            clean = "preset";

        if (IndexOf(clean) < 0)
            return clean;

        for (int n = 2; n < 100000; ++n)
        {
            const std::string candidate =
                clean +
                "-" +
                std::to_string(n);

            if (IndexOf(candidate) < 0)
                return candidate;
        }

        return clean;
    }


    // ============================================================
    // FILE IO
    // ============================================================

    PresetLoad PresetStore::Load(
        const std::wstring& path)
    {
        if (!FileExists(path))
            return PresetLoad::Missing;

        const std::string text =
            ReadTextFile(path);

        if (text.empty())
            return PresetLoad::Corrupt;

        std::istringstream stream(text);

        std::string line;

        bool sawVersion = false;

        std::vector<Preset> presets;
        std::string selected;

        Preset current{};
        bool haveCurrent = false;

        auto flush =
            [&]()
            {
                if (!haveCurrent)
                    return;

                if (!current.id.empty())
                    presets.push_back(current);

                current = {};
                haveCurrent = false;
            };

        while (
            std::getline(
                stream,
                line))
        {
            line =
                TrimLine(line);

            if (line.empty())
                continue;

            if (line[0] == ';')
                continue;

            const std::vector<std::string> fields =
                SplitFields(
                    line,
                    4);

            const std::string& tag =
                fields[0];

            if (tag == "version")
            {
                int version = -1;

                if (
                    fields.size() < 2 ||
                    !ParseIndex(
                        fields[1],
                        version) ||
                    version <= 0)
                {
                    return PresetLoad::Corrupt;
                }

                if (version > PRESET_FILE_VERSION)
                    return PresetLoad::FutureVersion;

                sawVersion = true;

                continue;
            }

            //
            // Anything before the version line means this is not a
            // preset file - refuse rather than reinterpret it.
            //

            if (!sawVersion)
                return PresetLoad::Corrupt;

            if (tag == "selected")
            {
                if (fields.size() >= 2)
                {
                    selected =
                        TrimLine(fields[1]);
                }

                continue;
            }

            if (tag == "preset")
            {
                flush();

                if (fields.size() < 2)
                    continue;

                current = {};

                current.id =
                    TrimLine(fields[1]);

                if (fields.size() >= 3)
                {
                    current.name =
                        Utf8ToW(
                            TrimLine(fields[2]));
                }

                if (current.name.empty())
                {
                    current.name =
                        Utf8ToW(current.id);
                }

                if (fields.size() >= 4)
                {
                    current.builtin =
                        TrimLine(fields[3]) ==
                        "builtin";
                }

                haveCurrent = true;

                continue;
            }

            if (
                tag == "color" &&
                haveCurrent &&
                fields.size() >= 3)
            {
                int index = -1;

                if (
                    !ParseIndex(
                        fields[1],
                        index) ||
                    index < 0 ||
                    index >= FAMILY_COUNT)
                {
                    continue;
                }

                Color color{};

                if (
                    ParseHexColorStrict(
                        fields[2],
                        color))
                {
                    current.family[index] =
                        color;
                }

                continue;
            }

            if (
                tag == "meta" &&
                haveCurrent &&
                fields.size() >= 3)
            {
                const std::string key =
                    TrimLine(fields[1]);

                if (key == "seed")
                {
                    current.seed =
                        strtoull(
                            TrimLine(
                                fields[2]).c_str(),
                            nullptr,
                            10);
                }
                else if (key == "params")
                {
                    current.params =
                        TrimLine(fields[2]);
                }

                continue;
            }
        }

        flush();

        if (
            !sawVersion ||
            presets.empty())
        {
            return PresetLoad::Corrupt;
        }

        m_presets =
            std::move(presets);

        m_selectedId =
            selected;

        return PresetLoad::Ok;
    }


    bool PresetStore::Save(
        const std::wstring& path) const
    {
        //
        // Refuse to write an empty list over a real file. An empty
        // store is a bug or a failed load, and the file it would
        // replace is the user's entire palette collection.
        //

        if (m_presets.empty())
            return false;

        std::ostringstream out;

        out
            << "version|"
            << PRESET_FILE_VERSION
            << "\n";

        const int selectedIndex =
            SelectedIndex();

        out
            << "selected|"
            << SanitizeField(
                selectedIndex >= 0
                ? m_presets[selectedIndex].id
                : std::string())
            << "\n";

        for (const Preset& preset : m_presets)
        {
            out
                << "\npreset|"
                << SanitizeField(preset.id)
                << "|"
                << SanitizeField(
                    WToUtf8(preset.name))
                << "|"
                << (preset.builtin
                    ? "builtin"
                    : "generated")
                << "\n";

            for (int i = 0; i < FAMILY_COUNT; ++i)
            {
                out
                    << "color|"
                    << i
                    << "|"
                    << FormatHex(
                        preset.family[i])
                    << "\n";
            }

            if (preset.seed)
            {
                out
                    << "meta|seed|"
                    << preset.seed
                    << "\n";
            }

            if (!preset.params.empty())
            {
                out
                    << "meta|params|"
                    << SanitizeField(
                        preset.params)
                    << "\n";
            }
        }

        BackupFile(path);

        return
            WriteTextFileAtomic(
                path,
                out.str());
    }

}

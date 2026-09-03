#pragma once

// ============================================================
// DARKMOD CORE - PRESETS
//
// A preset is a name plus twelve resolved colors, one per family.
// That is the whole model, and it is deliberately flatter than
// what came before.
//
// The launcher used to describe a preset in two pieces: a table
// compiled into the exe, keyed by the PaletteKind ordinal, and an
// override file keyed by the same ordinal. That works only while
// the list of presets is fixed at eight, because the ordinal IS
// the identity - delete one preset and every stored override
// after it starts pointing at the wrong palette. Since presets now
// have to be creatable and deletable, identity moves to a stable
// string id and the twelve colors are stored already resolved, so
// there is nothing left to key wrongly.
//
// darkmod_presets.ini is therefore the source of truth and has a
// single writer. darkmod_palette_overrides.ini is read exactly
// once, during migration, and then left alone as an archive - its
// format is unchanged, so nothing that reads it breaks.
// ============================================================

#include "DarkModColor.h"

#include <string>
#include <vector>

namespace dm
{

    // ------------------------------------------------------------
    // CUSTOM PALETTE
    //
    // Four colors the user edits by hand, expanded into the twelve
    // families. Kept as its own thing because the launcher has an
    // editor for exactly these four.
    // ------------------------------------------------------------

    struct CustomPaletteColors
    {
        Color background{ 0x1E, 0x1E, 0x2E };
        Color surface{ 0x31, 0x32, 0x44 };
        Color text{ 0xCD, 0xD6, 0xF4 };
        Color accent{ 0xF5, 0xC2, 0xE7 };
    };

    Color CustomFamilyColor(
        const CustomPaletteColors& custom,
        ColorFamily f);


    // ------------------------------------------------------------
    // PRESET
    // ------------------------------------------------------------

    struct Preset
    {
        //
        // Stable, ASCII, never shown to the user. Built-in presets
        // keep the ids the old ordinals stood for: dark, deepdark,
        // midnight, mocha, macchiato, frappe, latte, custom.
        //
        std::string id;

        std::wstring name;

        //
        // true for the eight presets the exe can regenerate from
        // its own tables. Only affects grouping in the menu and
        // what "restore defaults" recreates.
        //
        bool builtin = false;

        Color family[FAMILY_COUNT]{};

        //
        // Provenance, written by Palette Studio so a variant stays
        // reproducible and so the preference model can learn from
        // what the user actually kept. Zero/empty otherwise.
        //
        uint64_t seed = 0;
        std::string params;
    };


    // ------------------------------------------------------------
    // BUILT-IN TABLES
    //
    // The seed values for the eight presets that used to be
    // compiled in. They stay compiled in - not because a preset
    // must exist in code, but because "restore defaults" has to
    // work on a machine whose preset file was deleted.
    // ------------------------------------------------------------

    constexpr int BUILTIN_PRESET_COUNT = 8;

    const char* BuiltinPresetId(
        int index);

    const wchar_t* BuiltinPresetName(
        int index);

    //
    // Index BUILTIN_PRESET_COUNT - 1 is Custom and is derived from
    // the four custom colors rather than from a table.
    //
    Color BuiltinFamilyColor(
        int index,
        ColorFamily f,
        const CustomPaletteColors& custom);

    Preset MakeBuiltinPreset(
        int index,
        const CustomPaletteColors& custom);


    // ------------------------------------------------------------
    // LEGACY OVERRIDES  (darkmod_palette_overrides.ini)
    //
    // Read-only, one time, for migration.
    //
    // The writer that produced these files left std::hex set after
    // the first record, so family indices 10 and 11 - Mauve and
    // Pink - were written as "A" and "B" from the second record on.
    // The old reader parsed indices as decimal only, dropped those
    // records silently, and the next save no longer contained
    // them: hand-tuned colors disappeared. So this reader tries
    // decimal first and hex second, and reports how many records it
    // had to rescue that way.
    // ------------------------------------------------------------

    constexpr int OVERRIDE_FILE_VERSION = 2;
    constexpr int OVERRIDE_PALETTE_COUNT = 8;

    struct LegacyOverrides
    {
        Color color
            [OVERRIDE_PALETTE_COUNT]
        [FAMILY_COUNT]{};

        bool set
            [OVERRIDE_PALETTE_COUNT]
        [FAMILY_COUNT]{};

        int records = 0;

        //
        // Records whose family index only parsed as hex, i.e. the
        // ones the old reader lost.
        //
        int recovered = 0;

        //
        // A header naming a different version. The ordinals moved,
        // so the values cannot be trusted and nothing is loaded.
        //
        bool versionMismatch = false;
    };

    bool LoadLegacyOverrides(
        const std::wstring& path,
        LegacyOverrides& out);


    // ------------------------------------------------------------
    // PRESET STORE  (darkmod_presets.ini)
    //
    // version|1
    // selected|dark
    // preset|dark|Dark|builtin
    // color|0|#11111B
    // ... twelve color lines, family index 0..11
    // meta|seed|918273645
    // meta|params|darkness=0.62;contrast=1.15
    // ------------------------------------------------------------

    constexpr int PRESET_FILE_VERSION = 1;

    enum class PresetLoad
    {
        //
        // No file yet. Expected on first run; the caller migrates.
        //
        Missing,

        Ok,

        //
        // Present but unreadable. Nothing is loaded and nothing is
        // overwritten without the user asking.
        //
        Corrupt,

        //
        // Written by a newer build. Left strictly alone.
        //
        FutureVersion
    };


    class PresetStore
    {
    public:

        PresetLoad Load(
            const std::wstring& path);

        //
        // Backs the previous file up, then replaces it atomically.
        //
        bool Save(
            const std::wstring& path) const;


        const std::vector<Preset>& All() const
        {
            return m_presets;
        }

        int Count() const
        {
            return
                static_cast<int>(
                    m_presets.size());
        }

        int IndexOf(
            const std::string& id) const;

        const Preset* Find(
            const std::string& id) const;

        Preset* Find(
            const std::string& id);

        //
        // Never returns null: falls back to the first preset, and
        // to a built-in Dark if the list is somehow empty, because
        // every caller draws with these twelve colors.
        //
        const Preset& Selected() const;

        int SelectedIndex() const;

        const std::string& SelectedId() const
        {
            return m_selectedId;
        }

        void SetSelectedId(
            const std::string& id);


        //
        // Fills an empty store with the eight built-ins, applying
        // legacy overrides on top when they are supplied, so the
        // migrated file contains exactly the colors the user was
        // already seeing.
        //
        void SeedBuiltins(
            const CustomPaletteColors& custom,
            const LegacyOverrides* overrides);

        //
        // Recreates missing built-ins and resets the ones present
        // to their table values. Generated presets are untouched.
        //
        void RestoreBuiltins(
            const CustomPaletteColors& custom);

        void AddOrReplace(
            const Preset& preset);

        bool Remove(
            const std::string& id);

        bool Rename(
            const std::string& id,
            const std::wstring& name);

        //
        // Rewrites the twelve colors of the Custom preset from the
        // four editable colors, if that preset still exists.
        //
        void SyncCustomPreset(
            const CustomPaletteColors& custom);

        //
        // base plus a numeric suffix if needed.
        //
        std::string MakeUniqueId(
            const std::string& base) const;

    private:

        std::vector<Preset> m_presets;
        std::string m_selectedId;
    };

}

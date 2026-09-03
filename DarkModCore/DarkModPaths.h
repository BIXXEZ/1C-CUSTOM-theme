#pragma once

// ============================================================
// DARKMOD CORE - PATHS AND FILE IO
//
// Two very different callers share this:
//
//   the exe's  - want %LOCALAPPDATA%\DarkMod
//   the DLL    - must use its own module directory, because it is
//                loaded from wherever the launcher wrote it and
//                the config has to sit next to it
//
// So the data directory is resolved once at startup and cached.
// The DLL calls SetDataDirectory() with its module directory and
// never touches the %LOCALAPPDATA% path, which also keeps
// SHGetKnownFolderPath-style dependencies out of a library that
// gets injected into 1C.
// ============================================================

#include <windows.h>

#include <string>

namespace dm
{

    // ------------------------------------------------------------
    // FILE NAMES
    //
    // One list, so a rename cannot leave one binary looking for
    // the old name.
    // ------------------------------------------------------------

    constexpr const wchar_t* FILE_MAPPINGS = L"darkmod.ini";
    constexpr const wchar_t* FILE_FONTS = L"darkmod_fonts.ini";
    constexpr const wchar_t* FILE_COLORS = L"darkmod_colors.json";
    constexpr const wchar_t* FILE_OVERRIDES = L"darkmod_palette_overrides.ini";
    constexpr const wchar_t* FILE_CUSTOM = L"darkmod_custom_palette.ini";
    constexpr const wchar_t* FILE_PRESETS = L"darkmod_presets.ini";
    constexpr const wchar_t* FILE_FEEDBACK = L"palette_feedback.json";
    constexpr const wchar_t* FILE_LOG = L"darkmod.log";
    constexpr const wchar_t* FILE_STUDIO_LOG = L"palette_studio.log";
    constexpr const wchar_t* FILE_DLL = L"DarkModDLL.dll";


    // ------------------------------------------------------------
    // DIRECTORIES
    // ------------------------------------------------------------

    //
    // Directory of a loaded module. nullptr means the running exe.
    //
    std::wstring ModuleDirectory(
        HMODULE module = nullptr);

    //
    // %LOCALAPPDATA%\DarkMod, created if missing. Falls back to
    // the exe directory when the variable is unusable, which is
    // exactly what the pre-single-exe builds did.
    //
    std::wstring LocalAppDataDirectory();

    //
    // Pin the data directory. Call before any FilePath() use.
    //
    void SetDataDirectory(
        const std::wstring& dir);

    //
    // Resolves to LocalAppDataDirectory() the first time it is
    // asked, unless SetDataDirectory() already pinned one.
    //
    const std::wstring& DataDirectory();

    std::wstring FilePath(
        const wchar_t* name);

    bool FileExists(
        const std::wstring& path);


    // ------------------------------------------------------------
    // TEXT FILES
    //
    // Binary mode on purpose: these files are read back byte for
    // byte, and CRLF translation would make a saved file differ
    // from what was written.
    // ------------------------------------------------------------

    std::string ReadTextFile(
        const std::wstring& path);

    bool WriteTextFile(
        const std::wstring& path,
        const std::string& text);

    //
    // Write to path + ".tmp", then MoveFileEx over the target, so a
    // reader either sees the whole previous file or the whole new
    // one. The DLL writes darkmod_colors.json while the launcher is
    // polling it, which is the case this exists for.
    //
    bool WriteTextFileAtomic(
        const std::wstring& path,
        const std::string& text);

    //
    // Copy path -> path + ".bak" before path is replaced. Only
    // overwrites an existing backup when the current file has
    // content, so a truncated write cannot destroy the last good
    // copy.
    //
    bool BackupFile(
        const std::wstring& path);

}

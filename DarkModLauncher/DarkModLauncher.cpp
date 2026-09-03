#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <array>

#include "DarkModResource.h"

// ============================================================
// SHARED LOGIC
//
// Color model, family classification, path handling, file IO,
// the colors.json reader, the mapping format and the GDI drawing
// primitives all live in DarkModCore now. The launcher used to
// carry its own copy of each of them and the DLL a third, which
// is how COLOR_MATCH_RADIUS ended up with a "must mirror" comment
// instead of a single definition.
// ============================================================

#include "../DarkModCore/DarkModColor.h"
#include "../DarkModCore/DarkModColorsJson.h"
#include "../DarkModCore/DarkModMapping.h"
#include "../DarkModCore/DarkModPaths.h"
#include "../DarkModCore/DarkModPreset.h"
#include "../DarkModCore/DarkModUi.h"

using namespace dm;

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

// ============================================================
// DARKMOD LAUNCHER
// alphatest 0.1
//
// Custom dark Catppuccin UI.
// No external UI framework.
//
// Features:
//   - MOD slider Enable / Disable
//   - 1C process detection
//   - DLL injection
//   - Safe runtime Enable / Disable through DLL exports
//   - Palette selector
//   - Font selector
//   - Custom family color picker
//   - Live colors from darkmod_colors.json
//   - 12 semantic color families
//   - Scrollable family viewport
//   - Detailed real-color analysis window
//   - Animated UI
//   - Telegram bug-report link
//   - Embedded DarkModDLL payload
//
// Shipping unit is this exe alone. DarkModDLL.dll travels inside
// it as an RCDATA resource and is written out on startup.
//
// Runtime files, all in one directory (%LOCALAPPDATA%\DarkMod):
//   DarkModDLL.dll
//   darkmod.ini
//   darkmod_fonts.ini
//   darkmod_colors.json
//   darkmod_palette_overrides.ini
//   darkmod_custom_palette.ini
//   darkmod.log                     (written by the DLL)
//
// One directory and not two, because the injected DLL derives
// every path from its own module directory.
//
// DLL exports expected:
//   DarkModEnable
//   DarkModDisable
//   DarkModRefreshAnalysis
// ============================================================

// ============================================================
// COLORS
//
// Color, ToColorRef, ParseHexColor, FormatHex, ColorDistanceSq,
// LerpColor, DarkenColor, LightenColor and the twelve families
// come from DarkModCore.
// ============================================================

// ============================================================
// PRESETS
//
// The selected theme used to be an enum value, PaletteKind, and
// everything about a theme was reachable from that ordinal: a
// compiled-in color table, a chrome table, a name, a row in the
// override matrix. That works only while there are exactly eight
// themes forever.
//
// Now the list is data - loaded from darkmod_presets.ini, any
// length, presets addable and removable - so the ordinal is gone
// and the selection is a string id. Every place that used to
// switch on the ordinal now reads the twelve colors of the
// selected preset instead.
// ============================================================

static PresetStore g_presets;

static const Preset& CurrentPreset()
{
    return g_presets.Selected();
}

static bool g_uiDark = false;

// ============================================================
// LAUNCHER CHROME
//
// One complete ramp per palette. Before this table only UiBase()
// looked at the selected theme and every other accessor returned
// hardcoded Mocha values, so Latte drew #CDD6F4 text on its
// #EFF1F5 base - white on white - and all the dark presets looked
// identical no matter which one was selected.
//
// These colors are the launcher's own chrome only. What actually
// gets applied to 1C comes from PresetFamilyColor.
// ============================================================

struct UiTheme
{
    Color base;
    Color mantle;
    Color crust;
    Color surface0;
    Color surface1;
    Color surface2;
    Color text;
    Color sub1;
    Color sub0;
    Color blue;
    Color green;
    Color yellow;
    Color red;
    Color mauve;
    Color pink;
};

// Catppuccin Latte. Also the chrome shown before a palette is
// engaged, which is why the light branch below returns it.
static constexpr UiTheme UI_LATTE
{
    { 239,241,245 },
    { 230,233,239 },
    { 220,224,232 },
    { 204,208,218 },
    { 188,192,204 },
    { 172,176,190 },
    { 76,79,105 },
    { 92,96,120 },
    { 108,112,134 },
    { 30,102,245 },
    { 64,160,43 },
    { 223,142,29 },
    { 210,15,57 },
    { 136,57,239 },
    { 234,118,203 }
};

// Catppuccin Mocha.
static constexpr UiTheme UI_MOCHA
{
    { 30,30,46 },
    { 24,24,37 },
    { 17,17,27 },
    { 49,50,68 },
    { 69,71,90 },
    { 88,91,112 },
    { 205,214,244 },
    { 166,173,200 },
    { 147,153,178 },
    { 137,180,250 },
    { 166,227,161 },
    { 249,226,175 },
    { 243,139,168 },
    { 203,166,247 },
    { 245,194,231 }
};

// Catppuccin Macchiato.
static constexpr UiTheme UI_MACCHIATO
{
    { 36,39,58 },
    { 30,32,48 },
    { 24,25,38 },
    { 54,58,79 },
    { 73,77,100 },
    { 91,96,120 },
    { 202,211,245 },
    { 184,192,224 },
    { 165,173,203 },
    { 138,173,244 },
    { 166,218,149 },
    { 238,212,159 },
    { 237,135,150 },
    { 198,160,246 },
    { 245,189,230 }
};

// Catppuccin Frappe.
static constexpr UiTheme UI_FRAPPE
{
    { 48,52,70 },
    { 41,44,60 },
    { 35,38,52 },
    { 65,69,89 },
    { 81,87,109 },
    { 98,104,128 },
    { 198,208,245 },
    { 181,191,226 },
    { 165,173,206 },
    { 140,170,238 },
    { 166,209,137 },
    { 229,200,144 },
    { 231,130,132 },
    { 202,158,230 },
    { 244,184,228 }
};

// Neutral dark - achromatic on purpose, so it reads differently
// from the blue-tinted Catppuccin presets.
static constexpr UiTheme UI_DARK
{
    { 30,30,30 },
    { 24,24,24 },
    { 17,17,17 },
    { 45,45,45 },
    { 63,63,63 },
    { 82,82,82 },
    { 228,228,228 },
    { 189,189,189 },
    { 154,154,154 },
    { 108,182,255 },
    { 126,231,135 },
    { 227,179,65 },
    { 255,123,114 },
    { 210,168,255 },
    { 240,168,208 }
};

// Near black.
static constexpr UiTheme UI_DEEP_DARK
{
    { 8,8,8 },
    { 5,5,5 },
    { 0,0,0 },
    { 26,26,26 },
    { 42,42,42 },
    { 60,60,60 },
    { 232,232,232 },
    { 192,192,192 },
    { 148,148,148 },
    { 88,166,255 },
    { 86,211,100 },
    { 210,153,34 },
    { 248,81,73 },
    { 188,140,255 },
    { 219,97,162 }
};

// Deep blue.
static constexpr UiTheme UI_MIDNIGHT
{
    { 26,33,56 },
    { 21,27,46 },
    { 15,20,34 },
    { 38,48,77 },
    { 51,63,99 },
    { 68,81,121 },
    { 211,220,242 },
    { 174,185,214 },
    { 139,150,181 },
    { 122,162,247 },
    { 158,206,106 },
    { 224,175,104 },
    { 247,118,142 },
    { 187,154,247 },
    { 227,159,203 }
};

// ------------------------------------------------------------
// Built-in presets keep their hand-tuned chrome table, so their
// appearance is unchanged to the pixel. A generated preset has no
// table, so its chrome is derived from its own twelve families -
// otherwise every Studio preset would render in Mocha and the
// launcher would lie about what is selected.
//
// The family order is fixed: 0 White, 1 LightGray, 2 Gray,
// 3 DarkGray, 4 Black, 5 Red, 6 Peach, 7 Yellow, 8 Green,
// 9 Blue, 10 Mauve, 11 Pink. In DarkMod terms the first four are
// the background ramp of the recolored interface and Black is its
// text, which is exactly what chrome needs.
// ------------------------------------------------------------

static UiTheme DeriveUiTheme(
    const Preset& preset)
{
    const Color* f = preset.family;

    UiTheme t{};

    //
    // A light preset paints dark text on a light background, so
    // its ramp runs the other way and crust is the darkest end
    // rather than the lightest. Getting this backwards would put
    // the window background on the wrong side of the text.
    //
    const bool light =
        Brightness(f[2]) >
        Brightness(f[4]);

    if (light)
    {
        t.base = f[0];
        t.mantle = f[1];
        t.crust = f[2];
    }
    else
    {
        t.crust = f[0];
        t.mantle = f[1];
        t.base = f[2];
    }

    t.surface0 = f[3];

    t.surface1 =
        light
        ? DarkenColor(f[3], 0.14f)
        : LightenColor(f[3], 0.14f);

    t.surface2 =
        light
        ? DarkenColor(f[3], 0.28f)
        : LightenColor(f[3], 0.28f);

    t.text = f[4];

    t.sub1 =
        LerpColor(
            f[4],
            t.base,
            0.25f);

    t.sub0 =
        LerpColor(
            f[4],
            t.base,
            0.42f);

    t.red = f[5];
    t.yellow = f[7];
    t.green = f[8];
    t.blue = f[9];
    t.mauve = f[10];
    t.pink = f[11];

    return t;
}


static UiTheme UiThemeFor(
    const Preset& preset)
{
    if (preset.builtin)
    {
        if (preset.id == "dark")
            return UI_DARK;

        if (preset.id == "deepdark")
            return UI_DEEP_DARK;

        if (preset.id == "midnight")
            return UI_MIDNIGHT;

        if (preset.id == "mocha")
            return UI_MOCHA;

        if (preset.id == "macchiato")
            return UI_MACCHIATO;

        if (preset.id == "frappe")
            return UI_FRAPPE;

        if (preset.id == "latte")
            return UI_LATTE;

        //
        // Custom sets the 1C colors, not the launcher chrome;
        // Mocha is what it has always rendered as.
        //
        if (preset.id == "custom")
            return UI_MOCHA;
    }

    return
        DeriveUiTheme(
            preset);
}

static UiTheme CurrentUiTheme()
{
    //
    // Light chrome until a palette is actually engaged.
    //

    if (!g_uiDark)
        return UI_LATTE;

    return
        UiThemeFor(
            CurrentPreset());
}

static Color UiBase()
{
    return CurrentUiTheme().base;
}

static Color UiMantle()
{
    return CurrentUiTheme().mantle;
}

static Color UiCrust()
{
    return CurrentUiTheme().crust;
}

static Color UiSurface0()
{
    return CurrentUiTheme().surface0;
}

static Color UiSurface1()
{
    return CurrentUiTheme().surface1;
}

static Color UiSurface2()
{
    return CurrentUiTheme().surface2;
}

static Color UiText()
{
    return CurrentUiTheme().text;
}

static Color UiSub0()
{
    return CurrentUiTheme().sub0;
}

static Color UiSub1()
{
    return CurrentUiTheme().sub1;
}

static Color UiBlue()
{
    return CurrentUiTheme().blue;
}

static Color UiGreen()
{
    return CurrentUiTheme().green;
}

static Color UiYellow()
{
    return CurrentUiTheme().yellow;
}

static Color UiRed()
{
    return CurrentUiTheme().red;
}

static Color UiMauve()
{
    return CurrentUiTheme().mauve;
}

static Color UiPink()
{
    return CurrentUiTheme().pink;
}

#define CAT_BASE UiBase()
#define CAT_MANTLE UiMantle()
#define CAT_CRUST UiCrust()
#define CAT_SURFACE0 UiSurface0()
#define CAT_SURFACE1 UiSurface1()
#define CAT_SURFACE2 UiSurface2()
#define CAT_TEXT UiText()
#define CAT_SUBTEXT0 UiSub0()
#define CAT_SUBTEXT1 UiSub1()
#define CAT_BLUE UiBlue()
#define CAT_GREEN UiGreen()
#define CAT_YELLOW UiYellow()
#define CAT_RED UiRed()
#define CAT_MAUVE UiMauve()
#define CAT_PINK UiPink()

// ============================================================
// WINDOW
// ============================================================

static constexpr wchar_t WINDOW_CLASS[] =
L"DarkModeAlphaTest01";

static constexpr wchar_t WINDOW_TITLE[] =
L"DarkMode alphatest 0.1";

//
// The file names themselves are in DarkModCore, so a rename
// cannot leave one binary looking for the old one.
//
static constexpr const wchar_t* DLL_NAME = FILE_DLL;
static constexpr const wchar_t* COLORS_NAME = FILE_COLORS;
static constexpr const wchar_t* INI_NAME = FILE_MAPPINGS;
static constexpr const wchar_t* FONTS_INI_NAME = FILE_FONTS;


// ============================================================
// LAYOUT
// ============================================================

static constexpr int WINDOW_W = 620;
static constexpr int WINDOW_H = 650;

static constexpr int LEFT = 24;
static constexpr int RIGHT = WINDOW_W - 24;

static constexpr int HEADER_H = 64;

// MOD slider.
static constexpr int BUTTON_Y = 78;
static constexpr int BUTTON_H = 40;
static constexpr int SLIDER_W = 250;

// Theme / font selectors.
static constexpr int THEME_Y = 170;
static constexpr int FONT_Y = 170;
static constexpr int SELECTOR_H = 44;

static constexpr int THEME_X = 24;
static constexpr int THEME_RIGHT = 300;

static constexpr int FONT_X = 320;
static constexpr int FONT_RIGHT = 596;

// Live family view.
static constexpr int LIVE_LABEL_Y = 232;
static constexpr int LIVE_Y = 262;
static constexpr int ROW_H = 31;

static constexpr int FAMILY_VIEW_ROWS = 6;
static constexpr int FAMILY_VIEW_H =
FAMILY_VIEW_ROWS * ROW_H;

static constexpr int FAMILY_VIEW_BOTTOM =
LIVE_Y + FAMILY_VIEW_H;

static constexpr int DETAIL_Y =
FAMILY_VIEW_BOTTOM + 7;

static constexpr int DETAIL_H = 30;

//
// Removed: FOOTER_Y = WINDOW_H - 28.
//
// It mixed an outer window size into client coordinates, which
// is what broke the Telegram link. The footer is positioned by
// FooterRect / FooterHitRect from the real client height now.
//

// ============================================================
// BUTTONS
// ============================================================

enum class ButtonKind
{
    Enable,
    Disable,
    Refresh
};

struct AnimatedButton
{
    RECT rc{};

    ButtonKind kind{};

    bool hovered = false;
    bool pressed = false;

    float hover = 0.0f;
    float press = 0.0f;
};

// ============================================================
// UI PRESS TARGETS
// ============================================================

enum class UiPressTarget
{
    None,
    Slider,
    ThemeSelector,
    FontSelector,
    FamilyReplacement,
    Details,
    RefreshInput,
    Footer,
    ThemeItem,
    FontItem
};

static UiPressTarget g_pressedUi =
UiPressTarget::None;

// ============================================================
// LIVE COLOR / 12 COLOR FAMILIES
//
// LiveColor, ColorFamily, FamilyName and ClassifyFamily are in
// DarkModCore. Palette Studio classifies with the same thresholds
// as the launcher because it calls the same function, not because
// somebody kept two copies in step.
// ============================================================

static std::vector<LiveColor>
g_liveColors;
//
// Immutable source data for the current analysis.
// This is the ONLY color set used to build mappings.
//
// g_liveColors = current displayed analysis data.
// g_originalLiveColors = exact source snapshot.
//
static std::vector<LiveColor>
g_originalLiveColors;

static bool
g_originalAnalysisValid = false;

struct FamilyStat
{
    ColorFamily family{};
    double percentage = 0.0;
    uint64_t pixels = 0;
    Color representative{};
    bool present = false;
};

static std::array<FamilyStat, 12>
BuildFamilies()
{
    std::array<FamilyStat, 12> out{};

    //
    // Tracked separately from FamilyStat::pixels, which is the
    // running total for the family. Comparing a candidate against
    // the total meant the test failed for everything after the
    // first color, so the representative was whichever color came
    // first rather than the dominant one.
    //

    uint64_t bestPixels[12]{};

    for (int i = 0; i < 12; ++i)
    {
        out[i].family =
            static_cast<ColorFamily>(i);
    }

    for (const auto& lc : g_originalLiveColors)
    {
        Color c =
            ParseHexColor(lc.hex);

        ColorFamily f =
            ClassifyFamily(c);

        const size_t fi =
            (size_t)f;

        auto& x =
            out[fi];

        x.present = true;
        x.pixels += lc.pixels;
        x.percentage += lc.percentage;

        if (
            bestPixels[fi] == 0 ||
            lc.pixels > bestPixels[fi])
        {
            bestPixels[fi] = lc.pixels;
            x.representative = c;
        }
    }

    return out;
}


// ============================================================
// PALETTE / THEME
//
// PaletteName() was a switch over the eight enum values. Names
// are part of a preset now, so the display name is just
// CurrentPreset().name and a generated preset can carry one too.
// ============================================================


// ============================================================
// GLOBAL STATE
// ============================================================

static HWND g_hwnd = nullptr;

static HFONT g_font = nullptr;
static HFONT g_smallFont = nullptr;
static HFONT g_tinyFont = nullptr;

static AnimatedButton g_buttons[3]{};

static int g_hoverButton = -1;
static int g_pressedButton = -1;

static bool g_paletteOpen = false;
static bool g_fontOpen = false;
static bool g_darkGroupOpen = false;
static bool g_catppuccinGroupOpen = false;
static bool g_generatedGroupOpen = false;

//
// Pixels, not rows: the theme menu mixes 25-pixel headers with
// 31-pixel entries, so there is no single row height to count in.
//
static int g_themeScroll = 0;



static std::wstring g_fontSelection =
L"Segoe UI";

static bool g_darkModLoaded = false;
static bool g_darkModEnabled = false;

static DWORD g_currentPid = 0;
static std::wstring g_currentProcess;

static ULONGLONG g_lastJsonStamp = 0;

static bool g_dragging = false;
static float g_sliderPos = 0.0f;

static int g_animTimer = 1;
static int g_pollTimer = 2;

static int g_familyScroll = 0;


// ============================================================
// CUSTOM PALETTE
// ============================================================

struct CustomPalette
{
    std::wstring background = L"#1E1E2E";
    std::wstring surface = L"#313244";
    std::wstring text = L"#CDD6F4";
    std::wstring accent = L"#F5C2E7";
};

static CustomPalette g_custom;

//
// g_familyOverrides / g_familyOverrideSet / PaletteIndex lived
// here: an [8][12] matrix keyed by the PaletteKind ordinal. The
// ordinal was the identity, so deleting a preset would have
// shifted every stored color onto the wrong palette. The twelve
// colors now live in the preset itself.
//


// ============================================================
// CUSTOM WINDOW
//
// Kept from the original source for compatibility and reuse.
// The main Launcher no longer opens this popup for Custom.
// ============================================================

static HWND g_customWindow = nullptr;
static HWND g_detailsWindow = nullptr;
static int g_detailsScroll = 0;

static HWND g_editBg = nullptr;
static HWND g_editSurface = nullptr;
static HWND g_editText = nullptr;
static HWND g_editAccent = nullptr;


// ============================================================
// HELPERS
//
// ModuleDirectory(), LocalAppDataDirectory(), DataDirectory() and
// FilePath() come from DarkModCore, so the launcher, the DLL and
// Palette Studio agree on where the config lives.
// ============================================================


//
// One-time move of an existing setup into the data directory.
//
// Earlier builds kept the config next to the exe, so an upgrade
// would otherwise silently start from defaults and lose the
// tuned palettes. Copies only, and only what is missing, so
// running an old build again still finds its own files.
//
static void MigrateLegacyFiles(
    const std::wstring& from,
    const std::wstring& to)
{
    if (
        from.empty() ||
        to.empty() ||
        _wcsicmp(
            from.c_str(),
            to.c_str()) == 0)
    {
        return;
    }

    static const wchar_t* const names[] =
    {
        FILE_MAPPINGS,
        FILE_FONTS,
        FILE_COLORS,
        FILE_OVERRIDES,
        FILE_CUSTOM,
        FILE_PRESETS
    };

    for (const wchar_t* name : names)
    {
        const std::wstring source =
            from + L"\\" + name;

        const std::wstring target =
            to + L"\\" + name;

        if (
            GetFileAttributesW(
                source.c_str()) ==
            INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }

        //
        // TRUE = do not overwrite. The data directory always
        // wins; this only fills in what was never there.
        //
        CopyFileW(
            source.c_str(),
            target.c_str(),
            TRUE);
    }
}


static std::wstring ErrorText(
    DWORD code)
{
    wchar_t* buffer = nullptr;

    DWORD size =
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            code,
            0,
            reinterpret_cast<LPWSTR>(
                &buffer),
            0,
            nullptr);

    std::wstring result;

    if (size && buffer)
    {
        result.assign(
            buffer,
            size);

        while (
            !result.empty() &&
            (
                result.back() == L'\r' ||
                result.back() == L'\n' ||
                result.back() == L' '
                ))
        {
            result.pop_back();
        }
    }

    if (buffer)
        LocalFree(buffer);

    return result;
}


static void OpenTelegram()
{
    ShellExecuteW(
        nullptr,
        L"open",
        L"https://t.me/bixxez",
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
}


// ============================================================
// 1C PROCESS
// ============================================================

static bool Is1CProcess(
    const wchar_t* name)
{
    if (!name)
        return false;

    std::wstring s(name);

    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](wchar_t c)
        {
            return static_cast<wchar_t>(
                towlower(c));
        });

    return
        s == L"1cv8.exe" ||
        s == L"1cv8c.exe" ||
        s == L"1cv8s.exe";
}


struct ProcessInfo
{
    DWORD pid = 0;
    std::wstring name;
};


static std::vector<ProcessInfo>
Find1CProcesses()
{
    std::vector<ProcessInfo> result;

    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

    if (
        snapshot ==
        INVALID_HANDLE_VALUE)
    {
        return result;
    }

    PROCESSENTRY32W pe{};

    pe.dwSize =
        sizeof(pe);

    if (
        Process32FirstW(
            snapshot,
            &pe))
    {
        do
        {
            if (
                Is1CProcess(
                    pe.szExeFile))
            {
                result.push_back(
                    {
                        pe.th32ProcessID,
                        pe.szExeFile
                    });
            }

        } while (
            Process32NextW(
                snapshot,
                &pe));
    }

    CloseHandle(snapshot);

    return result;
}


static DWORD Find1CPid()
{
    const auto list =
        Find1CProcesses();

    if (list.empty())
        return 0;

    return list.front().pid;
}


// ============================================================
// MODULE DETECTION
// ============================================================

static bool IsDarkModLoaded(
    DWORD pid)
{
    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE |
            TH32CS_SNAPMODULE32,
            pid);

    if (
        snapshot ==
        INVALID_HANDLE_VALUE)
    {
        return false;
    }

    MODULEENTRY32W me{};

    me.dwSize =
        sizeof(me);

    bool found = false;

    if (
        Module32FirstW(
            snapshot,
            &me))
    {
        do
        {
            if (
                _wcsicmp(
                    me.szModule,
                    DLL_NAME) == 0)
            {
                found = true;
                break;
            }

        } while (
            Module32NextW(
                snapshot,
                &me));
    }

    CloseHandle(snapshot);

    return found;
}


// ============================================================
// REMOTE EXPORT
// ============================================================

static bool GetExportRVA(
    const std::wstring& dllPath,
    const char* exportName,
    DWORD& rva)
{
    rva = 0;

    HMODULE local =
        LoadLibraryExW(
            dllPath.c_str(),
            nullptr,
            DONT_RESOLVE_DLL_REFERENCES);

    if (!local)
        return false;

    FARPROC function =
        GetProcAddress(
            local,
            exportName);

    if (!function)
    {
        FreeLibrary(local);
        return false;
    }

    const uintptr_t base =
        reinterpret_cast<uintptr_t>(
            local);

    const uintptr_t address =
        reinterpret_cast<uintptr_t>(
            function);

    if (address < base)
    {
        FreeLibrary(local);
        return false;
    }

    const uintptr_t delta =
        address - base;

    if (delta > 0xFFFFFFFFull)
    {
        FreeLibrary(local);
        return false;
    }

    rva =
        static_cast<DWORD>(
            delta);

    FreeLibrary(local);

    return true;
}


static bool CallRemoteExport(
    DWORD pid,
    const char* exportName)
{
    struct RemoteModule
    {
        HMODULE base = nullptr;
    };

    RemoteModule remote{};

    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE |
            TH32CS_SNAPMODULE32,
            pid);

    if (
        snapshot ==
        INVALID_HANDLE_VALUE)
    {
        return false;
    }

    MODULEENTRY32W me{};

    me.dwSize =
        sizeof(me);

    bool found = false;

    if (
        Module32FirstW(
            snapshot,
            &me))
    {
        do
        {
            if (
                _wcsicmp(
                    me.szModule,
                    DLL_NAME) == 0)
            {
                remote.base =
                    me.hModule;

                found = true;

                break;
            }

        } while (
            Module32NextW(
                snapshot,
                &me));
    }

    CloseHandle(snapshot);

    if (!found)
        return false;

    DWORD rva = 0;

    if (
        !GetExportRVA(
            FilePath(DLL_NAME),
            exportName,
            rva))
    {
        return false;
    }

    const uintptr_t remoteAddress =
        reinterpret_cast<uintptr_t>(
            remote.base) +
        static_cast<uintptr_t>(
            rva);

    HANDLE process =
        OpenProcess(
            PROCESS_CREATE_THREAD |
            PROCESS_QUERY_INFORMATION,
            FALSE,
            pid);

    if (!process)
        return false;

    HANDLE thread =
        CreateRemoteThread(
            process,
            nullptr,
            0,
            reinterpret_cast<
            LPTHREAD_START_ROUTINE>(
                remoteAddress),
            nullptr,
            0,
            nullptr);

    if (!thread)
    {
        CloseHandle(process);
        return false;
    }

    DWORD wait =
        WaitForSingleObject(
            thread,
            5000);

    CloseHandle(thread);
    CloseHandle(process);

    return wait == WAIT_OBJECT_0;
}


// ============================================================
// EMBEDDED DLL
//
// DarkModDLL.dll is compiled into this exe as RCDATA (see
// DarkModLauncher.rc) and written back out to the data
// directory on startup, so only one file ever needs shipping.
//
// It cannot stay in memory: CreateRemoteThread(LoadLibraryW)
// takes a path, GetExportRVA reads the file, and
// CallRemoteExport locates the module in the target by name.
// ============================================================

static bool ReadEmbeddedDll(
    const unsigned char*& data,
    DWORD& size)
{
    data = nullptr;
    size = 0;

    HRSRC found =
        FindResourceW(
            nullptr,
            MAKEINTRESOURCEW(
                IDR_DARKMOD_DLL),
            RT_RCDATA);

    if (!found)
        return false;

    size =
        SizeofResource(
            nullptr,
            found);

    if (!size)
        return false;

    HGLOBAL loaded =
        LoadResource(
            nullptr,
            found);

    if (!loaded)
        return false;

    data =
        static_cast<
        const unsigned char*>(
            LockResource(
                loaded));

    //
    // No FreeResource: the block belongs to our own image and
    // stays mapped for the life of the process.
    //
    return data != nullptr;
}


//
// True when the file on disk is already byte for byte what we
// carry. This is the normal case on every launch after the
// first, and it is what keeps a DLL currently injected into 1C
// from being reported as an error - the locked file is the
// right file, so there is nothing to write.
//
static bool EmbeddedDllMatchesFile(
    const std::wstring& path,
    const unsigned char* data,
    DWORD size)
{
    HANDLE file =
        CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ |
            FILE_SHARE_WRITE |
            FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER length{};

    if (
        !GetFileSizeEx(
            file,
            &length) ||
        length.QuadPart !=
        static_cast<LONGLONG>(size))
    {
        CloseHandle(file);
        return false;
    }

    std::vector<unsigned char> onDisk(
        size);

    DWORD read = 0;

    const BOOL ok =
        ReadFile(
            file,
            onDisk.data(),
            size,
            &read,
            nullptr);

    CloseHandle(file);

    if (!ok || read != size)
        return false;

    return
        memcmp(
            onDisk.data(),
            data,
            size) == 0;
}


static bool ExtractEmbeddedDll()
{
    const std::wstring path =
        FilePath(DLL_NAME);

    const unsigned char* data = nullptr;
    DWORD size = 0;

    if (
        !ReadEmbeddedDll(
            data,
            size))
    {
        //
        // Only reachable if the resource failed to build into
        // the exe. An already extracted DLL still lets the
        // program run.
        //
        return
            GetFileAttributesW(
                path.c_str()) !=
            INVALID_FILE_ATTRIBUTES;
    }

    if (
        EmbeddedDllMatchesFile(
            path,
            data,
            size))
    {
        return true;
    }

    HANDLE file =
        CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;

        const BOOL ok =
            WriteFile(
                file,
                data,
                size,
                &written,
                nullptr);

        CloseHandle(file);

        if (ok && written == size)
            return true;

        return false;
    }

    const DWORD error =
        GetLastError();

    //
    // The usual reason the write fails: the DLL is loaded into
    // a running 1C right now, so the file is locked and cannot
    // be replaced with a different build.
    //
    const DWORD pid =
        Find1CPid();

    if (
        pid &&
        IsDarkModLoaded(pid))
    {
        MessageBoxW(
            g_hwnd,
            L"DarkModDLL.dll обновить не удалось: "
            L"библиотека сейчас внедрена в 1С.\n\n"
            L"Нажмите Remove DLL или закройте 1С "
            L"и запустите лаунчер снова.",
            WINDOW_TITLE,
            MB_ICONWARNING);

        //
        // The old copy is still usable, so this is not fatal.
        //
        return
            GetFileAttributesW(
                path.c_str()) !=
            INVALID_FILE_ATTRIBUTES;
    }

    MessageBoxW(
        g_hwnd,
        (
            L"Не удалось распаковать DarkModDLL.dll в\n" +
            path +
            L"\n\n" +
            ErrorText(error)
            ).c_str(),
        WINDOW_TITLE,
        MB_ICONERROR);

    return false;
}


// ============================================================
// INJECTION
// ============================================================

static bool InjectDLL(
    DWORD pid)
{
    const std::wstring dllPath =
        FilePath(DLL_NAME);

    if (
        GetFileAttributesW(
            dllPath.c_str()) ==
        INVALID_FILE_ATTRIBUTES)
    {
        //
        // ExtractEmbeddedDll runs at startup, so reaching this
        // means the file was removed underneath us or could
        // not be written at all.
        //
        MessageBoxW(
            g_hwnd,
            (
                L"DarkModDLL.dll отсутствует:\n" +
                dllPath
                ).c_str(),
            WINDOW_TITLE,
            MB_ICONERROR);

        return false;
    }

    HANDLE process =
        OpenProcess(
            PROCESS_CREATE_THREAD |
            PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE |
            PROCESS_VM_READ,
            FALSE,
            pid);

    if (!process)
    {
        MessageBoxW(
            g_hwnd,
            (
                L"OpenProcess failed:\n" +
                ErrorText(
                    GetLastError())
                ).c_str(),
            WINDOW_TITLE,
            MB_ICONERROR);

        return false;
    }

    const SIZE_T bytes =
        (dllPath.size() + 1) *
        sizeof(wchar_t);

    LPVOID remote =
        VirtualAllocEx(
            process,
            nullptr,
            bytes,
            MEM_COMMIT |
            MEM_RESERVE,
            PAGE_READWRITE);

    if (!remote)
    {
        CloseHandle(process);
        return false;
    }

    SIZE_T written = 0;

    if (
        !WriteProcessMemory(
            process,
            remote,
            dllPath.c_str(),
            bytes,
            &written) ||
        written != bytes)
    {
        VirtualFreeEx(
            process,
            remote,
            0,
            MEM_RELEASE);

        CloseHandle(process);

        return false;
    }

    HMODULE kernel32 =
        GetModuleHandleW(
            L"kernel32.dll");

    FARPROC loadLibrary =
        kernel32
        ? GetProcAddress(
            kernel32,
            "LoadLibraryW")
        : nullptr;

    if (!loadLibrary)
    {
        VirtualFreeEx(
            process,
            remote,
            0,
            MEM_RELEASE);

        CloseHandle(process);

        return false;
    }

    HANDLE thread =
        CreateRemoteThread(
            process,
            nullptr,
            0,
            reinterpret_cast<
            LPTHREAD_START_ROUTINE>(
                loadLibrary),
            remote,
            0,
            nullptr);

    if (!thread)
    {
        VirtualFreeEx(
            process,
            remote,
            0,
            MEM_RELEASE);

        CloseHandle(process);

        return false;
    }

    WaitForSingleObject(
        thread,
        10000);

    DWORD exitCode = 0;

    GetExitCodeThread(
        thread,
        &exitCode);

    CloseHandle(thread);

    VirtualFreeEx(
        process,
        remote,
        0,
        MEM_RELEASE);

    CloseHandle(process);

    return exitCode != 0;
}


// ============================================================
// ENABLE / DISABLE
// ============================================================

static bool EnableDarkMod(
    DWORD pid)
{
    if (!IsDarkModLoaded(pid))
    {
        if (!InjectDLL(pid))
            return false;

        Sleep(250);
    }

    //
    // Requires DarkModEnable export.
    //

    return CallRemoteExport(
        pid,
        "DarkModEnable");
}


static bool DisableDarkMod(
    DWORD pid)
{
    if (!IsDarkModLoaded(pid))
        return true;

    //
    // Requires DarkModDisable export.
    //

    return CallRemoteExport(
        pid,
        "DarkModDisable");
}


// ============================================================
// FORCE 1C REDRAW
// ============================================================

struct RedrawContext
{
    DWORD pid = 0;
};

static BOOL CALLBACK
RedrawWindowEnumProc(
    HWND hwnd,
    LPARAM lParam)
{
    auto* ctx =
        reinterpret_cast<RedrawContext*>(
            lParam);

    if (!ctx)
        return TRUE;

    DWORD pid = 0;

    GetWindowThreadProcessId(
        hwnd,
        &pid);

    if (pid != ctx->pid)
        return TRUE;

    if (!IsWindowVisible(hwnd))
        return TRUE;

    InvalidateRect(
        hwnd,
        nullptr,
        TRUE);

    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
        RDW_UPDATENOW |
        RDW_ERASE |
        RDW_ALLCHILDREN);

    UpdateWindow(hwnd);

    return TRUE;
}


static void ForceRedraw1C(
    DWORD pid)
{
    if (!pid)
        return;

    RedrawContext ctx{};
    ctx.pid = pid;

    EnumWindows(
        RedrawWindowEnumProc,
        reinterpret_cast<LPARAM>(&ctx));
}


// ============================================================
// FILE STAMP
// ============================================================

static ULONGLONG
GetFileStamp(
    const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};

    if (
        !GetFileAttributesExW(
            path.c_str(),
            GetFileExInfoStandard,
            &data))
    {
        return 0;
    }

    ULARGE_INTEGER t{};

    t.HighPart =
        data.ftLastWriteTime.dwHighDateTime;

    t.LowPart =
        data.ftLastWriteTime.dwLowDateTime;

    return t.QuadPart;
}


// ============================================================
// SIMPLE JSON
//
// ReadTextFile, the key scanners and ParseLiveColors are in
// DarkModCore, next to the writer in the DLL that produces this
// file - a change to one of them now cannot miss the other.
// ============================================================


// ============================================================
// COLORS JSON UPDATE
// ============================================================

// ------------------------------------------------------------
// Analysis payload rejection.
//
// darkmod_colors.json is a histogram of a GDI capture of the 1C
// window, so it is only "original colors" if the capture happened
// with MOD off AND after 1C had repainted. The old guard here -
// a plain g_darkModEnabled check - could not see the second half:
// OnDisable clears g_darkModEnabled BEFORE calling RefreshState(),
// so the guard is wide open exactly during the window where the
// DLL might still capture a modded frame.
//
// Two independent filters close it. The timestamp/mode filter
// mirrors the DLL-side quarantine; the own-output filter needs no
// timing at all and simply asks whether the set we just read looks
// like the palette we ourselves emitted.
// ------------------------------------------------------------

// Must match ANALYZE_QUARANTINE_MS in the DLL, with headroom for
// the launcher's own polling interval.
static constexpr ULONGLONG ANALYSIS_QUARANTINE_MS = 1500;

static ULONGLONG g_analysisQuarantineUntil = 0;

//
// Wraps the core check with the twelve colors of the selected
// preset. Defined further down, next to the preset accessors.
//
static bool CurrentPaletteIsOwnOutput(
    const std::vector<LiveColor>& colors);

static void UpdateLiveColors()
{
    //
    // Do not re-analyze while MOD is active.
    // Otherwise the already-remapped colors can
    // become the next "original" source set.
    //

    if (g_darkModEnabled)
        return;

    //
    // Nor immediately after leaving MOD: 1C may not have repainted
    // yet, so a capture taken now would record our own palette.
    //

    if (
        g_analysisQuarantineUntil &&
        GetTickCount64() <
        g_analysisQuarantineUntil)
    {
        return;
    }

    const std::wstring path =
        FilePath(COLORS_NAME);

    const ULONGLONG stamp =
        GetFileStamp(path);

    if (!stamp)
        return;

    if (
        stamp ==
        g_lastJsonStamp)
    {
        return;
    }

    g_lastJsonStamp =
        stamp;

    const std::string json =
        ReadTextFile(path);

    if (json.empty())
        return;

    //
    // The DLL only analyzes in ANALYZE mode, so anything else means
    // the file does not describe untouched input.
    //

    const std::string mode =
        ParseJsonMode(json);

    if (
        !mode.empty() &&
        mode != "ANALYZE")
    {
        return;
    }

    const std::vector<LiveColor>
        newColors =
        ParseLiveColors(json);

    if (newColors.empty())
        return;

    //
    // Last line of defence, independent of any timing: if the set
    // is mostly colors this palette produces, it is our own output
    // coming back at us. Keep the previous snapshot.
    //

    if (CurrentPaletteIsOwnOutput(newColors))
        return;

    g_originalLiveColors =
        newColors;

    g_liveColors =
        newColors;

    g_originalAnalysisValid =
        true;

    //
    // Keep scroll inside the new data range.
    //

    const int maxScroll =
        std::max(
            0,
            12 -
            FAMILY_VIEW_ROWS);

    g_familyScroll =
        std::clamp(
            g_familyScroll,
            0,
            maxScroll);

    InvalidateRect(
        g_hwnd,
        nullptr,
        FALSE);
}


// ============================================================
// PALETTE FILES
//
// WriteTextFile and WToUtf8 are in DarkModCore.
// ============================================================

static std::wstring ColorToWString(
    Color c)
{
    wchar_t buffer[16]{};

    swprintf_s(
        buffer,
        _countof(buffer),
        L"#%02X%02X%02X",
        static_cast<unsigned>(c.r),
        static_cast<unsigned>(c.g),
        static_cast<unsigned>(c.b));

    return buffer;
}


// ============================================================
// FAMILY COLORS
//
// PaletteHex / PaletteBackground / PaletteText / PaletteSurface /
// PaletteAccent used to live here: five switches over PaletteKind
// returning a single representative color each. Nothing called
// them - the family tables replaced them long ago - so they went
// with the enum.
//
// PaletteFamilyBaseColor's seven tables moved to DarkModCore as
// the built-in seeds, and PaletteFamilyColor is now a read of the
// selected preset: the colors in darkmod_presets.ini are already
// final, so there is no base-plus-override arithmetic left to do.
// ============================================================

static CustomPaletteColors CurrentCustomColors()
{
    CustomPaletteColors colors{};

    //
    // Explicit fallback: an unreadable value in the custom palette
    // file used to come back as the surface color, and a black
    // background appearing out of nowhere would look like a bug
    // rather than like a corrupt file.
    //
    colors.background =
        ParseHexColor(
            WToUtf8(
                g_custom.background),
            UiSurface1());

    colors.surface =
        ParseHexColor(
            WToUtf8(
                g_custom.surface),
            UiSurface1());

    colors.text =
        ParseHexColor(
            WToUtf8(
                g_custom.text),
            UiSurface1());

    colors.accent =
        ParseHexColor(
            WToUtf8(
                g_custom.accent),
            UiSurface1());

    return colors;
}


static Color PresetFamilyColor(
    const Preset& preset,
    ColorFamily f)
{
    const int fi =
        static_cast<int>(f);

    if (
        fi < 0 ||
        fi >= FAMILY_COUNT)
    {
        return UiBase();
    }

    return preset.family[fi];
}


static Color PresetFamilyColor(
    ColorFamily f)
{
    return
        PresetFamilyColor(
            CurrentPreset(),
            f);
}


// ------------------------------------------------------------
// "Is this our own output?" test for a freshly parsed analysis.
//
// Declared up next to UpdateLiveColors, defined here because it
// needs the selected preset's twelve destinations. The weighting
// by coverage rather than by entry count is in the core function:
// a poisoned capture is dominated by whatever we painted the
// background with, while a handful of incidental matches in a
// clean capture carry almost no area.
// ------------------------------------------------------------

static bool CurrentPaletteIsOwnOutput(
    const std::vector<LiveColor>& colors)
{
    return
        LooksLikeOwnOutput(
            colors,
            CurrentPreset().family);
}


// ------------------------------------------------------------
// "ИЗМЕНЕНО" BADGES AND THE RESET BUTTONS
//
// There is no override matrix any more, so "changed" means "this
// color differs from the built-in seed". A generated preset has
// no seed, so nothing reports as changed and the reset buttons
// stay disabled - which is the truth: there is nothing to go
// back to.
// ------------------------------------------------------------

static int BuiltinIndexOf(
    const std::string& id)
{
    for (
        int i = 0;
        i < BUILTIN_PRESET_COUNT;
        ++i)
    {
        if (id == BuiltinPresetId(i))
            return i;
    }

    return -1;
}


static bool PresetSeedColor(
    const Preset& preset,
    ColorFamily f,
    Color& out)
{
    if (!preset.builtin)
        return false;

    const int bi =
        BuiltinIndexOf(
            preset.id);

    if (bi < 0)
        return false;

    out =
        BuiltinFamilyColor(
            bi,
            f,
            CurrentCustomColors());

    return true;
}


static bool HasFamilyOverride(
    const Preset& preset,
    ColorFamily f)
{
    Color seed{};

    if (
        !PresetSeedColor(
            preset,
            f,
            seed))
    {
        return false;
    }

    return
        !ColorEquals(
            seed,
            PresetFamilyColor(
                preset,
                f));
}


static bool HasPaletteOverrides(
    const Preset& preset)
{
    for (
        int i = 0;
        i < FAMILY_COUNT;
        ++i)
    {
        if (
            HasFamilyOverride(
                preset,
                FamilyAt(i)))
        {
            return true;
        }
    }

    return false;
}


// ============================================================
// CUSTOM PALETTE FILE
//
// darkmod_custom_palette.ini already existed on disk but nothing
// read it, so g_custom reset to the built-in values on every
// launch and the Custom preset silently ignored what was stored.
// Format is one "key=#RRGGBB" per line.
// ============================================================

static bool LoadCustomPalette()
{
    std::ifstream f(
        FilePath(
            L"darkmod_custom_palette.ini"));

    if (!f)
        return false;

    std::string line;

    while (
        std::getline(
            f,
            line))
    {
        const size_t eq =
            line.find('=');

        if (eq == std::string::npos)
            continue;

        const std::string key =
            line.substr(
                0,
                eq);

        std::string value =
            line.substr(
                eq + 1);

        //
        // Tolerate CRLF files.
        //

        while (
            !value.empty() &&
            (value.back() == '\r' ||
                value.back() == '\n'))
        {
            value.pop_back();
        }

        if (
            value.size() != 7 ||
            value[0] != '#')
        {
            continue;
        }

        //
        // Hex literals are pure ASCII, so a straight widen is
        // enough - no need for a full UTF-8 conversion here.
        //

        const std::wstring wide(
            value.begin(),
            value.end());

        if (key == "background")
            g_custom.background = wide;
        else if (key == "surface")
            g_custom.surface = wide;
        else if (key == "text")
            g_custom.text = wide;
        else if (key == "accent")
            g_custom.accent = wide;
    }

    return true;
}

static void SaveCustomPaletteFile()
{
    std::ostringstream out;

    out
        << "background="
        << WToUtf8(g_custom.background)
        << "\n"
        << "surface="
        << WToUtf8(g_custom.surface)
        << "\n"
        << "text="
        << WToUtf8(g_custom.text)
        << "\n"
        << "accent="
        << WToUtf8(g_custom.accent)
        << "\n";

    WriteTextFile(
        FilePath(
            L"darkmod_custom_palette.ini"),
        out.str());
}


// ============================================================
// PRESET FILE
//
// darkmod_presets.ini has exactly one writer, and it is here. The
// twelve colors it stores are already final, so there is no
// base-plus-override arithmetic left to get wrong and no ordinal
// left to point at the wrong preset after a deletion.
//
// darkmod_palette_overrides.ini is no longer written at all. Its
// writer set std::hex for the color bytes and never reset it, so
// from the second record on the *indices* went out in hex too -
// family 10 as "A", family 11 as "B" - while the reader parsed
// indices as decimal only and silently dropped exactly those
// records. The next save then omitted them, so hand-tuned Mauve
// and Pink colors disappeared on their own between two launches.
// A std::dec would repair the next file; removing the writer
// removes the whole class of bug. The file is still read once,
// tolerantly, to migrate what is already in it.
// ============================================================

//
// Set when the file on disk is corrupt or was written by a newer
// build. Presets are then seeded in memory so the window still
// paints, but nothing is written over data we cannot read.
//
static bool g_presetsReadOnly = false;


static void SavePresets()
{
    if (g_presetsReadOnly)
        return;

    g_presets.Save(
        FilePath(FILE_PRESETS));
}


// ------------------------------------------------------------
// MIGRATION
//
// Built-ins first, then the legacy overrides on top, so the
// migrated file holds exactly the colors the user was already
// seeing - including the two records the old reader lost.
// ------------------------------------------------------------

static void SeedPresetsFromLegacy()
{
    LegacyOverrides legacy{};

    const bool haveLegacy =
        LoadLegacyOverrides(
            FilePath(FILE_OVERRIDES),
            legacy);

    const bool usable =
        haveLegacy &&
        !legacy.versionMismatch;

    g_presets.SeedBuiltins(
        CurrentCustomColors(),
        usable
        ? &legacy
        : nullptr);

    if (
        haveLegacy &&
        legacy.versionMismatch)
    {
        //
        // The old reader discarded such a file without a word,
        // which is how "my themes reset themselves" went unnoticed
        // for so long. Say it once, during migration.
        //
        MessageBoxW(
            nullptr,
            L"darkmod_palette_overrides.ini записан другой версией "
            L"DarkMod, поэтому ручные настройки цветов перенести "
            L"не удалось.\n\n"
            L"Стандартные пресеты созданы заново. Старый файл "
            L"оставлен на диске.",
            WINDOW_TITLE,
            MB_ICONWARNING);
    }
}


static void LoadPresets()
{
    const PresetLoad status =
        g_presets.Load(
            FilePath(FILE_PRESETS));

    switch (status)
    {
    case PresetLoad::Ok:

        //
        // Nothing to do. A list without any built-in in it is
        // legitimate - deleting all eight and keeping only
        // generated presets is a supported thing to want.
        //
        break;

    case PresetLoad::Missing:

        SeedPresetsFromLegacy();

        SavePresets();

        break;

    case PresetLoad::Corrupt:
    case PresetLoad::FutureVersion:

        g_presetsReadOnly = true;

        g_presets.SeedBuiltins(
            CurrentCustomColors(),
            nullptr);

        MessageBoxW(
            nullptr,
            status == PresetLoad::FutureVersion
            ? L"darkmod_presets.ini создан более новой версией "
            L"DarkMod и не читается этой.\n\n"
            L"Файл оставлен без изменений, поэтому правки палитр "
            L"в этом запуске не сохранятся. Обновите DarkMod или "
            L"переименуйте файл, чтобы начать заново."
            : L"darkmod_presets.ini не читается - файл повреждён."
            L"\n\n"
            L"Он оставлен без изменений, поэтому правки палитр в "
            L"этом запуске не сохранятся. Переименуйте или удалите "
            L"файл, чтобы создать пресеты заново.",
            WINDOW_TITLE,
            MB_ICONWARNING);

        break;
    }
}


static void SelectPreset(
    const std::string& id)
{
    if (g_presets.SelectedId() == id)
        return;

    g_presets.SetSelectedId(id);

    //
    // The selection lived only in memory before, which is the
    // second half of "themes keep resetting": every launch started
    // on Dark again no matter what was chosen.
    //
    SavePresets();
}


static bool AnyBuiltinPresetMissing()
{
    for (
        int i = 0;
        i < BUILTIN_PRESET_COUNT;
        ++i)
    {
        if (
            !g_presets.Find(
                BuiltinPresetId(i)))
        {
            return true;
        }
    }

    return false;
}


static void RestoreBuiltinPresets()
{
    g_presets.RestoreBuiltins(
        CurrentCustomColors());

    SavePresets();
}


// ------------------------------------------------------------
// FAMILY EDITING
//
// Same names and same call shape as the old override mutators,
// but they edit the selected preset instead of a [palette][family]
// matrix. "Reset" means "back to the built-in seed", so it does
// nothing for a generated preset - there is nothing to go back to,
// and the button that calls it is disabled for exactly that
// reason.
// ------------------------------------------------------------

static void SetFamilyOverride(
    ColorFamily f,
    Color color)
{
    const int fi =
        static_cast<int>(f);

    if (
        fi < 0 ||
        fi >= FAMILY_COUNT)
    {
        return;
    }

    Preset* preset =
        g_presets.Find(
            g_presets.Selected().id);

    if (!preset)
        return;

    preset->family[fi] =
        color;

    SavePresets();
}


static void ResetFamilyOverride(
    ColorFamily f)
{
    const int fi =
        static_cast<int>(f);

    if (
        fi < 0 ||
        fi >= FAMILY_COUNT)
    {
        return;
    }

    Preset* preset =
        g_presets.Find(
            g_presets.Selected().id);

    if (!preset)
        return;

    Color seed{};

    if (
        !PresetSeedColor(
            *preset,
            f,
            seed))
    {
        return;
    }

    preset->family[fi] =
        seed;

    SavePresets();
}


static void ResetPaletteOverrides()
{
    Preset* preset =
        g_presets.Find(
            g_presets.Selected().id);

    if (!preset)
        return;

    bool changed = false;

    for (
        int i = 0;
        i < FAMILY_COUNT;
        ++i)
    {
        Color seed{};

        if (
            !PresetSeedColor(
                *preset,
                FamilyAt(i),
                seed))
        {
            continue;
        }

        preset->family[i] =
            seed;

        changed = true;
    }

    if (changed)
        SavePresets();
}



// ============================================================
// WRITE CURRENT PALETTE
// ============================================================

// ============================================================
// DESTINATION AMBIGUITY WARNING
//
// The DLL matches a source color with a fuzzy radius of 18, so a
// destination that lands within that radius of some source is
// indistinguishable from it once it is on screen. Before the DLL
// learned to refuse its own output, that produced closed cycles -
// the log shows #313244 -> #FFFFFF alongside #FFFFFF -> #313244,
// i.e. the background we had just darkened being repainted white
// by the rule meant for the original background.
//
// What matters is WHICH source it lands near:
//
//   - near a source of the SAME family: that source maps to this
//     very destination, so re-matching is a no-op. The log's
//     #24273A -> #24273A and #313244 -> #313244 identity lines are
//     this case, and it is why Macchiato works fine despite its
//     White destination sitting only 49 away from a source.
//
//   - near a source of a DIFFERENT family: re-matching sends the
//     color somewhere else entirely. This is the destructive case,
//     e.g. Dark's Black -> #FFFFFF sitting 27 away from the
//     #FCFCFC background, which belongs to White and maps to
//     #313244 - so bright white text collapsed onto the background
//     color.
//
// The DLL now blocks both, so this is a warning rather than a
// correction. It deliberately does NOT rewrite the color: moving
// #FFFFFF out of every source's neighbourhood would land it on
// #414141, and Latte's #EFF1F5 background on #3F4145, destroying
// the choice the user actually made.
// ============================================================

//
// COLOR_MATCH_RADIUS, COLOR_MATCH_RADIUS_SQ, ColorDistanceSq and
// FormatHex used to be declared here with a "must mirror the DLL"
// comment on top. A comment is not a guarantee; they live in
// DarkModCore now, so the launcher, the DLL and Palette Studio
// cannot disagree about the radius by construction.
//

// Families whose replacement color is ambiguous against a source
// belonging to a different family. Surfaced in the family list.
static bool g_familyCollision[12]{};

//
// The exact set of source colors the rule file is built from.
// Shared by the writer and the collision check so the warning can
// never describe a different rule set than the one on disk.
//
static std::vector<Color> BuildSourceList()
{
    if (!g_originalLiveColors.empty())
    {
        std::vector<Color> sources;

        sources.reserve(
            g_originalLiveColors.size());

        for (const auto& lc : g_originalLiveColors)
        {
            sources.push_back(
                ParseHexColor(
                    lc.hex));
        }

        return sources;
    }

    //
    // Safe bootstrap mappings before the
    // first analysis result exists.
    //

    return
    {
        { 255,255,255 },
        { 0,0,0 },
        { 204,204,204 },
        { 153,153,153 },
        { 102,102,102 },
        { 34,34,34 },
        { 68,68,68 }
    };
}

//
// Recompute the per-family ambiguity flags for the current palette.
// Cheap enough (12 families x source count of int math) to run from
// the paint handler, which keeps the badges correct after a palette
// switch or an override change without extra bookkeeping.
//
static void RecomputeFamilyCollisions()
{
    Color familyDst[12]{};

    for (int i = 0; i < 12; ++i)
    {
        familyDst[i] =
            PresetFamilyColor(
                FamilyAt(i));

        g_familyCollision[i] = false;
    }

    for (const Color& src : BuildSourceList())
    {
        const int srcFamily =
            std::clamp(
                static_cast<int>(
                    ClassifyFamily(src)),
                0,
                11);

        for (int i = 0; i < 12; ++i)
        {
            //
            // Same-family proximity is an identity no-op, only a
            // cross-family neighbour can send the color elsewhere.
            //

            if (i == srcFamily)
                continue;

            if (
                ColorDistanceSq(
                    familyDst[i],
                    src) <=
                COLOR_MATCH_RADIUS_SQ)
            {
                g_familyCollision[i] = true;
            }
        }
    }
}

static void WriteCurrentPalette()
{
    //
    // Use every analyzed source color.
    // Each source is assigned to one semantic
    // family and receives the current family
    // replacement color.
    //

    const std::vector<Color> sources =
        BuildSourceList();

    Color familyDst[12]{};

    for (int i = 0; i < 12; ++i)
    {
        familyDst[i] =
            PresetFamilyColor(
                FamilyAt(i));
    }

    RecomputeFamilyCollisions();

    std::ostringstream ini;

    for (const Color& src : sources)
    {
        const int fi =
            std::clamp(
                static_cast<int>(
                    ClassifyFamily(src)),
                0,
                11);

        ini
            << "1|"
            << FormatHex(src)
            << "|"
            << FormatHex(familyDst[fi])
            << "\n";
    }

    WriteTextFile(
        FilePath(INI_NAME),
        ini.str());
}


static void WriteCurrentFonts()
{
    std::string font =
        WToUtf8(
            g_fontSelection);

    std::ostringstream ini;

    ini
        << "1|Arial|"
        << font
        << "\n"

        << "1|Tahoma|"
        << font
        << "\n"

        << "1|Times New Roman|"
        << font
        << "\n"

        << "1|Courier New|"
        << font
        << "\n";

    WriteTextFile(
        FilePath(
            FONTS_INI_NAME),
        ini.str());
}


// ============================================================
// UI TEXT
// ============================================================

static std::wstring StatusText()
{
    if (!g_currentPid)
        return L"1С НЕ НАЙДЕНА";

    if (g_darkModEnabled)
        return L"1С MODDED";

    return L"1С АНАЛИЗ";
}


// ============================================================
// GDI HELPERS
//
// ToColorRef, LerpColor, DarkenColor, LightenColor,
// FillRectColor, DrawTextColor, RoundedRect and DrawCircle are in
// DarkModCore now, so Palette Studio draws the same panels from
// the same code instead of a second copy that drifts.
// ============================================================



// ============================================================
// MOUSE HELPERS
// ============================================================

static POINT ClientMousePos()
{
    POINT p{};

    GetCursorPos(&p);

    if (g_hwnd)
        ScreenToClient(
            g_hwnd,
            &p);

    return p;
}


static bool IsPressedTarget(
    UiPressTarget target)
{
    return
        g_pressedUi ==
        target;
}


static Color InteractiveFill(
    Color normal,
    bool hovered,
    bool pressed)
{
    Color result =
        normal;

    if (hovered)
    {
        result =
            LightenColor(
                result,
                g_uiDark
                ? 0.08f
                : 0.06f);
    }

    if (pressed)
    {
        result =
            DarkenColor(
                result,
                g_uiDark
                ? 0.12f
                : 0.08f);
    }

    return result;
}


// ============================================================
// THEME DROPDOWN DATA
//
// The menu was a fixed array of ten rows over the eight enum
// values. Presets are data now and there can be any number of
// them, so the rows are built from the store each time it is
// asked and the panel scrolls once it no longer fits.
//
// Grouping is presentation only: the two built-in groups keep the
// headers and the order they have always had, generated presets
// get their own collapsible group, and anything else - Custom, or
// a built-in id this build does not know - lands in a flat row
// between them. A group with no members contributes nothing, so
// deleting all of Catppuccin removes its header too.
// ============================================================

enum class ThemeGroup
{
    Dark,
    Catppuccin,
    Custom,
    Generated
};

struct ThemeMenuEntry
{
    bool header = false;

    //
    // A row that does something instead of selecting something.
    // There is exactly one: "вернуть стандартные", and it only
    // appears when a built-in is actually missing.
    //
    bool action = false;

    ThemeGroup group =
        ThemeGroup::Dark;

    //
    // Owned, not a literal: a generated preset's name comes from
    // the file and the indent is applied when the row is built.
    //
    std::wstring text;

    std::string presetId;
};


//
// Display order inside the two built-in groups. The store keeps
// the eight seeds in the order they were written, which puts Mocha
// before Latte, but the menu has always listed Catppuccin light to
// dark. Ids not named here follow in store order.
//
static const char* const THEME_ORDER_DARK[] =
{
    "dark",
    "deepdark",
    "midnight"
};

static const char* const THEME_ORDER_CATPPUCCIN[] =
{
    "latte",
    "frappe",
    "macchiato",
    "mocha"
};


static bool IdInList(
    const std::string& id,
    const char* const* list,
    size_t count)
{
    for (
        size_t i = 0;
        i < count;
        ++i)
    {
        if (id == list[i])
            return true;
    }

    return false;
}


static ThemeGroup GroupOfPreset(
    const Preset& preset)
{
    if (!preset.builtin)
        return ThemeGroup::Generated;

    if (
        IdInList(
            preset.id,
            THEME_ORDER_DARK,
            _countof(THEME_ORDER_DARK)))
    {
        return ThemeGroup::Dark;
    }

    if (
        IdInList(
            preset.id,
            THEME_ORDER_CATPPUCCIN,
            _countof(THEME_ORDER_CATPPUCCIN)))
    {
        return ThemeGroup::Catppuccin;
    }

    return ThemeGroup::Custom;
}


//
// Ids of one group, in display order.
//
static std::vector<std::string> ThemeGroupMembers(
    ThemeGroup group,
    const char* const* order,
    size_t orderCount)
{
    std::vector<std::string> ids;

    for (
        size_t i = 0;
        i < orderCount;
        ++i)
    {
        const Preset* preset =
            g_presets.Find(
                order[i]);

        if (
            preset &&
            GroupOfPreset(*preset) == group)
        {
            ids.push_back(
                preset->id);
        }
    }

    for (const Preset& preset : g_presets.All())
    {
        if (GroupOfPreset(preset) != group)
            continue;

        if (
            IdInList(
                preset.id,
                order,
                orderCount))
        {
            continue;
        }

        ids.push_back(
            preset.id);
    }

    return ids;
}


//
// "Catppuccin Mocha" under a CATPPUCCIN header reads as a stutter,
// and the menu used to show just "Mocha" there. The stored name is
// left alone - it is what the selector and the export file show -
// so only the row text loses the prefix, and only when something is
// left after it.
//
static std::wstring ThemeRowText(
    ThemeGroup group,
    const std::wstring& name)
{
    if (group == ThemeGroup::Catppuccin)
    {
        static const std::wstring prefix =
            L"Catppuccin ";

        if (
            name.size() > prefix.size() &&
            name.compare(
                0,
                prefix.size(),
                prefix) == 0)
        {
            return
                name.substr(
                    prefix.size());
        }
    }

    return name;
}


static void AppendThemeGroup(
    std::vector<ThemeMenuEntry>& entries,
    ThemeGroup group,
    const wchar_t* label,
    bool open,
    const char* const* order,
    size_t orderCount)
{
    const std::vector<std::string> ids =
        ThemeGroupMembers(
            group,
            order,
            orderCount);

    if (ids.empty())
        return;

    ThemeMenuEntry header{};

    header.header = true;
    header.group = group;

    header.text =
        (open
            ? L"− "
            : L"+ ") +
        std::wstring(label);

    entries.push_back(
        std::move(header));

    if (!open)
        return;

    for (const std::string& id : ids)
    {
        const Preset* preset =
            g_presets.Find(id);

        if (!preset)
            continue;

        ThemeMenuEntry row{};

        row.group = group;
        row.presetId = id;

        row.text =
            L"    " +
            ThemeRowText(
                group,
                preset->name);

        entries.push_back(
            std::move(row));
    }
}


//
// Rebuilt on every call, as before. Cheap - a handful of string
// copies - and it means no cache can disagree with the store
// after a preset is added or removed.
//
static const std::vector<ThemeMenuEntry>&
GetThemeMenuEntries()
{
    static std::vector<ThemeMenuEntry> entries;

    entries.clear();

    AppendThemeGroup(
        entries,
        ThemeGroup::Dark,
        L"DARK",
        g_darkGroupOpen,
        THEME_ORDER_DARK,
        _countof(THEME_ORDER_DARK));

    AppendThemeGroup(
        entries,
        ThemeGroup::Catppuccin,
        L"CATPPUCCIN",
        g_catppuccinGroupOpen,
        THEME_ORDER_CATPPUCCIN,
        _countof(THEME_ORDER_CATPPUCCIN));

    //
    // Flat rows, no header: Custom has never had one.
    //
    for (const Preset& preset : g_presets.All())
    {
        if (
            GroupOfPreset(preset) !=
            ThemeGroup::Custom)
        {
            continue;
        }

        ThemeMenuEntry row{};

        row.group = ThemeGroup::Custom;
        row.presetId = preset.id;
        row.text = preset.name;

        entries.push_back(
            std::move(row));
    }

    AppendThemeGroup(
        entries,
        ThemeGroup::Generated,
        L"СОЗДАННЫЕ",
        g_generatedGroupOpen,
        nullptr,
        0);

    //
    // Only when something is gone. Palette Studio is allowed to
    // delete the standard presets, and without this row that is a
    // one-way door; with it, the menu of a normal installation
    // looks exactly as it always has.
    //
    if (AnyBuiltinPresetMissing())
    {
        ThemeMenuEntry row{};

        row.action = true;
        row.text = L"↺ Вернуть стандартные";

        entries.push_back(
            std::move(row));
    }

    return entries;
}


static int ThemeMenuPanelTop()
{
    return THEME_Y +
        SELECTOR_H +
        6;
}


static int ThemeMenuRowHeight(
    const ThemeMenuEntry& entry)
{
    return
        entry.header
        ? 25
        : 31;
}


static int ThemeMenuTotalHeight()
{
    int height = 0;

    for (
        const ThemeMenuEntry& entry :
        GetThemeMenuEntries())
    {
        height +=
            ThemeMenuRowHeight(entry);
    }

    return height;
}


// ------------------------------------------------------------
// SCROLLING
//
// Palette Studio can install hundreds of presets, and a panel
// taller than the window is a panel with unreachable rows. The
// visible height is capped and the rest scrolls with the wheel.
// ------------------------------------------------------------

static constexpr int THEME_MENU_MAX_H = 279;

static int ThemeMenuVisibleHeight()
{
    return
        std::min(
            ThemeMenuTotalHeight(),
            THEME_MENU_MAX_H);
}


static int ThemeMenuMaxScroll()
{
    return
        std::max(
            0,
            ThemeMenuTotalHeight() -
            ThemeMenuVisibleHeight());
}


static void ClampThemeScroll()
{
    g_themeScroll =
        std::clamp(
            g_themeScroll,
            0,
            ThemeMenuMaxScroll());
}


static RECT ThemeMenuPanelRect()
{
    const int top =
        ThemeMenuPanelTop();

    return
    {
        THEME_X,
        top,
        THEME_RIGHT,
        top +
        ThemeMenuVisibleHeight()
    };
}


static int ThemeMenuRowY(
    int index)
{
    int y =
        ThemeMenuPanelTop() -
        g_themeScroll;

    const std::vector<ThemeMenuEntry>& entries =
        GetThemeMenuEntries();

    const int count =
        static_cast<int>(
            entries.size());

    for (
        int i = 0;
        i < index &&
        i < count;
        ++i)
    {
        y +=
            ThemeMenuRowHeight(
                entries[i]);
    }

    return y;
}


//
// A scrolled row can be half outside the panel, so a row only
// counts as hit when its middle is inside - clicking the sliver
// of a row cut by the panel edge would be a misclick either way.
//
static bool ThemeMenuRowVisible(
    int rowY,
    int rowH)
{
    const RECT panel =
        ThemeMenuPanelRect();

    const int middle =
        rowY + rowH / 2;

    return
        middle >= panel.top &&
        middle < panel.bottom;
}


//
// ThemeItemAt() used to turn a y coordinate into a row index for
// the click handler. The handler walks the rows itself now, since
// it needs the entry anyway to tell a header from a preset, so a
// second traversal with its own copy of the visibility rule was
// one place too many for that rule to live.
//


static bool ThemeMenuContains(
    int x,
    int y)
{
    return
        RectContains(
            ThemeMenuPanelRect(),
            x,
            y);
}



// ============================================================
// FONT DROPDOWN
// ============================================================

static const wchar_t* g_fonts[] =
{
    L"Segoe UI",
    L"Inter",
    L"Arial",
    L"Tahoma",
    L"Verdana",
    L"Consolas",
    L"Cascadia Mono",
    L"JetBrains Mono"
};

static constexpr int FONT_COUNT =
static_cast<int>(
    _countof(g_fonts));

static int FontMenuTop()
{
    return
        FONT_Y +
        SELECTOR_H +
        6;
}


static int FontItemAt(
    int y)
{
    const int top =
        FontMenuTop();

    if (
        y < top ||
        y >= top + FONT_COUNT * 31)
    {
        return -1;
    }

    return
        (y - top) / 31;
}


static bool FontMenuContains(
    int x,
    int y)
{
    RECT panel{
        FONT_X,
        FontMenuTop(),
        FONT_RIGHT,
        FontMenuTop() +
        FONT_COUNT * 31
    };

    return
        RectContains(
            panel,
            x,
            y);
}


// ============================================================
// PAINT
// ============================================================

static void DrawHeader(
    HDC hdc)
{
    RECT title{
        LEFT,
        18,
        360,
        54
    };

    DrawTextColor(
        hdc,
        L"DarkMode alphatest 0.1",
        title,
        UiText(),
        DT_LEFT |
        DT_VCENTER |
        DT_SINGLELINE);

    RECT statusText{
        488,
        19,
        555,
        53
    };

    DrawTextColor(
        hdc,
        L"1C",
        statusText,
        UiText(),
        DT_RIGHT |
        DT_VCENTER |
        DT_SINGLELINE);

    Color statusColor =
        !g_currentPid
        ? Color{ 243,139,168 }
        : (
            g_darkModEnabled
            ? Color{ 166,227,161 }
            : Color{ 249,226,175 }
            );

    RECT dot{
        563,
        27,
        575,
        39
    };

    DrawCircle(
        hdc,
        dot,
        statusColor);
}


static RECT SliderVisualRect()
{
    const int center =
        WINDOW_W / 2;

    const int left =
        center -
        SLIDER_W / 2;

    return
    {
        left,
        BUTTON_Y,
        left + SLIDER_W,
        BUTTON_Y + BUTTON_H
    };
}


static RECT SliderHitRect()
{
    RECT rc =
        SliderVisualRect();

    rc.left -= 14;
    rc.right += 14;
    rc.top -= 10;
    rc.bottom += 10;

    return rc;
}


static int SliderXFromValue(
    float value)
{
    RECT track =
        SliderVisualRect();

    const int left =
        track.left + 7;

    const int right =
        track.right - 7;

    return
        static_cast<int>(
            left +
            (right - left) *
            std::clamp(
                value,
                0.0f,
                1.0f));
}


static void DrawButtons(
    HDC hdc)
{
    //
    // Main MOD switch.
    //

    RECT track =
        SliderVisualRect();

    POINT mp =
        ClientMousePos();

    const bool hovered =
        RectContains(
            SliderHitRect(),
            mp.x,
            mp.y);

    const bool pressed =
        IsPressedTarget(
            UiPressTarget::Slider);

    Color trackBase =
        g_darkModEnabled
        ? UiGreen()
        : UiSurface0();

    trackBase =
        InteractiveFill(
            trackBase,
            hovered,
            pressed);

    RoundedRect(
        hdc,
        track,
        18,
        trackBase);

    //
    // Centered "MOD" label.
    //

    RECT label{
        track.left,
        track.top,
        track.right,
        track.bottom
    };

    DrawTextColor(
        hdc,
        L"MOD",
        label,
        g_darkModEnabled
        ? UiCrust()
        : UiSub1(),
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE);

    //
    // Thumb.
    //

    const int thumbCenter =
        SliderXFromValue(
            g_dragging
            ? g_sliderPos
            : (g_darkModEnabled
                ? 1.0f
                : 0.0f));

    const int thumbW = 32;

    RECT knob{
        thumbCenter - thumbW / 2,
        track.top + 4,
        thumbCenter + thumbW / 2,
        track.bottom - 4
    };

    Color knobColor =
        g_darkModEnabled
        ? UiCrust()
        : UiText();

    if (hovered)
    {
        knobColor =
            LightenColor(
                knobColor,
                g_uiDark
                ? 0.10f
                : 0.08f);
    }

    if (pressed)
    {
        knobColor =
            DarkenColor(
                knobColor,
                0.08f);
    }

    RoundedRect(
        hdc,
        knob,
        15,
        knobColor);
}


static void DrawSectionLabel(
    HDC hdc,
    const wchar_t* text,
    int x,
    int y,
    int width)
{
    RECT rc{
        x,
        y,
        x + width,
        y + 22
    };

    DrawTextColor(
        hdc,
        text,
        rc,
        UiSub0(),
        DT_LEFT |
        DT_VCENTER |
        DT_SINGLELINE);
}


//
// ParseHexColor lived here and fell back to UiSurface1() for
// anything it could not read. It is in DarkModCore now, where the
// fallback is a parameter, so the call sites that care pass the
// same surface color and the DLL gets the same parser.
//


// ============================================================
// SELECTOR
// ============================================================

static void DrawSelector(
    HDC hdc,
    const RECT& rc,
    const std::wstring& label,
    bool hovered,
    bool pressed)
{
    Color selectorFill =
        InteractiveFill(
            UiSurface0(),
            hovered,
            pressed);

    RoundedRect(
        hdc,
        rc,
        9,
        selectorFill);

    RECT textRc{
        rc.left + 16,
        rc.top,
        rc.right - 44,
        rc.bottom
    };

    DrawTextColor(
        hdc,
        label,
        textRc,
        UiText(),
        DT_LEFT |
        DT_VCENTER |
        DT_SINGLELINE);

    RECT arrow{
        rc.right - 34,
        rc.top,
        rc.right - 10,
        rc.bottom
    };

    DrawTextColor(
        hdc,
        L"▾",
        arrow,
        hovered
        ? UiText()
        : UiSub1(),
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE);
}


// ============================================================
// LIVE FAMILIES
// ============================================================

static int FamilyVisibleIndexAt(
    int y)
{
    if (
        y < LIVE_Y ||
        y >= FAMILY_VIEW_BOTTOM)
    {
        return -1;
    }

    const int row =
        (y - LIVE_Y) / ROW_H;

    const int index =
        g_familyScroll + row;

    if (
        index < 0 ||
        index >= 12)
    {
        return -1;
    }

    return index;
}


static RECT FamilyReplacementRect(
    int visibleRow)
{
    const int y =
        LIVE_Y +
        visibleRow * ROW_H;

    return
    {
        RIGHT - 33,
        y + 5,
        RIGHT - 1,
        y + 25
    };
}


static void DrawFamilyScrollBar(
    HDC hdc)
{
    //
    // Nothing to scroll when every family fits in the viewport.
    //
    // The previous guard read "scroll <= 0 && scroll >= 6", which
    // no single value satisfies, so it never fired. Harmless today
    // - 12 families never fit in 6 rows - but it hid the intent.
    //

    if constexpr (12 <= FAMILY_VIEW_ROWS)
    {
        return;
    }

    const int x =
        RIGHT - 48;

    const int top =
        LIVE_Y + 4;

    const int bottom =
        FAMILY_VIEW_BOTTOM - 4;

    RECT track{
        x,
        top,
        x + 5,
        bottom
    };

    RoundedRect(
        hdc,
        track,
        3,
        UiSurface0());

    const float ratio =
        FAMILY_VIEW_ROWS /
        12.0f;

    const int calculatedThumbH =
        static_cast<int>(
            (bottom - top) *
            ratio);

    const int thumbH =
        calculatedThumbH < 22
        ? 22
        : calculatedThumbH;

    const int range =
        (bottom - top) -
        thumbH;


    const int maxScroll =
        std::max(
            1,
            12 -
            FAMILY_VIEW_ROWS);

    const int thumbY =
        top +
        static_cast<int>(
            range *
            (
                g_familyScroll /
                static_cast<float>(
                    maxScroll)
                ));

    RECT thumb{
        x - 1,
        thumbY,
        x + 6,
        thumbY + thumbH
    };

    RoundedRect(
        hdc,
        thumb,
        4,
        UiSub0());
}


static void DrawLiveColors(
    HDC hdc)
{
    DrawSectionLabel(
        hdc,
        L"ОРИГИНАЛЬНЫЕ ЦВЕТА",
        LEFT,
        LIVE_LABEL_Y,
        260);

    DrawSectionLabel(
        hdc,
        L"ЗАМЕНА",
        RIGHT - 170,
        LIVE_LABEL_Y,
        170);

    auto fam =
        BuildFamilies();

    //
    // Keep the ambiguity badges in step with the current palette and
    // overrides without needing every mutation site to remember.
    //

    RecomputeFamilyCollisions();

    //
    // Clip drawing to the family viewport.
    //

    HRGN clipRegion =
        CreateRectRgn(
            LEFT,
            LIVE_Y,
            RIGHT,
            FAMILY_VIEW_BOTTOM);

    SelectClipRgn(
        hdc,
        clipRegion);

    POINT mp =
        ClientMousePos();

    for (
        int visibleRow = 0;
        visibleRow < FAMILY_VIEW_ROWS;
        ++visibleRow)
    {
        const int index =
            g_familyScroll +
            visibleRow;

        if (
            index < 0 ||
            index >= 12)
        {
            continue;
        }

        const int y =
            LIVE_Y +
            visibleRow *
            ROW_H;

        const auto& f =
            fam[(size_t)index];

        Color original =
            f.present
            ? f.representative
            : UiSurface0();

        Color replacement =
            PresetFamilyColor(
                f.family);

        RECT rowHit{
            LEFT,
            y,
            RIGHT - 42,
            y + ROW_H
        };

        const bool rowHovered =
            RectContains(
                rowHit,
                mp.x,
                mp.y);

        const bool replacementHovered =
            RectContains(
                FamilyReplacementRect(
                    visibleRow),
                mp.x,
                mp.y);

        //
        // Original color circle.
        //

        RECT sw{
            LEFT + 2,
            y + 6,
            LEFT + 20,
            y + 24
        };

        Color originalCircle =
            original;

        if (rowHovered)
        {
            originalCircle =
                LightenColor(
                    originalCircle,
                    0.10f);
        }

        DrawCircle(
            hdc,
            sw,
            originalCircle);

        //
        // Family name.
        //

        RECT name{
            LEFT + 30,
            y,
            LEFT + 122,
            y + ROW_H
        };

        DrawTextColor(
            hdc,
            FamilyName(
                f.family),
            name,
            f.present
            ? (
                rowHovered
                ? UiText()
                : UiText()
                )
            : UiSub0(),
            DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE);

        //
        // Percentage bar.
        //

        const int barLeft =
            LEFT + 132;

        const int barRight =
            RIGHT - 135;

        RECT bar{
            barLeft,
            y + 10,
            barRight,
            y + 20
        };

        RoundedRect(
            hdc,
            bar,
            5,
            UiSurface0());

        const int fillWidth =
            static_cast<int>(
                (bar.right - bar.left) *
                std::clamp(
                    f.percentage / 100.0,
                    0.0,
                    1.0));

        if (fillWidth > 0)
        {
            RECT fill{
                bar.left,
                bar.top,
                bar.left + fillWidth,
                bar.bottom
            };

            RoundedRect(
                hdc,
                fill,
                5,
                original);
        }

        //
        // Percentage.
        //

        RECT pct{
            barRight + 8,
            y,
            RIGHT - 86,
            y + ROW_H
        };

        wchar_t ps[32]{};

        swprintf_s(
            ps,
            _countof(ps),
            L"%.1f%%",
            f.percentage);

        DrawTextColor(
            hdc,
            ps,
            pct,
            rowHovered
            ? UiText()
            : UiSub1(),
            DT_RIGHT |
            DT_VCENTER |
            DT_SINGLELINE);

        //
        // Replacement circle.
        //

        RECT rr =
            FamilyReplacementRect(
                visibleRow);

        Color replacementColor =
            replacement;

        if (replacementHovered)
        {
            replacementColor =
                LightenColor(
                    replacementColor,
                    0.10f);
        }

        if (IsPressedTarget(
            UiPressTarget::
            FamilyReplacement))
        {
            if (
                replacementHovered)
            {
                replacementColor =
                    DarkenColor(
                        replacementColor,
                        0.08f);
            }
        }

        DrawCircle(
            hdc,
            rr,
            replacementColor);

        //
        // Ambiguity marker.
        //
        // This replacement color sits within the DLL's match radius
        // of an original color that belongs to a DIFFERENT family,
        // so on screen the two are indistinguishable. The DLL no
        // longer cascades because of it, but the choice is still
        // ambiguous and worth showing - it is what made Dark's
        // "black text -> bright white" collapse onto the background.
        //

        RECT marker{
            RIGHT - 57,
            y,
            RIGHT - 37,
            y + ROW_H
        };

        if (g_familyCollision[index])
        {
            DrawTextColor(
                hdc,
                L"!",
                marker,
                UiYellow(),
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE);
        }
        else if (replacementHovered)
        {
            //
            // Subtle arrow only when hovering.
            //

            DrawTextColor(
                hdc,
                L"→",
                marker,
                UiSub0(),
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE);
        }
    }

    SelectClipRgn(
        hdc,
        nullptr);

    DeleteObject(
        clipRegion);

    DrawFamilyScrollBar(
        hdc);
}


// ============================================================
// BOTTOM ACTIONS
// ============================================================

static RECT DetailsRect()
{
    return
    {
        LEFT,
        DETAIL_Y,
        LEFT + 230,
        DETAIL_Y + DETAIL_H
    };
}


static RECT RefreshInputRect()
{
    return
    {
        LEFT + 250,
        DETAIL_Y,
        RIGHT,
        DETAIL_Y + DETAIL_H
    };
}


static void DrawBottomActions(
    HDC hdc)
{
    POINT mp =
        ClientMousePos();

    const RECT details =
        DetailsRect();

    const RECT refresh =
        RefreshInputRect();

    const bool detailsHovered =
        RectContains(
            details,
            mp.x,
            mp.y);

    const bool refreshHovered =
        RectContains(
            refresh,
            mp.x,
            mp.y);

    DrawTextColor(
        hdc,
        g_originalLiveColors.empty()
        ? L"Ожидание анализа…"
        : L"Посмотреть все",
        details,
        detailsHovered
        ? UiBlue()
        : UiSub1(),
        DT_LEFT |
        DT_VCENTER |
        DT_SINGLELINE);

    DrawTextColor(
        hdc,
        L"Обновить вводные",
        refresh,
        refreshHovered
        ? UiText()
        : UiSub0(),
        DT_RIGHT |
        DT_VCENTER |
        DT_SINGLELINE);
}


// ============================================================
// FOOTER
// ============================================================

static RECT FooterRect(
    int clientHeight)
{
    return
    {
        LEFT + 350,
        clientHeight - 30,
        RIGHT,
        clientHeight - 5
    };
}


//
// Where the footer actually is on screen, for hit testing.
//
// Both hit tests used to build the rect from FOOTER_Y, which is
// WINDOW_H - 28 - and WINDOW_H is the size handed to
// CreateWindowExW, an OUTER size for a WS_CAPTION window. The
// client area is shorter by the caption and border, so the
// clickable strip sat entirely below the visible client area and
// the Telegram link never responded. The gap grows with DPI
// scaling.
//
// Asking the window for its client height and reusing FooterRect
// keeps the hit test on exactly the pixels DrawFooter painted.
//
static RECT FooterHitRect()
{
    RECT client{};

    if (
        !g_hwnd ||
        !GetClientRect(
            g_hwnd,
            &client))
    {
        return FooterRect(
            WINDOW_H);
    }

    return FooterRect(
        client.bottom);
}


static void DrawFooter(
    HDC hdc,
    int clientHeight)
{
    HGDIOBJ oldFont =
        SelectObject(
            hdc,
            g_smallFont);

    POINT mp =
        ClientMousePos();

    RECT rc =
        FooterRect(
            clientHeight);

    const bool hovered =
        RectContains(
            rc,
            mp.x,
            mp.y);

    DrawTextColor(
        hdc,
        L"Нашли баг? @bixxez",
        rc,
        hovered
        ? UiBlue()
        : UiSub1(),
        DT_RIGHT |
        DT_VCENTER |
        DT_SINGLELINE);

    SelectObject(
        hdc,
        oldFont);
}


// ============================================================
// DROPDOWN
// ============================================================

static void DrawDropdown(
    HDC hdc)
{
    if (
        !g_paletteOpen &&
        !g_fontOpen)
    {
        return;
    }

    POINT mp =
        ClientMousePos();

    if (g_paletteOpen)
    {
        const std::vector<ThemeMenuEntry>& entries =
            GetThemeMenuEntries();

        ClampThemeScroll();

        const RECT panel =
            ThemeMenuPanelRect();

        RoundedRect(
            hdc,
            panel,
            10,
            UiSurface0());

        //
        // Rows are laid out in unclipped coordinates and the panel
        // does the cutting, so a scrolled list shows a partial row
        // at the edge instead of a gap.
        //
        const int savedDc =
            SaveDC(hdc);

        IntersectClipRect(
            hdc,
            panel.left,
            panel.top,
            panel.right,
            panel.bottom);

        const int count =
            static_cast<int>(
                entries.size());

        for (
            int i = 0;
            i < count;
            ++i)
        {
            const ThemeMenuEntry& entry =
                entries[i];

            const int rowY =
                ThemeMenuRowY(i);

            const int rowH =
                ThemeMenuRowHeight(entry);

            if (rowY + rowH <= panel.top)
                continue;

            if (rowY >= panel.bottom)
                break;

            RECT row{
                THEME_X + 7,
                rowY,
                THEME_RIGHT - 7,
                rowY + rowH
            };

            const bool hovered =
                RectContains(
                    row,
                    mp.x,
                    mp.y) &&
                ThemeMenuRowVisible(
                    rowY,
                    rowH);

            //
            // Group header ("+ DARK" / "− CATPPUCCIN").
            // Plain label, no row background - it's a
            // section divider, not a selectable preset.
            //

            if (entry.header)
            {
                DrawTextColor(
                    hdc,
                    entry.text.c_str(),
                    {
                        row.left + 3,
                        row.top,
                        row.right - 3,
                        row.bottom
                    },
                    hovered
                    ? UiText()
                    : UiSub0(),
                    DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE);

                continue;
            }

            //
            // Selectable preset entry.
            //

            const bool selected =
                !entry.action &&
                entry.presetId ==
                CurrentPreset().id;

            Color fill =
                selected
                ? UiSurface1()
                : UiSurface0();

            fill =
                InteractiveFill(
                    fill,
                    hovered,
                    IsPressedTarget(
                        UiPressTarget::
                        ThemeItem));

            RoundedRect(
                hdc,
                row,
                7,
                fill);

            DrawTextColor(
                hdc,
                entry.text.c_str(),
                {
                    row.left + 10,
                    row.top,
                    row.right - 10,
                    row.bottom
                },
                entry.action
                ? UiSub0()
                : selected
                ? UiText()
                : UiSub1(),
                DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS);
        }

        RestoreDC(
            hdc,
            savedDc);

        //
        // Scroll hint: a thin bar on the right edge, only when
        // there is something out of view.
        //
        if (ThemeMenuMaxScroll() > 0)
        {
            const int trackH =
                panel.bottom -
                panel.top -
                12;

            const int thumbH =
                std::max(
                    24,
                    trackH *
                    ThemeMenuVisibleHeight() /
                    std::max(
                        1,
                        ThemeMenuTotalHeight()));

            const int thumbY =
                panel.top +
                6 +
                (trackH - thumbH) *
                g_themeScroll /
                std::max(
                    1,
                    ThemeMenuMaxScroll());

            RoundedRect(
                hdc,
                {
                    panel.right - 6,
                    thumbY,
                    panel.right - 3,
                    thumbY + thumbH
                },
                2,
                UiSurface2());
        }
    }


    if (g_fontOpen)
    {
        const int panelTop =
            FontMenuTop();

        const int panelBottom =
            panelTop +
            FONT_COUNT * 31;

        RECT panel{
            FONT_X,
            panelTop,
            FONT_RIGHT,
            panelBottom
        };

        RoundedRect(
            hdc,
            panel,
            10,
            UiSurface0());

        for (
            int i = 0;
            i < FONT_COUNT;
            ++i)
        {
            RECT row{
                panel.left + 7,
                panel.top + 5 + i * 31,
                panel.right - 7,
                panel.top + 5 + (i + 1) * 31
            };

            const bool hovered =
                RectContains(
                    row,
                    mp.x,
                    mp.y);

            const bool selected =
                _wcsicmp(
                    g_fonts[i],
                    g_fontSelection.c_str()) == 0;

            Color fill =
                selected
                ? UiSurface1()
                : UiSurface0();

            fill =
                InteractiveFill(
                    fill,
                    hovered,
                    IsPressedTarget(
                        UiPressTarget::
                        FontItem));

            RoundedRect(
                hdc,
                row,
                7,
                fill);

            DrawTextColor(
                hdc,
                g_fonts[i],
                {
                    row.left + 10,
                    row.top,
                    row.right - 10,
                    row.bottom
                },
                selected
                ? UiText()
                : UiSub1(),
                DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE);
        }
    }
}


// ============================================================
// MAIN PAINT
// ============================================================

static void Paint(
    HWND hwnd,
    HDC hdc)
{
    RECT client{};

    GetClientRect(
        hwnd,
        &client);

    const int width =
        client.right;

    const int height =
        client.bottom;

    HDC buffer =
        CreateCompatibleDC(
            hdc);

    HBITMAP bitmap =
        CreateCompatibleBitmap(
            hdc,
            width,
            height);

    HGDIOBJ old =
        SelectObject(
            buffer,
            bitmap);

    FillRectColor(
        buffer,
        client,
        UiBase());

    DrawHeader(
        buffer);

    DrawButtons(
        buffer);

    //
    // Theme selector.
    //

    POINT mp =
        ClientMousePos();

    RECT themeRc{
        THEME_X,
        THEME_Y,
        THEME_RIGHT,
        THEME_Y + SELECTOR_H
    };

    const bool themeHovered =
        RectContains(
            themeRc,
            mp.x,
            mp.y);

    DrawSectionLabel(
        buffer,
        L"ТЕМА",
        THEME_X,
        THEME_Y - 27,
        THEME_RIGHT - THEME_X);

    DrawSelector(
        buffer,
        themeRc,
        CurrentPreset().name.c_str(),
        themeHovered,
        IsPressedTarget(
            UiPressTarget::
            ThemeSelector));

    //
    // Font selector.
    //

    RECT fontRc{
        FONT_X,
        FONT_Y,
        FONT_RIGHT,
        FONT_Y + SELECTOR_H
    };

    const bool fontHovered =
        RectContains(
            fontRc,
            mp.x,
            mp.y);

    DrawSectionLabel(
        buffer,
        L"ТЕКСТ",
        FONT_X,
        FONT_Y - 27,
        FONT_RIGHT - FONT_X);

    DrawSelector(
        buffer,
        fontRc,
        g_fontSelection,
        fontHovered,
        IsPressedTarget(
            UiPressTarget::
            FontSelector));

    //
    // Color families.
    //

    DrawLiveColors(
        buffer);

    //
    // Bottom actions.
    //

    DrawBottomActions(
        buffer);

    //
    // Footer.
    //

    DrawFooter(
        buffer,
        height);

    //
    // Dropdowns last, so they are visually above
    // the normal content.
    //

    DrawDropdown(
        buffer);

    BitBlt(
        hdc,
        0,
        0,
        width,
        height,
        buffer,
        0,
        0,
        SRCCOPY);

    SelectObject(
        buffer,
        old);

    DeleteObject(bitmap);
    DeleteDC(buffer);
}


// ============================================================
// BUTTON SETUP
// ============================================================

static void SetupButtons()
{
    RECT slider =
        SliderVisualRect();

    g_buttons[0] =
    {
        slider,
        ButtonKind::Enable
    };

    g_buttons[1] =
    {
        {0,0,0,0},
        ButtonKind::Refresh
    };

    g_buttons[2] =
    {
        {0,0,0,0},
        ButtonKind::Disable
    };
}


// ============================================================
// HIT TEST
// ============================================================

static int ButtonAt(
    int x,
    int y)
{
    if (
        RectContains(
            SliderHitRect(),
            x,
            y))
    {
        return 0;
    }

    return -1;
}


// ============================================================
// FAMILY HIT TEST
// ============================================================

static bool IsFamilyReplacementAt(
    int x,
    int y,
    int& familyIndex)
{
    const int visibleRow =
        (y - LIVE_Y) / ROW_H;

    if (
        visibleRow < 0 ||
        visibleRow >= FAMILY_VIEW_ROWS)
    {
        return false;
    }

    const int index =
        g_familyScroll +
        visibleRow;

    if (
        index < 0 ||
        index >= 12)
    {
        return false;
    }

    RECT rr =
        FamilyReplacementRect(
            visibleRow);

    if (
        !RectContains(
            rr,
            x,
            y))
    {
        return false;
    }

    familyIndex =
        index;

    return true;
}


// ============================================================
// COLOR PICKER
// ============================================================

static bool ParseHexColorRef(
    const std::wstring& text,
    COLORREF& color)
{
    if (
        text.size() != 7 ||
        text[0] != L'#')
    {
        return false;
    }

    auto hex =
        [](wchar_t c) -> int
        {
            if (
                c >= L'0' &&
                c <= L'9')
            {
                return c - L'0';
            }

            if (
                c >= L'A' &&
                c <= L'F')
            {
                return c - L'A' + 10;
            }

            if (
                c >= L'a' &&
                c <= L'f')
            {
                return c - L'a' + 10;
            }

            return -1;
        };

    int r1 = hex(text[1]);
    int r2 = hex(text[2]);

    int g1 = hex(text[3]);
    int g2 = hex(text[4]);

    int b1 = hex(text[5]);
    int b2 = hex(text[6]);

    if (
        r1 < 0 ||
        r2 < 0 ||
        g1 < 0 ||
        g2 < 0 ||
        b1 < 0 ||
        b2 < 0)
    {
        return false;
    }

    const BYTE r =
        static_cast<BYTE>(
            r1 * 16 + r2);

    const BYTE g =
        static_cast<BYTE>(
            g1 * 16 + g2);

    const BYTE b =
        static_cast<BYTE>(
            b1 * 16 + b2);

    color =
        RGB(
            r,
            g,
            b);

    return true;
}


static std::wstring ColorRefHex(
    COLORREF color)
{
    wchar_t buffer[16]{};

    swprintf_s(
        buffer,
        _countof(buffer),
        L"#%02X%02X%02X",
        GetRValue(color),
        GetGValue(color),
        GetBValue(color));

    return buffer;
}


static bool PickColor(
    std::wstring& value)
{
    COLORREF initial =
        RGB(
            UiSurface0().r,
            UiSurface0().g,
            UiSurface0().b);

    ParseHexColorRef(
        value,
        initial);

    static COLORREF customColors[16]{};

    CHOOSECOLORW cc{};

    cc.lStructSize =
        sizeof(cc);

    cc.hwndOwner =
        g_hwnd;

    cc.rgbResult =
        initial;

    cc.lpCustColors =
        customColors;

    cc.Flags =
        CC_FULLOPEN |
        CC_RGBINIT;

    if (
        !ChooseColorW(
            &cc))
    {
        return false;
    }

    value =
        ColorRefHex(
            cc.rgbResult);

    return true;
}


// ============================================================
// OLD CUSTOM WINDOW
//
// Preserved from the original source.
// Main Launcher no longer opens it.
// ============================================================

static void CreateEdit(
    HWND parent,
    HWND& out,
    const wchar_t* value,
    int y)
{
    out =
        CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            value,
            WS_CHILD |
            WS_VISIBLE |
            ES_AUTOHSCROLL,
            170,
            y,
            170,
            30,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

    SendMessageW(
        out,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            g_font),
        TRUE);
}


static void SaveCustomPalette()
{
    wchar_t buffer[128]{};

    if (g_editBg)
    {
        GetWindowTextW(
            g_editBg,
            buffer,
            128);

        g_custom.background =
            buffer;
    }

    if (g_editSurface)
    {
        GetWindowTextW(
            g_editSurface,
            buffer,
            128);

        g_custom.surface =
            buffer;
    }

    if (g_editText)
    {
        GetWindowTextW(
            g_editText,
            buffer,
            128);

        g_custom.text =
            buffer;
    }

    if (g_editAccent)
    {
        GetWindowTextW(
            g_editAccent,
            buffer,
            128);

        g_custom.accent =
            buffer;
    }

    SaveCustomPaletteFile();

    //
    // The four edited colors expand into the twelve families of
    // the Custom preset, so the stored preset and the editor can
    // never drift apart.
    //
    g_presets.SyncCustomPreset(
        CurrentCustomColors());

    SelectPreset("custom");

    SavePresets();

    //
    // Do not auto-apply to 1C.
    //

    DestroyWindow(
        g_customWindow);

    g_customWindow =
        nullptr;

    InvalidateRect(
        g_hwnd,
        nullptr,
        FALSE);
}


static LRESULT CALLBACK
CustomWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};

        HDC hdc =
            BeginPaint(
                hwnd,
                &ps);

        RECT rc{};

        GetClientRect(
            hwnd,
            &rc);

        FillRectColor(
            hdc,
            rc,
            UiBase());

        DrawTextColor(
            hdc,
            L"Custom Palette",
            {
                20,
                16,
                330,
                46
            },
            UiText(),
            DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE);

        struct Row
        {
            const wchar_t* name;
            const std::wstring* value;
            int y;
        };

        Row rows[] =
        {
            {
                L"Background",
                &g_custom.background,
                68
            },

            {
                L"Surface",
                &g_custom.surface,
                116
            },

            {
                L"Text",
                &g_custom.text,
                164
            },

            {
                L"Accent",
                &g_custom.accent,
                212
            }
        };

        for (
            const auto& row :
            rows)
        {
            DrawTextColor(
                hdc,
                row.name,
                {
                    22,
                    row.y,
                    150,
                    row.y + 36
                },
                UiSub1(),
                DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE);

            COLORREF picked =
                RGB(
                    UiSurface1().r,
                    UiSurface1().g,
                    UiSurface1().b);

            ParseHexColorRef(
                *row.value,
                picked);

            HBRUSH brush =
                CreateSolidBrush(
                    picked);

            HPEN pen =
                CreatePen(
                    PS_SOLID,
                    2,
                    ToColorRef(
                        UiSurface2()));

            HGDIOBJ oldBrush =
                SelectObject(
                    hdc,
                    brush);

            HGDIOBJ oldPen =
                SelectObject(
                    hdc,
                    pen);

            Ellipse(
                hdc,
                175,
                row.y + 5,
                203,
                row.y + 33);

            SelectObject(
                hdc,
                oldBrush);

            SelectObject(
                hdc,
                oldPen);

            DeleteObject(brush);
            DeleteObject(pen);

            DrawTextColor(
                hdc,
                *row.value,
                {
                    220,
                    row.y,
                    330,
                    row.y + 36
                },
                UiText(),
                DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE);
        }

        RoundedRect(
            hdc,
            {
                22,
                276,
                160,
                314
            },
            8,
            UiSurface0());

        DrawTextColor(
            hdc,
            L"Сохранить",
            {
                22,
                276,
                160,
                314
            },
            UiGreen(),
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE);

        RoundedRect(
            hdc,
            {
                174,
                276,
                312,
                314
            },
            8,
            UiSurface0());

        DrawTextColor(
            hdc,
            L"Отмена",
            {
                174,
                276,
                312,
                314
            },
            UiText(),
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE);

        EndPaint(
            hwnd,
            &ps);

        return 0;
    }

    case WM_LBUTTONUP:
    {
        const int x =
            GET_X_LPARAM(lParam);

        const int y =
            GET_Y_LPARAM(lParam);

        if (
            x >= 160 &&
            x <= 340 &&
            y >= 55 &&
            y <= 105)
        {
            PickColor(
                g_custom.background);

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        if (
            x >= 160 &&
            x <= 340 &&
            y >= 105 &&
            y <= 153)
        {
            PickColor(
                g_custom.surface);

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        if (
            x >= 160 &&
            x <= 340 &&
            y >= 153 &&
            y <= 201)
        {
            PickColor(
                g_custom.text);

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        if (
            x >= 160 &&
            x <= 340 &&
            y >= 201 &&
            y <= 249)
        {
            PickColor(
                g_custom.accent);

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        if (
            x >= 22 &&
            x <= 160 &&
            y >= 276 &&
            y <= 314)
        {
            g_presets.SyncCustomPreset(
                CurrentCustomColors());

            SelectPreset("custom");

            SavePresets();

            WriteCurrentPalette();

            DestroyWindow(
                hwnd);

            return 0;
        }

        if (
            x >= 174 &&
            x <= 312 &&
            y >= 276 &&
            y <= 314)
        {
            DestroyWindow(
                hwnd);

            return 0;
        }

        return 0;
    }

    case WM_SETCURSOR:
    {
        POINT p{};

        GetCursorPos(
            &p);

        ScreenToClient(
            hwnd,
            &p);

        if (
            (
                p.y >= 55 &&
                p.y <= 249
                ) ||
            (
                p.y >= 276 &&
                p.y <= 314
                ))
        {
            SetCursor(
                LoadCursorW(
                    nullptr,
                    IDC_HAND));

            return TRUE;
        }

        break;
    }

    case WM_DESTROY:
    {
        g_customWindow =
            nullptr;

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam);
}


static void OpenCustomPaletteWindow()
{
    //
    // Preserved for compatibility.
    // The new Launcher does not use this popup.
    //

    if (g_customWindow)
    {
        SetForegroundWindow(
            g_customWindow);

        return;
    }

    WNDCLASSEXW wc{};

    wc.cbSize =
        sizeof(wc);

    wc.lpfnWndProc =
        CustomWndProc;

    wc.hInstance =
        GetModuleHandleW(nullptr);

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);

    wc.lpszClassName =
        L"DarkModCustomPalette";

    RegisterClassExW(
        &wc);

    g_customWindow =
        CreateWindowExW(
            WS_EX_TOOLWINDOW,
            wc.lpszClassName,
            L"Custom Palette",
            WS_POPUP |
            WS_CAPTION |
            WS_SYSMENU,
            0,
            0,
            360,
            350,
            g_hwnd,
            nullptr,
            wc.hInstance,
            nullptr);

    if (!g_customWindow)
        return;

    RECT owner{};

    GetWindowRect(
        g_hwnd,
        &owner);

    const int x =
        owner.left +
        80;

    const int y =
        owner.top +
        100;

    SetWindowPos(
        g_customWindow,
        HWND_TOP,
        x,
        y,
        380,
        360,
        SWP_SHOWWINDOW);

    ShowWindow(
        g_customWindow,
        SW_SHOW);

    UpdateWindow(
        g_customWindow);
}


// ============================================================
// REFRESH ANALYSIS INPUTS
// ============================================================

static bool RefreshAnalysisInputs(
    DWORD pid)
{
    if (g_darkModEnabled)
    {
        MessageBoxW(
            g_hwnd,
            L"Нельзя обновлять вводные данные, пока MOD включён.\n\nСначала выключи MOD.",
            WINDOW_TITLE,
            MB_ICONWARNING);

        return false;
    }

    if (!IsDarkModLoaded(pid))
        return false;

    //
    // Explicitly ask the DLL to refresh its analysis.
    //

    const bool result =
        CallRemoteExport(
            pid,
            "DarkModRefreshAnalysis");

    if (result)
    {
        //
        // Make the launcher see the new JSON
        // immediately, without waiting for the
        // polling timer.
        //

        g_lastJsonStamp = 0;

        UpdateLiveColors();

        ForceRedraw1C(
            pid);

        InvalidateRect(
            g_hwnd,
            nullptr,
            FALSE);
    }

    return result;
}


// ============================================================
// DETAILS WINDOW
// ============================================================

static void OpenDetailsWindow()
{
    if (g_detailsWindow)
    {
        SetForegroundWindow(
            g_detailsWindow);

        return;
    }

    WNDCLASSEXW wc{};

    wc.cbSize =
        sizeof(wc);

    wc.lpfnWndProc =
        [](HWND h,
            UINT m,
            WPARAM w,
            LPARAM l) -> LRESULT
        {
            switch (m)
            {
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};

                HDC dc =
                    BeginPaint(
                        h,
                        &ps);

                RECT rc{};

                GetClientRect(
                    h,
                    &rc);

                FillRectColor(
                    dc,
                    rc,
                    UiBase());

                //
                // Header.
                //

                DrawTextColor(
                    dc,
                    L"Все найденные цвета",
                    {
                        16,
                        12,
                        rc.right - 16,
                        42
                    },
                    UiText(),
                    DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE);

                DrawTextColor(
                    dc,
                    L"ОРИГИНАЛ",
                    {
                        16,
                        47,
                        300,
                        71
                    },
                    UiSub0(),
                    DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE);

                DrawTextColor(
                    dc,
                    L"ЗАМЕНА",
                    {
                        360,
                        47,
                        430,
                        71
                    },
                    UiSub0(),
                    DT_RIGHT |
                    DT_VCENTER |
                    DT_SINGLELINE);

                const int rowH =
                    28;

                const int contentTop =
                    74;

                const int visibleRows =
                    std::max(
                        1,
                        static_cast<int>(
                            (rc.bottom -
                                contentTop -
                                10) /
                            rowH));

                const int count =
                    static_cast<int>(
                        g_originalLiveColors.size());

                const int maxScroll =
                    std::max(
                        0,
                        count -
                        visibleRows);

                g_detailsScroll =
                    std::clamp(
                        g_detailsScroll,
                        0,
                        maxScroll);

                const int start =
                    g_detailsScroll;

                int y =
                    contentTop;

                for (
                    int i = start;
                    i < count &&
                    y < rc.bottom - 10;
                    ++i)
                {
                    const auto& lc =
                        g_originalLiveColors[
                            static_cast<size_t>(i)];

                    Color src =
                        ParseHexColor(
                            lc.hex);

                    Color dst =
                        PresetFamilyColor(
                            ClassifyFamily(src));

                    //
                    // Source circle.
                    //

                    DrawCircle(
                        dc,
                        {
                            16,
                            y + 5,
                            32,
                            y + 21
                        },
                        src);

                    //
                    // Destination circle.
                    //

                    DrawCircle(
                        dc,
                        {
                            360,
                            y + 5,
                            376,
                            y + 21
                        },
                        dst);

                    wchar_t line[128]{};

                    std::wstring wsHex(
                        lc.hex.begin(),
                        lc.hex.end());

                    swprintf_s(
                        line,
                        _countof(line),
                        L"%ls    %.2f%%    %llu px",
                        wsHex.c_str(),
                        lc.percentage,
                        static_cast<unsigned long long>(
                            lc.pixels));

                    DrawTextColor(
                        dc,
                        line,
                        {
                            44,
                            y,
                            346,
                            y + rowH
                        },
                        UiText(),
                        DT_LEFT |
                        DT_VCENTER |
                        DT_SINGLELINE);

                    DrawTextColor(
                        dc,
                        L"→",
                        {
                            326,
                            y,
                            354,
                            y + rowH
                        },
                        UiSub1(),
                        DT_CENTER |
                        DT_VCENTER |
                        DT_SINGLELINE);

                    ++y;
                    y += rowH - 1;
                }

                EndPaint(
                    h,
                    &ps);

                return 0;
            }

            case WM_MOUSEWHEEL:
            {
                int delta =
                    GET_WHEEL_DELTA_WPARAM(w);

                if (delta > 0)
                {
                    g_detailsScroll -= 3;
                }
                else if (delta < 0)
                {
                    g_detailsScroll += 3;
                }

                g_detailsScroll =
                    std::max(
                        0,
                        g_detailsScroll);

                InvalidateRect(
                    h,
                    nullptr,
                    FALSE);

                return 0;
            }

            case WM_SETCURSOR:
            {
                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_ARROW));

                return TRUE;
            }

            case WM_DESTROY:
            {
                g_detailsWindow =
                    nullptr;

                g_detailsScroll =
                    0;

                return 0;
            }
            }

            return DefWindowProcW(
                h,
                m,
                w,
                l);
        };

    wc.hInstance =
        GetModuleHandleW(nullptr);

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);

    wc.lpszClassName =
        L"DarkModDetailsWindow";

    RegisterClassExW(
        &wc);

    g_detailsWindow =
        CreateWindowExW(
            WS_EX_TOOLWINDOW,
            wc.lpszClassName,
            L"DarkMod — подробный анализ",
            WS_POPUP |
            WS_CAPTION |
            WS_SYSMENU,
            0,
            0,
            460,
            700,
            g_hwnd,
            nullptr,
            wc.hInstance,
            nullptr);

    if (!g_detailsWindow)
        return;

    RECT owner{};

    GetWindowRect(
        g_hwnd,
        &owner);

    SetWindowPos(
        g_detailsWindow,
        HWND_TOP,
        owner.left + 90,
        owner.top + 20,
        460,
        700,
        SWP_SHOWWINDOW);

    ShowWindow(
        g_detailsWindow,
        SW_SHOW);

    UpdateWindow(
        g_detailsWindow);
}


// ============================================================
// FAMILY PICKER
// ============================================================
//
// The previous source used a 12-color popup.
// It is now reduced to a direct system color picker
// for Custom and a direct preset-color selection for
// all fixed presets.
//
// This keeps the actual family configuration in the
// main Launcher UI, as requested.
// ============================================================

static int g_familyPick = -1;
static HWND g_familyWindow = nullptr;

//
// Picker geometry. The window grew to make room for the extra
// swatch rows and the button row.
//
static constexpr int PICKER_W = 390;
static constexpr int PICKER_H = 382;

static constexpr int PICKER_COLS = 6;
static constexpr int PICKER_ROWS = 4;
static constexpr int PICKER_SWATCHES =
PICKER_COLS * PICKER_ROWS;

static constexpr int PICKER_SWATCH = 40;
static constexpr int PICKER_PITCH_X = 58;
static constexpr int PICKER_PITCH_Y = 56;
static constexpr int PICKER_GRID_X = 16;
static constexpr int PICKER_GRID_Y = 72;
static constexpr int PICKER_LABEL_H = 16;
static constexpr int PICKER_BUTTON_Y = 306;
static constexpr int PICKER_BUTTON_H = 32;

//
// Swatch palette for one preset.
//
// Rows 1-2 are the 12 family defaults, unchanged from before.
// Row 3 adds the preset's remaining chrome tones and row 4 a plain
// grayscale ramp, because several presets offered nothing in
// between: Deep Dark's twelve defaults are four near-blacks and
// eight near-whites, so "replace the white background with gray"
// was simply not expressible.
//
static void BuildPickerSwatches(
    const Preset& preset,
    Color out[PICKER_SWATCHES])
{
    for (int i = 0; i < 12; ++i)
    {
        const ColorFamily f =
            FamilyAt(i);

        Color seed{};

        //
        // The seed rather than the current value, so a family that
        // has already been changed still offers its default back.
        // A generated preset has no seed, so it offers what it has.
        //
        out[i] =
            PresetSeedColor(
                preset,
                f,
                seed)
            ? seed
            : PresetFamilyColor(
                preset,
                f);
    }

    const UiTheme t =
        UiThemeFor(preset);

    out[12] = t.crust;
    out[13] = t.mantle;
    out[14] = t.surface1;
    out[15] = t.surface2;
    out[16] = t.sub0;
    out[17] = t.text;

    out[18] = Color{ 255,255,255 };
    out[19] = Color{ 204,204,204 };
    out[20] = Color{ 153,153,153 };
    out[21] = Color{ 102,102,102 };
    out[22] = Color{ 51,51,51 };
    out[23] = Color{ 0,0,0 };
}

static RECT PickerSwatchRect(
    int index)
{
    const int col =
        index % PICKER_COLS;

    const int row =
        index / PICKER_COLS;

    const int x =
        PICKER_GRID_X +
        col * PICKER_PITCH_X;

    const int y =
        PICKER_GRID_Y +
        row * PICKER_PITCH_Y;

    return
    {
        x,
        y,
        x + PICKER_SWATCH,
        y + PICKER_SWATCH
    };
}

static RECT PickerSwatchLabelRect(
    int index)
{
    const RECT s =
        PickerSwatchRect(index);

    return
    {
        s.left - 9,
        s.bottom,
        s.left - 9 + PICKER_PITCH_X,
        s.bottom + PICKER_LABEL_H
    };
}

static RECT PickerCustomRect()
{
    return
    {
        16,
        PICKER_BUTTON_Y,
        126,
        PICKER_BUTTON_Y + PICKER_BUTTON_H
    };
}

static RECT PickerResetFamilyRect()
{
    return
    {
        136,
        PICKER_BUTTON_Y,
        246,
        PICKER_BUTTON_Y + PICKER_BUTTON_H
    };
}

static RECT PickerResetPaletteRect()
{
    return
    {
        256,
        PICKER_BUTTON_Y,
        366,
        PICKER_BUTTON_Y + PICKER_BUTTON_H
    };
}

//
// Shared by both paint branches so every preset gets the same
// controls: an arbitrary RGB value and a way out of an override.
//
static void DrawPickerButtonRow(
    HDC dc,
    HWND h)
{
    POINT mp{};

    GetCursorPos(
        &mp);

    ScreenToClient(
        h,
        &mp);

    struct PickerButton
    {
        RECT rect;
        const wchar_t* text;
        bool enabled;
    };

    const PickerButton buttons[3]
    {
        {
            PickerCustomRect(),
            L"Свой цвет…",
            true
        },
        {
            PickerResetFamilyRect(),
            L"Сброс цвета",
            HasFamilyOverride(
                CurrentPreset(),
                FamilyAt(g_familyPick))
        },
        {
            PickerResetPaletteRect(),
            L"Сброс палитры",
            HasPaletteOverrides(
                CurrentPreset())
        }
    };

    for (const PickerButton& b : buttons)
    {
        const bool hovered =
            b.enabled &&
            PtInRect(
                &b.rect,
                mp) != FALSE;

        Color fill =
            UiSurface0();

        if (hovered)
        {
            fill =
                LightenColor(
                    fill,
                    0.14f);
        }

        RoundedRect(
            dc,
            b.rect,
            10,
            fill);

        DrawTextColor(
            dc,
            b.text,
            b.rect,
            b.enabled
            ? UiText()
            : UiSub1(),
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE);
    }
}

static void OpenFamilyPicker(
    int familyIndex)
{
    if (
        familyIndex < 0 ||
        familyIndex >= 12)
    {
        return;
    }

    g_familyPick =
        familyIndex;

    if (g_familyWindow)
    {
        SetForegroundWindow(
            g_familyWindow);

        return;
    }

    WNDCLASSEXW wc{};

    wc.cbSize =
        sizeof(wc);

    wc.lpfnWndProc =
        [](HWND h,
            UINT m,
            WPARAM w,
            LPARAM l) -> LRESULT
        {
            switch (m)
            {
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};

                HDC dc =
                    BeginPaint(
                        h,
                        &ps);

                RECT rc{};

                GetClientRect(
                    h,
                    &rc);

                FillRectColor(
                    dc,
                    rc,
                    UiBase());

                DrawTextColor(
                    dc,
                    FamilyName(
                        static_cast<ColorFamily>(
                            g_familyPick)),
                    {
                        16,
                        12,
                        350,
                        42
                    },
                    UiText(),
                    DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE);

                DrawTextColor(
                    dc,
                    CurrentPreset().name.c_str(),
                    {
                        16,
                        38,
                        350,
                        62
                    },
                    UiSub0(),
                    DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE);

                //
                // SWATCH GRID
                //
                // Same grid for every preset, Custom included.
                // Custom used to skip it entirely and go straight
                // to the system dialog; that dialog is now the
                // "Свой цвет…" button and available everywhere.
                //

                Color swatches[PICKER_SWATCHES]{};

                BuildPickerSwatches(
                    CurrentPreset(),
                    swatches);

                POINT mp{};

                GetCursorPos(
                    &mp);

                ScreenToClient(
                    h,
                    &mp);

                const bool hasOverride =
                    HasFamilyOverride(
                        CurrentPreset(),
                        FamilyAt(
                            g_familyPick));

                //
                // The current value, not the stored override: after the
                // switch to presets the twelve colors are already final,
                // so "which swatch is active" is simply "which swatch
                // equals what the preset uses".
                //
                const Color activeColor =
                    hasOverride
                    ? PresetFamilyColor(
                        FamilyAt(
                            g_familyPick))
                    : Color{};

                for (
                    int i = 0;
                    i < PICKER_SWATCHES;
                    ++i)
                {
                    const RECT colorRect =
                        PickerSwatchRect(i);

                    const Color color =
                        swatches[i];

                    const bool hovered =
                        PtInRect(
                            &colorRect,
                            mp) != FALSE;

                    Color drawColor =
                        color;

                    if (hovered)
                    {
                        drawColor =
                            LightenColor(
                                drawColor,
                                0.10f);
                    }

                    RoundedRect(
                        dc,
                        colorRect,
                        10,
                        drawColor);

                    //
                    // Border for current override.
                    //

                    if (
                        hasOverride &&
                        activeColor.r == color.r &&
                        activeColor.g == color.g &&
                        activeColor.b == color.b)
                    {
                        HPEN pen =
                            CreatePen(
                                PS_SOLID,
                                2,
                                ToColorRef(
                                    UiText()));

                        HGDIOBJ oldPen =
                            SelectObject(
                                dc,
                                pen);

                        HGDIOBJ oldBrush =
                            SelectObject(
                                dc,
                                GetStockObject(
                                    NULL_BRUSH));

                        RoundRect(
                            dc,
                            colorRect.left,
                            colorRect.top,
                            colorRect.right,
                            colorRect.bottom,
                            10,
                            10);

                        SelectObject(
                            dc,
                            oldBrush);

                        SelectObject(
                            dc,
                            oldPen);

                        DeleteObject(
                            pen);
                    }

                    //
                    // Hex label under the swatch.
                    //

                    wchar_t text[16]{};

                    swprintf_s(
                        text,
                        _countof(text),
                        L"#%02X%02X%02X",
                        static_cast<unsigned>(
                            color.r),
                        static_cast<unsigned>(
                            color.g),
                        static_cast<unsigned>(
                            color.b));

                    DrawTextColor(
                        dc,
                        text,
                        PickerSwatchLabelRect(i),
                        hovered
                        ? UiText()
                        : UiSub0(),
                        DT_CENTER |
                        DT_SINGLELINE);
                }

                DrawPickerButtonRow(
                    dc,
                    h);

                EndPaint(
                    h,
                    &ps);

                return 0;
            }

            case WM_MOUSEMOVE:
            {
                InvalidateRect(
                    h,
                    nullptr,
                    FALSE);

                TRACKMOUSEEVENT tme{};

                tme.cbSize =
                    sizeof(tme);

                tme.dwFlags =
                    TME_LEAVE;

                tme.hwndTrack =
                    h;

                TrackMouseEvent(
                    &tme);

                return 0;
            }

            case WM_MOUSELEAVE:
            {
                InvalidateRect(
                    h,
                    nullptr,
                    FALSE);

                return 0;
            }

            case WM_LBUTTONUP:
            {
                const int x =
                    GET_X_LPARAM(l);

                const int y =
                    GET_Y_LPARAM(l);

                //
                // RESET ROW
                //
                // Checked before the swatch grid so a click on a
                // button can never fall through to a color.
                //

                {
                    const POINT click{ x, y };

                    const RECT custom =
                        PickerCustomRect();

                    const RECT resetFamily =
                        PickerResetFamilyRect();

                    const RECT resetPalette =
                        PickerResetPaletteRect();

                    const bool hitCustom =
                        PtInRect(
                            &custom,
                            click) != FALSE;

                    const bool hitFamily =
                        PtInRect(
                            &resetFamily,
                            click) != FALSE;

                    const bool hitPalette =
                        PtInRect(
                            &resetPalette,
                            click) != FALSE;

                    if (hitCustom)
                    {
                        //
                        // Arbitrary RGB, now available for every
                        // preset rather than Custom alone.
                        //

                        std::wstring value =
                            ColorToWString(
                                PresetFamilyColor(
                                    FamilyAt(
                                        g_familyPick)));

                        if (
                            PickColor(
                                value))
                        {
                            SetFamilyOverride(
                                FamilyAt(
                                    g_familyPick),
                                ParseHexColor(
                                    WToUtf8(
                                        value)));

                            DestroyWindow(
                                h);

                            InvalidateRect(
                                g_hwnd,
                                nullptr,
                                FALSE);
                        }

                        return 0;
                    }

                    if (hitFamily || hitPalette)
                    {
                        if (
                            hitFamily &&
                            HasFamilyOverride(
                                CurrentPreset(),
                                FamilyAt(
                                    g_familyPick)))
                        {
                            ResetFamilyOverride(
                                FamilyAt(
                                    g_familyPick));
                        }
                        else if (
                            hitPalette &&
                            HasPaletteOverrides(
                                CurrentPreset()))
                        {
                            ResetPaletteOverrides();
                        }
                        else
                        {
                            //
                            // Disabled button: swallow the click.
                            //

                            return 0;
                        }

                        InvalidateRect(
                            h,
                            nullptr,
                            FALSE);

                        InvalidateRect(
                            g_hwnd,
                            nullptr,
                            FALSE);

                        return 0;
                    }
                }

                //
                // SWATCH GRID
                //

                {
                    const POINT click{ x, y };

                    Color swatches[PICKER_SWATCHES]{};

                    BuildPickerSwatches(
                        CurrentPreset(),
                        swatches);

                    for (
                        int i = 0;
                        i < PICKER_SWATCHES;
                        ++i)
                    {
                        const RECT sr =
                            PickerSwatchRect(i);

                        if (
                            PtInRect(
                                &sr,
                                click) == FALSE)
                        {
                            continue;
                        }

                        SetFamilyOverride(
                            FamilyAt(
                                g_familyPick),
                            swatches[i]);

                        DestroyWindow(
                            h);

                        InvalidateRect(
                            g_hwnd,
                            nullptr,
                            FALSE);

                        return 0;
                    }
                }

                return 0;
            }

            case WM_SETCURSOR:
            {
                POINT p{};

                GetCursorPos(
                    &p);

                ScreenToClient(
                    h,
                    &p);

                const RECT custom =
                    PickerCustomRect();

                const RECT resetFamily =
                    PickerResetFamilyRect();

                const RECT resetPalette =
                    PickerResetPaletteRect();

                bool clickable =
                    PtInRect(
                        &custom,
                        p) != FALSE ||
                    (PtInRect(
                        &resetFamily,
                        p) != FALSE &&
                        HasFamilyOverride(
                            CurrentPreset(),
                            FamilyAt(
                                g_familyPick))) ||
                    (PtInRect(
                        &resetPalette,
                        p) != FALSE &&
                        HasPaletteOverrides(
                            CurrentPreset()));

                for (
                    int i = 0;
                    !clickable &&
                    i < PICKER_SWATCHES;
                    ++i)
                {
                    const RECT sr =
                        PickerSwatchRect(i);

                    clickable =
                        PtInRect(
                            &sr,
                            p) != FALSE;
                }

                if (clickable)
                {
                    SetCursor(
                        LoadCursorW(
                            nullptr,
                            IDC_HAND));

                    return TRUE;
                }

                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_ARROW));

                return TRUE;
            }

            case WM_DESTROY:
            {
                g_familyWindow =
                    nullptr;

                return 0;
            }
            }

            return DefWindowProcW(
                h,
                m,
                w,
                l);
        };

    wc.hInstance =
        GetModuleHandleW(nullptr);

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);

    wc.lpszClassName =
        L"DarkModFamilyPicker";

    RegisterClassExW(
        &wc);

    g_familyWindow =
        CreateWindowExW(
            WS_EX_TOOLWINDOW,
            wc.lpszClassName,
            L"Выбор цвета",
            WS_POPUP |
            WS_CAPTION |
            WS_SYSMENU,
            0,
            0,
            PICKER_W,
            PICKER_H,
            g_hwnd,
            nullptr,
            wc.hInstance,
            nullptr);

    if (!g_familyWindow)
        return;

    RECT owner{};

    GetWindowRect(
        g_hwnd,
        &owner);

    SetWindowPos(
        g_familyWindow,
        HWND_TOP,
        owner.left + 100,
        owner.top + 150,
        PICKER_W,
        PICKER_H,
        SWP_SHOWWINDOW);

    ShowWindow(
        g_familyWindow,
        SW_SHOW);

    UpdateWindow(
        g_familyWindow);
}

// ============================================================
// HIT TEST FOR EVERYTHING
// ============================================================

static bool IsInteractiveAt(
    int x,
    int y)
{
    if (
        RectContains(
            SliderHitRect(),
            x,
            y))
    {
        return true;
    }

    RECT themeRc{
        THEME_X,
        THEME_Y,
        THEME_RIGHT,
        THEME_Y + SELECTOR_H
    };

    if (
        RectContains(
            themeRc,
            x,
            y))
    {
        return true;
    }

    RECT fontRc{
        FONT_X,
        FONT_Y,
        FONT_RIGHT,
        FONT_Y + SELECTOR_H
    };

    if (
        RectContains(
            fontRc,
            x,
            y))
    {
        return true;
    }

    if (
        ThemeMenuContains(
            x,
            y))
    {
        return true;
    }

    if (
        FontMenuContains(
            x,
            y))
    {
        return true;
    }

    int familyIndex = -1;

    if (
        IsFamilyReplacementAt(
            x,
            y,
            familyIndex))
    {
        return true;
    }

    if (
        RectContains(
            DetailsRect(),
            x,
            y))
    {
        return true;
    }

    if (
        RectContains(
            RefreshInputRect(),
            x,
            y))
    {
        return true;
    }

    RECT footer =
        FooterHitRect();

    if (
        RectContains(
            footer,
            x,
            y))
    {
        return true;
    }

    return false;
}


// ============================================================
// MOUSE ANIMATION
// ============================================================

static void UpdateAnimations()
{
    for (
        int i = 0;
        i < 3;
        ++i)
    {
        auto& b =
            g_buttons[i];

        const float speed =
            0.14f;

        if (b.hovered)
        {
            b.hover =
                std::min(
                    1.0f,
                    b.hover + speed);
        }
        else
        {
            b.hover =
                std::max(
                    0.0f,
                    b.hover - speed);
        }

        if (b.pressed)
        {
            b.press =
                std::min(
                    1.0f,
                    b.press + speed);
        }
        else
        {
            b.press =
                std::max(
                    0.0f,
                    b.press - speed);
        }
    }

    InvalidateRect(
        g_hwnd,
        nullptr,
        FALSE);
}


// ============================================================
// REFRESH STATE
// ============================================================

static void RefreshState()
{
    g_uiDark =
        g_darkModEnabled;

    const DWORD pid =
        Find1CPid();

    if (!pid)
    {
        g_currentPid = 0;

        g_currentProcess.clear();

        g_darkModLoaded =
            false;

        g_darkModEnabled =
            false;

        g_uiDark =
            false;

        g_sliderPos =
            0.0f;
    }
    else
    {
        g_currentPid =
            pid;

        const auto list =
            Find1CProcesses();

        if (!list.empty())
        {
            g_currentProcess =
                list.front().name;
        }

        g_darkModLoaded =
            IsDarkModLoaded(pid);

        //
        // DLL is a resident runtime component.
        // Once 1C appears, inject automatically.
        // The slider itself controls only MOD state.
        //

        if (!g_darkModLoaded)
        {
            if (
                InjectDLL(pid))
            {
                g_darkModLoaded =
                    IsDarkModLoaded(pid);
            }
        }

        //
        // Do not disturb the slider while the
        // user is physically dragging it.
        //

        if (!g_dragging)
        {
            g_sliderPos =
                g_darkModEnabled
                ? 1.0f
                : 0.0f;
        }
    }

    //
    // Do not update live source colors while MOD
    // is active.
    //

    if (!g_darkModEnabled)
    {
        UpdateLiveColors();
    }

    InvalidateRect(
        g_hwnd,
        nullptr,
        FALSE);
}


// ============================================================
// COLOR APPLY
// ============================================================

static void ApplyPalette()
{
    //
    // Palette changes are intentionally NOT pushed
    // into 1C immediately.
    //
    // The actual apply point is the MOD slider.
    //

    WriteCurrentPalette();

    InvalidateRect(
        g_hwnd,
        nullptr,
        FALSE);
}


static void ApplyFont()
{
    //
    // Font selection is also pending until the
    // next MOD apply.
    //

    WriteCurrentFonts();

    InvalidateRect(
        g_hwnd,
        nullptr,
        FALSE);
}


// ============================================================
// ACTIONS
// ============================================================

static void OnEnable()
{
    const DWORD pid =
        Find1CPid();

    if (!pid)
    {
        MessageBoxW(
            g_hwnd,
            L"1С процесс не найден.",
            WINDOW_TITLE,
            MB_ICONWARNING);

        g_darkModEnabled =
            false;

        g_sliderPos =
            0.0f;

        RefreshState();

        return;
    }

    //
    // Ensure the DLL is resident.
    //

    if (!g_darkModLoaded)
    {
        if (!InjectDLL(pid))
            return;

        Sleep(250);

        g_darkModLoaded =
            IsDarkModLoaded(pid);
    }

    //
    // Apply pending configuration exactly once
    // when the slider is turned ON.
    //

    WriteCurrentPalette();
    WriteCurrentFonts();

    if (
        EnableDarkMod(
            pid))
    {
        g_darkModEnabled =
            true;

        g_uiDark =
            true;

        g_sliderPos =
            1.0f;

        //
        // Immediate visual update.
        //

        ForceRedraw1C(
            pid);

    }
    else
    {
        MessageBoxW(
            g_hwnd,
            L"Не удалось применить DarkMod.\n\nПроверь экспорт DarkModEnable.",
            WINDOW_TITLE,
            MB_ICONERROR);

        g_darkModEnabled =
            false;

        g_uiDark =
            false;

        g_sliderPos =
            0.0f;
    }

    RefreshState();
}


static void OnDisable()
{
    const DWORD pid =
        Find1CPid();

    if (
        !pid ||
        !IsDarkModLoaded(pid))
    {
        g_darkModEnabled =
            false;

        g_uiDark =
            false;

        g_sliderPos =
            0.0f;

        RefreshState();

        return;
    }

    if (
        DisableDarkMod(
            pid))
    {
        g_darkModEnabled =
            false;

        g_uiDark =
            false;

        g_sliderPos =
            0.0f;

        //
        // Clearing g_darkModEnabled above reopens the guard in
        // UpdateLiveColors, and RefreshState() below calls it. Hold
        // analysis ingest off until 1C has actually repainted, or
        // the capture the DLL is about to take - still showing our
        // palette - would be adopted as the next "original" colors.
        //

        g_analysisQuarantineUntil =
            GetTickCount64() +
            ANALYSIS_QUARANTINE_MS;

        //
        // Force 1C to repaint immediately after
        // the runtime effect disappears.
        //

        ForceRedraw1C(
            pid);

    }
    else
    {
        MessageBoxW(
            g_hwnd,
            L"Не удалось выключить DarkMod.\n\nПроверь экспорт DarkModDisable.",
            WINDOW_TITLE,
            MB_ICONERROR);
    }

    RefreshState();
}


static void OnRefresh()
{
    RefreshState();
}


// ============================================================
// WINDOW PROCEDURE
// ============================================================

static LRESULT CALLBACK
WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hwnd =
            hwnd;

        SetupButtons();

        SetTimer(
            hwnd,
            g_animTimer,
            15,
            nullptr);

        SetTimer(
            hwnd,
            g_pollTimer,
            500,
            nullptr);

        g_sliderPos =
            g_darkModEnabled
            ? 1.0f
            : 0.0f;

        RefreshState();

        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
    {
        if (
            wParam ==
            g_animTimer)
        {
            UpdateAnimations();
        }

        if (
            wParam ==
            g_pollTimer)
        {
            RefreshState();
        }

        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        POINT p{};

        p.x =
            GET_X_LPARAM(
                lParam);

        p.y =
            GET_Y_LPARAM(
                lParam);

        ScreenToClient(
            hwnd,
            &p);

        //
        // Theme menu, checked first: it is drawn on top of whatever
        // it overlaps, so it has to take the wheel first too.
        //
        if (
            g_paletteOpen &&
            ThemeMenuContains(
                p.x,
                p.y) &&
            ThemeMenuMaxScroll() > 0)
        {
            const int delta =
                GET_WHEEL_DELTA_WPARAM(
                    wParam);

            //
            // One entry row per notch, so a wheel click moves the
            // list by a whole preset rather than a fraction of one.
            //
            if (delta > 0)
            {
                g_themeScroll -= 31;
            }
            else if (delta < 0)
            {
                g_themeScroll += 31;
            }

            ClampThemeScroll();

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        //
        // Main family viewport.
        //

        if (
            p.x >= LEFT &&
            p.x <= RIGHT &&
            p.y >= LIVE_Y &&
            p.y < FAMILY_VIEW_BOTTOM)
        {
            const int delta =
                GET_WHEEL_DELTA_WPARAM(
                    wParam);

            if (delta > 0)
            {
                g_familyScroll -= 1;
            }
            else if (delta < 0)
            {
                g_familyScroll += 1;
            }

            g_familyScroll =
                std::clamp(
                    g_familyScroll,
                    0,
                    std::max(
                        0,
                        12 -
                        FAMILY_VIEW_ROWS));

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        return DefWindowProcW(
            hwnd,
            msg,
            wParam,
            lParam);
    }

    case WM_MOUSEMOVE:
    {
        const int x =
            GET_X_LPARAM(lParam);

        const int y =
            GET_Y_LPARAM(lParam);

        const int button =
            ButtonAt(
                x,
                y);

        for (
            int i = 0;
            i < 3;
            ++i)
        {
            g_buttons[i].hovered =
                (i == button);
        }

        if (button < 0 &&
            g_hoverButton >= 0)
        {
            g_hoverButton =
                -1;
        }
        else
        {
            g_hoverButton =
                button;

        }
        //
        // Theme menu hover.
        //
        // The menu itself is drawn dynamically,
        // so forcing repaint here gives smooth
        // hover feedback for group headers and
        // preset entries.
        //

        if (g_paletteOpen)
        {
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        if (g_dragging)
        {
            RECT track =
                SliderVisualRect();

            const int left =
                track.left + 7;

            const int right =
                track.right - 7;

            if (right > left)
            {
                g_sliderPos =
                    std::clamp(
                        (
                            x - left
                            ) /
                        static_cast<float>(
                            right - left),
                        0.0f,
                        1.0f);
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        TRACKMOUSEEVENT tme{};

        tme.cbSize =
            sizeof(tme);

        tme.dwFlags =
            TME_LEAVE;

        tme.hwndTrack =
            hwnd;

        TrackMouseEvent(
            &tme);

        return 0;
    }

    case WM_MOUSELEAVE:
    {
        g_hoverButton = -1;

        for (
            auto& b :
            g_buttons)
        {
            b.hovered = false;
        }

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);

        return 0;
    }

    //
    // Right click on a replacement circle clears the override for
    // that family and returns it to the palette default. This is
    // the only way back out of an override besides the picker's
    // reset row.
    //
    case WM_RBUTTONDOWN:
    {
        const int x =
            GET_X_LPARAM(lParam);

        const int y =
            GET_Y_LPARAM(lParam);

        int familyIndex = -1;

        if (
            !g_paletteOpen &&
            IsFamilyReplacementAt(
                x,
                y,
                familyIndex) &&
            HasFamilyOverride(
                CurrentPreset(),
                FamilyAt(
                    familyIndex)))
        {
            ResetFamilyOverride(
                FamilyAt(
                    familyIndex));

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        const int x =
            GET_X_LPARAM(lParam);

        const int y =
            GET_Y_LPARAM(lParam);

        POINT click{
            x,
            y
        };

        // --------------------------------------------------------
        // 1. Theme dropdown.
        // --------------------------------------------------------

        if (g_paletteOpen)
        {
            const std::vector<ThemeMenuEntry>& entries =
                GetThemeMenuEntries();

            ClampThemeScroll();

            //
            // Check every visible menu row.
            // Headers are handled separately from
            // actual preset entries.
            //

            const int count =
                static_cast<int>(
                    entries.size());

            for (
                int i = 0;
                i < count;
                ++i)
            {
                const ThemeMenuEntry& entry =
                    entries[i];

                const int rowY =
                    ThemeMenuRowY(i);

                const int rowH =
                    ThemeMenuRowHeight(entry);

                RECT row{
                    THEME_X + 7,
                    rowY,
                    THEME_RIGHT - 7,
                    rowY + rowH
                };

                if (!RectContains(
                    row,
                    x,
                    y))
                {
                    continue;
                }

                //
                // Scrolled out of the panel: the row is drawn
                // clipped, so it must not be clickable either.
                //
                if (!ThemeMenuRowVisible(
                    rowY,
                    rowH))
                {
                    continue;
                }

                //
                // GROUP HEADER
                //
                // + DARK / - DARK
                // + CATPPUCCIN / - CATPPUCCIN
                // + СОЗДАННЫЕ / - СОЗДАННЫЕ
                //
                if (entry.header)
                {
                    //
                    // One group open at a time, as before: the
                    // panel is short and three expanded groups
                    // would need scrolling for no reason.
                    //
                    const bool wasOpen =
                        entry.group == ThemeGroup::Dark
                        ? g_darkGroupOpen
                        : entry.group == ThemeGroup::Catppuccin
                        ? g_catppuccinGroupOpen
                        : g_generatedGroupOpen;

                    g_darkGroupOpen = false;
                    g_catppuccinGroupOpen = false;
                    g_generatedGroupOpen = false;

                    if (!wasOpen)
                    {
                        switch (entry.group)
                        {
                        case ThemeGroup::Dark:
                            g_darkGroupOpen = true;
                            break;

                        case ThemeGroup::Catppuccin:
                            g_catppuccinGroupOpen = true;
                            break;

                        case ThemeGroup::Generated:
                            g_generatedGroupOpen = true;
                            break;

                        default:
                            break;
                        }
                    }

                    g_themeScroll = 0;

                    InvalidateRect(
                        hwnd,
                        nullptr,
                        FALSE);

                    return 0;
                }
                //
                // "ВЕРНУТЬ СТАНДАРТНЫЕ"
                //
                // Stays open afterwards: the eight presets have just
                // reappeared above this row, and closing the menu
                // would hide the only evidence that it worked.
                //
                if (entry.action)
                {
                    g_pressedUi =
                        UiPressTarget::
                        ThemeItem;

                    RestoreBuiltinPresets();

                    g_themeScroll = 0;

                    InvalidateRect(
                        hwnd,
                        nullptr,
                        FALSE);

                    return 0;
                }

                //
                // NORMAL PRESET
                //

                g_pressedUi =
                    UiPressTarget::
                    ThemeItem;

                SelectPreset(
                    entry.presetId);

                //
                // Custom is now a normal Theme state.
                // It does NOT open the old popup window.
                //

                g_paletteOpen =
                    false;

                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);

                return 0;
            }


            //
            // Click outside the Theme menu.
            //

            if (
                !ThemeMenuContains(
                    x,
                    y))
            {
                g_paletteOpen =
                    false;

                g_pressedUi =
                    UiPressTarget::None;

                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);

                return 0;
            }
        }
        // --------------------------------------------------------
        // 2. Font dropdown.
        // --------------------------------------------------------

        if (g_fontOpen)
        {
            const int item =
                FontItemAt(y);

            if (item >= 0)
            {
                if (
                    item < FONT_COUNT)
                {
                    g_pressedUi =
                        UiPressTarget::
                        FontItem;

                    g_fontSelection =
                        g_fonts[item];

                    g_fontOpen =
                        false;

                    ApplyFont();

                    InvalidateRect(
                        hwnd,
                        nullptr,
                        FALSE);

                    return 0;
                }
            }

            if (
                !FontMenuContains(
                    x,
                    y))
            {
                g_fontOpen =
                    false;

                g_pressedUi =
                    UiPressTarget::None;

                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);
            }
        }

        // --------------------------------------------------------
        // 3. MOD slider / drag.
        // --------------------------------------------------------

        if (
            RectContains(
                SliderHitRect(),
                x,
                y))
        {
            g_pressedUi =
                UiPressTarget::Slider;

            g_dragging =
                true;

            SetCapture(
                hwnd);

            RECT track =
                SliderVisualRect();

            const int left =
                track.left + 7;

            const int right =
                track.right - 7;

            if (right > left)
            {
                g_sliderPos =
                    std::clamp(
                        (
                            x - left
                            ) /
                        static_cast<float>(
                            right - left),
                        0.0f,
                        1.0f);
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        // --------------------------------------------------------
        // 4. Theme selector.
        // --------------------------------------------------------

        RECT themeRc{
            THEME_X,
            THEME_Y,
            THEME_RIGHT,
            THEME_Y + SELECTOR_H
        };

        if (
            RectContains(
                themeRc,
                x,
                y))
        {
            g_pressedUi =
                UiPressTarget::
                ThemeSelector;

            g_paletteOpen =
                !g_paletteOpen;

            g_fontOpen =
                false;

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        // --------------------------------------------------------
        // 5. Font selector.
        // --------------------------------------------------------

        RECT fontRc{
            FONT_X,
            FONT_Y,
            FONT_RIGHT,
            FONT_Y + SELECTOR_H
        };

        if (
            RectContains(
                fontRc,
                x,
                y))
        {
            g_pressedUi =
                UiPressTarget::
                FontSelector;

            g_fontOpen =
                !g_fontOpen;

            g_paletteOpen =
                false;

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        // --------------------------------------------------------
        // 6. Family replacement.
        // --------------------------------------------------------

        int familyIndex = -1;

        if (
            IsFamilyReplacementAt(
                x,
                y,
                familyIndex))
        {
            g_pressedUi =
                UiPressTarget::
                FamilyReplacement;

            OpenFamilyPicker(
                familyIndex);

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        // --------------------------------------------------------
        // 7. Details.
        // --------------------------------------------------------

        if (
            RectContains(
                DetailsRect(),
                x,
                y))
        {
            g_pressedUi =
                UiPressTarget::
                Details;

            OpenDetailsWindow();

            return 0;
        }

        // --------------------------------------------------------
        // 8. Refresh analysis inputs.
        // --------------------------------------------------------

        if (
            RectContains(
                RefreshInputRect(),
                x,
                y))
        {
            g_pressedUi =
                UiPressTarget::
                RefreshInput;

            const DWORD pid =
                Find1CPid();

            if (pid)
            {
                RefreshAnalysisInputs(
                    pid);
            }

            return 0;
        }

        // --------------------------------------------------------
        // 9. Telegram footer.
        // --------------------------------------------------------

        RECT footer =
            FooterHitRect();

        if (
            RectContains(
                footer,
                x,
                y))
        {
            g_pressedUi =
                UiPressTarget::
                Footer;

            OpenTelegram();

            return 0;
        }

        return 0;
    }

    case WM_LBUTTONUP:
    {
        const int x =
            GET_X_LPARAM(lParam);

        const int y =
            GET_Y_LPARAM(lParam);

        //
        // Slider drag release.
        //

        if (g_dragging)
        {
            g_dragging =
                false;

            ReleaseCapture();

            const bool targetOn =
                g_sliderPos >= 0.5f;

            const bool currentOn =
                g_darkModEnabled;

            g_pressedUi =
                UiPressTarget::None;

            if (
                targetOn !=
                currentOn)
            {
                if (targetOn)
                {
                    OnEnable();
                }
                else
                {
                    OnDisable();
                }
            }
            else
            {
                g_sliderPos =
                    currentOn
                    ? 1.0f
                    : 0.0f;
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            return 0;
        }

        //
        // Other press animations.
        //

        g_pressedUi =
            UiPressTarget::None;

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);

        return 0;
    }

    case WM_SETCURSOR:
    {
        POINT p{};

        GetCursorPos(
            &p);

        ScreenToClient(
            hwnd,
            &p);

        if (
            IsInteractiveAt(
                p.x,
                p.y))
        {
            SetCursor(
                LoadCursorW(
                    nullptr,
                    IDC_HAND));

            return TRUE;
        }

        return DefWindowProcW(
            hwnd,
            msg,
            wParam,
            lParam);
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};

        HDC hdc =
            BeginPaint(
                hwnd,
                &ps);

        Paint(
            hwnd,
            hdc);

        EndPaint(
            hwnd,
            &ps);

        return 0;
    }

    case WM_DESTROY:
    {
        KillTimer(
            hwnd,
            g_animTimer);

        KillTimer(
            hwnd,
            g_pollTimer);

        PostQuitMessage(0);

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam);
}


// ============================================================
// MAIN
// ============================================================

int WINAPI
wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow)
{
    //
    // Runtime files live in %LOCALAPPDATA%\DarkMod, not next to
    // the exe, so the shipped exe stays a single file and works
    // from a read-only location.
    //
    const std::wstring exeDir =
        ModuleDirectory();

    //
    // Pinned before anything calls FilePath(), because the DLL
    // resolves its own paths from its module directory and the exe
    // must not disagree with it.
    //
    SetDataDirectory(
        LocalAppDataDirectory());

    MigrateLegacyFiles(
        exeDir,
        DataDirectory());

    //
    // Write out the DLL we carry before anything reads the
    // config, so injection has a file to point at.
    //
    ExtractEmbeddedDll();

    //
    // Bootstrap the file on first run so the Custom base colors
    // are visible and editable on disk.
    //

    if (!LoadCustomPalette())
        SaveCustomPaletteFile();

    //
    // After the custom colors, because a first-run migration seeds
    // the Custom preset from them.
    //
    LoadPresets();

    g_font =
        CreateFontW(
            16,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH |
            FF_DONTCARE,
            L"Segoe UI");

    g_smallFont =
        CreateFontW(
            12,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH |
            FF_DONTCARE,
            L"Segoe UI");

    g_tinyFont =
        CreateFontW(
            9,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH |
            FF_DONTCARE,
            L"Segoe UI");

    WNDCLASSEXW wc{};

    wc.cbSize =
        sizeof(wc);

    wc.lpfnWndProc =
        WndProc;

    wc.hInstance =
        hInstance;

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    wc.hbrBackground =
        CreateSolidBrush(
            ToColorRef(
                UiBase()));

    wc.lpszClassName =
        WINDOW_CLASS;

    if (
        !RegisterClassExW(
            &wc))
    {
        return 1;
    }

    HWND hwnd =
        CreateWindowExW(
            0,
            WINDOW_CLASS,
            WINDOW_TITLE,
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WINDOW_W,
            WINDOW_H,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

    if (!hwnd)
        return 1;

    //
    // Center.
    //

    RECT screen{};

    SystemParametersInfoW(
        SPI_GETWORKAREA,
        0,
        &screen,
        0);

    const int x =
        screen.left +
        (
            (
                screen.right -
                screen.left -
                WINDOW_W
                ) / 2
            );

    const int y =
        screen.top +
        (
            (
                screen.bottom -
                screen.top -
                WINDOW_H
                ) / 2
            );

    SetWindowPos(
        hwnd,
        nullptr,
        x,
        y,
        0,
        0,
        SWP_NOSIZE |
        SWP_NOZORDER);

    ShowWindow(
        hwnd,
        nCmdShow);

    UpdateWindow(
        hwnd);

    MSG msg{};

    while (
        GetMessageW(
            &msg,
            nullptr,
            0,
            0))
    {
        TranslateMessage(
            &msg);

        DispatchMessageW(
            &msg);
    }

    if (g_font)
        DeleteObject(g_font);

    if (g_smallFont)
        DeleteObject(g_smallFont);

    if (g_tinyFont)
        DeleteObject(g_tinyFont);

    return static_cast<int>(
        msg.wParam);
}
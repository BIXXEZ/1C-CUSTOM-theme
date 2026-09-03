#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// ============================================================
// DARKMOD ALPHA 0.4 / PASS 1.1
//
// Single-file renderer color replacement + screen analyzer.
//
// Files next to DLL:
//
//   DarkModDLL.dll
//   darkmod.ini
//   darkmod.log
//   darkmod_colors.json
//
// INI:
//
//   1|#FFFFFF|#CDD6F4
//   1|#000000|#11111B
//   1|#333333|#45475A
//
// Runtime:
//
//   Cairo direct colors:
//       cairo_set_source_rgb
//       cairo_set_source_rgba
//
//   Cairo patterns:
//       cairo_pattern_create_rgb
//       cairo_pattern_create_rgba
//       cairo_pattern_add_color_stop_rgb
//       cairo_pattern_add_color_stop_rgba
//       cairo_mesh_pattern_set_corner_color_rgb
//       cairo_mesh_pattern_set_corner_color_rgba
//
//   Analysis:
//       client-area screenshot about once per second
//       quantized color histogram
//       percentages
//
// ============================================================

#include <windows.h>
#include <detours.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>


// ============================================================
// CAIRO TYPES
// ============================================================

struct cairo_t;
struct cairo_pattern_t;
struct cairo_surface_t;


// ============================================================
// CAIRO ENUMS
// ============================================================

enum cairo_surface_type_t
{
    CAIRO_SURFACE_TYPE_IMAGE = 0,
    CAIRO_SURFACE_TYPE_PDF = 1,
    CAIRO_SURFACE_TYPE_PS = 2,
    CAIRO_SURFACE_TYPE_XLIB = 3,
    CAIRO_SURFACE_TYPE_XCB = 4,
    CAIRO_SURFACE_TYPE_GL = 5,
    CAIRO_SURFACE_TYPE_WIN32 = 6,
    CAIRO_SURFACE_TYPE_BEOS = 7,
    CAIRO_SURFACE_TYPE_DIRECTFB = 8,
    CAIRO_SURFACE_TYPE_SVG = 9,
    CAIRO_SURFACE_TYPE_OS2 = 10,
    CAIRO_SURFACE_TYPE_WIN32_PRINTING = 11,
    CAIRO_SURFACE_TYPE_QUARTZ = 12,
    CAIRO_SURFACE_TYPE_QUARTZ_IMAGE = 13,
    CAIRO_SURFACE_TYPE_SCRIPT = 14,
    CAIRO_SURFACE_TYPE_QT = 15,
    CAIRO_SURFACE_TYPE_RECORDING = 16,
    CAIRO_SURFACE_TYPE_VG = 17,
    CAIRO_SURFACE_TYPE_GLITZ = 18,
    CAIRO_SURFACE_TYPE_DRM = 19,
    CAIRO_SURFACE_TYPE_TEE = 20,
    CAIRO_SURFACE_TYPE_XML = 21,
    CAIRO_SURFACE_TYPE_SKIA = 22,
    CAIRO_SURFACE_TYPE_COGL = 23,
    CAIRO_SURFACE_TYPE_WIN32_DB = 24
};

enum cairo_format_t
{
    CAIRO_FORMAT_ARGB32 = 0,
    CAIRO_FORMAT_RGB24 = 1,
    CAIRO_FORMAT_A8 = 2,
    CAIRO_FORMAT_A1 = 3,
    CAIRO_FORMAT_RGB16_565 = 4,
    CAIRO_FORMAT_RGB30 = 5,
    CAIRO_FORMAT_RGB96F = 6,
    CAIRO_FORMAT_RGBA128F = 7
};


// ============================================================
// FUNCTION TYPES
// ============================================================

using PFN_cairo_select_font_face =
void(__cdecl*)(
    cairo_t*,
    const char*,
    int,
    int);


using PFN_cairo_toy_font_face_create =
void* (__cdecl*)(
    const char*,
    int,
    int);

using PFN_cairo_set_source_rgb =
void(__cdecl*)(
    cairo_t*,
    double,
    double,
    double);

using PFN_cairo_set_source_rgba =
void(__cdecl*)(
    cairo_t*,
    double,
    double,
    double,
    double);

using PFN_cairo_set_source =
void(__cdecl*)(
    cairo_t*,
    cairo_pattern_t*);

using PFN_cairo_set_source_surface =
void(__cdecl*)(
    cairo_t*,
    cairo_surface_t*,
    double,
    double);

using PFN_cairo_pattern_create_rgb =
cairo_pattern_t * (__cdecl*)(
    double,
    double,
    double);

using PFN_cairo_pattern_create_rgba =
cairo_pattern_t * (__cdecl*)(
    double,
    double,
    double,
    double);

using PFN_cairo_pattern_create_linear =
cairo_pattern_t * (__cdecl*)(
    double,
    double,
    double,
    double);

using PFN_cairo_pattern_create_radial =
cairo_pattern_t * (__cdecl*)(
    double,
    double,
    double,
    double,
    double,
    double);

using PFN_cairo_pattern_add_color_stop_rgb =
void(__cdecl*)(
    cairo_pattern_t*,
    double,
    double,
    double,
    double);

using PFN_cairo_pattern_add_color_stop_rgba =
void(__cdecl*)(
    cairo_pattern_t*,
    double,
    double,
    double,
    double,
    double);

using PFN_cairo_mesh_pattern_set_corner_color_rgb =
void(__cdecl*)(
    cairo_pattern_t*,
    unsigned int,
    double,
    double,
    double);

using PFN_cairo_mesh_pattern_set_corner_color_rgba =
void(__cdecl*)(
    cairo_pattern_t*,
    unsigned int,
    double,
    double,
    double,
    double);

using PFN_cairo_surface_flush =
void(__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_surface_get_type =
cairo_surface_type_t(__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_image_surface_get_data =
unsigned char* (__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_image_surface_get_width =
int(__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_image_surface_get_height =
int(__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_image_surface_get_stride =
int(__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_image_surface_get_format =
cairo_format_t(__cdecl*)(
    cairo_surface_t*);

using PFN_cairo_surface_destroy =
unsigned int(__cdecl*)(
    cairo_surface_t*);


// ============================================================
// REAL FUNCTIONS
// ============================================================

static PFN_cairo_select_font_face
RealSelectFontFace = nullptr;

static PFN_cairo_toy_font_face_create
RealToyFontFaceCreate = nullptr;

static PFN_cairo_set_source_rgb
RealSetSourceRGB = nullptr;

static PFN_cairo_set_source_rgba
RealSetSourceRGBA = nullptr;

static PFN_cairo_set_source
RealSetSource = nullptr;

static PFN_cairo_set_source_surface
RealSetSourceSurface = nullptr;

static PFN_cairo_pattern_create_rgb
RealPatternCreateRGB = nullptr;

static PFN_cairo_pattern_create_rgba
RealPatternCreateRGBA = nullptr;

static PFN_cairo_pattern_create_linear
RealPatternCreateLinear = nullptr;

static PFN_cairo_pattern_create_radial
RealPatternCreateRadial = nullptr;

static PFN_cairo_pattern_add_color_stop_rgb
RealPatternAddColorStopRGB = nullptr;

static PFN_cairo_pattern_add_color_stop_rgba
RealPatternAddColorStopRGBA = nullptr;

static PFN_cairo_mesh_pattern_set_corner_color_rgb
RealMeshCornerRGB = nullptr;

static PFN_cairo_mesh_pattern_set_corner_color_rgba
RealMeshCornerRGBA = nullptr;

static PFN_cairo_surface_flush
RealSurfaceFlush = nullptr;

static PFN_cairo_surface_get_type
RealSurfaceGetType = nullptr;

static PFN_cairo_image_surface_get_data
RealImageGetData = nullptr;

static PFN_cairo_image_surface_get_width
RealImageGetWidth = nullptr;

static PFN_cairo_image_surface_get_height
RealImageGetHeight = nullptr;

static PFN_cairo_image_surface_get_stride
RealImageGetStride = nullptr;

static PFN_cairo_image_surface_get_format
RealImageGetFormat = nullptr;

static PFN_cairo_surface_destroy
RealSurfaceDestroy = nullptr;


// ============================================================
// DLL STATE
// ============================================================

static HMODULE g_self = nullptr;

static HANDLE g_worker = nullptr;

static std::atomic<bool> g_running{ true };

// Runtime mode:
// ANALYZE = collect original renderer/screen data, do not replace.
// MOD     = apply current mappings and stop screen analysis.
enum class DarkModMode : int
{
    ANALYZE = 0,
    MOD = 1
};

static std::atomic<DarkModMode> g_mode{ DarkModMode::ANALYZE };
static std::atomic<bool> g_applyRequested{ false };
static std::atomic<bool> g_analysisResetRequested{ true };

// Screen analysis quarantine deadline (GetTickCount64 units).
//
// RunScreenAnalysis captures the client area with GDI, so
// darkmod_colors.json is literally a histogram of what 1C shows.
// Leaving MOD flips the mode to ANALYZE and resets the analysis
// timer, which makes the very next 25 ms tick capture the screen -
// while 1C is still displaying the modded pixels. Those palette
// colors then land in the json as "original colors", the launcher
// turns them into rules whose SOURCES are our own output, and the
// next enable destroys itself.
//
// Hold analysis off until the redraw requested on disable has had
// time to land.
static std::atomic<ULONGLONG> g_analyzeNotBefore{ 0 };
static constexpr ULONGLONG ANALYZE_QUARANTINE_MS = 1200;

// Forward declarations for runtime state helpers.
static bool IsAnalyzeMode();
static bool IsModMode();
static void RecordOriginalSourceColor(
    double r,
    double g,
    double b);
static void ResetAnalysisState();

// Defined next to g_originalPixels, declared here because
// ResetAnalysisState needs it and lives further up the file.
static void ClearOriginalSurfaceCache();

static std::wstring g_dllDir;
static std::wstring g_iniPath;
static std::wstring g_logPath;
static std::wstring g_colorsPath;
static std::wstring g_fontsIniPath;

// ============================================================
// LOGGING
// ============================================================

static HANDLE g_logHandle =
INVALID_HANDLE_VALUE;

static std::mutex g_logMutex;


static std::wstring NowText()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);

    std::wstringstream ss;

    ss
        << std::setfill(L'0')
        << std::setw(2)
        << st.wHour
        << L":"
        << std::setw(2)
        << st.wMinute
        << L":"
        << std::setw(2)
        << st.wSecond
        << L"."
        << std::setw(3)
        << st.wMilliseconds;

    return ss.str();
}


static void Log(
    const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(
        g_logMutex);

    if (
        g_logHandle ==
        INVALID_HANDLE_VALUE)
    {
        return;
    }

    std::wstringstream line;

    line
        << L"["
        << NowText()
        << L"] "
        << message
        << L"\r\n";

    const std::wstring text =
        line.str();

    int bytes =
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
        return;

    std::string utf8(
        static_cast<size_t>(bytes),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(
            text.size()),
        utf8.data(),
        bytes,
        nullptr,
        nullptr);

    DWORD written = 0;

    WriteFile(
        g_logHandle,
        utf8.data(),
        static_cast<DWORD>(
            utf8.size()),
        &written,
        nullptr);
}


// ============================================================
// PATH
// ============================================================

static std::wstring GetModuleDir(
    HMODULE module)
{
    wchar_t path[MAX_PATH]{};

    DWORD n =
        GetModuleFileNameW(
            module,
            path,
            MAX_PATH);

    if (!n)
        return L".";

    std::wstring s(
        path,
        n);

    const size_t pos =
        s.find_last_of(
            L"\\/");

    if (
        pos ==
        std::wstring::npos)
    {
        return L".";
    }

    return s.substr(
        0,
        pos);
}


// ============================================================
// RGB MAPPING
// ============================================================

struct Mapping
{
    uint8_t sr = 0;
    uint8_t sg = 0;
    uint8_t sb = 0;

    uint8_t dr = 0;
    uint8_t dg = 0;
    uint8_t db = 0;

    bool enabled = false;
};


// Shared immutable mapping table.
// Hooks only read it.
// Worker replaces whole table when INI changes.

using MappingList =
std::vector<Mapping>;

static std::shared_ptr<const MappingList>
g_mappings =
std::make_shared<
    const MappingList>();
// Original renderer colors collected only while in ANALYZE mode.
// Same 5-bit/channel quantization as the screen analyzer.
static constexpr size_t SOURCE_HISTOGRAM_SIZE = 32768;
static std::array<std::atomic<uint64_t>, SOURCE_HISTOGRAM_SIZE>
g_sourceHistogram{};
static std::atomic<uint64_t> g_sourceColorSamples{ 0 };



// ============================================================
// STRING HELPERS
// ============================================================

static std::wstring Trim(
    std::wstring s)
{
    auto notSpace =
        [](wchar_t c)
        {
            return !iswspace(c);
        };

    s.erase(
        s.begin(),
        std::find_if(
            s.begin(),
            s.end(),
            notSpace));

    s.erase(
        std::find_if(
            s.rbegin(),
            s.rend(),
            notSpace).base(),
        s.end());

    return s;
}

// ============================================================
// FONT SUPPORT
// ============================================================

struct FontMapping
{
    std::string source;
    std::string destination;
    bool enabled = false;
};

using FontMappingList =
std::vector<FontMapping>;

static std::shared_ptr<const FontMappingList>
g_fontMappings =
std::make_shared<
    const FontMappingList>();


// ============================================================
// UTF-16 -> UTF-8
// ============================================================

static std::string WideToUtf8(
    const wchar_t* text)
{
    if (!text)
        return {};

    const int len =
        static_cast<int>(
            wcslen(text));

    if (len <= 0)
        return {};

    const int bytes =
        WideCharToMultiByte(
            CP_UTF8,
            0,
            text,
            len,
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
        text,
        len,
        result.data(),
        bytes,
        nullptr,
        nullptr);

    return result;
}


// ============================================================
// UTF-8 -> UTF-16
// ============================================================

static std::wstring Utf8ToWide(
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
// LOAD FONT MAPPINGS
// ============================================================

static void LoadFontMappings(
    bool logResult)
{
    std::wifstream file(
        g_fontsIniPath);

    if (!file)
    {
        if (logResult)
        {
            Log(
                L"Font INI not found: " +
                g_fontsIniPath);
        }

        return;
    }

    auto next =
        std::make_shared<FontMappingList>();

    std::wstring line;

    while (
        std::getline(
            file,
            line))
    {
        line =
            Trim(line);

        if (line.empty())
            continue;

        if (line[0] == L';')
            continue;

        const size_t p1 =
            line.find(L'|');

        if (
            p1 ==
            std::wstring::npos)
        {
            continue;
        }

        const size_t p2 =
            line.find(
                L'|',
                p1 + 1);

        if (
            p2 ==
            std::wstring::npos)
        {
            continue;
        }

        const std::wstring enabledText =
            Trim(
                line.substr(
                    0,
                    p1));

        const std::wstring source =
            Trim(
                line.substr(
                    p1 + 1,
                    p2 - p1 - 1));

        const std::wstring destination =
            Trim(
                line.substr(
                    p2 + 1));

        FontMapping m{};

        m.source =
            WideToUtf8(
                source.c_str());

        m.destination =
            WideToUtf8(
                destination.c_str());

        m.enabled =
            enabledText == L"1" ||
            enabledText == L"true" ||
            enabledText == L"TRUE";

        if (
            !m.source.empty() &&
            !m.destination.empty())
        {
            next->push_back(
                std::move(m));
        }
    }

    std::atomic_store_explicit(
        &g_fontMappings,
        std::static_pointer_cast<
        const FontMappingList>(
            next),
        std::memory_order_release);

    if (logResult)
    {
        Log(
            L"Font mappings loaded: " +
            std::to_wstring(
                next->size()));

        for (
            const auto& m :
            *next)
        {
            Log(
                L"FONT: " +
                Utf8ToWide(
                    m.source) +
                L" -> " +
                Utf8ToWide(
                    m.destination));
        }
    }
}


// ============================================================
// FIND FONT REPLACEMENT
// ============================================================

static const std::string*
FindFontReplacement(
    const char* family)
{
    if (!family)
        return nullptr;

    auto mappings =
        std::atomic_load_explicit(
            &g_fontMappings,
            std::memory_order_acquire);

    for (
        const FontMapping& m :
        *mappings)
    {
        if (!m.enabled)
            continue;

        if (
            _stricmp(
                family,
                m.source.c_str()) == 0)
        {
            return &m.destination;
        }
    }

    return nullptr;
}


// ============================================================
// FONT HOOK
// ============================================================

static void __cdecl
HookSelectFontFace(
    cairo_t* cr,
    const char* family,
    int slant,
    int weight)
{
    if (!RealSelectFontFace)
        return;

    const std::string* replacement =
        IsModMode()
        ? FindFontReplacement(family)
        : nullptr;

    if (replacement)
    {
        Log(
            L"FONT REPLACE: " +
            Utf8ToWide(
                family ? family : "") +
            L" -> " +
            Utf8ToWide(
                *replacement));

        RealSelectFontFace(
            cr,
            replacement->c_str(),
            slant,
            weight);

        return;
    }

    RealSelectFontFace(
        cr,
        family,
        slant,
        weight);
}


// ============================================================
// TOY FONT FACE CREATE HOOK
// ============================================================

static void*
__cdecl
HookToyFontFaceCreate(
    const char* family,
    int slant,
    int weight)
{
    if (!RealToyFontFaceCreate)
        return nullptr;

    const std::string* replacement =
        IsModMode()
        ? FindFontReplacement(family)
        : nullptr;

    if (replacement)
    {
        Log(
            L"FONT CREATE REPLACE: " +
            Utf8ToWide(
                family ? family : "") +
            L" -> " +
            Utf8ToWide(
                *replacement));

        return
            RealToyFontFaceCreate(
                replacement->c_str(),
                slant,
                weight);
    }

    return
        RealToyFontFaceCreate(
            family,
            slant,
            weight);
}


static int HexDigit(
    wchar_t c)
{
    if (
        c >= L'0' &&
        c <= L'9')
    {
        return c - L'0';
    }

    if (
        c >= L'a' &&
        c <= L'f')
    {
        return c - L'a' + 10;
    }

    if (
        c >= L'A' &&
        c <= L'F')
    {
        return c - L'A' + 10;
    }

    return -1;
}


static bool ParseRGB(
    const std::wstring& text,
    int& r,
    int& g,
    int& b)
{
    if (
        text.size() != 7 ||
        text[0] != L'#')
    {
        return false;
    }

    int d[6]{};

    for (int i = 0; i < 6; ++i)
    {
        d[i] =
            HexDigit(
                text[
                    static_cast<size_t>(
                        i + 1)]);

        if (d[i] < 0)
            return false;
    }

    r = d[0] * 16 + d[1];
    g = d[2] * 16 + d[3];
    b = d[4] * 16 + d[5];

    return true;
}


// ============================================================
// LOAD MAPPINGS
// ============================================================

static bool LoadMappings(
    bool logResult)
{
    std::wifstream file(
        g_iniPath);

    if (!file)
    {
        if (logResult)
        {
            Log(
                L"INI not found: " +
                g_iniPath);
        }

        return false;
    }

    auto next =
        std::make_shared<MappingList>();

    std::wstring line;

    while (
        std::getline(
            file,
            line))
    {
        line =
            Trim(line);

        if (line.empty())
            continue;

        if (line[0] == L';')
            continue;

        const size_t p1 =
            line.find(L'|');

        if (
            p1 ==
            std::wstring::npos)
        {
            continue;
        }

        const size_t p2 =
            line.find(
                L'|',
                p1 + 1);

        if (
            p2 ==
            std::wstring::npos)
        {
            continue;
        }

        const std::wstring enabledText =
            Trim(
                line.substr(
                    0,
                    p1));

        const std::wstring srcText =
            Trim(
                line.substr(
                    p1 + 1,
                    p2 - p1 - 1));

        const std::wstring dstText =
            Trim(
                line.substr(
                    p2 + 1));

        int sr, sg, sb;
        int dr, dg, db;

        if (
            !ParseRGB(
                srcText,
                sr,
                sg,
                sb))
        {
            continue;
        }

        if (
            !ParseRGB(
                dstText,
                dr,
                dg,
                db))
        {
            continue;
        }

        Mapping m{};

        m.sr =
            static_cast<uint8_t>(sr);

        m.sg =
            static_cast<uint8_t>(sg);

        m.sb =
            static_cast<uint8_t>(sb);

        m.dr =
            static_cast<uint8_t>(dr);

        m.dg =
            static_cast<uint8_t>(dg);

        m.db =
            static_cast<uint8_t>(db);

        m.enabled =
            enabledText == L"1" ||
            enabledText == L"true" ||
            enabledText == L"TRUE";

        next->push_back(m);
    }

    std::atomic_store_explicit(
        &g_mappings,
        std::static_pointer_cast<
        const MappingList>(
            next),
        std::memory_order_release);

    if (logResult)
    {
        Log(
            L"Mappings loaded: " +
            std::to_wstring(
                next->size()));
    }

    return true;
}


// ============================================================
// COLOR CONVERSION
// ============================================================

static int ToByte(
    double value)
{
    if (value <= 0.0)
        return 0;

    if (value >= 1.0)
        return 255;

    return static_cast<int>(
        std::lround(
            value * 255.0));
}


static double ToUnit(
    int value)
{
    return
        static_cast<double>(value) /
        255.0;
}


// ============================================================
// RUNTIME MODE HELPERS
// ============================================================

static bool IsAnalyzeMode()
{
    return g_mode.load(std::memory_order_acquire) ==
        DarkModMode::ANALYZE;
}

static bool IsModMode()
{
    return g_mode.load(std::memory_order_acquire) ==
        DarkModMode::MOD;
}

static void RecordOriginalSourceColor(
    double r,
    double g,
    double b)
{
    if (!IsAnalyzeMode())
        return;

    const int R = ToByte(r);
    const int G = ToByte(g);
    const int B = ToByte(b);

    const uint32_t index =
        (static_cast<uint32_t>(R >> 3) << 10) |
        (static_cast<uint32_t>(G >> 3) << 5) |
        static_cast<uint32_t>(B >> 3);

    g_sourceHistogram[index].fetch_add(
        1,
        std::memory_order_relaxed);

    g_sourceColorSamples.fetch_add(
        1,
        std::memory_order_relaxed);
}


static std::wstring ColorHex(
    int r,
    int g,
    int b)
{
    std::wstringstream ss;

    ss
        << L"#"
        << std::uppercase
        << std::hex
        << std::setfill(L'0')
        << std::setw(2)
        << r
        << std::setw(2)
        << g
        << std::setw(2)
        << b;

    return ss.str();
}


// ============================================================
// COLOR REPLACEMENT
// ============================================================

// ------------------------------------------------------------
// Mapping matcher.
//
// The old implementation required an exact RGB match. That is
// too strict for Cairo because anti-aliasing, gradients and
// compositing constantly produce nearby shades. Pass 1.1 keeps
// exact matching first, then allows a small RGB distance.
// This is intentionally conservative so unrelated colors are not
// swallowed by a palette entry.
//
// Anti-cascade rule (three-level priority).
//
// The fuzzy radius created a feedback loop: a color WE produced
// could land within matching distance of some rule's source and
// get replaced again. With the real 1C background at #FCFCFC and
// its text at #040404, a palette that maps White -> #313244 and
// Black -> #FFFFFF collides on both ends (#313244 vs the #343434
// panel color is 269, #FFFFFF vs #FCFCFC is only 27 - both inside
// 324). The result was closed cycles such as
//   #313244 -> #FFFFFF together with #FFFFFF -> #313244,
// i.e. the background we had just painted dark being repainted
// white by the rule meant for the original background, and text
// converging onto the background color.
//
// The destination colors are already present in the very list we
// load here, so no extra state is needed. Priority:
//   1. exact source match            -> apply (deliberate rule)
//   2. the color IS one of our own destinations -> no match
//   3. nearest source within radius   -> apply
//
// Level 1 stays ahead of level 2 so an explicit rule is never
// blocked; only the fuzzy step is fenced off. That breaks every
// cycle while leaving anti-alias coverage at radius 18 intact.
// ------------------------------------------------------------
static constexpr int COLOR_MATCH_RADIUS = 18;
static constexpr int COLOR_MATCH_RADIUS_SQ =
COLOR_MATCH_RADIUS * COLOR_MATCH_RADIUS;

// Tolerance for recognizing our own output. Destinations travel
// through double -> byte conversion, so allow for rounding but
// nothing more; this must stay far below COLOR_MATCH_RADIUS_SQ.
static constexpr int DESTINATION_MATCH_TOLERANCE_SQ = 4;

static bool FindColorMapping(
    int R,
    int G,
    int B,
    int& outR,
    int& outG,
    int& outB)
{
    auto mappings =
        std::atomic_load_explicit(
            &g_mappings,
            std::memory_order_acquire);

    const Mapping* nearest = nullptr;
    int nearestDistance =
        COLOR_MATCH_RADIUS_SQ + 1;

    bool isOwnOutput = false;

    for (const Mapping& m : *mappings)
    {
        if (!m.enabled)
            continue;

        const int dr = R - static_cast<int>(m.sr);
        const int dg = G - static_cast<int>(m.sg);
        const int db = B - static_cast<int>(m.sb);

        const int distance =
            dr * dr + dg * dg + db * db;

        // Level 1: exact match wins immediately, ahead of the
        // destination check, so a deliberate rule always applies.
        if (distance == 0)
        {
            outR = m.dr;
            outG = m.dg;
            outB = m.db;
            return true;
        }

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = &m;
        }

        // Level 2 evidence: is the incoming color a color THIS
        // rule set produces? Keep scanning - an exact source match
        // later in the list still outranks this.
        if (!isOwnOutput)
        {
            const int ddr = R - static_cast<int>(m.dr);
            const int ddg = G - static_cast<int>(m.dg);
            const int ddb = B - static_cast<int>(m.db);

            if (ddr * ddr + ddg * ddg + ddb * ddb <=
                DESTINATION_MATCH_TOLERANCE_SQ)
            {
                isOwnOutput = true;
            }
        }
    }

    // Level 2: our own output never feeds back into the matcher.
    if (isOwnOutput)
        return false;

    if (!nearest)
        return false;

    // Level 3: nearest source within the fuzzy radius.
    outR = nearest->dr;
    outG = nearest->dg;
    outB = nearest->db;
    return nearestDistance <= COLOR_MATCH_RADIUS_SQ;
}

static bool ApplyMapping(
    double& r,
    double& g,
    double& b)
{
    if (!IsModMode())
        return false;

    const int R = ToByte(r);
    const int G = ToByte(g);
    const int B = ToByte(b);

    int outR = 0;
    int outG = 0;
    int outB = 0;

    if (!FindColorMapping(
        R, G, B,
        outR, outG, outB))
    {
        return false;
    }

    r = ToUnit(outR);
    g = ToUnit(outG);
    b = ToUnit(outB);

    return true;
}


// ============================================================
// STATISTICS
// ============================================================

static std::atomic<uint64_t>
g_rgbCalls{ 0 };

static std::atomic<uint64_t>
g_rgbaCalls{ 0 };

static std::atomic<uint64_t>
g_patternRgbCalls{ 0 };

static std::atomic<uint64_t>
g_patternRgbaCalls{ 0 };

static std::atomic<uint64_t>
g_colorStopRgbCalls{ 0 };

static std::atomic<uint64_t>
g_colorStopRgbaCalls{ 0 };

static std::atomic<uint64_t>
g_meshRgbCalls{ 0 };

static std::atomic<uint64_t>
g_meshRgbaCalls{ 0 };

static std::atomic<uint64_t>
g_replacements{ 0 };

static std::atomic<uint64_t>
g_surfacePixelReplacements{ 0 };


// ============================================================
// ONE-TIME REPLACEMENT LOG
// ============================================================

static std::mutex g_replaceLogMutex;

static std::array<bool, 32768>
g_replaceSeen{};


static uint32_t ReplacementKey(
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


static void LogReplacementOnce(
    const wchar_t* method,
    int oldR,
    int oldG,
    int oldB,
    int newR,
    int newG,
    int newB)
{
    const uint32_t key =
        ReplacementKey(
            oldR,
            oldG,
            oldB);

    {
        std::lock_guard<
            std::mutex> lock(
                g_replaceLogMutex);

        if (
            g_replaceSeen[key])
        {
            return;
        }

        g_replaceSeen[key] = true;
    }

    Log(
        std::wstring(method) +
        L" " +
        ColorHex(
            oldR,
            oldG,
            oldB) +
        L" -> " +
        ColorHex(
            newR,
            newG,
            newB));
}


// ============================================================
// DIRECT RGB
// ============================================================

static void __cdecl
HookSetSourceRGB(
    cairo_t* cr,
    double r,
    double g,
    double b)
{
    ++g_rgbCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"RGB",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (RealSetSourceRGB)
    {
        RealSetSourceRGB(
            cr,
            r,
            g,
            b);
    }
}


// ============================================================
// DIRECT RGBA
// ============================================================

static void __cdecl
HookSetSourceRGBA(
    cairo_t* cr,
    double r,
    double g,
    double b,
    double a)
{
    ++g_rgbaCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"RGBA",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (RealSetSourceRGBA)
    {
        RealSetSourceRGBA(
            cr,
            r,
            g,
            b,
            a);
    }
}


// ============================================================
// SET SOURCE
// ============================================================
//
// Arbitrary pattern pointer.
//
// We deliberately do not mutate the pattern here because
// cairo_pattern_t ownership/sharing is opaque.
//
// Actual color interception happens while the pattern is built.
// ============================================================

static void __cdecl
HookSetSource(
    cairo_t* cr,
    cairo_pattern_t* pattern)
{
    if (RealSetSource)
    {
        RealSetSource(
            cr,
            pattern);
    }
}


// ============================================================
// SET SOURCE SURFACE
// ============================================================

static void __cdecl
HookSetSourceSurface(
    cairo_t* cr,
    cairo_surface_t* surface,
    double x,
    double y)
{
    if (RealSetSourceSurface)
    {
        RealSetSourceSurface(
            cr,
            surface,
            x,
            y);
    }
}


// ============================================================
// PATTERN RGB
// ============================================================

static cairo_pattern_t* __cdecl
HookPatternCreateRGB(
    double r,
    double g,
    double b)
{
    ++g_patternRgbCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"PATTERN_RGB",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (!RealPatternCreateRGB)
        return nullptr;

    return RealPatternCreateRGB(
        r,
        g,
        b);
}


// ============================================================
// PATTERN RGBA
// ============================================================

static cairo_pattern_t* __cdecl
HookPatternCreateRGBA(
    double r,
    double g,
    double b,
    double a)
{
    ++g_patternRgbaCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"PATTERN_RGBA",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (!RealPatternCreateRGBA)
        return nullptr;

    return RealPatternCreateRGBA(
        r,
        g,
        b,
        a);
}


// ============================================================
// LINEAR PATTERN
// ============================================================

static cairo_pattern_t* __cdecl
HookPatternCreateLinear(
    double x0,
    double y0,
    double x1,
    double y1)
{
    if (!RealPatternCreateLinear)
        return nullptr;

    return RealPatternCreateLinear(
        x0,
        y0,
        x1,
        y1);
}


// ============================================================
// RADIAL PATTERN
// ============================================================

static cairo_pattern_t* __cdecl
HookPatternCreateRadial(
    double cx0,
    double cy0,
    double r0,
    double cx1,
    double cy1,
    double r1)
{
    if (!RealPatternCreateRadial)
        return nullptr;

    return RealPatternCreateRadial(
        cx0,
        cy0,
        r0,
        cx1,
        cy1,
        r1);
}


// ============================================================
// COLOR STOP RGB
// ============================================================

static void __cdecl
HookPatternAddColorStopRGB(
    cairo_pattern_t* pattern,
    double offset,
    double r,
    double g,
    double b)
{
    ++g_colorStopRgbCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"COLOR_STOP_RGB",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (RealPatternAddColorStopRGB)
    {
        RealPatternAddColorStopRGB(
            pattern,
            offset,
            r,
            g,
            b);
    }
}


// ============================================================
// COLOR STOP RGBA
// ============================================================

static void __cdecl
HookPatternAddColorStopRGBA(
    cairo_pattern_t* pattern,
    double offset,
    double r,
    double g,
    double b,
    double a)
{
    ++g_colorStopRgbaCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"COLOR_STOP_RGBA",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (RealPatternAddColorStopRGBA)
    {
        RealPatternAddColorStopRGBA(
            pattern,
            offset,
            r,
            g,
            b,
            a);
    }
}


// ============================================================
// MESH RGB
// ============================================================

static void __cdecl
HookMeshCornerRGB(
    cairo_pattern_t* pattern,
    unsigned int corner,
    double r,
    double g,
    double b)
{
    ++g_meshRgbCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"MESH_RGB",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (RealMeshCornerRGB)
    {
        RealMeshCornerRGB(
            pattern,
            corner,
            r,
            g,
            b);
    }
}


// ============================================================
// MESH RGBA
// ============================================================

static void __cdecl
HookMeshCornerRGBA(
    cairo_pattern_t* pattern,
    unsigned int corner,
    double r,
    double g,
    double b,
    double a)
{
    ++g_meshRgbaCalls;

    RecordOriginalSourceColor(r, g, b);

    const int oldR = ToByte(r);
    const int oldG = ToByte(g);
    const int oldB = ToByte(b);

    if (
        ApplyMapping(
            r,
            g,
            b))
    {
        ++g_replacements;

        LogReplacementOnce(
            L"MESH_RGBA",
            oldR,
            oldG,
            oldB,
            ToByte(r),
            ToByte(g),
            ToByte(b));
    }

    if (RealMeshCornerRGBA)
    {
        RealMeshCornerRGBA(
            pattern,
            corner,
            r,
            g,
            b,
            a);
    }
}


// ============================================================
// SURFACE ANALYSIS
// ============================================================

static std::mutex
g_surfaceMutex;

static std::unordered_map<
    cairo_surface_t*,
    ULONGLONG>
    g_surfaceLastScan;
static void ResetAnalysisState()
{
    for (auto& bin : g_sourceHistogram)
    {
        bin.store(
            0,
            std::memory_order_relaxed);
    }

    g_sourceColorSamples.store(
        0,
        std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(
            g_surfaceMutex);

        g_surfaceLastScan.clear();
    }

    // Drop the per-surface baselines too. They are only valid for
    // the rule set that was active when they were taken; keeping
    // them across a mode or palette change is what froze panels in
    // the previous palette.
    ClearOriginalSurfaceCache();

    g_analysisResetRequested.store(
        true,
        std::memory_order_release);
}




static void AnalyzeCairoImageSurface(
    cairo_surface_t* surface)
{
    if (!surface)
        return;

    if (
        !RealSurfaceGetType ||
        !RealImageGetData ||
        !RealImageGetWidth ||
        !RealImageGetHeight ||
        !RealImageGetStride ||
        !RealImageGetFormat)
    {
        return;
    }

    if (
        RealSurfaceGetType(surface) !=
        CAIRO_SURFACE_TYPE_IMAGE)
    {
        return;
    }

    const ULONGLONG now =
        GetTickCount64();

    {
        std::lock_guard<
            std::mutex> lock(
                g_surfaceMutex);

        auto it =
            g_surfaceLastScan.find(
                surface);

        if (
            it !=
            g_surfaceLastScan.end())
        {
            if (
                now - it->second <
                1000)
            {
                return;
            }

            it->second = now;
        }
        else
        {
            g_surfaceLastScan.emplace(
                surface,
                now);
        }
    }

    unsigned char* data =
        RealImageGetData(
            surface);

    const int width =
        RealImageGetWidth(
            surface);

    const int height =
        RealImageGetHeight(
            surface);

    const int stride =
        RealImageGetStride(
            surface);

    const cairo_format_t format =
        RealImageGetFormat(
            surface);

    if (
        !data ||
        width <= 0 ||
        height <= 0 ||
        stride <= 0)
    {
        return;
    }

    if (
        format !=
        CAIRO_FORMAT_ARGB32 &&
        format !=
        CAIRO_FORMAT_RGB24)
    {
        return;
    }

    //
    // Nothing is sampled here any more.
    //
    // This used to walk up to 250k pixels per surface and then throw
    // every value away: the loop body ended in (void)r/(void)g/(void)b
    // and fed no histogram. darkmod_colors.json comes from the GDI
    // capture in RunScreenAnalysis, not from this function. The walk
    // also ran in MOD, where it was pure overhead on every flush.
    //
    // The per-surface throttle above is kept so the surface set stays
    // observable, and the validation is kept so the hook remains a
    // safe place to reintroduce renderer-side sampling later.
    //
}


// ============================================================
// SURFACE FLUSH HOOK
// ============================================================

// ------------------------------------------------------------
// Original (unmutated) pixel cache.
//
// ApplyMappingToImageSurface writes replacement colors directly
// into the live surface buffer. Without remembering what was
// there originally, a second pass (triggered by any later rule
// change) would run FindColorMapping over pixels that are
// already a REPLACEMENT color from a previous pass, not the
// color the app actually drew. If that replacement color happens
// to match another rule's source - which is common, since plain
// black/white get reused everywhere - it gets replaced again,
// and unrelated elements that merely share a literal RGB value
// end up converging onto the same color over repeated redraws.
//
// Fix: snapshot the untouched pixels the first time we see a
// given surface, and always remap FROM that snapshot INTO the
// live buffer. Every pass then reflects "original artwork + the
// CURRENT rule set", never "previous output + new rules", so
// repeated flushes are idempotent and edits stop bleeding into
// content that was already on screen.
//
// The snapshot must be taken in ANALYZE mode. Taking it lazily on
// the first MOD flush was the flaw: by then the vector hooks
// (cairo_set_source_*) have already substituted colors into that
// same buffer, so the "original artwork" could itself be our own
// output. Remapping on top of it is what left panels stuck in the
// previous palette.
//
// capturedInAnalyze records which side of that line a snapshot
// came from. Only an ANALYZE snapshot is genuine original artwork
// and safe to remap. A surface first seen during MOD has already
// been recolored by the vector path, which is the correct result
// on its own, so it is recorded and then left untouched.
//
// Note: a snapshot describes the content a surface had when it was
// taken. This assumes 1C does not reuse one long-lived surface for
// substantially different content, which matches the fact that
// HookSurfaceDestroy sees surfaces come and go regularly.
// ------------------------------------------------------------

struct OriginalSurfaceData
{
    int width = 0;
    int height = 0;
    int stride = 0;
    bool capturedInAnalyze = false;
    std::vector<unsigned char> pixels;
};

static std::mutex
g_originalPixelsMutex;

static std::unordered_map<
    cairo_surface_t*,
    OriginalSurfaceData>
    g_originalPixels;

static void ClearOriginalSurfaceCache()
{
    std::lock_guard<std::mutex> lock(
        g_originalPixelsMutex);

    g_originalPixels.clear();
}

static void ApplyMappingToImageSurface(
    cairo_surface_t* surface)
{
    if (!surface)
        return;

    const bool modMode = IsModMode();

    if (!RealSurfaceGetType ||
        !RealImageGetData ||
        !RealImageGetWidth ||
        !RealImageGetHeight ||
        !RealImageGetStride ||
        !RealImageGetFormat)
    {
        return;
    }

    if (RealSurfaceGetType(surface) !=
        CAIRO_SURFACE_TYPE_IMAGE)
    {
        return;
    }

    unsigned char* data =
        RealImageGetData(surface);

    const int width =
        RealImageGetWidth(surface);
    const int height =
        RealImageGetHeight(surface);
    const int stride =
        RealImageGetStride(surface);
    const cairo_format_t format =
        RealImageGetFormat(surface);

    if (!data || width <= 0 || height <= 0 || stride <= 0)
        return;

    if (format != CAIRO_FORMAT_ARGB32 &&
        format != CAIRO_FORMAT_RGB24)
    {
        return;
    }

    // Do not allow an unusually large image surface to stall the
    // renderer. Normal 1C UI surfaces are far below this limit.
    const uint64_t totalPixels =
        static_cast<uint64_t>(width) *
        static_cast<uint64_t>(height);

    if (totalPixels > 2000000ull)
        return;

    const size_t bufferSize =
        static_cast<size_t>(stride) *
        static_cast<size_t>(height);

    std::lock_guard<std::mutex> lock(
        g_originalPixelsMutex);

    auto cached =
        g_originalPixels.find(
            surface);

    const bool sizeChanged =
        cached != g_originalPixels.end() &&
        (cached->second.width != width ||
            cached->second.height != height ||
            cached->second.stride != stride);

    if (!modMode)
    {
        // ANALYZE: nothing has been substituted, so the live buffer
        // IS the original artwork. Keep the baseline in step with it
        // and write nothing back.
        if (cached == g_originalPixels.end() || sizeChanged)
        {
            OriginalSurfaceData snapshot;

            snapshot.width = width;
            snapshot.height = height;
            snapshot.stride = stride;
            snapshot.capturedInAnalyze = true;

            snapshot.pixels.assign(
                data,
                data + bufferSize);

            g_originalPixels.insert_or_assign(
                surface,
                std::move(snapshot));
        }
        else
        {
            cached->second.capturedInAnalyze = true;

            cached->second.pixels.assign(
                data,
                data + bufferSize);
        }

        return;
    }

    // MOD from here on.

    if (cached == g_originalPixels.end() || sizeChanged)
    {
        // First sighting happened while MOD was already active, so
        // the vector hooks have had their say and this buffer is
        // NOT original artwork. Record that fact and leave the
        // pixels alone - the vector path already produced the right
        // colors, and remapping them again is exactly the cascade
        // that turned a darkened background back to white.
        OriginalSurfaceData snapshot;

        snapshot.width = width;
        snapshot.height = height;
        snapshot.stride = stride;
        snapshot.capturedInAnalyze = false;

        snapshot.pixels.assign(
            data,
            data + bufferSize);

        g_originalPixels.insert_or_assign(
            surface,
            std::move(snapshot));

        return;
    }

    if (!cached->second.capturedInAnalyze)
        return;

    const unsigned char* original =
        cached->second.pixels.data();

    for (int y = 0; y < height; ++y)
    {
        unsigned char* row =
            data + static_cast<size_t>(y) *
            static_cast<size_t>(stride);

        const unsigned char* originalRow =
            original + static_cast<size_t>(y) *
            static_cast<size_t>(stride);

        for (int x = 0; x < width; ++x)
        {
            unsigned char* px =
                row + static_cast<size_t>(x) * 4;

            const unsigned char* originalPx =
                originalRow + static_cast<size_t>(x) * 4;

            if (format == CAIRO_FORMAT_ARGB32 && originalPx[3] == 0)
                continue;

            const int oldB = originalPx[0];
            const int oldG = originalPx[1];
            const int oldR = originalPx[2];

            int newR = 0;
            int newG = 0;
            int newB = 0;

            if (!FindColorMapping(
                oldR, oldG, oldB,
                newR, newG, newB))
            {
                // No rule matches the ORIGINAL color right now.
                // Make sure the live pixel reflects the original
                // too, in case an earlier (now changed or removed)
                // rule had previously painted over it.
                px[0] = originalPx[0];
                px[1] = originalPx[1];
                px[2] = originalPx[2];

                continue;
            }

            px[0] = static_cast<unsigned char>(newB);
            px[1] = static_cast<unsigned char>(newG);
            px[2] = static_cast<unsigned char>(newR);

            ++g_surfacePixelReplacements;
        }
    }
}

static void __cdecl
HookSurfaceFlush(
    cairo_surface_t* surface)
{
    // Important: run BEFORE the real flush.
    //
    // In MOD this catches UI elements that are already rasterized
    // into a Cairo image and therefore never call
    // cairo_set_source_* again. In ANALYZE it captures the clean
    // baseline those replacements will later be derived from.
    ApplyMappingToImageSurface(surface);

    if (RealSurfaceFlush)
    {
        RealSurfaceFlush(
            surface);
    }

    AnalyzeCairoImageSurface(
        surface);
}


// ============================================================
// SURFACE DESTROY
// ============================================================

static unsigned int __cdecl
HookSurfaceDestroy(
    cairo_surface_t* surface)
{
    {
        std::lock_guard<
            std::mutex> lock(
                g_surfaceMutex);

        g_surfaceLastScan.erase(
            surface);
    }

    {
        std::lock_guard<
            std::mutex> lock(
                g_originalPixelsMutex);

        g_originalPixels.erase(
            surface);
    }

    if (RealSurfaceDestroy)
    {
        return RealSurfaceDestroy(
            surface);
    }

    return 0;
}


// ============================================================
// DETOURS HELPER
// ============================================================

static bool Attach(
    PVOID* target,
    PVOID hook)
{
    if (
        !target ||
        !*target)
    {
        return false;
    }

    return
        DetourAttach(
            target,
            hook) ==
        NO_ERROR;
}


// ============================================================
// RESOLVE
// ============================================================

static void* Resolve(
    HMODULE module,
    const char* name)
{
    if (!module)
        return nullptr;

    return reinterpret_cast<void*>(
        GetProcAddress(
            module,
            name));
}


// ============================================================
// INSTALL CAIRO
// ============================================================

static bool InstallCairo()
{
    HMODULE cairo =
        GetModuleHandleW(
            L"cairo.dll");

    if (!cairo)
    {
        cairo =
            GetModuleHandleW(
                L"cairo-2.dll");
    }

    if (!cairo)
        return false;

    Log(
        L"Cairo found.");

    //
    // Resolve everything.
    //
    // ------------------------------------------------------------
    // FONT
    // ------------------------------------------------------------

    RealSelectFontFace =
        reinterpret_cast<
        PFN_cairo_select_font_face>(
            Resolve(
                cairo,
                "cairo_select_font_face"));

    RealToyFontFaceCreate =
        reinterpret_cast<
        PFN_cairo_toy_font_face_create>(
            Resolve(
                cairo,
                "cairo_toy_font_face_create"));

    // ------------------------------------------------------------
    // SOURCE
    // ------------------------------------------------------------

    RealSetSourceRGB =
        reinterpret_cast<
        PFN_cairo_set_source_rgb>(
            Resolve(
                cairo,
                "cairo_set_source_rgb"));

    RealSetSourceRGBA =
        reinterpret_cast<
        PFN_cairo_set_source_rgba>(
            Resolve(
                cairo,
                "cairo_set_source_rgba"));

    RealSetSource =
        reinterpret_cast<
        PFN_cairo_set_source>(
            Resolve(
                cairo,
                "cairo_set_source"));

    RealSetSourceSurface =
        reinterpret_cast<
        PFN_cairo_set_source_surface>(
            Resolve(
                cairo,
                "cairo_set_source_surface"));

    RealPatternCreateRGB =
        reinterpret_cast<
        PFN_cairo_pattern_create_rgb>(
            Resolve(
                cairo,
                "cairo_pattern_create_rgb"));

    RealPatternCreateRGBA =
        reinterpret_cast<
        PFN_cairo_pattern_create_rgba>(
            Resolve(
                cairo,
                "cairo_pattern_create_rgba"));

    RealPatternCreateLinear =
        reinterpret_cast<
        PFN_cairo_pattern_create_linear>(
            Resolve(
                cairo,
                "cairo_pattern_create_linear"));

    RealPatternCreateRadial =
        reinterpret_cast<
        PFN_cairo_pattern_create_radial>(
            Resolve(
                cairo,
                "cairo_pattern_create_radial"));

    RealPatternAddColorStopRGB =
        reinterpret_cast<
        PFN_cairo_pattern_add_color_stop_rgb>(
            Resolve(
                cairo,
                "cairo_pattern_add_color_stop_rgb"));

    RealPatternAddColorStopRGBA =
        reinterpret_cast<
        PFN_cairo_pattern_add_color_stop_rgba>(
            Resolve(
                cairo,
                "cairo_pattern_add_color_stop_rgba"));

    RealMeshCornerRGB =
        reinterpret_cast<
        PFN_cairo_mesh_pattern_set_corner_color_rgb>(
            Resolve(
                cairo,
                "cairo_mesh_pattern_set_corner_color_rgb"));

    RealMeshCornerRGBA =
        reinterpret_cast<
        PFN_cairo_mesh_pattern_set_corner_color_rgba>(
            Resolve(
                cairo,
                "cairo_mesh_pattern_set_corner_color_rgba"));

    RealSurfaceFlush =
        reinterpret_cast<
        PFN_cairo_surface_flush>(
            Resolve(
                cairo,
                "cairo_surface_flush"));

    RealSurfaceGetType =
        reinterpret_cast<
        PFN_cairo_surface_get_type>(
            Resolve(
                cairo,
                "cairo_surface_get_type"));

    RealImageGetData =
        reinterpret_cast<
        PFN_cairo_image_surface_get_data>(
            Resolve(
                cairo,
                "cairo_image_surface_get_data"));

    RealImageGetWidth =
        reinterpret_cast<
        PFN_cairo_image_surface_get_width>(
            Resolve(
                cairo,
                "cairo_image_surface_get_width"));

    RealImageGetHeight =
        reinterpret_cast<
        PFN_cairo_image_surface_get_height>(
            Resolve(
                cairo,
                "cairo_image_surface_get_height"));

    RealImageGetStride =
        reinterpret_cast<
        PFN_cairo_image_surface_get_stride>(
            Resolve(
                cairo,
                "cairo_image_surface_get_stride"));

    RealImageGetFormat =
        reinterpret_cast<
        PFN_cairo_image_surface_get_format>(
            Resolve(
                cairo,
                "cairo_image_surface_get_format"));

    RealSurfaceDestroy =
        reinterpret_cast<
        PFN_cairo_surface_destroy>(
            Resolve(
                cairo,
                "cairo_surface_destroy"));

    LONG result =
        DetourTransactionBegin();

    if (result != NO_ERROR)
    {
        Log(
            L"DetourTransactionBegin failed: " +
            std::to_wstring(result));

        return false;
    }

    result =
        DetourUpdateThread(
            GetCurrentThread());

    if (result != NO_ERROR)
    {
        DetourTransactionAbort();

        Log(
            L"DetourUpdateThread failed: " +
            std::to_wstring(result));

        return false;
    }

    int attached = 0;
    // ------------------------------------------------------------
    // FONT HOOKS
    // ------------------------------------------------------------

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSelectFontFace),
            reinterpret_cast<PVOID>(
                HookSelectFontFace)))
    {
        ++attached;

        Log(
            L"Hook: cairo_select_font_face");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealToyFontFaceCreate),
            reinterpret_cast<PVOID>(
                HookToyFontFaceCreate)))
    {
        ++attached;

        Log(
            L"Hook: cairo_toy_font_face_create");
    }
    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSetSourceRGB),
            reinterpret_cast<PVOID>(
                HookSetSourceRGB)))
    {
        ++attached;
        Log(
            L"Hook: cairo_set_source_rgb");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSetSourceRGBA),
            reinterpret_cast<PVOID>(
                HookSetSourceRGBA)))
    {
        ++attached;
        Log(
            L"Hook: cairo_set_source_rgba");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSetSource),
            reinterpret_cast<PVOID>(
                HookSetSource)))
    {
        ++attached;
        Log(
            L"Hook: cairo_set_source");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSetSourceSurface),
            reinterpret_cast<PVOID>(
                HookSetSourceSurface)))
    {
        ++attached;
        Log(
            L"Hook: cairo_set_source_surface");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealPatternCreateRGB),
            reinterpret_cast<PVOID>(
                HookPatternCreateRGB)))
    {
        ++attached;
        Log(
            L"Hook: cairo_pattern_create_rgb");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealPatternCreateRGBA),
            reinterpret_cast<PVOID>(
                HookPatternCreateRGBA)))
    {
        ++attached;
        Log(
            L"Hook: cairo_pattern_create_rgba");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealPatternCreateLinear),
            reinterpret_cast<PVOID>(
                HookPatternCreateLinear)))
    {
        ++attached;
        Log(
            L"Hook: cairo_pattern_create_linear");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealPatternCreateRadial),
            reinterpret_cast<PVOID>(
                HookPatternCreateRadial)))
    {
        ++attached;
        Log(
            L"Hook: cairo_pattern_create_radial");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealPatternAddColorStopRGB),
            reinterpret_cast<PVOID>(
                HookPatternAddColorStopRGB)))
    {
        ++attached;
        Log(
            L"Hook: cairo_pattern_add_color_stop_rgb");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealPatternAddColorStopRGBA),
            reinterpret_cast<PVOID>(
                HookPatternAddColorStopRGBA)))
    {
        ++attached;
        Log(
            L"Hook: cairo_pattern_add_color_stop_rgba");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealMeshCornerRGB),
            reinterpret_cast<PVOID>(
                HookMeshCornerRGB)))
    {
        ++attached;
        Log(
            L"Hook: cairo_mesh_pattern_set_corner_color_rgb");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealMeshCornerRGBA),
            reinterpret_cast<PVOID>(
                HookMeshCornerRGBA)))
    {
        ++attached;
        Log(
            L"Hook: cairo_mesh_pattern_set_corner_color_rgba");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSurfaceFlush),
            reinterpret_cast<PVOID>(
                HookSurfaceFlush)))
    {
        ++attached;
        Log(
            L"Hook: cairo_surface_flush");
    }

    if (
        Attach(
            reinterpret_cast<PVOID*>(
                &RealSurfaceDestroy),
            reinterpret_cast<PVOID>(
                HookSurfaceDestroy)))
    {
        ++attached;
        Log(
            L"Hook: cairo_surface_destroy");
    }

    result =
        DetourTransactionCommit();

    if (result != NO_ERROR)
    {
        Log(
            L"DetourTransactionCommit failed: " +
            std::to_wstring(result));

        return false;
    }

    Log(
        L"Alpha 0.4 Pass 1.1 Cairo hooks installed: " +
        std::to_wstring(attached));

    return attached > 0;
}


// ============================================================
// WINDOW SEARCH
// ============================================================

struct WindowSearch
{
    DWORD pid = 0;
    HWND hwnd = nullptr;
    long long area = -1;
};


static BOOL CALLBACK EnumWindowsProc(
    HWND hwnd,
    LPARAM param)
{
    auto* search =
        reinterpret_cast<
        WindowSearch*>(
            param);

    DWORD pid = 0;

    GetWindowThreadProcessId(
        hwnd,
        &pid);

    if (pid != search->pid)
        return TRUE;

    if (!IsWindowVisible(hwnd))
        return TRUE;

    if (GetWindow(hwnd, GW_OWNER))
        return TRUE;

    RECT rc{};

    if (
        !GetClientRect(
            hwnd,
            &rc))
    {
        return TRUE;
    }

    const long long w =
        std::max<long>(
            0,
            rc.right - rc.left);

    const long long h =
        std::max<long>(
            0,
            rc.bottom - rc.top);

    const long long area =
        w * h;

    if (area > search->area)
    {
        search->area = area;
        search->hwnd = hwnd;
    }

    return TRUE;
}


static HWND FindMainWindow()
{
    WindowSearch search{};

    search.pid =
        GetCurrentProcessId();

    EnumWindows(
        EnumWindowsProc,
        reinterpret_cast<LPARAM>(
            &search));

    return search.hwnd;
}


// ============================================================
// SCREEN CAPTURE
// ============================================================

struct CapturedScreen
{
    int width = 0;
    int height = 0;

    //
    // BGRA, 4 bytes/pixel.
    //

    std::vector<uint8_t> pixels;
};


// ============================================================
// CAPTURE WITH PRINTWINDOW FIRST
// ============================================================

static bool CaptureClient(
    HWND hwnd,
    CapturedScreen& out)
{
    out = {};

    if (
        !hwnd ||
        !IsWindow(hwnd))
    {
        return false;
    }

    RECT rc{};

    if (
        !GetClientRect(
            hwnd,
            &rc))
    {
        return false;
    }

    const int width =
        rc.right - rc.left;

    const int height =
        rc.bottom - rc.top;

    if (
        width <= 0 ||
        height <= 0)
    {
        return false;
    }

    HDC targetDC =
        GetDC(hwnd);

    if (!targetDC)
        return false;

    HDC memoryDC =
        CreateCompatibleDC(
            targetDC);

    if (!memoryDC)
    {
        ReleaseDC(
            hwnd,
            targetDC);

        return false;
    }

    HBITMAP bitmap =
        CreateCompatibleBitmap(
            targetDC,
            width,
            height);

    if (!bitmap)
    {
        DeleteDC(memoryDC);

        ReleaseDC(
            hwnd,
            targetDC);

        return false;
    }

    HGDIOBJ old =
        SelectObject(
            memoryDC,
            bitmap);

    //
    // First try WM_PRINT / PrintWindow.
    //

    BOOL ok =
        PrintWindow(
            hwnd,
            memoryDC,
            PW_CLIENTONLY);

    //
    // Fallback to BitBlt.
    //

    if (!ok)
    {
        ok =
            BitBlt(
                memoryDC,
                0,
                0,
                width,
                height,
                targetDC,
                0,
                0,
                SRCCOPY);
    }

    if (!ok)
    {
        SelectObject(
            memoryDC,
            old);

        DeleteObject(bitmap);
        DeleteDC(memoryDC);
        ReleaseDC(hwnd, targetDC);

        return false;
    }

    BITMAPINFO bmi{};

    bmi.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    bmi.bmiHeader.biWidth =
        width;

    bmi.bmiHeader.biHeight =
        -height;

    bmi.bmiHeader.biPlanes =
        1;

    bmi.bmiHeader.biBitCount =
        32;

    bmi.bmiHeader.biCompression =
        BI_RGB;

    const size_t bytes =
        static_cast<size_t>(width) *
        static_cast<size_t>(height) *
        4;

    out.width = width;
    out.height = height;

    out.pixels.resize(bytes);

    const int result =
        GetDIBits(
            memoryDC,
            bitmap,
            0,
            static_cast<UINT>(height),
            out.pixels.data(),
            &bmi,
            DIB_RGB_COLORS);

    SelectObject(
        memoryDC,
        old);

    DeleteObject(bitmap);
    DeleteDC(memoryDC);
    ReleaseDC(hwnd, targetDC);

    if (
        result != height)
    {
        out = {};
        return false;
    }

    return true;
}


// ============================================================
// SCREEN HISTOGRAM
//
// 5 bits/channel:
//
// 32 * 32 * 32 = 32768 bins
//
// This is deliberately compact and fast.
//
// ============================================================

static constexpr size_t
SCREEN_HISTOGRAM_SIZE =
32768;


struct ScreenHistogram
{
    std::array<uint64_t,
        SCREEN_HISTOGRAM_SIZE>
        bins{};

    uint64_t pixels = 0;
};


static uint32_t ScreenBin(
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


static ScreenHistogram AnalyzeScreen(
    const CapturedScreen& screen)
{
    ScreenHistogram result{};

    if (screen.pixels.empty())
        return result;

    const size_t count =
        screen.pixels.size() / 4;

    //
    // Full scan. For a 1936x1048 window this is
    // about 2 million pixels, which is fine once/sec.
    //

    for (
        size_t i = 0;
        i < count;
        ++i)
    {
        const uint8_t* p =
            &screen.pixels[
                i * 4];

        const int B = p[0];
        const int G = p[1];
        const int R = p[2];

        //
        // DIB is opaque for our use.
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


// ============================================================
// JSON
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


struct ScreenColor
{
    uint64_t pixels = 0;

    int r = 0;
    int g = 0;
    int b = 0;
};


static std::vector<ScreenColor>
BuildTopScreenColors(
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
                    std::min(
                        255,
                        r5 * 8 + 4);

                c.g =
                    std::min(
                        255,
                        g5 * 8 + 4);

                c.b =
                    std::min(
                        255,
                        b5 * 8 + 4);

                c.pixels =
                    pixels;

                result.push_back(c);
            }
        }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const ScreenColor& a,
            const ScreenColor& b)
        {
            return a.pixels >
                b.pixels;
        });

    if (result.size() > limit)
        result.resize(limit);

    return result;
}


static bool WriteColorsJson(
    const CapturedScreen& screen,
    const ScreenHistogram& histogram)
{
    const std::wstring tmpPath =
        g_colorsPath +
        L".tmp";

    std::ofstream file(
        tmpPath,
        std::ios::binary |
        std::ios::trunc);

    if (!file)
        return false;

    SYSTEMTIME st{};
    GetLocalTime(&st);

    file
        << "{\n"
        << "  \"version\": 1,\n"
        << "  \"alpha_version\": \"0.4\",\n"
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
        << GetCurrentProcessId()
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
        << g_replacements.load()
        << ",\n"

        << "  \"mode\": \"ANALYZE\",\n"

        << "  \"colors\": [\n";

    const auto top =
        BuildTopScreenColors(
            histogram,
            100);

    for (
        size_t i = 0;
        i < top.size();
        ++i)
    {
        const auto& c =
            top[i];

        const double percentage =
            histogram.pixels > 0
            ? static_cast<double>(
                c.pixels) *
            100.0 /
            static_cast<double>(
                histogram.pixels)
            : 0.0;

        file
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
            file << ",";
        }

        file << "\n";
    }

    file
        << "  ]\n"
        << "}\n";

    file.flush();

    if (!file)
        return false;

    file.close();

    //
    // Atomic-ish replacement:
    // GUI either sees old complete JSON or new complete JSON.
    //

    if (!MoveFileExW(
        tmpPath.c_str(),
        g_colorsPath.c_str(),
        MOVEFILE_REPLACE_EXISTING |
        MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(
            tmpPath.c_str());

        return false;
    }

    return true;
}


// ============================================================
// SCREEN ANALYZER
//
// One pass every 1000 ms.
//
// ============================================================

static void RunScreenAnalysis()
{
    if (!IsAnalyzeMode())
        return;

    HWND hwnd =
        FindMainWindow();

    if (!hwnd)
        return;

    CapturedScreen screen;

    if (!CaptureClient(
        hwnd,
        screen))
    {
        return;
    }

    ScreenHistogram histogram =
        AnalyzeScreen(
            screen);

    WriteColorsJson(
        screen,
        histogram);
}


// ============================================================
// STATISTICS LOG
// ============================================================

static void WriteStats()
{
    Log(
        L"========== ALPHA 0.4 PASS 1.1 STATS ==========");

    Log(
        L"RGB calls: " +
        std::to_wstring(
            g_rgbCalls.load()));

    Log(
        L"RGBA calls: " +
        std::to_wstring(
            g_rgbaCalls.load()));

    Log(
        L"Pattern RGB calls: " +
        std::to_wstring(
            g_patternRgbCalls.load()));

    Log(
        L"Pattern RGBA calls: " +
        std::to_wstring(
            g_patternRgbaCalls.load()));

    Log(
        L"ColorStop RGB calls: " +
        std::to_wstring(
            g_colorStopRgbCalls.load()));

    Log(
        L"ColorStop RGBA calls: " +
        std::to_wstring(
            g_colorStopRgbaCalls.load()));

    Log(
        L"Mesh RGB calls: " +
        std::to_wstring(
            g_meshRgbCalls.load()));

    Log(
        L"Mesh RGBA calls: " +
        std::to_wstring(
            g_meshRgbaCalls.load()));

    Log(
        L"Replacements: " +
        std::to_wstring(
            g_replacements.load()));

    Log(
        L"Surface pixel replacements: " +
        std::to_wstring(
            g_surfacePixelReplacements.load()));

    Log(
        L"Mode: " +
        std::wstring(
            IsModMode()
            ? L"MOD"
            : L"ANALYZE"));

    Log(
        L"Original renderer color samples: " +
        std::to_wstring(
            g_sourceColorSamples.load()));

    Log(
        L"======================================");
}

// ============================================================
// FILE WRITE TIME
// ============================================================

static ULONGLONG GetFileWriteStamp(
    const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};

    if (!GetFileAttributesExW(
        path.c_str(),
        GetFileExInfoStandard,
        &data))
    {
        return 0;
    }

    ULARGE_INTEGER value{};

    value.HighPart =
        data.ftLastWriteTime.dwHighDateTime;

    value.LowPart =
        data.ftLastWriteTime.dwLowDateTime;

    return value.QuadPart;
}
// ============================================================
// FORCE TARGET WINDOW REDRAW
// ============================================================
//
// Applying a new palette changes renderer input, but an already
// painted 1C window may not repaint immediately. Invalidate the
// current process' visible top-level windows so Cairo gets another
// paint pass without restarting 1C.
//
static void RequestTargetRedraw()
{
    EnumWindows(
        [](HWND hwnd, LPARAM) -> BOOL
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);

            if (pid != GetCurrentProcessId())
                return TRUE;

            if (!IsWindowVisible(hwnd))
                return TRUE;

            RedrawWindow(
                hwnd,
                nullptr,
                nullptr,
                RDW_INVALIDATE |
                RDW_ERASE |
                RDW_ALLCHILDREN);

            return TRUE;
        },
        0);
}

// ============================================================
// WORKER
// ============================================================

static DWORD WINAPI
Worker(
    LPVOID)
{
    g_dllDir =
        GetModuleDir(
            g_self);

    g_iniPath =
        g_dllDir +
        L"\\darkmod.ini";

    g_fontsIniPath =
        g_dllDir +
        L"\\darkmod_fonts.ini";

    g_logPath =
        g_dllDir +
        L"\\darkmod.log";

    g_colorsPath =
        g_dllDir +
        L"\\darkmod_colors.json";

    //
    // IMPORTANT:
    // Every DLL session starts with a clean log.
    //

    g_logHandle =
        CreateFileW(
            g_logPath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ |
            FILE_SHARE_WRITE,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (
        g_logHandle ==
        INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    Log(
        L"========================================");

    Log(
        L"DarkMod Alpha 0.4 / Pass 1.1");

    Log(
        L"Runtime state machine: ANALYZE / MOD");

    Log(
        L"DLL directory: " +
        g_dllDir);

    Log(
        L"INI: " +
        g_iniPath);

    Log(
        L"LOG: " +
        g_logPath);

    Log(
        L"COLORS: " +
        g_colorsPath);

#if defined(_WIN64)

    Log(
        L"Architecture: x64");

#else

    Log(
        L"Architecture: x86");

#endif

    Log(
        L"PID: " +
        std::to_wstring(
            GetCurrentProcessId()));

    Log(
        L"========================================");

    Log(
        L"Initial mode: ANALYZE");

    // Preload configuration for the first APPLY.
    // ApplyMapping() is gated by g_mode, so these values are
    // never used while the DLL is in ANALYZE.
    LoadMappings(true);
    LoadFontMappings(true);

    //
    // Wait for Cairo.
    //

    bool installed = false;

    while (
        g_running &&
        !installed)
    {
        installed =
            InstallCairo();

        if (!installed)
        {
            Sleep(250);
        }
    }

    //
    // Main runtime loop.
    //

    ULONGLONG lastIni =
        GetTickCount64();

    ULONGLONG lastAnalysis = 0;
    ULONGLONG lastStats =
        GetTickCount64();

    while (g_running)
    {
        const ULONGLONG now =
            GetTickCount64();

        // --------------------------------------------------------
        // Explicit APPLY command.
        // The launcher owns the configuration lifecycle.
        // --------------------------------------------------------
        if (
            g_applyRequested.exchange(
                false,
                std::memory_order_acq_rel))
        {
            // Load the new config before switching the hooks into
            // MOD, avoiding a race where old mappings could be used.
            LoadMappings(true);
            LoadFontMappings(true);

            g_mode.store(
                DarkModMode::MOD,
                std::memory_order_release);

            {
                std::lock_guard<std::mutex> lock(
                    g_replaceLogMutex);
                g_replaceSeen.fill(false);
            }

            RequestTargetRedraw();

            Log(
                L"MOD APPLY configuration loaded; mode=MOD; redraw requested.");
        }

        // --------------------------------------------------------
        // ANALYZE mode: one screen analysis approximately per second.
        // --------------------------------------------------------
        if (IsAnalyzeMode())
        {
            if (
                g_analysisResetRequested.exchange(
                    false,
                    std::memory_order_acq_rel))
            {
                lastAnalysis = 0;
            }

            // Skip while the post-MOD quarantine is open: capturing
            // now would record our own output as original colors.
            const ULONGLONG notBefore =
                g_analyzeNotBefore.load(
                    std::memory_order_acquire);

            if (
                now >= notBefore &&
                now - lastAnalysis >=
                1000)
            {
                lastAnalysis = now;
                RunScreenAnalysis();
            }
        }

        // --------------------------------------------------------
        // Statistics.
        // --------------------------------------------------------
        if (
            now - lastStats >=
            10000)
        {
            lastStats = now;

            WriteStats();

            g_replacements.store(
                0);

            g_surfacePixelReplacements.store(
                0);
        }

        Sleep(25);
    }

    Log(
        L"Worker stopping.");

    WriteStats();

    if (
        g_logHandle !=
        INVALID_HANDLE_VALUE)
    {
        CloseHandle(
            g_logHandle);

        g_logHandle =
            INVALID_HANDLE_VALUE;
    }

    return 0;
}




// ============================================================
// MOD CONTROL EXPORTS
// ============================================================

extern "C"
__declspec(dllexport)
DWORD WINAPI
DarkModEnable(
    LPVOID)
{
    if (!g_running.load(std::memory_order_acquire))
        return 0;

    // Worker will load mappings/font mappings, then atomically
    // switch to MOD. No screen analysis happens in MOD.
    g_applyRequested.store(
        true,
        std::memory_order_release);

    Log(
        L"DarkModEnable: APPLY requested.");

    return 1;
}


extern "C"
__declspec(dllexport)
DWORD WINAPI
DarkModRefreshAnalysis(
    LPVOID)
{
    if (!g_running.load(std::memory_order_acquire))
        return 0;

    if (!IsAnalyzeMode())
    {
        Log(L"DarkModRefreshAnalysis: refused because MOD is active.");
        return 0;
    }

    ResetAnalysisState();
    g_analysisResetRequested.store(true, std::memory_order_release);
    Log(L"DarkModRefreshAnalysis: fresh input analysis requested.");
    return 1;
}


extern "C"
__declspec(dllexport)
DWORD WINAPI
DarkModDisable(
    LPVOID)
{
    if (!g_running.load(std::memory_order_acquire))
        return 0;

    // This is deliberately NOT a DLL unload.
    g_mode.store(
        DarkModMode::ANALYZE,
        std::memory_order_release);

    g_replacements.store(0);

    // Hold screen analysis off until 1C has repainted, so the
    // modded frame cannot be recorded as "original colors".
    g_analyzeNotBefore.store(
        GetTickCount64() + ANALYZE_QUARANTINE_MS,
        std::memory_order_release);

    ResetAnalysisState();
    RequestTargetRedraw();

    Log(
        L"DarkModDisable: OFF requested; mode=ANALYZE; redraw requested.");

    return 1;
}


// ============================================================
// LEGACY UNLOAD COMPATIBILITY
// ============================================================
// Older launchers may still call DarkModUnload.
// It is now exactly MOD OFF and never detaches/unloads.
// ============================================================



extern "C"
__declspec(dllexport)
DWORD WINAPI
DarkModUnload(
    LPVOID)
{
    return DarkModDisable(nullptr);
}


// ============================================================
// DLL MAIN
// ============================================================

BOOL APIENTRY
DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID)
{
    if (
        reason ==
        DLL_PROCESS_ATTACH)
    {
        g_self =
            hModule;

        DisableThreadLibraryCalls(
            hModule);

        g_running =
            true;

        g_mode.store(
            DarkModMode::ANALYZE,
            std::memory_order_release);

        g_applyRequested.store(
            false,
            std::memory_order_release);

        g_analysisResetRequested.store(
            true,
            std::memory_order_release);

        g_worker =
            CreateThread(
                nullptr,
                0,
                Worker,
                nullptr,
                0,
                nullptr);
    }
    else if (
        reason ==
        DLL_PROCESS_DETACH)
    {
        g_running =
            false;

        //
        // Do NOT wait here.
        //

        g_worker =
            nullptr;
    }

    return TRUE;
}
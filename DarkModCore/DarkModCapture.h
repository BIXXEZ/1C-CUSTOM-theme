#pragma once

// ============================================================
// DARKMOD CORE - WINDOW CAPTURE
//
// Plain public GDI against an HWND: PrintWindow(PW_CLIENTONLY)
// with a BitBlt fallback, then GetDIBits into a 32bpp top-down
// buffer.
//
// Because it needs nothing but a window handle, Palette Studio can
// take its source snapshot of 1C without injecting anything - it
// is the same capture the DLL already performs, not a second
// incompatible one, so the histogram Studio computes matches the
// one behind darkmod_colors.json.
// ============================================================

#include <windows.h>

#include <cstdint>
#include <vector>

namespace dm
{

    struct CapturedScreen
    {
        int width = 0;
        int height = 0;

        //
        // BGRA, 4 bytes/pixel, top row first.
        //
        std::vector<uint8_t> pixels;

        bool Empty() const
        {
            return
                pixels.empty() ||
                width <= 0 ||
                height <= 0;
        }
    };


    //
    // Largest visible unowned top-level window of a process. The
    // DLL passes GetCurrentProcessId(), Studio passes the pid of
    // 1C - previously this was hardcoded to the current process,
    // which is why it had to move here to be usable from an exe.
    //
    HWND FindMainWindow(
        DWORD pid);

    bool CaptureClient(
        HWND hwnd,
        CapturedScreen& out);

}

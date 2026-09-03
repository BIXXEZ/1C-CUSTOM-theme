// ============================================================
// DARKMOD CORE - WINDOW CAPTURE
// ============================================================

#include "DarkModCapture.h"

#include <algorithm>

namespace dm
{

    // ============================================================
    // MAIN WINDOW SEARCH
    // ============================================================

    namespace
    {

        struct WindowSearch
        {
            DWORD pid = 0;
            HWND hwnd = nullptr;
            long long area = 0;
        };


        BOOL CALLBACK EnumWindowsProc(
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

            //
            // Owned windows are dialogs and popups; the main frame
            // is the one nothing owns.
            //

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
                (std::max<long>)(
                    0,
                    rc.right - rc.left);

            const long long h =
                (std::max<long>)(
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

    }


    HWND FindMainWindow(
        DWORD pid)
    {
        if (!pid)
            return nullptr;

        WindowSearch search{};

        search.pid = pid;

        EnumWindows(
            EnumWindowsProc,
            reinterpret_cast<LPARAM>(
                &search));

        return search.hwnd;
    }


    // ============================================================
    // CAPTURE
    // ============================================================

    bool CaptureClient(
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
        // PrintWindow asks the window to redraw itself, which also
        // works when it is partly covered. BitBlt only copies what
        // is actually on screen, so it is the fallback.
        //

        BOOL ok =
            PrintWindow(
                hwnd,
                memoryDC,
                PW_CLIENTONLY);

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

        //
        // Negative height = top-down rows, so pixel (0,0) is the
        // top-left corner and index math needs no flip.
        //

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

        if (result != height)
        {
            out = {};
            return false;
        }

        return true;
    }

}

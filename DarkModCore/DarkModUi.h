#pragma once

// ============================================================
// DARKMOD CORE - UI PRIMITIVES
//
// Only the stateless GDI helpers. Every DarkMod window is drawn
// by hand in WM_PAINT, and both the launcher and Palette Studio
// draw the same rounded panels, so these live here to keep the
// two looking like one product.
//
// Nothing here touches global window state - the launcher's
// hover/press bookkeeping and its ClientMousePos() stay in the
// launcher, because they depend on its own g_hwnd.
// ============================================================

#include "DarkModColor.h"

#include <string>

namespace dm
{

    void FillRectColor(
        HDC hdc,
        const RECT& rc,
        Color c);

    void DrawTextColor(
        HDC hdc,
        const std::wstring& text,
        const RECT& rc,
        Color color,
        UINT format);

    //
    // Filled rounded rectangle. Pen and brush are the same color on
    // purpose: GDI draws the border with the pen, and a mismatch
    // shows up as a one-pixel outline of the wrong shade.
    //
    void RoundedRect(
        HDC hdc,
        const RECT& rc,
        int radius,
        Color fill);

    void DrawCircle(
        HDC hdc,
        const RECT& rc,
        Color fill);

    //
    // Outline only, for the analysis overlay. Not used by the
    // launcher.
    //
    void RectOutline(
        HDC hdc,
        const RECT& rc,
        int thickness,
        Color color);

    bool RectContains(
        const RECT& rc,
        int x,
        int y);

    inline RECT MakeRect(
        int left,
        int top,
        int right,
        int bottom)
    {
        RECT rc{};

        rc.left = left;
        rc.top = top;
        rc.right = right;
        rc.bottom = bottom;

        return rc;
    }

}

// ============================================================
// DARKMOD CORE - UI PRIMITIVES
// ============================================================

#include "DarkModUi.h"

namespace dm
{

    void FillRectColor(
        HDC hdc,
        const RECT& rc,
        Color c)
    {
        HBRUSH brush =
            CreateSolidBrush(
                ToColorRef(c));

        FillRect(
            hdc,
            &rc,
            brush);

        DeleteObject(brush);
    }


    void DrawTextColor(
        HDC hdc,
        const std::wstring& text,
        const RECT& rc,
        Color color,
        UINT format)
    {
        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            ToColorRef(color));

        DrawTextW(
            hdc,
            text.c_str(),
            -1,
            const_cast<RECT*>(
                &rc),
            format);
    }


    void RoundedRect(
        HDC hdc,
        const RECT& rc,
        int radius,
        Color fill)
    {
        HBRUSH brush =
            CreateSolidBrush(
                ToColorRef(fill));

        HPEN pen =
            CreatePen(
                PS_SOLID,
                1,
                ToColorRef(fill));

        HGDIOBJ oldBrush =
            SelectObject(
                hdc,
                brush);

        HGDIOBJ oldPen =
            SelectObject(
                hdc,
                pen);

        RoundRect(
            hdc,
            rc.left,
            rc.top,
            rc.right,
            rc.bottom,
            radius,
            radius);

        SelectObject(
            hdc,
            oldBrush);

        SelectObject(
            hdc,
            oldPen);

        DeleteObject(brush);
        DeleteObject(pen);
    }


    void DrawCircle(
        HDC hdc,
        const RECT& rc,
        Color fill)
    {
        HBRUSH brush =
            CreateSolidBrush(
                ToColorRef(fill));

        HPEN pen =
            CreatePen(
                PS_SOLID,
                1,
                ToColorRef(fill));

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
            rc.left,
            rc.top,
            rc.right,
            rc.bottom);

        SelectObject(
            hdc,
            oldBrush);

        SelectObject(
            hdc,
            oldPen);

        DeleteObject(brush);
        DeleteObject(pen);
    }


    void RectOutline(
        HDC hdc,
        const RECT& rc,
        int thickness,
        Color color)
    {
        if (thickness <= 0)
            return;

        HBRUSH brush =
            CreateSolidBrush(
                ToColorRef(color));

        RECT edge{};

        //
        // Four filled strips rather than a pen: a pen thicker than
        // one pixel centers itself on the path, so the frame would
        // creep outside the rectangle it is supposed to mark.
        //

        edge = rc;
        edge.bottom = rc.top + thickness;
        FillRect(hdc, &edge, brush);

        edge = rc;
        edge.top = rc.bottom - thickness;
        FillRect(hdc, &edge, brush);

        edge = rc;
        edge.top += thickness;
        edge.bottom -= thickness;
        edge.right = rc.left + thickness;
        FillRect(hdc, &edge, brush);

        edge = rc;
        edge.top += thickness;
        edge.bottom -= thickness;
        edge.left = rc.right - thickness;
        FillRect(hdc, &edge, brush);

        DeleteObject(brush);
    }


    bool RectContains(
        const RECT& rc,
        int x,
        int y)
    {
        POINT p{
            x,
            y
        };

        return
            PtInRect(
                &rc,
                p) != FALSE;
    }

}

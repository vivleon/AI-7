using System.Drawing;
using System.Drawing.Drawing2D;
using Snake.Shared;

namespace Snake.Client.WinForms.Rendering;

public static class SkinPainter
{
    static Brush BodyBrush(SnakeSkin skin)
        => new SolidBrush(ColorTranslator.FromHtml(skin.BodyColorHex));

    static string Norm(string? s) => string.IsNullOrWhiteSpace(s) ? "none" : s.ToLowerInvariant();

    public static void PaintCell(Graphics g, Rectangle cell, SnakeSkin skin, bool head = false)
    {
        using var b = BodyBrush(skin);
        g.FillRectangle(b, cell);

        var pattern = Norm(skin.Pattern);
        switch (pattern)
        {
            case "stripe": DrawStripe(g, cell); break;
            case "dot": DrawDot(g, cell); break;
            case "rainbow": DrawRainbow(g, cell); break;   // 추가
            case "gradient": DrawGradient(g, cell, skin.BodyColorHex); break; // 추가
            case "scale": DrawScale(g, cell); break;     // 추가(비늘)
            case "feather": DrawFeather(g, cell); break;   // 추가(깃털)
            case "solid":
            case "none":
            default: /* 폴백: 아무 것도 안 그림 */ break;
        }

        if (!head) return;

        var cap = Norm(skin.HeadCap);
        if (cap == "horn") DrawHorn(g, cell);
        else if (cap == "arrow") DrawArrow(g, cell);
        // cap 미지정/모름 → 아무 것도 그리지 않음(안전)
    }

    static void DrawStripe(Graphics g, Rectangle r)
    {
        using var pen = new Pen(Color.FromArgb(40, Color.White), 2);
        g.DrawLine(pen, r.Left, r.Top, r.Right, r.Bottom);
    }
    static void DrawDot(Graphics g, Rectangle r)
    {
        using var pen = new Pen(Color.FromArgb(60, Color.White), 2);
        g.DrawEllipse(pen, r);
    }
    static void DrawRainbow(Graphics g, Rectangle r)
    {
        using var lg = new LinearGradientBrush(r, Color.Red, Color.Violet, 0f);
        g.FillRectangle(lg, r);
    }
    static void DrawGradient(Graphics g, Rectangle r, string baseHex)
    {
        var baseC = ColorTranslator.FromHtml(baseHex);
        using var lg = new LinearGradientBrush(r, ControlPaint.Light(baseC), ControlPaint.Dark(baseC), 45f);
        g.FillRectangle(lg, r);
    }
    static void DrawScale(Graphics g, Rectangle r)
    {
        using var pen = new Pen(Color.FromArgb(70, Color.White), 1);
        int step = Math.Max(3, r.Width / 4);
        for (int y = r.Bottom; y > r.Top - step; y -= step)
            g.DrawArc(pen, r.Left, y - step, r.Width, step * 2, 180, 180);
    }
    static void DrawFeather(Graphics g, Rectangle r)
    {
        using var pen = new Pen(Color.FromArgb(60, Color.White), 1);
        g.DrawLine(pen, r.Left + r.Width / 2, r.Top, r.Left + r.Width / 2, r.Bottom);
        g.DrawBezier(pen, r.Left + r.Width / 2, r.Top + 2, r.Right - 2, r.Top + 4, r.Right - 2, r.Bottom - 4, r.Left + r.Width / 2, r.Bottom - 2);
        g.DrawBezier(pen, r.Left + r.Width / 2, r.Top + 2, r.Left + 2, r.Top + 4, r.Left + 2, r.Bottom - 4, r.Left + r.Width / 2, r.Bottom - 2);
    }
    static void DrawHorn(Graphics g, Rectangle r)
    {
        g.FillPolygon(Brushes.White, new[]
        {
            new Point(r.Left+2, r.Top+2),
            new Point(r.Left+6, r.Top+2),
            new Point(r.Left+2, r.Top+6)
        });
    }
    static void DrawArrow(Graphics g, Rectangle r)
    {
        g.FillPolygon(Brushes.White, new[]
        {
            new Point(r.Left + r.Width/2, r.Top+2),
            new Point(r.Right-2, r.Top + r.Height/2),
            new Point(r.Left + r.Width/2, r.Bottom-2)
        });
    }
}

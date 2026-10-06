using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace Hronograf;

/// <summary>Палитра, шрифты и графические помощники интерфейса.</summary>
public static class Theme
{
    public static readonly Color Bg = Color.FromArgb(9, 13, 26);
    public static readonly Color Panel = Color.FromArgb(16, 23, 42);
    public static readonly Color Panel2 = Color.FromArgb(22, 31, 55);
    public static readonly Color Border = Color.FromArgb(37, 50, 82);
    public static readonly Color Text = Color.FromArgb(234, 238, 247);
    public static readonly Color Muted = Color.FromArgb(136, 149, 180);
    public static readonly Color Gold = Color.FromArgb(242, 184, 75);
    public static readonly Color Cyan = Color.FromArgb(76, 201, 240);
    public static readonly Color Green = Color.FromArgb(118, 226, 148);
    public static readonly Color Coral = Color.FromArgb(255, 122, 112);
    public static readonly Color Violet = Color.FromArgb(167, 139, 250);

    public static readonly Color[] ScenarioColors = { Coral, Cyan, Green };

    public static readonly Color[] TechColors =
    {
        Gold, Gold, Gold, Gold, Gold, Gold, Cyan, Cyan, Cyan, Green, Green, Violet, Violet
    };

    /// <summary>Коэффициент масштаба экрана (100% = 1.0).</summary>
    public static float Scale { get; set; } = 1f;
    public static int Px(float v) => (int)Math.Round(v * Scale);
    public static float Pf(float v) => v * Scale;

    static readonly HashSet<string> Installed =
        new(new InstalledFontCollection().Families.Select(f => f.Name), StringComparer.OrdinalIgnoreCase);

    static string Pick(params string[] names) =>
        names.FirstOrDefault(Installed.Contains) ?? FontFamily.GenericSansSerif.Name;

    public static readonly string Ui = Pick("Segoe UI", "Arial", "DejaVu Sans");
    public static readonly string UiBold = Pick("Segoe UI Semibold", "Segoe UI", "Arial", "DejaVu Sans");
    public static readonly string Display = Pick("Bahnschrift SemiBold", "Bahnschrift", "Segoe UI Semibold", "Segoe UI", "Arial");
    public static readonly string DisplayLight = Pick("Bahnschrift Light", "Bahnschrift", "Segoe UI Light", "Segoe UI", "Arial");
    public static readonly string Serif = Pick("Georgia", "Times New Roman", "DejaVu Serif");
    public static readonly string Script = Pick("Segoe Print", "Segoe Script", "Georgia", "Times New Roman");
    public static readonly string Mono = Pick("Consolas", "Cascadia Mono", "Courier New", "DejaVu Sans Mono");

    static readonly Dictionary<(string, float, FontStyle), Font> FontCache = new();

    public static Font F(float size, FontStyle style = FontStyle.Regular, string? family = null)
    {
        var key = (family ?? Ui, size, style);
        if (!FontCache.TryGetValue(key, out var f))
        {
            f = new Font(key.Item1, size, style, GraphicsUnit.Point);
            FontCache[key] = f;
        }
        return f;
    }

    public static Color A(Color c, int alpha) => Color.FromArgb(Math.Clamp(alpha, 0, 255), c);

    public static Color Mix(Color a, Color b, float t)
    {
        t = Math.Clamp(t, 0f, 1f);
        return Color.FromArgb(
            (int)(a.A + (b.A - a.A) * t), (int)(a.R + (b.R - a.R) * t),
            (int)(a.G + (b.G - a.G) * t), (int)(a.B + (b.B - a.B) * t));
    }

    public static Color Scale3(Color c, float k) => Color.FromArgb(c.A,
        Math.Clamp((int)(c.R * k), 0, 255), Math.Clamp((int)(c.G * k), 0, 255), Math.Clamp((int)(c.B * k), 0, 255));

    public static GraphicsPath Round(RectangleF r, float radius)
    {
        var p = new GraphicsPath();
        float d = Math.Min(radius * 2, Math.Min(r.Width, r.Height));
        if (d <= 0.5f) { p.AddRectangle(r); return p; }
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    public static void Quality(Graphics g, bool clearType = true)
    {
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.TextRenderingHint = clearType ? TextRenderingHint.ClearTypeGridFit : TextRenderingHint.AntiAliasGridFit;
    }

    public static void FillRound(Graphics g, RectangleF r, float radius, Color c)
    {
        using var p = Round(r, radius);
        using var b = new SolidBrush(c);
        g.FillPath(b, p);
    }

    public static void StrokeRound(Graphics g, RectangleF r, float radius, Color c, float width = 1f)
    {
        using var p = Round(r, radius);
        using var pen = new Pen(c, width);
        g.DrawPath(pen, p);
    }

    /// <summary>Карточка «стекло»: полупрозрачная заливка, тонкая рамка и блик сверху.</summary>
    public static void Card(Graphics g, RectangleF r, float radius, Color fill, Color border)
    {
        using var p = Round(r, radius);
        using (var b = new SolidBrush(fill)) g.FillPath(b, p);
        using (var lg = new LinearGradientBrush(new RectangleF(r.X, r.Y - 1, r.Width, r.Height / 2 + 2),
                   Color.FromArgb(22, 255, 255, 255), Color.FromArgb(0, 255, 255, 255), LinearGradientMode.Vertical))
        {
            var old = g.Clip;
            g.SetClip(p, CombineMode.Intersect);
            g.FillRectangle(lg, r.X, r.Y, r.Width, r.Height / 2);
            g.Clip = old;
        }
        using var pen = new Pen(border, 1f);
        g.DrawPath(pen, p);
    }

    public static void Str(Graphics g, string s, Font f, Color c, float x, float y)
    {
        using var b = new SolidBrush(c);
        g.DrawString(s, f, b, x, y, StringFormat.GenericTypographic);
    }

    public static void TextIn(Graphics g, string s, Font f, Color c, RectangleF r,
                              StringAlignment h = StringAlignment.Near, StringAlignment v = StringAlignment.Center,
                              bool ellipsis = true)
    {
        using var b = new SolidBrush(c);
        using var sf = new StringFormat(StringFormat.GenericTypographic)
        {
            Alignment = h, LineAlignment = v,
            Trimming = ellipsis ? StringTrimming.EllipsisCharacter : StringTrimming.None,
            FormatFlags = StringFormatFlags.NoClip
        };
        if (ellipsis) sf.FormatFlags |= StringFormatFlags.NoWrap;
        g.DrawString(s, f, b, r, sf);
    }

    public static SizeF Measure(Graphics g, string s, Font f) => g.MeasureString(s, f, int.MaxValue, StringFormat.GenericTypographic);

    /// <summary>Мягкое свечение текста — несколько полупрозрачных проходов.</summary>
    public static void GlowText(Graphics g, string s, Font f, Color c, float x, float y, int glow = 3)
    {
        using var b = new SolidBrush(A(c, 40));
        for (int dx = -glow; dx <= glow; dx += glow)
            for (int dy = -glow; dy <= glow; dy += glow)
                if (dx != 0 || dy != 0) g.DrawString(s, f, b, x + dx * 0.6f, y + dy * 0.6f, StringFormat.GenericTypographic);
        Str(g, s, f, c, x, y);
    }
}

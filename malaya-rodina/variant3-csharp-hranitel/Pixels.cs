namespace Hranitel;

/// <summary>
/// Пиксельный холст. В консоли один символ «▀» = два пикселя по вертикали
/// (верхний — цвет символа, нижний — цвет фона), поэтому картинка получается
/// вдвое чётче обычной ASCII-графики.
/// </summary>
public sealed class Pixels
{
    public int W { get; }
    public int H { get; }
    readonly Rgb[] p;

    public Pixels(int w, int h)
    {
        W = w; H = h;
        p = new Rgb[w * h];
    }

    public Rgb Get(int x, int y) => x < 0 || y < 0 || x >= W || y >= H ? Pal.Bg : p[y * W + x];

    public void Set(int x, int y, Rgb c)
    {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        p[y * W + x] = c;
    }

    public void Blend(int x, int y, Rgb c, double a)
    {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        p[y * W + x] = p[y * W + x].Mix(c, a);
    }

    public void Rect(int x, int y, int w, int h, Rgb c)
    {
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) Set(xx, yy, c);
    }

    public void VGradient(int y0, int y1, Rgb top, Rgb bottom)
    {
        for (int y = y0; y < y1; y++)
        {
            var c = top.Mix(bottom, (y - y0) / (double)Math.Max(1, y1 - y0 - 1));
            for (int x = 0; x < W; x++) Set(x, y, c);
        }
    }

    public void Disc(double cx, double cy, double r, Rgb c, double alpha = 1)
    {
        for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
            for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++)
            {
                double d = Math.Sqrt((x + 0.5 - cx) * (x + 0.5 - cx) + (y + 0.5 - cy) * (y + 0.5 - cy));
                if (d <= r) Blend(x, y, c, alpha);
            }
    }

    /// <summary>Мягкое свечение (окна, фонари, звёзды).</summary>
    public void Glow(double cx, double cy, double r, Rgb c, double strength)
    {
        for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
            for (int x = (int)(cx - r); x <= (int)(cx + r); x++)
            {
                double d = Math.Sqrt((x + 0.5 - cx) * (x + 0.5 - cx) + (y + 0.5 - cy) * (y + 0.5 - cy)) / r;
                if (d < 1) Blend(x, y, c, strength * (1 - d) * (1 - d));
            }
    }

    public void Line(int x0, int y0, int x1, int y1, Rgb c, double alpha = 1)
    {
        int dx = Math.Abs(x1 - x0), dy = -Math.Abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
        while (true)
        {
            Blend(x0, y0, c, alpha);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    /// <summary>«Объектив эпохи»: сепия для старых лет, выцветание для советской плёнки.</summary>
    public void Grade(double sepia, double fade, double sat = 1)
    {
        for (int i = 0; i < p.Length; i++)
        {
            var c = p[i];
            double lum = 0.299 * c.R + 0.587 * c.G + 0.114 * c.B;
            double r = lum + (c.R - lum) * sat, g = lum + (c.G - lum) * sat, b = lum + (c.B - lum) * sat;
            if (sepia > 0)
            {
                r += (lum * 1.07 + 12 - r) * sepia; g += (lum * 0.86 + 6 - g) * sepia; b += (lum * 0.62 - b) * sepia;
            }
            if (fade > 0)
            {
                r = r * (1 - 0.18 * fade) + 22 * fade; g = g * (1 - 0.18 * fade) + 16 * fade; b = b * (1 - 0.24 * fade) + 12 * fade;
            }
            p[i] = Rgb.Of((int)r, (int)g, (int)b);
        }
    }

    /// <summary>Вывод в экранный буфер: каждая клетка — два пикселя.</summary>
    public void BlitTo(Screen s, int sx, int sy)
    {
        for (int y = 0; y + 1 < H; y += 2)
            for (int x = 0; x < W; x++)
                s.Cell(sx + x, sy + y / 2, '▀', Get(x, y), Get(x, y + 1));
    }

    // ---- пиксельный шрифт 5×7 для заголовка ----
    static readonly Dictionary<char, string[]> Font = new()
    {
        ['М'] = new[] { "X...X", "XX.XX", "X.X.X", "X...X", "X...X", "X...X", "X...X" },
        ['А'] = new[] { ".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X" },
        ['Л'] = new[] { "..XXX", ".X..X", ".X..X", ".X..X", ".X..X", "X...X", "X...X" },
        ['Я'] = new[] { ".XXXX", "X...X", "X...X", ".XXXX", "..X.X", ".X..X", "X...X" },
        ['Р'] = new[] { "XXXX.", "X...X", "X...X", "XXXX.", "X....", "X....", "X...." },
        ['О'] = new[] { ".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX." },
        ['Д'] = new[] { "..XX.", ".X.X.", ".X.X.", ".X.X.", ".X.X.", "XXXXX", "X...X" },
        ['И'] = new[] { "X...X", "X...X", "X..XX", "X.X.X", "XX..X", "X...X", "X...X" },
        ['Н'] = new[] { "X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X" },
        [' '] = new[] { "...", "...", "...", "...", "...", "...", "..." },
    };

    public static int TextWidth(string s) => s.Sum(c => (Font.TryGetValue(c, out var g) ? g[0].Length : 3) + 1) - 1;

    public void Text(int x, int y, string s, Rgb a, Rgb b, Rgb shadow)
    {
        int total = TextWidth(s), cx = x;
        foreach (var c in s)
        {
            if (!Font.TryGetValue(c, out var g)) continue;
            for (int row = 0; row < g.Length; row++)
                for (int col = 0; col < g[row].Length; col++)
                    if (g[row][col] == 'X')
                    {
                        var color = a.Mix(b, (cx + col - x) / (double)Math.Max(1, total));
                        Set(cx + col + 1, y + row + 1, shadow);
                        Set(cx + col, y + row, color);
                    }
            cx += g[0].Length + 1;
        }
    }
}

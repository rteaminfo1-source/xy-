using System.Runtime.InteropServices;
using System.Text;

namespace Hranitel;

/// <summary>Цвет 24-bit.</summary>
public readonly record struct Rgb(byte R, byte G, byte B)
{
    public static Rgb Of(int r, int g, int b) =>
        new((byte)Math.Clamp(r, 0, 255), (byte)Math.Clamp(g, 0, 255), (byte)Math.Clamp(b, 0, 255));

    public static Rgb Hex(string h)
    {
        h = h.TrimStart('#');
        return new(Convert.ToByte(h[..2], 16), Convert.ToByte(h[2..4], 16), Convert.ToByte(h[4..6], 16));
    }

    public Rgb Mix(Rgb o, double t)
    {
        t = Math.Clamp(t, 0, 1);
        return Of((int)(R + (o.R - R) * t), (int)(G + (o.G - G) * t), (int)(B + (o.B - B) * t));
    }

    public Rgb Scale(double k) => Of((int)(R * k), (int)(G * k), (int)(B * k));
    public string Css => $"#{R:x2}{G:x2}{B:x2}";
}

public static class Pal
{
    public static readonly Rgb Bg = Rgb.Hex("090d1a");
    public static readonly Rgb Panel = Rgb.Hex("10172a");
    public static readonly Rgb Panel2 = Rgb.Hex("18223d");
    public static readonly Rgb Border = Rgb.Hex("2a3a60");
    public static readonly Rgb Text = Rgb.Hex("eaeef7");
    public static readonly Rgb Muted = Rgb.Hex("8895b4");
    public static readonly Rgb Gold = Rgb.Hex("f2b84b");
    public static readonly Rgb Cyan = Rgb.Hex("4cc9f0");
    public static readonly Rgb Green = Rgb.Hex("76e294");
    public static readonly Rgb Coral = Rgb.Hex("ff7a70");
    public static readonly Rgb Violet = Rgb.Hex("a78bfa");
    public static readonly Rgb Pink = Rgb.Hex("ff9fb2");
}

/// <summary>
/// Экранный буфер: символы + цвет текста + цвет фона.
/// Кадр собирается целиком и выводится одной строкой ANSI — без мерцания.
/// </summary>
public sealed class Screen
{
    public int W { get; }
    public int H { get; }
    readonly char[] ch;
    readonly Rgb[] fg, bg;

    public Screen(int w, int h)
    {
        W = w; H = h;
        ch = new char[w * h]; fg = new Rgb[w * h]; bg = new Rgb[w * h];
        Clear(Pal.Bg);
    }

    public void Clear(Rgb back)
    {
        Array.Fill(ch, ' '); Array.Fill(fg, Pal.Text); Array.Fill(bg, back);
    }

    public void Cell(int x, int y, char c, Rgb f, Rgb? b = null)
    {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        int i = y * W + x;
        ch[i] = c; fg[i] = f;
        if (b.HasValue) bg[i] = b.Value;
    }

    public Rgb BgAt(int x, int y) => x < 0 || y < 0 || x >= W || y >= H ? Pal.Bg : bg[y * W + x];

    public void Put(int x, int y, string s, Rgb f, Rgb? b = null)
    {
        for (int k = 0; k < s.Length; k++) Cell(x + k, y, s[k], f, b);
    }

    /// <summary>Текст с цветовым градиентом слева направо.</summary>
    public void PutGradient(int x, int y, string s, Rgb a, Rgb c, Rgb? b = null)
    {
        for (int k = 0; k < s.Length; k++) Cell(x + k, y, s[k], a.Mix(c, s.Length > 1 ? k / (double)(s.Length - 1) : 0), b);
    }

    public void Center(int y, string s, Rgb f, Rgb? b = null, int x0 = 0, int w = -1)
    {
        if (w < 0) w = W;
        Put(x0 + Math.Max(0, (w - s.Length) / 2), y, s, f, b);
    }

    public void Fill(int x, int y, int w, int h, Rgb back, char c = ' ', Rgb? f = null)
    {
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) Cell(xx, yy, c, f ?? Pal.Text, back);
    }

    /// <summary>Рамка с заголовком.</summary>
    public void Box(int x, int y, int w, int h, Rgb border, Rgb? fill = null, string? title = null, Rgb? titleColor = null)
    {
        if (fill.HasValue) Fill(x, y, w, h, fill.Value);
        var b = fill;
        for (int k = 1; k < w - 1; k++) { Cell(x + k, y, '─', border, b); Cell(x + k, y + h - 1, '─', border, b); }
        for (int k = 1; k < h - 1; k++) { Cell(x, y + k, '│', border, b); Cell(x + w - 1, y + k, '│', border, b); }
        Cell(x, y, '┌', border, b); Cell(x + w - 1, y, '┐', border, b);
        Cell(x, y + h - 1, '└', border, b); Cell(x + w - 1, y + h - 1, '┘', border, b);
        if (title != null) Put(x + 2, y, $" {title} ", titleColor ?? border, b);
    }

    /// <summary>Полоса-индикатор: █ — целая клетка, ▌ — половина (символы есть в любом консольном шрифте).</summary>
    public void Bar(int x, int y, int w, double v, Rgb col, Rgb back)
    {
        double cells = Math.Clamp(v, 0, 1) * w;
        for (int k = 0; k < w; k++)
        {
            double part = Math.Clamp(cells - k, 0, 1);
            Cell(x + k, y, part > 0.75 ? '█' : part > 0.25 ? '▌' : ' ', col, back);
        }
    }

    /// <summary>Перенос текста по словам. Возвращает число строк.</summary>
    public int Wrap(int x, int y, int w, string text, Rgb f, Rgb? b = null, int maxLines = 99, int reveal = int.MaxValue)
    {
        int line = 0, shown = 0;
        foreach (var para in text.Split('\n'))
        {
            var words = para.Split(' ');
            var cur = new StringBuilder();
            void Emit()
            {
                if (line < maxLines)
                {
                    var s = cur.ToString();
                    int n = Math.Clamp(reveal - shown, 0, s.Length);
                    Put(x, y + line, s[..n], f, b);
                    shown += s.Length;
                }
                line++;
                cur.Clear();
            }
            foreach (var word in words)
            {
                if (cur.Length > 0 && cur.Length + 1 + word.Length > w) Emit();
                if (cur.Length > 0) cur.Append(' ');
                cur.Append(word);
            }
            Emit();
        }
        return line;
    }

    public static int WrapCount(int w, string text)
    {
        int lines = 0;
        foreach (var para in text.Split('\n'))
        {
            int len = 0; lines++;
            foreach (var word in para.Split(' '))
            {
                if (len > 0 && len + 1 + word.Length > w) { lines++; len = 0; }
                len += (len > 0 ? 1 : 0) + word.Length;
            }
        }
        return lines;
    }

    public void Flush()
    {
        var sb = new StringBuilder(W * H * 14);
        sb.Append("\x1b[H");
        Rgb? cf = null, cb = null;
        for (int y = 0; y < H; y++)
        {
            sb.Append("\x1b[").Append(y + 1).Append(";1H");
            for (int x = 0; x < W; x++)
            {
                int i = y * W + x;
                if (cf != fg[i]) { var c = fg[i]; sb.Append("\x1b[38;2;").Append(c.R).Append(';').Append(c.G).Append(';').Append(c.B).Append('m'); cf = c; }
                if (cb != bg[i]) { var c = bg[i]; sb.Append("\x1b[48;2;").Append(c.R).Append(';').Append(c.G).Append(';').Append(c.B).Append('m'); cb = c; }
                sb.Append(ch[i]);
            }
        }
        sb.Append("\x1b[0m");
        Term.Out.Write(sb);
        Term.Out.Flush();
    }
}

/// <summary>Настройка консоли: UTF-8, 24-битный цвет, альтернативный экран, клавиатура.</summary>
public static class Term
{
    public static TextWriter Out { get; private set; } = Console.Out;
    static bool active;

    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr GetStdHandle(int n);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool GetConsoleMode(IntPtr h, out uint mode);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetConsoleMode(IntPtr h, uint mode);

    public static void Init()
    {
        Console.OutputEncoding = new UTF8Encoding(false);
        if (OperatingSystem.IsWindows())
        {
            try
            {
                var h = GetStdHandle(-11);
                if (GetConsoleMode(h, out uint mode)) SetConsoleMode(h, mode | 0x0004 | 0x0008); // VT-последовательности
                Console.Title = "Хранитель малой родины";
                int w = Math.Min(Console.LargestWindowWidth, 120), hh = Math.Min(Console.LargestWindowHeight, 38);
                if (Console.WindowWidth < w || Console.WindowHeight < hh)
                {
                    Console.SetBufferSize(Math.Max(Console.BufferWidth, w), Math.Max(Console.BufferHeight, hh));
                    Console.SetWindowSize(w, hh);
                }
            }
            catch { /* Windows Terminal сам управляет размером окна */ }
        }
        Out = new StreamWriter(Console.OpenStandardOutput(), new UTF8Encoding(false), 1 << 16) { AutoFlush = false };
        Out.Write("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[2J");
        Out.Flush();
        active = true;
        Console.CancelKeyPress += (_, _) => Restore();
        AppDomain.CurrentDomain.ProcessExit += (_, _) => Restore();
    }

    public static void Restore()
    {
        if (!active) return;
        active = false;
        Out.Write("\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l");
        Out.Flush();
    }

    public static int Width => Math.Max(80, SafeWidth());
    public static int Height => Math.Max(26, SafeHeight());

    static int SafeWidth() { try { return Console.WindowWidth; } catch { return 110; } }
    static int SafeHeight() { try { return Console.WindowHeight; } catch { return 34; } }

    public static bool KeyAvailable
    {
        get { try { return Console.KeyAvailable; } catch { return false; } }
    }

    public static ConsoleKeyInfo ReadKey() => Console.ReadKey(true);

    /// <summary>Ждёт клавишу, вызывая анимацию draw(t) ~15 раз в секунду.</summary>
    public static ConsoleKeyInfo Animate(Action<double> draw)
    {
        var clock = System.Diagnostics.Stopwatch.StartNew();
        while (true)
        {
            draw(clock.Elapsed.TotalSeconds);
            for (int i = 0; i < 7; i++)
            {
                if (KeyAvailable) return ReadKey();
                Thread.Sleep(10);
            }
        }
    }
}

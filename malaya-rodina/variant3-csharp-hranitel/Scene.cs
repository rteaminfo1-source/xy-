namespace Hranitel;

public enum Mood { Day, Dusk, Night, Dawn, Overcast }

/// <summary>Что изображено на панораме села.</summary>
public sealed class SceneState
{
    public int Year = 1925;
    public Mood Mood = Mood.Dusk;
    public double People = 60;           // 0..100 — сколько домов жилые
    public double Decline;               // 0..1 — доля брошенных домов
    public HashSet<string> Flags = new();

    public bool Has(string f) => Flags.Contains(f);

    /// <summary>«Типичная» история села — для заставки и музея технологий.</summary>
    public static SceneState ForYear(int year, Mood? mood = null)
    {
        var s = new SceneState { Year = year };
        if (year >= 1952) s.Flags.Add("electric");
        if (year >= 1936 && year < 2005) s.Flags.Add("radio");
        if (year >= 1946) s.Flags.Add("memorial");
        if (year >= 1965) s.Flags.Add("club");
        if (year >= 1970) s.Flags.Add("road");
        if (year >= 2008) s.Flags.Add("mobile");
        if (year >= 2019) s.Flags.Add("fiber");
        if (year >= 2027) { s.Flags.Add("cowork"); s.Flags.Add("museum"); s.Flags.Add("smartfarm"); }
        if (year >= 2035) { s.Flags.Add("solar"); s.Flags.Add("drones"); }
        s.People = year switch { < 1940 => 70, < 1960 => 62, < 1985 => 66, < 2000 => 52, < 2027 => 40, _ => 62 };
        s.Decline = year switch { < 1985 => 0, < 2000 => 0.15, < 2027 => 0.35, _ => 0.1 };
        s.Mood = mood ?? (year switch
        {
            < 1930 => Mood.Dusk, < 1945 => Mood.Day, < 1960 => Mood.Dawn, < 1990 => Mood.Night,
            < 2000 => Mood.Overcast, < 2015 => Mood.Dusk, < 2027 => Mood.Day, _ => Mood.Night
        });
        return s;
    }
}

/// <summary>Рисует панораму села: небо, холмы, поля, дома, технологии своего времени.</summary>
public static class Scene
{
    static double H01(int a, int b = 0)
    {
        unchecked
        {
            uint h = (uint)(a * 374761393 + b * 668265263);
            h = (h ^ (h >> 13)) * 1274126177u;
            return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0;
        }
    }

    static (Rgb top, Rgb bottom, double light, double glow) Sky(Mood m) => m switch
    {
        Mood.Day => (Rgb.Hex("4a86c8"), Rgb.Hex("bcd7ee"), 1.0, 0.0),
        Mood.Dusk => (Rgb.Hex("2a2a62"), Rgb.Hex("f19a63"), 0.72, 0.55),
        Mood.Night => (Rgb.Hex("060a1e"), Rgb.Hex("1d2c5a"), 0.38, 1.0),
        Mood.Dawn => (Rgb.Hex("6a6aa8"), Rgb.Hex("f6c1a0"), 0.85, 0.3),
        _ => (Rgb.Hex("7d8794"), Rgb.Hex("b9bfc6"), 0.8, 0.1),
    };

    public static Pixels Render(SceneState s, int w, int h, double t)
    {
        var px = new Pixels(w, h);
        var (top, bottom, light, glow) = Sky(s.Mood);
        int horizon = (int)(h * 0.50);
        int baseY = (int)(h * 0.80);     // линия домов
        int roadY = baseY + 1;
        Rgb L(Rgb c) => c.Scale(light);

        // ---- небо ----
        px.VGradient(0, horizon + 2, top, bottom);
        if (s.Mood == Mood.Night)
            for (int i = 0; i < w * horizon / 45; i++)
            {
                int sx = (int)(H01(i, 1) * w), sy = (int)(H01(i, 2) * horizon * 0.85);
                double tw = 0.5 + 0.5 * Math.Sin(t * 2 + i);
                px.Blend(sx, sy, Rgb.Hex("ffffff"), 0.15 + 0.45 * tw * H01(i, 3));
            }
        switch (s.Mood)
        {
            case Mood.Day: px.Glow(w * 0.82, h * 0.16, 9, Rgb.Hex("fff6d0"), 0.7); px.Disc(w * 0.82, h * 0.16, 2.6, Rgb.Hex("fffbe8")); break;
            case Mood.Dusk: px.Glow(w * 0.7, horizon - 2, 16, Rgb.Hex("ffcf7a"), 0.8); px.Disc(w * 0.7, horizon - 1, 3.5, Rgb.Hex("ffe2a8")); break;
            case Mood.Dawn: px.Glow(w * 0.25, horizon - 1, 14, Rgb.Hex("ffe0b0"), 0.7); px.Disc(w * 0.25, horizon, 3, Rgb.Hex("fff0d0")); break;
            case Mood.Night: px.Glow(w * 0.86, h * 0.14, 6, Rgb.Hex("cfe0ff"), 0.4); px.Disc(w * 0.86, h * 0.14, 2.2, Rgb.Hex("eef4ff")); break;
        }
        // облака
        for (int k = 0; k < 4; k++)
        {
            double cx = (H01(k, 7) * w + t * (1.5 + k)) % (w + 30) - 15, cy = h * (0.08 + 0.1 * H01(k, 8));
            var cc = s.Mood == Mood.Night ? Rgb.Hex("2a3560") : Rgb.Hex("ffffff");
            for (int j = 0; j < 4; j++) px.Disc(cx + j * 3, cy + (j % 2), 2.2 + H01(k, j) * 1.5, cc, s.Mood == Mood.Overcast ? 0.5 : 0.35);
        }

        // ---- холмы и лес ----
        for (int x = 0; x < w; x++)
        {
            double hill = horizon - 3 - 3 * Math.Sin(x * 0.045 + 1.3) - 2 * Math.Sin(x * 0.11);
            for (int y = (int)hill; y < baseY; y++) px.Set(x, y, L(Rgb.Hex("5d7f9c").Mix(bottom, 0.45)));
            double forest = horizon + 1 - 2 * H01(x / 2, 9) - (x % 3 == 0 ? 1 : 0);
            for (int y = (int)forest; y < baseY; y++) px.Set(x, y, L(Rgb.Hex("2d5a3a")));
        }
        // церковный холм
        int churchX = (int)(w * 0.14);
        for (int x = churchX - 12; x <= churchX + 12; x++)
        {
            double hy = horizon - 1 + Math.Pow(Math.Abs(x - churchX) / 12.0, 2) * 4;
            for (int y = (int)hy; y < baseY; y++) px.Set(x, y, L(Rgb.Hex("4d7a45")));
        }

        // ---- поля ----
        bool overgrown = s.Has("overgrown") || (s.Year >= 1996 && s.Year < 2027 && !s.Has("coop") && !s.Has("smartfarm"));
        for (int y = horizon + 3; y < baseY; y++)
            for (int x = 0; x < w; x++)
            {
                int band = (y - horizon) / 2 + (int)(H01(x / 14, 3) * 3);
                Rgb c;
                if (s.Has("smartfarm")) c = (band % 2 == 0) ? Rgb.Hex("6fc35e") : Rgb.Hex("8fd36a");
                else if (overgrown) c = Rgb.Hex("7a9a58").Mix(Rgb.Hex("9a9a62"), H01(x, y));
                else c = (band % 3) switch { 0 => Rgb.Hex("d8b85a"), 1 => Rgb.Hex("b9c45f"), _ => Rgb.Hex("cfa85a") };
                px.Set(x, y, L(c));
            }
        if (overgrown)
            for (int i = 0; i < w / 3; i++)
            {
                int bx = (int)(H01(i, 11) * w), by = horizon + 3 + (int)(H01(i, 12) * (baseY - horizon - 5));
                px.Set(bx, by, L(Rgb.Hex("e8e8e0"))); px.Set(bx, by - 1, L(Rgb.Hex("5e8a46")));
            }
        if (s.Has("smartfarm"))
            for (int i = 0; i < 9; i++)
            {
                int bx = (int)(w * (0.3 + 0.07 * i)), by = horizon + 4 + (i % 3) * 2;
                double blink = (t * 2 + i) % 2 < 0.3 ? 1 : 0.4;
                px.Glow(bx, by, 2, Pal.Cyan, 0.7 * blink * Math.Max(0.4, glow));
                px.Set(bx, by, Pal.Cyan);
            }
        if (s.Has("solar"))
            for (int i = 0; i < 6; i++)
            {
                int sx = (int)(w * 0.62) + i * 5, sy = horizon + 4;
                px.Rect(sx, sy, 4, 2, Rgb.Hex("1e3a78"));
                px.Set(sx, sy, Rgb.Hex("6ea8ff"));
            }

        // трактор в поле
        if (s.Year >= 1934 && s.Year < 1995 && s.Mood != Mood.Night && !overgrown)
        {
            int tx = (int)(t * 2.5 % (w + 20)) - 10, ty = horizon + 5;
            px.Rect(tx, ty, 4, 2, Rgb.Hex("e07a2a")); px.Rect(tx + 3, ty - 1, 2, 1, Rgb.Hex("e07a2a"));
            px.Set(tx, ty + 2, Rgb.Hex("1a1a1a")); px.Set(tx + 3, ty + 2, Rgb.Hex("1a1a1a"));
        }

        // ---- земля, дорога, река ----
        px.Rect(0, baseY, w, h - baseY, L(Rgb.Hex("4f7d3c")));
        var roadColor = s.Has("road") ? Rgb.Hex("5d6270") : Rgb.Hex("9a7a55");
        px.Rect(0, roadY, w, 2, L(roadColor));
        if (s.Has("road")) for (int x = 2; x < w; x += 6) px.Rect(x, roadY, 3, 1, L(Rgb.Hex("c8c8c0")));
        int riverY = h - 3;
        for (int y = riverY; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                var c = Rgb.Hex("3b78b4").Mix(bottom, 0.25);
                if ((x + (int)(t * 6) + y * 3) % 11 == 0) c = c.Mix(Rgb.Hex("ffffff"), 0.35);
                px.Set(x, y, L(c));
            }

        // ---- церковь ----
        {
            bool closed = s.Year >= 1932 && s.Year < 2001;
            int cy = horizon - 1;
            var wall = closed ? Rgb.Hex("a8a49a") : Rgb.Hex("f2eee4");
            px.Rect(churchX - 3, cy - 5, 7, 6, L(wall));
            px.Rect(churchX - 1, cy - 9, 3, 4, L(wall));
            var dome = closed ? Rgb.Hex("7c807c") : Rgb.Hex("f0c040");
            px.Disc(churchX + 0.5, cy - 10, 2.2, L(dome));
            if (!closed) px.Line(churchX, cy - 15, churchX, cy - 12, L(Rgb.Hex("f0c040")));
            px.Rect(churchX - 1, cy - 3, 3, 4, L(Rgb.Hex("6a4a30")));
            if (!closed && glow > 0.3 && s.Year >= 2001) px.Glow(churchX, cy - 6, 9, Rgb.Hex("ffd890"), 0.35 * glow);
        }

        // ---- вышки связи ----
        int towerX = (int)(w * 0.9);
        if (s.Has("mobile") && !s.Has("tower_water"))
        {
            int ty0 = horizon - 16;
            px.Line(towerX - 2, baseY - 1, towerX, ty0, L(Rgb.Hex("b8bcc8")));
            px.Line(towerX + 2, baseY - 1, towerX, ty0, L(Rgb.Hex("b8bcc8")));
            for (int y = ty0 + 3; y < baseY; y += 3) px.Line(towerX - 1, y, towerX + 1, y, L(Rgb.Hex("9aa0ac")));
            px.Rect(towerX - 1, ty0 + 1, 3, 2, L(Rgb.Hex("e8ecf2")));
            if ((t % 1.2) < 0.25) { px.Glow(towerX, ty0, 3, Rgb.Hex("ff3030"), 0.9); px.Set(towerX, ty0, Rgb.Hex("ff4040")); }
            if (s.Year >= 2029) px.Glow(towerX, ty0 + 2, 5, Pal.Cyan, 0.25 + 0.2 * Math.Sin(t * 3));
        }
        if (s.Has("tower_water"))
        {
            int ty0 = horizon - 10;
            px.Line(towerX - 2, baseY - 1, towerX - 1, ty0 + 4, L(Rgb.Hex("8a6a50")));
            px.Line(towerX + 2, baseY - 1, towerX + 1, ty0 + 4, L(Rgb.Hex("8a6a50")));
            px.Rect(towerX - 3, ty0, 7, 4, L(Rgb.Hex("c25a44")));
            px.Line(towerX, ty0 - 5, towerX, ty0, L(Rgb.Hex("d0d4dc")));
            if ((t % 1.2) < 0.25) px.Glow(towerX, ty0 - 5, 3, Rgb.Hex("ff3030"), 0.9);
        }
        if (s.Has("solar") || (s.Year >= 2035 && s.Has("drones")))
        {
            int wx = (int)(w * 0.78), wy = horizon - 9;
            px.Line(wx, baseY - 1, wx, wy, L(Rgb.Hex("e8ecf2")));
            for (int k = 0; k < 3; k++)
            {
                double a = t * 2.2 + k * 2 * Math.PI / 3;
                px.Line(wx, wy, wx + (int)Math.Round(Math.Cos(a) * 5), wy + (int)Math.Round(Math.Sin(a) * 5), L(Rgb.Hex("ffffff")));
            }
        }

        // ---- дома ----
        int slots = Math.Max(6, (w - 20) / 9);
        int lived = (int)Math.Round(slots * Math.Clamp(s.People / 100.0, 0.1, 1.0));
        int abandoned = (int)Math.Round(slots * Math.Clamp(s.Decline, 0, 0.8));
        var order = Enumerable.Range(0, slots).OrderBy(k => Math.Abs(k - slots / 2.0) + H01(k, 5) * 3).ToList();
        int clubSlot = order[1], schoolSlot = order[3];
        for (int r = 0; r < slots; r++)
        {
            int k = order[r];
            int x = 10 + k * 9 + (int)(H01(k, 6) * 2);
            if (x + 8 > towerX - 3 || x < churchX + 10) continue;
            if (k == clubSlot && (s.Has("club") || s.Has("cowork")))
            {
                DrawClub(px, x - 2, baseY, s, light, glow, t);
                continue;
            }
            if (k == schoolSlot && (s.Has("school") || s.Has("museum")))
            {
                DrawSchool(px, x - 1, baseY, s, light, glow);
                continue;
            }
            int state = r < lived ? 0 : (r < lived + abandoned ? 1 : 2);
            if (state == 2) { px.Rect(x + 1, baseY - 1, 5, 1, L(Rgb.Hex("6b5a46"))); continue; } // фундамент
            DrawHouse(px, x, baseY, k, state == 1, s, light, glow, t);
        }

        // ---- мемориал ----
        if (s.Has("memorial"))
        {
            int mx = churchX + 10;
            px.Rect(mx, baseY - 8, 2, 8, L(Rgb.Hex("ece8de")));
            px.Set(mx, baseY - 9, L(Rgb.Hex("ece8de")));
            px.Set(mx, baseY - 6, Rgb.Hex("d02828"));
            double f = 0.6 + 0.4 * Math.Sin(t * 9);
            px.Glow(mx + 1, baseY - 1, 3 + f, Rgb.Hex("ff8a30"), 0.6 + 0.3 * glow);
        }

        // ---- столбы и провода ----
        if (s.Has("electric"))
        {
            int poleTop = baseY - 9;
            var wood = L(Rgb.Hex("5a4030"));
            for (int x = 4; x < w; x += 16)
            {
                px.Line(x, baseY + 1, x, poleTop, wood);
                px.Line(x - 1, poleTop + 1, x + 1, poleTop + 1, wood);
                if (x + 16 < w)
                    for (int dx = 0; dx <= 16; dx++)
                    {
                        double sag = 1.6 * Math.Sin(Math.PI * dx / 16.0);
                        px.Blend(x + dx, poleTop + 1 + (int)Math.Round(sag), Rgb.Hex("1c1c22"), 0.7);
                    }
                if (s.Year >= 1965 && glow > 0.2 && x % 32 == 4)
                {
                    px.Glow(x, poleTop + 2, 6, s.Year > 2026 ? Rgb.Hex("dff0ff") : Rgb.Hex("ffd28a"), 0.55 * glow);
                    px.Set(x, poleTop + 2, Rgb.Hex("fff4d8"));
                }
            }
        }
        if (s.Has("radio"))
        {
            int rx = (int)(w * 0.5);
            px.Line(rx, baseY + 1, rx, baseY - 12, L(Rgb.Hex("5a4030")));
            px.Rect(rx - 2, baseY - 13, 2, 2, L(Rgb.Hex("c8ccd2")));
            px.Set(rx - 3, baseY - 13, L(Rgb.Hex("e8ecf2")));
        }
        if (s.Has("hydro"))
        {
            px.Rect(4, h - 5, 6, 3, L(Rgb.Hex("8a8a90")));
            px.Rect(5, h - 7, 4, 2, L(Rgb.Hex("b05a40")));
            if (glow > 0.2) px.Glow(7, h - 5, 4, Rgb.Hex("ffd28a"), 0.5 * glow);
        }

        // ---- транспорт ----
        if (s.Has("road") && s.Year >= 1965)
        {
            int bx = w - (int)(t * 7 % (w + 24));
            var body = s.Year > 2026 ? Rgb.Hex("e8f4ff") : Rgb.Hex("e8c040");
            px.Rect(bx, roadY - 2, 7, 3, L(body));
            px.Rect(bx + 1, roadY - 2, 5, 1, s.Year > 2026 ? Pal.Cyan : Rgb.Hex("6a8aa8"));
            px.Set(bx + 1, roadY + 1, Rgb.Hex("181818")); px.Set(bx + 5, roadY + 1, Rgb.Hex("181818"));
            if (glow > 0.3) px.Glow(bx - 1, roadY - 1, 4, Rgb.Hex("fff0c0"), 0.6 * glow);
        }

        // ---- дроны ----
        if (s.Has("drones"))
            for (int k = 0; k < 4; k++)
            {
                double a = t * (0.4 + 0.1 * k) + k * 1.7;
                int dx = (int)(w * (0.3 + 0.18 * k) + Math.Sin(a) * 10), dy = (int)(h * 0.22 + Math.Sin(a * 1.6) * 4);
                px.Set(dx - 1, dy, Rgb.Hex("dfe8f0")); px.Set(dx + 1, dy, Rgb.Hex("dfe8f0")); px.Set(dx, dy, Rgb.Hex("aab4c0"));
                if ((t * 3 + k) % 1 < 0.4) px.Glow(dx, dy + 1, 2.5, k % 2 == 0 ? Pal.Cyan : Rgb.Hex("ff5050"), 0.8);
            }

        // ---- голограмма цифрового двойника ----
        if (s.Has("twin"))
        {
            // AR-рамки вокруг старых домов и бегущая линия сканера
            for (int k = 0; k < 3; k++)
            {
                int hx = (int)(w * (0.33 + 0.12 * k)), hy = baseY - 10;
                for (int x = hx; x <= hx + 10; x += 2) { px.Blend(x, hy, Pal.Cyan, 0.7); px.Blend(x, baseY, Pal.Cyan, 0.7); }
                for (int y = hy; y <= baseY; y += 2) { px.Blend(hx, y, Pal.Cyan, 0.7); px.Blend(hx + 10, y, Pal.Cyan, 0.7); }
            }
            int scanY = baseY - 10 + (int)(t * 6 % 11);
            for (int x = (int)(w * 0.33); x <= (int)(w * 0.57) + 10; x++) px.Blend(x, scanY, Pal.Cyan, 0.35);
        }

        // ---- «объектив эпохи» ----
        if (s.Year < 1950) px.Grade(s.Year < 1940 ? 0.85 : 0.6, 0);
        else if (s.Year < 1985) px.Grade(0, 0.6, 0.85);
        else if (s.Year < 2005) px.Grade(0, 0.25, 0.95);
        return px;
    }

    static void DrawHouse(Pixels px, int x, int baseY, int k, bool abandoned, SceneState s, double light, double glow, double t)
    {
        Rgb L(Rgb c) => c.Scale(light);
        bool modern = s.Year >= 2030 && !abandoned && H01(k, 21) < (s.Has("smartfarm") || s.Has("cowork") ? 0.45 : 0.15) && !s.Has("twin");
        bool restored = s.Year >= 2027 && !abandoned && (s.Has("museum") || s.Has("twin")) && H01(k, 22) < 0.5;
        Rgb wall, roof;
        if (abandoned) { wall = Rgb.Hex("7a6a58"); roof = Rgb.Hex("5e544a"); }
        else if (modern) { wall = Rgb.Hex("e8ebf0"); roof = Rgb.Hex("3c4252"); }
        else if (restored) { wall = Rgb.Hex("a8743e"); roof = Rgb.Hex("7a4a2a"); }
        else if (s.Year < 1958) { wall = Rgb.Hex("7a5638"); roof = Rgb.Hex("a08a58"); }
        else
        {
            wall = (k % 4) switch { 0 => Rgb.Hex("6a8ab8"), 1 => Rgb.Hex("7aa070"), 2 => Rgb.Hex("c8a058"), _ => Rgb.Hex("b87858") };
            roof = (k % 3) switch { 0 => Rgb.Hex("9a3a30"), 1 => Rgb.Hex("3a6a4a"), _ => Rgb.Hex("6a6e78") };
        }
        px.Rect(x, baseY - 4, 7, 4, L(wall));
        for (int r = 0; r < 4; r++) px.Rect(x - 1 + r, baseY - 5 - r, 9 - r * 2, 1, L(roof.Scale(1 - r * 0.05)));
        if (restored) { px.Rect(x, baseY - 4, 7, 1, L(Rgb.Hex("f0e6d0"))); }
        if (modern && s.Has("solar")) px.Rect(x + 1, baseY - 6, 4, 1, Rgb.Hex("2a4a9a"));
        if (abandoned)
        {
            px.Set(x + 4, baseY - 6, L(Rgb.Hex("2a2420"))); px.Set(x + 5, baseY - 5, L(Rgb.Hex("2a2420")));
            px.Rect(x + 2, baseY - 3, 2, 1, Rgb.Hex("141210"));
            px.Line(x + 1, baseY - 4, x + 4, baseY - 2, L(Rgb.Hex("5a4632")));
            return;
        }
        // окна и свет
        bool electric = s.Has("electric") || s.Year >= 1952;
        var winOff = L(Rgb.Hex("2a3040"));
        if (modern)
        {
            px.Rect(x + 1, baseY - 3, 5, 1, glow > 0.2 ? Rgb.Hex("bfe8ff") : L(Rgb.Hex("8ab8d8")));
            if (glow > 0.2) px.Glow(x + 3.5, baseY - 2.5, 5, Rgb.Hex("cfeeff"), 0.45 * glow);
        }
        else if (glow > 0.2)
        {
            var lamp = electric ? Rgb.Hex("ffd27a") : Rgb.Hex("ff9a48");
            bool on = H01(k, 23) < (electric ? 0.85 : 0.6);
            px.Rect(x + 2, baseY - 3, 2, 1, on ? lamp : winOff);
            if (on) px.Glow(x + 3, baseY - 2.5, electric ? 5 : 3, lamp, (electric ? 0.5 : 0.3) * glow);
        }
        else px.Rect(x + 2, baseY - 3, 2, 1, winOff);
        px.Set(x + 5, baseY - 2, L(Rgb.Hex("4a3424"))); px.Set(x + 5, baseY - 1, L(Rgb.Hex("4a3424")));
        // антенна / тарелка / дымок
        if (s.Year >= 1965 && s.Year < 2015 && k % 2 == 0)
        {
            px.Line(x + 3, baseY - 9, x + 3, baseY - 12, L(Rgb.Hex("c8ccd2")));
            px.Line(x + 1, baseY - 11, x + 5, baseY - 11, L(Rgb.Hex("c8ccd2")));
        }
        if (s.Year >= 2000 && s.Year < 2030 && k % 3 == 1) px.Set(x + 6, baseY - 4, L(Rgb.Hex("e8e8e8")));
        if (s.Year < 1975 && s.Mood != Mood.Night)
            for (int i = 0; i < 3; i++)
            {
                double ph = (t * 0.8 + i * 0.33 + k * 0.17) % 1.0;
                px.Blend(x + 5 + (int)(ph * 3), baseY - 9 - (int)(ph * 7), Rgb.Hex("d8d8d8"), 0.5 * (1 - ph));
            }
        if (s.Has("fiber") && s.Year >= 2019 && glow > 0.5 && k % 2 == 1) px.Glow(x + 1, baseY - 2, 2, Pal.Cyan, 0.5 * glow);
    }

    static void DrawClub(Pixels px, int x, int baseY, SceneState s, double light, double glow, double t)
    {
        Rgb L(Rgb c) => c.Scale(light);
        bool cowork = s.Has("cowork") && s.Year >= 2026;
        var wall = cowork ? Rgb.Hex("2e3448") : Rgb.Hex("d8c8b0");
        px.Rect(x, baseY - 6, 12, 6, L(wall));
        px.Rect(x - 1, baseY - 7, 14, 1, L(cowork ? Rgb.Hex("1e2232") : Rgb.Hex("b04a3a")));
        if (cowork)
        {
            px.Rect(x + 1, baseY - 5, 10, 2, glow > 0.2 ? Rgb.Hex("7ad8ff") : L(Rgb.Hex("5aa8d0")));
            px.Glow(x + 6, baseY - 4, 9, Pal.Cyan, 0.25 + 0.5 * glow);
        }
        else
        {
            for (int k = 1; k < 12; k += 3) px.Line(x + k, baseY - 5, x + k, baseY - 1, L(Rgb.Hex("f4ecdc")));
            if (glow > 0.2) px.Glow(x + 6, baseY - 3, 6, Rgb.Hex("ffd27a"), 0.4 * glow);
        }
    }

    static void DrawSchool(Pixels px, int x, int baseY, SceneState s, double light, double glow)
    {
        Rgb L(Rgb c) => c.Scale(light);
        bool museum = s.Has("museum") && s.Year >= 2026;
        px.Rect(x, baseY - 6, 10, 6, L(museum ? Rgb.Hex("b07a44") : Rgb.Hex("e0b850")));
        px.Rect(x - 1, baseY - 7, 12, 1, L(museum ? Rgb.Hex("6a3e22") : Rgb.Hex("8a4a3a")));
        for (int k = 1; k < 10; k += 3) px.Rect(x + k, baseY - 4, 2, 1, glow > 0.2 ? Rgb.Hex("ffd890") : L(Rgb.Hex("3a4050")));
        if (museum) px.Glow(x + 5, baseY - 4, 8, Pal.Gold, 0.2 + 0.4 * glow);
    }
}

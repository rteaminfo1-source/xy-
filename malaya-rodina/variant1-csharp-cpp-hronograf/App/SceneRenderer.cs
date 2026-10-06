using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace Hronograf;

/// <summary>
/// Рисует «дневной» кадр села сверху (рельеф, поля, лес, дома) и собирает
/// источники света для ночного режима. Цветокоррекцию эпохи затем делает C++.
/// </summary>
public static class SceneRenderer
{
    static float Hash(int x, int y, int salt = 0)
    {
        unchecked
        {
            uint h = (uint)(x * 374761393 + y * 668265263 + salt * 1442695041);
            h = (h ^ (h >> 13)) * 1274126177u;
            return ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216f;
        }
    }

    static readonly Color[] FieldCrops =
    {
        Color.FromArgb(218, 186, 92), Color.FromArgb(160, 192, 92), Color.FromArgb(204, 172, 118), Color.FromArgb(226, 204, 120)
    };

    static Color TerrainColor(byte t, int x, int y, int year)
    {
        switch (t)
        {
            case T.Water: return Color.FromArgb(56, 118, 178);
            case T.Sand: return Color.FromArgb(218, 202, 156);
            case T.Meadow: return Color.FromArgb(140, 182, 102);
            case T.Field:
                if (year < 1930) // чересполосица: узкие крестьянские полосы
                    return FieldCrops[(int)(Hash(x / 9, y, 3) * 4) & 3];
                return FieldCrops[(int)(Hash(x / 6, y / 4, 5) * 4) & 3];
            case T.Forest: return Color.FromArgb(44, 100, 56);
            case T.Road: return Color.FromArgb(166, 134, 96);
            case T.Asphalt: return Color.FromArgb(92, 97, 110);
            case T.Garden: return Color.FromArgb(108, 150, 70);
            case T.Fallow: return Color.FromArgb(168, 164, 108);
            case T.YoungForest: return Color.FromArgb(104, 150, 84);
            case T.Bridge: return year < 1965 ? Color.FromArgb(140, 106, 72) : Color.FromArgb(152, 152, 158);
            case T.Plaza: return Color.FromArgb(202, 196, 180);
            case T.Marsh: return Color.FromArgb(104, 140, 108);
            case T.SmartField: return Color.FromArgb(120, 206, 110);
            default: return Color.Magenta;
        }
    }

    public sealed class Result : IDisposable
    {
        public required Bitmap Bitmap { get; init; }
        public required HgLight[] Lights { get; init; }
        public required HgBuilding[] Buildings { get; init; }
        public required float Cell { get; init; }
        public void Dispose() => Bitmap.Dispose();
    }

    public static Result Render(Chronicle c, int year, Size size)
    {
        int W = c.MapW, H = c.MapH;
        float cs = Math.Min(size.Width / (float)W, size.Height / (float)H);
        int pw = Math.Max(1, (int)(W * cs)), ph = Math.Max(1, (int)(H * cs));
        var terr = c.Terrain(year);
        var shade = c.Shade;
        var st = c.Stats(year);
        var buildings = c.Buildings(year);

        // 1) Базовый слой: одна клетка = один пиксель, затем масштаб без сглаживания.
        using var cells = new Bitmap(W, H, PixelFormat.Format32bppArgb);
        var data = cells.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var px = new int[W * H];
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                int i = y * W + x;
                var col = TerrainColor(terr[i], x, y, year);
                float k = 0.80f + 0.40f * shade[i] / 255f;
                k *= 0.96f + 0.08f * Hash(x, y, 1);
                if (terr[i] == T.Water) k = 0.92f + 0.10f * Hash(x, y, 2);
                px[i] = Theme.Scale3(col, k).ToArgb();
            }
        Marshal.Copy(px, 0, data.Scan0, px.Length);
        cells.UnlockBits(data);

        var bmp = new Bitmap(pw, ph, PixelFormat.Format32bppPArgb);
        using var g = Graphics.FromImage(bmp);
        g.InterpolationMode = InterpolationMode.NearestNeighbor;
        g.PixelOffsetMode = PixelOffsetMode.Half;
        g.DrawImage(cells, new Rectangle(0, 0, pw, ph));
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        DrawTerrainDetails(g, terr, W, H, cs, year);

        var lights = new List<HgLight>(512);
        AddStreetLamps(lights, terr, W, H, cs, year, st, c.Scenario);
        foreach (var b in buildings.OrderBy(b => b.Y))
            DrawBuilding(g, b, cs, year, st, lights);

        return new Result { Bitmap = bmp, Lights = lights.ToArray(), Buildings = buildings, Cell = cs };
    }

    // -----------------------------------------------------------------
    static void DrawTerrainDetails(Graphics g, byte[] terr, int W, int H, float cs, int year)
    {
        using var furrow = new Pen(Color.FromArgb(46, 70, 50, 20), Math.Max(1f, cs * 0.12f));
        using var bed = new Pen(Color.FromArgb(60, 40, 70, 25), Math.Max(1f, cs * 0.14f));
        using var smart = new Pen(Color.FromArgb(70, 200, 255, 230), Math.Max(1f, cs * 0.08f));
        using var ripple = new Pen(Color.FromArgb(70, 210, 235, 255), Math.Max(1f, cs * 0.12f));
        using var reed = new Pen(Color.FromArgb(120, 60, 90, 50), Math.Max(1f, cs * 0.1f));
        using var treeShadow = new SolidBrush(Color.FromArgb(70, 10, 30, 15));
        using var treeDark = new SolidBrush(Color.FromArgb(36, 86, 46));
        using var treeLight = new SolidBrush(Color.FromArgb(64, 128, 72));
        using var birch = new SolidBrush(Color.FromArgb(128, 178, 98));
        using var birchTrunk = new SolidBrush(Color.FromArgb(240, 240, 232));
        using var weeds = new SolidBrush(Color.FromArgb(70, 120, 110, 60));

        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                byte t = terr[y * W + x];
                float X = x * cs, Y = y * cs;
                switch (t)
                {
                    case T.Field:
                    {
                        bool vertical = Hash(x / 6, y / 4, 9) > 0.5f && year >= 1930;
                        for (int k = 1; k <= 2; k++)
                        {
                            float o = cs * k / 3f;
                            if (vertical) g.DrawLine(furrow, X + o, Y, X + o, Y + cs);
                            else g.DrawLine(furrow, X, Y + o, X + cs, Y + o);
                        }
                        break;
                    }
                    case T.SmartField:
                        g.DrawLine(smart, X, Y + cs * 0.5f, X + cs, Y + cs * 0.5f);
                        if (Hash(x, y, 4) < 0.04f) g.FillEllipse(Brushes.White, X + cs * 0.35f, Y + cs * 0.35f, cs * 0.3f, cs * 0.3f);
                        break;
                    case T.Garden:
                        g.DrawLine(bed, X + cs * 0.25f, Y, X + cs * 0.25f, Y + cs);
                        g.DrawLine(bed, X + cs * 0.75f, Y, X + cs * 0.75f, Y + cs);
                        break;
                    case T.Water:
                        if (Hash(x, y, 6) < 0.16f)
                        {
                            float ox = Hash(x, y, 7) * cs * 0.5f;
                            g.DrawLine(ripple, X + ox, Y + cs * 0.5f, X + ox + cs * 0.55f, Y + cs * 0.5f);
                        }
                        break;
                    case T.Marsh:
                        if (Hash(x, y, 8) < 0.5f)
                        {
                            g.DrawLine(reed, X + cs * 0.3f, Y + cs * 0.8f, X + cs * 0.35f, Y + cs * 0.2f);
                            g.DrawLine(reed, X + cs * 0.6f, Y + cs * 0.85f, X + cs * 0.7f, Y + cs * 0.3f);
                        }
                        break;
                    case T.Fallow:
                        if (Hash(x, y, 10) < 0.45f)
                            g.FillEllipse(weeds, X + Hash(x, y, 11) * cs * 0.6f, Y + Hash(x, y, 12) * cs * 0.6f, cs * 0.4f, cs * 0.4f);
                        break;
                    case T.YoungForest:
                    {
                        float r = cs * (0.32f + 0.12f * Hash(x, y, 13));
                        float cx = X + cs * (0.3f + 0.4f * Hash(x, y, 14)), cy = Y + cs * (0.3f + 0.4f * Hash(x, y, 15));
                        g.FillEllipse(birch, cx - r, cy - r, r * 2, r * 2);
                        g.FillRectangle(birchTrunk, cx - cs * 0.05f, cy - cs * 0.05f, cs * 0.12f, cs * 0.12f);
                        break;
                    }
                    case T.Forest:
                    {
                        if (Hash(x, y, 16) > 0.82f) break;
                        float r = cs * (0.55f + 0.2f * Hash(x, y, 17));
                        float cx = X + cs * (0.25f + 0.5f * Hash(x, y, 18)), cy = Y + cs * (0.25f + 0.5f * Hash(x, y, 19));
                        g.FillEllipse(treeShadow, cx - r + cs * 0.22f, cy - r + cs * 0.22f, r * 2, r * 2);
                        g.FillEllipse(treeDark, cx - r, cy - r, r * 2, r * 2);
                        g.FillEllipse(treeLight, cx - r * 0.75f, cy - r * 0.8f, r * 1.1f, r * 1.1f);
                        break;
                    }
                }
            }
    }

    static void AddStreetLamps(List<HgLight> lights, byte[] terr, int W, int H, float cs, int year, HgStats st, int scenario)
    {
        if (year < 1965 || st.Tech[Te.Electric] < 0.8) return;
        bool led = year > 2026 && scenario != 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                if (terr[y * W + x] != T.Asphalt || Hash(x, y, 21) > 0.16f) continue;
                if (led) lights.Add(new HgLight((x + 0.5f) * cs, (y + 0.5f) * cs, cs * 2.2f, 0.85f, 0.95f, 1f, 0.42f));
                else lights.Add(new HgLight((x + 0.5f) * cs, (y + 0.5f) * cs, cs * 1.9f, 1f, 0.78f, 0.45f, 0.38f));
            }
    }

    // -----------------------------------------------------------------
    //  Здания
    // -----------------------------------------------------------------
    static readonly (Color a, Color b)[] IronRoofs =
    {
        (Color.FromArgb(170, 76, 60), Color.FromArgb(136, 58, 48)),
        (Color.FromArgb(82, 130, 96), Color.FromArgb(62, 104, 74)),
        (Color.FromArgb(96, 114, 144), Color.FromArgb(74, 90, 116)),
        (Color.FromArgb(136, 100, 72), Color.FromArgb(108, 78, 56)),
    };

    static void Shadow(Graphics g, RectangleF r, float cs)
    {
        using var sh = new SolidBrush(Color.FromArgb(80, 8, 14, 10));
        g.FillRectangle(sh, r.X + cs * 0.28f, r.Y + cs * 0.28f, r.Width, r.Height);
    }

    static void Roof(Graphics g, RectangleF r, Color a, Color b)
    {
        using (var ba = new SolidBrush(a)) g.FillRectangle(ba, r.X, r.Y, r.Width, r.Height / 2);
        using (var bb = new SolidBrush(b)) g.FillRectangle(bb, r.X, r.Y + r.Height / 2, r.Width, r.Height / 2);
        using var ridge = new Pen(Theme.Scale3(b, 0.7f), Math.Max(1f, r.Height * 0.06f));
        g.DrawLine(ridge, r.X, r.Y + r.Height / 2, r.Right, r.Y + r.Height / 2);
        using var edge = new Pen(Color.FromArgb(90, 0, 0, 0), 1f);
        g.DrawRectangle(edge, r.X, r.Y, r.Width, r.Height);
    }

    static void Boards(Graphics g, RectangleF r)
    {
        using var pen = new Pen(Color.FromArgb(150, 60, 44, 30), Math.Max(1f, r.Width * 0.07f));
        g.DrawLine(pen, r.X + r.Width * 0.15f, r.Y + r.Height * 0.2f, r.Right - r.Width * 0.15f, r.Bottom - r.Height * 0.2f);
        g.DrawLine(pen, r.Right - r.Width * 0.15f, r.Y + r.Height * 0.2f, r.X + r.Width * 0.15f, r.Bottom - r.Height * 0.2f);
    }

    static void Rubble(Graphics g, RectangleF r, int id)
    {
        using var outline = new Pen(Color.FromArgb(150, 120, 112, 100), Math.Max(1f, r.Width * 0.06f));
        g.DrawRectangle(outline, r.X, r.Y, r.Width, r.Height);
        using var stone = new SolidBrush(Color.FromArgb(150, 128, 120, 108));
        for (int i = 0; i < 6; i++)
        {
            float s = r.Width * (0.08f + 0.08f * Hash(id, i, 31));
            g.FillEllipse(stone, r.X + Hash(id, i, 32) * (r.Width - s), r.Y + Hash(id, i, 33) * (r.Height - s), s, s);
        }
    }

    static void Light(List<HgLight> l, RectangleF r, float cs, Color c, float radius, float intensity, bool blink = false) =>
        l.Add(new HgLight(r.X + r.Width / 2, r.Y + r.Height / 2, cs * radius, c.R / 255f, c.G / 255f, c.B / 255f, intensity, blink));

    static readonly Color Warm = Color.FromArgb(255, 196, 112);
    static readonly Color Kerosene = Color.FromArgb(255, 150, 70);
    static readonly Color Neon = Color.FromArgb(90, 210, 255);

    static void DrawBuilding(Graphics g, HgBuilding b, float cs, int year, HgStats st, List<HgLight> lights)
    {
        var full = new RectangleF(b.X * cs, b.Y * cs, b.W * cs, b.H * cs);
        var r = RectangleF.Inflate(full, -cs * 0.14f, -cs * 0.14f);
        bool closed = (b.Flags & F.Closed) != 0, modern = (b.Flags & F.Modern) != 0,
             restored = (b.Flags & F.Restored) != 0, lit = (b.Flags & F.Lit) != 0;

        switch (b.Type)
        {
            case B.House:
            case B.HouseAbandoned:
            {
                bool abandoned = b.Type == B.HouseAbandoned;
                Shadow(g, r, cs);
                Color ra, rb;
                if (abandoned) { ra = Color.FromArgb(116, 106, 94); rb = Color.FromArgb(92, 84, 76); }
                else if (modern) { ra = Color.FromArgb(70, 76, 92); rb = Color.FromArgb(54, 59, 72); }
                else if (restored) { ra = Color.FromArgb(184, 126, 74); rb = Color.FromArgb(154, 102, 58); }
                else if (year < 1955 + b.Variant * 4) { ra = Color.FromArgb(156, 136, 96); rb = Color.FromArgb(128, 110, 78); }
                else (ra, rb) = IronRoofs[b.Variant & 3];
                Roof(g, r, ra, rb);

                if (modern && (b.Flags & F.SolarRoof) != 0)
                {
                    var panel = new RectangleF(r.X + r.Width * 0.1f, r.Y + r.Height * 0.08f, r.Width * 0.8f, r.Height * 0.36f);
                    using var pb = new SolidBrush(Color.FromArgb(30, 58, 112));
                    g.FillRectangle(pb, panel);
                    using var grid = new Pen(Color.FromArgb(120, 120, 200, 255), 1f);
                    for (int k = 1; k < 4; k++) g.DrawLine(grid, panel.X + panel.Width * k / 4, panel.Y, panel.X + panel.Width * k / 4, panel.Bottom);
                }
                if (restored)
                {
                    using var gold = new Pen(Color.FromArgb(200, 255, 214, 130), Math.Max(1f, cs * 0.1f));
                    g.DrawRectangle(gold, r.X, r.Y, r.Width, r.Height);
                }
                if (!modern && (abandoned || year < 2010))
                {
                    using var ch = new SolidBrush(Color.FromArgb(90, 70, 60));
                    g.FillRectangle(ch, r.X + r.Width * 0.68f, r.Y + r.Height * 0.12f, r.Width * 0.14f, r.Height * 0.2f);
                }
                if ((b.Flags & F.Tv) != 0 && !modern)
                {
                    using var ant = new Pen(Color.FromArgb(220, 225, 230, 235), Math.Max(1f, cs * 0.07f));
                    float ax = r.X + r.Width * 0.3f, ay = r.Y + r.Height * 0.32f;
                    g.DrawLine(ant, ax - r.Width * 0.18f, ay, ax + r.Width * 0.18f, ay);
                    g.DrawLine(ant, ax, ay, ax, ay + r.Height * 0.2f);
                }
                if (abandoned)
                {
                    using var hole = new SolidBrush(Color.FromArgb(46, 40, 36));
                    g.FillPolygon(hole, new[] { new PointF(r.X + r.Width * 0.55f, r.Y + r.Height * 0.5f),
                        new PointF(r.Right - 1, r.Y + r.Height * 0.55f), new PointF(r.Right - 1, r.Bottom - 1) });
                    break;
                }
                // Свет в окнах.
                if ((b.Flags & F.Electric) != 0)
                    Light(lights, r, cs, modern ? Color.FromArgb(240, 236, 220) : Warm, 2.6f, 0.62f + 0.2f * Hash(b.Id, 0, 3));
                else
                    Light(lights, r, cs, Kerosene, 1.7f, 0.34f);
                if ((b.Flags & F.Internet) != 0)
                {
                    using var dot = new SolidBrush(Color.FromArgb(230, 90, 220, 255));
                    g.FillEllipse(dot, r.X + r.Width * 0.12f, r.Bottom - r.Height * 0.3f, cs * 0.28f, cs * 0.28f);
                    lights.Add(new HgLight(r.X + r.Width * 0.2f, r.Bottom - r.Height * 0.2f, cs * 1.1f, 0.35f, 0.85f, 1f, 0.35f));
                }
                break;
            }
            case B.Ruin:
                Rubble(g, r, b.Id);
                break;

            case B.Church:
            {
                Shadow(g, r, cs);
                using (var wall = new SolidBrush(closed ? Color.FromArgb(178, 172, 160) : Color.FromArgb(240, 236, 226)))
                    g.FillRectangle(wall, r);
                var dome = closed ? Color.FromArgb(120, 124, 120) : (year < 1932 || year >= 2001 ? Color.FromArgb(238, 190, 70) : Color.FromArgb(70, 130, 100));
                float d = r.Width * 0.62f;
                using (var db = new SolidBrush(dome)) g.FillEllipse(db, r.X + (r.Width - d) / 2, r.Y + (r.Height - d) / 2, d, d);
                using (var hl = new SolidBrush(Color.FromArgb(120, 255, 255, 255))) g.FillEllipse(hl, r.X + r.Width * 0.34f, r.Y + r.Height * 0.3f, d * 0.3f, d * 0.3f);
                float sd = r.Width * 0.22f;
                using (var sb = new SolidBrush(Theme.Scale3(dome, 0.9f)))
                    foreach (var (fx, fy) in new[] { (0.12f, 0.12f), (0.66f, 0.12f), (0.12f, 0.66f), (0.66f, 0.66f) })
                        g.FillEllipse(sb, r.X + r.Width * fx, r.Y + r.Height * fy, sd, sd);
                if (closed) Boards(g, r);
                else if (lit && year >= 2001) Light(lights, r, cs, Color.FromArgb(255, 214, 140), 4.2f, 0.55f);
                break;
            }
            case B.School:
            case B.Museum:
            case B.Hub:
            case B.Club:
            case B.Shop:
            case B.Fap:
            {
                Shadow(g, r, cs);
                Color ra, rb;
                if (b.Type == B.Hub) { ra = Color.FromArgb(52, 60, 78); rb = Color.FromArgb(40, 46, 62); }
                else if (b.Type == B.Museum) { ra = Color.FromArgb(190, 136, 82); rb = Color.FromArgb(160, 110, 64); }
                else if (modern) { ra = Color.FromArgb(226, 232, 240); rb = Color.FromArgb(196, 204, 216); }
                else if (b.Type == B.School) { ra = Color.FromArgb(222, 170, 72); rb = Color.FromArgb(192, 142, 56); }
                else if (b.Type == B.Club) { ra = Color.FromArgb(176, 70, 64); rb = Color.FromArgb(146, 56, 52); }
                else if (b.Type == B.Shop) { ra = Color.FromArgb(80, 130, 190); rb = Color.FromArgb(64, 108, 162); }
                else { ra = Color.FromArgb(236, 236, 236); rb = Color.FromArgb(206, 206, 210); }
                if (closed) { ra = Theme.Mix(ra, Color.FromArgb(120, 116, 110), 0.65f); rb = Theme.Mix(rb, Color.FromArgb(100, 96, 92), 0.65f); }
                Roof(g, r, ra, rb);

                if (b.Type == B.Fap)
                {
                    using var red = new SolidBrush(closed ? Color.FromArgb(130, 110, 110) : Color.FromArgb(220, 50, 50));
                    float s = Math.Min(r.Width, r.Height) * 0.5f, cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
                    g.FillRectangle(red, cx - s / 2, cy - s / 6, s, s / 3);
                    g.FillRectangle(red, cx - s / 6, cy - s / 2, s / 3, s);
                }
                if (b.Type == B.Hub)
                {
                    using var glass = new Pen(Color.FromArgb(200, 90, 210, 255), Math.Max(1f, cs * 0.12f));
                    for (int k = 1; k < 4; k++) g.DrawLine(glass, r.X + r.Width * k / 4, r.Y + r.Height * 0.15f, r.X + r.Width * k / 4, r.Bottom - r.Height * 0.15f);
                }
                if (b.Type == B.Museum || restored)
                {
                    using var gold = new Pen(Color.FromArgb(230, 255, 210, 120), Math.Max(1f, cs * 0.12f));
                    g.DrawRectangle(gold, r.X, r.Y, r.Width, r.Height);
                }
                if (b.Type == B.Club && !closed)
                {
                    using var col = new Pen(Color.FromArgb(200, 250, 240, 230), Math.Max(1f, cs * 0.1f));
                    for (int k = 1; k < 5; k++) g.DrawLine(col, r.X + r.Width * k / 5, r.Bottom - r.Height * 0.3f, r.X + r.Width * k / 5, r.Bottom);
                }
                if (closed) { Boards(g, r); break; }

                if (b.Type == B.Hub) Light(lights, r, cs, Neon, 5f, 0.85f);
                else if (b.Type == B.Museum) Light(lights, r, cs, Color.FromArgb(255, 205, 120), 4.6f, 0.75f);
                else if (modern) Light(lights, r, cs, Color.FromArgb(220, 240, 255), 4.4f, 0.7f);
                else if ((b.Flags & F.Electric) != 0 || st.Tech[Te.Electric] > 0.4) Light(lights, r, cs, Warm, 3.6f, 0.55f);
                break;
            }
            case B.Farm:
            case B.Mts:
            {
                Shadow(g, r, cs);
                Color ra = b.Type == B.Farm ? Color.FromArgb(150, 150, 146) : Color.FromArgb(108, 124, 140);
                Color rb = Theme.Scale3(ra, 0.82f);
                if (closed) { ra = Theme.Mix(ra, Color.FromArgb(110, 100, 90), 0.5f); rb = Theme.Mix(rb, Color.FromArgb(90, 80, 72), 0.5f); }
                Roof(g, r, ra, rb);
                if (closed) { Boards(g, r); break; }
                if (b.Type == B.Mts && st.Tech[Te.Mech] > 0.2)
                {
                    using var tr = new SolidBrush(Color.FromArgb(230, 120, 40));
                    for (int k = 0; k < 3; k++)
                        g.FillRectangle(tr, r.Right + cs * 0.3f, r.Y + k * cs * 0.9f, cs * 0.7f, cs * 0.5f);
                }
                if (b.Type == B.Farm)
                {
                    using var cow = new SolidBrush(Color.FromArgb(240, 240, 236));
                    for (int k = 0; k < 5; k++)
                        g.FillEllipse(cow, r.X + Hash(b.Id, k, 41) * r.Width, r.Bottom + cs * 0.3f + Hash(b.Id, k, 42) * cs, cs * 0.35f, cs * 0.25f);
                }
                if (st.Tech[Te.Electric] > 0.5) Light(lights, r, cs, Warm, 3f, 0.4f);
                break;
            }
            case B.Greenhouse:
            {
                Shadow(g, r, cs);
                using (var gb = new SolidBrush(Color.FromArgb(200, 214, 244, 238))) g.FillRectangle(gb, r);
                using var arch = new Pen(Color.FromArgb(160, 90, 170, 160), Math.Max(1f, cs * 0.1f));
                for (int k = 1; k < 6; k++) g.DrawLine(arch, r.X + r.Width * k / 6, r.Y, r.X + r.Width * k / 6, r.Bottom);
                Light(lights, r, cs, Color.FromArgb(240, 90, 230), 4.6f, 0.8f);
                break;
            }
            case B.Solar:
            {
                using (var pb = new SolidBrush(Color.FromArgb(28, 52, 104))) g.FillRectangle(pb, r);
                using var grid = new Pen(Color.FromArgb(140, 130, 200, 255), 1f);
                for (int k = 1; k < 6; k++) g.DrawLine(grid, r.X + r.Width * k / 6, r.Y, r.X + r.Width * k / 6, r.Bottom);
                g.DrawLine(grid, r.X, r.Y + r.Height / 2, r.Right, r.Y + r.Height / 2);
                using var hl = new Pen(Color.FromArgb(90, 255, 255, 255), 1f);
                g.DrawLine(hl, r.X, r.Y, r.Right, r.Y);
                break;
            }
            case B.Wind:
            {
                using var bb = new SolidBrush(Color.FromArgb(240, 244, 248));
                g.FillEllipse(bb, full.X + cs * 0.25f, full.Y + cs * 0.25f, cs * 0.5f, cs * 0.5f);
                Light(lights, full, cs, Color.FromArgb(255, 40, 30), 1.4f, 0.9f, blink: true);
                break;
            }
            case B.DronePort:
            {
                using (var pad = new SolidBrush(Color.FromArgb(58, 64, 80))) g.FillRectangle(pad, r);
                float d = Math.Min(r.Width, r.Height) * 0.8f;
                var c = new RectangleF(r.X + (r.Width - d) / 2, r.Y + (r.Height - d) / 2, d, d);
                using var ring = new Pen(Color.FromArgb(230, 90, 220, 255), Math.Max(1f, cs * 0.15f));
                g.DrawEllipse(ring, c);
                Light(lights, r, cs, Neon, 3.6f, 0.7f, blink: true);
                break;
            }
            case B.Tower:
            {
                float cx = full.X + full.Width / 2, cy = full.Y + full.Height / 2;
                Color mast = b.Variant switch { 0 => Color.FromArgb(120, 100, 80), 1 => Color.FromArgb(220, 60, 50), 3 => Color.FromArgb(235, 240, 248), _ => Color.FromArgb(170, 176, 186) };
                using (var sh = new SolidBrush(Color.FromArgb(90, 0, 0, 0))) g.FillEllipse(sh, cx - cs * 0.3f + cs * 0.4f, cy - cs * 0.3f + cs * 0.4f, cs * 0.6f, cs * 0.6f);
                using (var mb = new SolidBrush(mast)) g.FillEllipse(mb, cx - cs * 0.38f, cy - cs * 0.38f, cs * 0.76f, cs * 0.76f);
                Color wave = b.Variant == 3 ? Theme.Cyan : (b.Variant == 0 ? Theme.Gold : Color.FromArgb(240, 240, 255));
                for (int k = 1; k <= 3; k++)
                {
                    using var wp = new Pen(Color.FromArgb(150 - k * 38, wave), Math.Max(1f, cs * 0.1f));
                    float rr = cs * (0.6f + k * 0.75f);
                    g.DrawArc(wp, cx - rr, cy - rr, rr * 2, rr * 2, -60, 120);
                    g.DrawArc(wp, cx - rr, cy - rr, rr * 2, rr * 2, 120, 120);
                }
                if (b.Variant >= 1) Light(lights, full, cs, Color.FromArgb(255, 40, 30), 1.6f, 0.95f, blink: true);
                break;
            }
            case B.Memorial:
            {
                float cx = full.X + full.Width / 2, cy = full.Y + full.Height / 2;
                using (var flowers = new SolidBrush(Color.FromArgb(220, 200, 40, 50)))
                    for (int k = 0; k < 8; k++)
                    {
                        double a = k * Math.PI / 4;
                        g.FillEllipse(flowers, cx + (float)Math.Cos(a) * cs * 1.05f - cs * 0.14f, cy + (float)Math.Sin(a) * cs * 1.05f - cs * 0.14f, cs * 0.28f, cs * 0.28f);
                    }
                using (var sh = new SolidBrush(Color.FromArgb(90, 0, 0, 0))) g.FillRectangle(sh, cx - cs * 0.3f + cs * 0.5f, cy - cs * 0.3f + cs * 0.3f, cs * 0.6f, cs * 0.6f);
                using (var ob = new SolidBrush(Color.FromArgb(236, 232, 222)))
                    g.FillPolygon(ob, new[] { new PointF(cx, cy - cs * 0.55f), new PointF(cx + cs * 0.4f, cy), new PointF(cx, cy + cs * 0.55f), new PointF(cx - cs * 0.4f, cy) });
                using (var star = new SolidBrush(Color.FromArgb(214, 40, 40))) g.FillEllipse(star, cx - cs * 0.15f, cy - cs * 0.15f, cs * 0.3f, cs * 0.3f);
                Light(lights, full, cs, Color.FromArgb(255, 130, 50), lit ? 2.6f : 1.6f, lit ? 0.9f : 0.5f);
                break;
            }
            case B.Mill:
            {
                Shadow(g, r, cs);
                Roof(g, r, closed ? Color.FromArgb(120, 108, 96) : Color.FromArgb(150, 116, 80), closed ? Color.FromArgb(100, 90, 80) : Color.FromArgb(124, 94, 64));
                using var wheel = new Pen(Color.FromArgb(200, 90, 64, 40), Math.Max(1f, cs * 0.18f));
                g.DrawEllipse(wheel, r.X - cs * 0.9f, r.Y + r.Height * 0.2f, cs * 0.8f, cs * 0.8f);
                if (closed) Boards(g, r);
                break;
            }
        }
    }
}

using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;

namespace Hronograf;

/// <summary>
/// Главный «видоискатель»: кадр села → объектив эпохи (C++) → интерфейс камеры эпохи.
/// </summary>
public sealed class MapView : Control
{
    public Chronicle? Chronicle { get; set; }
    public float YearF { get; set; } = 1900;
    public int Year => (int)Math.Floor(YearF);
    public float Night { get; set; }
    public float Time { get; set; }
    public int Frame { get; set; }
    public double LastLensMs { get; private set; }
    public bool ShowTodayCard { get; set; }
    public float FlashAlpha { get; set; }

    SceneRenderer.Result? scene;
    ChronicleEvent? newestEvent;
    readonly RectangleF[] todayButtons = new RectangleF[3];

    /// <summary>Клик по карточке сценария на развилке «сегодня».</summary>
    public event Action<int>? ScenarioPicked;

    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);
        if (!ShowTodayCard) return;
        for (int s = 0; s < 3; s++)
            if (todayButtons[s].Contains(e.Location)) { ScenarioPicked?.Invoke(s); return; }
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        Cursor = ShowTodayCard && todayButtons.Any(b => b.Contains(e.Location)) ? Cursors.Hand : Cursors.Default;
    }
    float newestSince;
    Bitmap? frame;
    int sceneYear = int.MinValue, sceneScenario = -1;
    Size sceneSize;
    RectangleF mapRect;

    public MapView()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;
    }

    public void ResetScene() => sceneYear = int.MinValue;

    protected override void Dispose(bool disposing)
    {
        if (disposing) { scene?.Dispose(); frame?.Dispose(); }
        base.Dispose(disposing);
    }

    void EnsureScene()
    {
        if (Chronicle == null) return;
        int pad = Theme.Px(18);
        var avail = new Size(Math.Max(64, Width - pad * 2), Math.Max(64, Height - pad * 2));
        float cs = Math.Min(avail.Width / (float)Chronicle.MapW, avail.Height / (float)Chronicle.MapH);
        var size = new Size((int)(Chronicle.MapW * cs), (int)(Chronicle.MapH * cs));
        mapRect = new RectangleF((Width - size.Width) / 2f, (Height - size.Height) / 2f, size.Width, size.Height);

        if (scene != null && sceneYear == Year && sceneScenario == Chronicle.Scenario && sceneSize == size) return;
        scene?.Dispose();
        scene = SceneRenderer.Render(Chronicle, Year, size);
        sceneYear = Year; sceneScenario = Chronicle.Scenario; sceneSize = size;
        if (frame == null || frame.Size != scene.Bitmap.Size)
        {
            frame?.Dispose();
            frame = new Bitmap(scene.Bitmap.Width, scene.Bitmap.Height, PixelFormat.Format32bppPArgb);
        }
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Bg);
        if (Chronicle == null) return;
        EnsureScene();
        if (scene == null || frame == null) return;

        // 1) Копия чистого кадра → обработка объективом эпохи в C++.
        using (var fg = Graphics.FromImage(frame))
        {
            fg.CompositingMode = CompositingMode.SourceCopy;
            fg.DrawImageUnscaled(scene.Bitmap, 0, 0);
        }
        var bits = frame.LockBits(new Rectangle(0, 0, frame.Width, frame.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppPArgb);
        var lens = new HgLens { Year = YearF, Night = Night, Time = Time, Frame = Frame };
        LastLensMs = Native.hg_lens(bits.Scan0, frame.Width, frame.Height, bits.Stride, scene.Lights, scene.Lights.Length, ref lens);
        frame.UnlockBits(bits);

        Theme.Quality(g, clearType: false);
        // Тень под «фотографией».
        for (int i = 6; i >= 1; i--)
            Theme.FillRound(g, RectangleF.Inflate(mapRect, i * 2.2f, i * 2.2f), Theme.Pf(10 + i * 2), Color.FromArgb(10, 0, 0, 0));
        g.InterpolationMode = InterpolationMode.NearestNeighbor;
        g.PixelOffsetMode = PixelOffsetMode.Half;
        g.DrawImageUnscaled(frame, (int)mapRect.X, (int)mapRect.Y);
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;

        // 2) Интерфейс «камеры» своей эпохи.
        var state = g.Save();
        g.SetClip(RectangleF.Inflate(mapRect, Theme.Pf(14), Theme.Pf(14)));
        if (Year < 1960) FilmOverlay();
        else if (Year < 1985) PrintOverlay();
        else if (Year < 2006) SoapboxOverlay();
        else if (Year <= 2026) PhoneOverlay();
        else ArOverlay();
        g.Restore(state);

        LensBadge(g);
        Toasts(g);
        if (ShowTodayCard) TodayCard(g);
        if (FlashAlpha > 0.01f)
            using (var fb = new SolidBrush(Color.FromArgb((int)(FlashAlpha * 255), 255, 252, 240)))
                g.FillRectangle(fb, mapRect);

        // ---- локальные функции оверлеев ----
        void FilmOverlay()
        {
            // Плёнка: тёмные поля с перфорацией сверху и снизу.
            float band = Math.Max(Theme.Pf(16), mapRect.Height * 0.045f);
            using var black = new SolidBrush(Color.FromArgb(235, 14, 12, 10));
            g.FillRectangle(black, mapRect.X, mapRect.Y, mapRect.Width, band);
            g.FillRectangle(black, mapRect.X, mapRect.Bottom - band, mapRect.Width, band);
            float hw = band * 0.75f, hh = band * 0.48f, step = hw * 2.2f;
            float shift = (Frame * 1.5f) % step;
            using var hole = new SolidBrush(Color.FromArgb(200, 214, 200, 170));
            for (float x = mapRect.X - shift + step * 0.3f; x < mapRect.Right; x += step)
            {
                Theme.FillRound(g, new RectangleF(x, mapRect.Y + (band - hh) / 2, hw, hh), hh * 0.3f, hole.Color);
                Theme.FillRound(g, new RectangleF(x, mapRect.Bottom - band + (band - hh) / 2, hw, hh), hh * 0.3f, hole.Color);
            }
            var cap = $"{Chronicle!.Name}, {Year} г.";
            var f = Theme.F(15, FontStyle.Italic, Theme.Serif);
            var sz = Theme.Measure(g, cap, f);
            Theme.Str(g, cap, f, Color.FromArgb(235, 236, 222, 190), mapRect.Right - sz.Width - Theme.Pf(22), mapRect.Bottom - band - sz.Height - Theme.Pf(10));
        }

        void PrintOverlay()
        {
            // Отпечаток с белыми полями и подписью от руки.
            float m = Theme.Pf(12);
            using var paper = new SolidBrush(Color.FromArgb(246, 241, 232, 214));
            using var path = new GraphicsPath { FillMode = FillMode.Alternate };
            path.AddRectangle(RectangleF.Inflate(mapRect, 1, 1));
            path.AddRectangle(RectangleF.Inflate(mapRect, -m, -m));
            g.FillPath(paper, path);
            var cap = $"{Chronicle!.Name}, лето {Year}";
            var f = Theme.F(14, FontStyle.Regular, Theme.Script);
            var sz = Theme.Measure(g, cap, f);
            var r = new RectangleF(mapRect.Right - sz.Width - m - Theme.Pf(18), mapRect.Bottom - m - sz.Height - Theme.Pf(14), sz.Width + Theme.Pf(16), sz.Height + Theme.Pf(8));
            Theme.FillRound(g, r, Theme.Pf(3), Color.FromArgb(200, 241, 232, 214));
            Theme.Str(g, cap, f, Color.FromArgb(60, 70, 120), r.X + Theme.Pf(8), r.Y + Theme.Pf(4));
        }

        void SoapboxOverlay()
        {
            // Оранжевая дата «мыльницы».
            var date = $"'{Year % 100:00}  {1 + (Year * 7) % 12}  {1 + (Year * 13) % 28}";
            var f = Theme.F(20, FontStyle.Bold, Theme.Mono);
            var sz = Theme.Measure(g, date, f);
            float x = mapRect.Right - sz.Width - Theme.Pf(26), y = mapRect.Bottom - sz.Height - Theme.Pf(22);
            Theme.GlowText(g, date, f, Color.FromArgb(255, 140, 40), x, y, 3);
        }

        void PhoneOverlay()
        {
            using var grid = new Pen(Color.FromArgb(70, 255, 255, 255), 1f);
            for (int k = 1; k < 3; k++)
            {
                g.DrawLine(grid, mapRect.X + mapRect.Width * k / 3, mapRect.Y, mapRect.X + mapRect.Width * k / 3, mapRect.Bottom);
                g.DrawLine(grid, mapRect.X, mapRect.Y + mapRect.Height * k / 3, mapRect.Right, mapRect.Y + mapRect.Height * k / 3);
            }
            // Рамка фокуса.
            float s = Math.Min(mapRect.Width, mapRect.Height) * 0.16f;
            float cx = mapRect.X + mapRect.Width / 2, cy = mapRect.Y + mapRect.Height / 2;
            using var focus = new Pen(Color.FromArgb(230, 255, 214, 70), Theme.Pf(2));
            Corners(g, focus, new RectangleF(cx - s, cy - s * 0.7f, s * 2, s * 1.4f), s * 0.3f);
            // Кнопка спуска и индикаторы.
            float br = Theme.Pf(20), sx = mapRect.Right - br - Theme.Pf(18);
            using var ring = new Pen(Color.FromArgb(220, 255, 255, 255), Theme.Pf(3));
            g.DrawEllipse(ring, sx - br, cy - br, br * 2, br * 2);
            using var dot = new SolidBrush(Color.FromArgb(200, 255, 255, 255));
            g.FillEllipse(dot, sx - br * 0.72f, cy - br * 0.72f, br * 1.44f, br * 1.44f);
            var f = Theme.F(9, FontStyle.Bold);
            Theme.Str(g, "HDR   4K·60", f, Color.FromArgb(230, 255, 255, 255), mapRect.X + Theme.Pf(16), mapRect.Y + Theme.Pf(14));
            var t = $"{(Year >= 2016 ? "5G" : Year >= 2012 ? "4G" : "3G")}   12:{(Frame / 30) % 60:00}";
            var sz = Theme.Measure(g, t, f);
            Theme.Str(g, t, f, Color.FromArgb(230, 255, 255, 255), mapRect.Right - sz.Width - Theme.Pf(16), mapRect.Y + Theme.Pf(14));
        }

        void ArOverlay()
        {
            var cyan = Theme.Cyan;
            using var pen = new Pen(Color.FromArgb(220, cyan), Theme.Pf(2));
            Corners(g, pen, RectangleF.Inflate(mapRect, -Theme.Pf(10), -Theme.Pf(10)), Theme.Pf(34));
            // Сканирующая линия.
            float sy = mapRect.Y + (Time * 0.18f % 1f) * mapRect.Height;
            using (var scan = new LinearGradientBrush(new RectangleF(mapRect.X, sy - Theme.Pf(40), mapRect.Width, Theme.Pf(41)),
                       Color.FromArgb(0, cyan), Color.FromArgb(70, cyan), LinearGradientMode.Vertical))
                g.FillRectangle(scan, mapRect.X, sy - Theme.Pf(40), mapRect.Width, Theme.Pf(40));
            // Прицел.
            float cx = mapRect.X + mapRect.Width / 2, cy = mapRect.Y + mapRect.Height / 2, r = Theme.Pf(16);
            using var thin = new Pen(Color.FromArgb(160, cyan), 1f);
            g.DrawEllipse(thin, cx - r, cy - r, r * 2, r * 2);
            g.DrawLine(thin, cx - r * 1.8f, cy, cx - r * 0.5f, cy); g.DrawLine(thin, cx + r * 0.5f, cy, cx + r * 1.8f, cy);
            g.DrawLine(thin, cx, cy - r * 1.8f, cx, cy - r * 0.5f); g.DrawLine(thin, cx, cy + r * 0.5f, cx, cy + r * 1.8f);

            var mono = Theme.F(9, FontStyle.Bold, Theme.Mono);
            var st = Chronicle!.Stats(Year);
            Theme.FillRound(g, new RectangleF(mapRect.X + Theme.Pf(14), mapRect.Y + Theme.Pf(14), Theme.Pf(470), Theme.Pf(40)),
                Theme.Pf(6), Color.FromArgb(150, 4, 16, 28));
            Theme.Str(g, $"ДРОН-КАРТОГРАФ · ВЫС 180 м · {Year}", mono, Color.FromArgb(230, cyan), mapRect.X + Theme.Pf(22), mapRect.Y + Theme.Pf(20));
            Theme.Str(g, $"ИИ-АНАЛИЗ: {Texts.ScenarioNames[Chronicle.Scenario].ToUpperInvariant()} · ЖИТЕЛЕЙ {st.Population} · КАЧЕСТВО {st.Quality:0}",
                mono, Color.FromArgb(200, cyan), mapRect.X + Theme.Pf(22), mapRect.Y + Theme.Pf(36));

            // Дроны и турбины — живая анимация поверх кадра.
            foreach (var b in scene!.Buildings)
            {
                float bx = mapRect.X + (b.X + b.W / 2f) * scene.Cell, by = mapRect.Y + (b.Y + b.H / 2f) * scene.Cell;
                if (b.Type == B.Wind)
                {
                    using var blade = new Pen(Color.FromArgb(235, 255, 255, 255), Theme.Pf(2));
                    float L = scene.Cell * 2.2f;
                    for (int k = 0; k < 3; k++)
                    {
                        double a = Time * 2.4 + k * Math.PI * 2 / 3 + b.Id;
                        g.DrawLine(blade, bx, by, bx + (float)Math.Cos(a) * L, by + (float)Math.Sin(a) * L);
                    }
                }
                if (b.Type == B.DronePort)
                    for (int k = 0; k < 3; k++)
                    {
                        double t = Time * (0.25 + k * 0.07) + k * 2.1;
                        float dx = bx + (float)Math.Sin(t * 1.3) * scene.Cell * (10 + k * 4);
                        float dy = by + (float)Math.Sin(t * 2.1 + k) * scene.Cell * (6 + k * 3);
                        using var db = new SolidBrush(Color.FromArgb(240, 230, 250, 255));
                        g.FillEllipse(db, dx - Theme.Pf(3), dy - Theme.Pf(3), Theme.Pf(6), Theme.Pf(6));
                        using var halo = new Pen(Color.FromArgb(160, cyan), 1f);
                        g.DrawEllipse(halo, dx - Theme.Pf(7), dy - Theme.Pf(7), Theme.Pf(14), Theme.Pf(14));
                    }
            }

            // Подписи ключевых объектов (дополненная реальность).
            var tag = Theme.F(8.5f, FontStyle.Bold);
            var seen = new HashSet<int>();
            var placed = new List<RectangleF> { new(mapRect.X, mapRect.Y, Theme.Pf(490), Theme.Pf(84)) };
            foreach (var b in scene.Buildings)
            {
                string? label = b.Type switch
                {
                    B.Hub => "Цифровой хаб",
                    B.Museum => "Центр памяти",
                    B.Greenhouse when (b.Flags & F.Modern) != 0 => "Умные теплицы",
                    B.Solar => "Солнечная станция",
                    B.DronePort => "Дронопорт",
                    B.School when (b.Flags & F.Modern) != 0 => "Новая школа",
                    B.Memorial => "Обелиск",
                    B.Church => "Храм",
                    B.Wind => "Ветропарк",
                    B.Tower when b.Variant == 3 => "5G",
                    _ => null
                };
                if (label == null || !seen.Add(b.Type)) continue;
                float bx = mapRect.X + (b.X + b.W / 2f) * scene.Cell, by = mapRect.Y + b.Y * scene.Cell;
                var sz = Theme.Measure(g, label, tag);
                var tr = new RectangleF(bx + Theme.Pf(14), by - Theme.Pf(30), sz.Width + Theme.Pf(14), sz.Height + Theme.Pf(6));
                if (tr.Right > mapRect.Right - 4) tr.X = bx - Theme.Pf(14) - tr.Width;
                if (tr.Y < mapRect.Y + 4) tr.Y = by + Theme.Pf(16);
                for (int k = 0; k < 6 && placed.Any(p => p.IntersectsWith(RectangleF.Inflate(tr, 3, 2))); k++)
                {
                    if (k == 2) tr.X = tr.X > bx ? bx - Theme.Pf(14) - tr.Width : bx + Theme.Pf(14);
                    tr.Y += (k % 2 == 0 ? -1 : 1) * (tr.Height + Theme.Pf(4)) * (k / 2 + 1);
                }
                placed.Add(tr);
                g.DrawLine(thin, bx, by, tr.X < bx ? tr.Right : tr.X, tr.Y + tr.Height / 2);
                g.FillEllipse(Brushes.White, bx - 2, by - 2, 4, 4);
                Theme.FillRound(g, tr, tr.Height / 2, Color.FromArgb(190, 8, 30, 46));
                Theme.StrokeRound(g, tr, tr.Height / 2, Color.FromArgb(200, cyan));
                Theme.Str(g, label, tag, Color.FromArgb(240, 220, 248, 255), tr.X + Theme.Pf(7), tr.Y + Theme.Pf(3));
            }
        }
    }

    static void Corners(Graphics g, Pen pen, RectangleF r, float len)
    {
        g.DrawLines(pen, new[] { new PointF(r.X, r.Y + len), new PointF(r.X, r.Y), new PointF(r.X + len, r.Y) });
        g.DrawLines(pen, new[] { new PointF(r.Right - len, r.Y), new PointF(r.Right, r.Y), new PointF(r.Right, r.Y + len) });
        g.DrawLines(pen, new[] { new PointF(r.X, r.Bottom - len), new PointF(r.X, r.Bottom), new PointF(r.X + len, r.Bottom) });
        g.DrawLines(pen, new[] { new PointF(r.Right - len, r.Bottom), new PointF(r.Right, r.Bottom), new PointF(r.Right, r.Bottom - len) });
    }

    void LensBadge(Graphics g)
    {
        var text = "ОБЪЕКТИВ · " + Texts.Lens(Year).ToUpperInvariant();
        var f = Theme.F(8.5f, FontStyle.Bold);
        var sz = Theme.Measure(g, text, f);
        float top = Year < 1960 ? mapRect.Y + Math.Max(Theme.Pf(16), mapRect.Height * 0.045f) + Theme.Pf(8) : mapRect.Y + Theme.Pf(14);
        if (Year > 2026) top = mapRect.Y + Theme.Pf(56);
        if (Year >= 2006 && Year <= 2026) top = mapRect.Y + Theme.Pf(36);
        var r = new RectangleF(mapRect.X + Theme.Pf(16), top, sz.Width + Theme.Pf(30), sz.Height + Theme.Pf(10));
        Theme.FillRound(g, r, r.Height / 2, Color.FromArgb(170, 8, 12, 24));
        Theme.StrokeRound(g, r, r.Height / 2, Color.FromArgb(90, 255, 255, 255));
        float pulse = 0.5f + 0.5f * (float)Math.Sin(Time * 4);
        using (var dot = new SolidBrush(Color.FromArgb((int)(140 + 115 * pulse), 255, 70, 60)))
            g.FillEllipse(dot, r.X + Theme.Pf(9), r.Y + r.Height / 2 - Theme.Pf(4), Theme.Pf(8), Theme.Pf(8));
        Theme.Str(g, text, f, Color.FromArgb(235, 240, 244, 255), r.X + Theme.Pf(22), r.Y + Theme.Pf(5));
    }

    void Toasts(Graphics g)
    {
        var events = Chronicle!.Events().Where(e => e.Year <= Year).TakeLast(3).ToList();
        if (events.Count == 0) return;
        if (!ReferenceEquals(events[^1], newestEvent)) { newestEvent = events[^1]; newestSince = Time; }
        float w = Math.Min(mapRect.Width * 0.56f, Theme.Pf(520));
        float x = mapRect.X + Theme.Pf(16);
        float bottomInset = Year < 1960 ? Math.Max(Theme.Pf(16), mapRect.Height * 0.045f) : (Year < 1985 ? Theme.Pf(12) : 0);
        float y = mapRect.Bottom - bottomInset - Theme.Pf(14);
        var fText = Theme.F(9.5f);
        var fYear = Theme.F(9.5f, FontStyle.Bold, Theme.Display);
        for (int i = events.Count - 1; i >= 0; i--)
        {
            var ev = events[i];
            bool newest = i == events.Count - 1;
            int alpha = newest ? 255 : (int)Math.Clamp(200 - (events.Count - 1 - i) * 55, 60, 255);
            var size = g.MeasureString(ev.Text, fText, (int)(w - Theme.Pf(78)));
            float h = Math.Max(Theme.Pf(34), size.Height + Theme.Pf(14));
            y -= h;
            float slide = newest ? Math.Max(0, 1 - (Time - newestSince) * 3f) * Theme.Pf(24) : 0;
            var r = new RectangleF(x + slide, y, w, h);
            var accent = ev.Scenario >= 0 ? Theme.ScenarioColors[ev.Scenario] : Theme.Gold;
            Theme.FillRound(g, r, Theme.Pf(10), Color.FromArgb((int)(alpha * 0.86f), 10, 15, 30));
            Theme.StrokeRound(g, r, Theme.Pf(10), Theme.A(accent, newest ? 170 : 60));
            var yr = new RectangleF(r.X + Theme.Pf(8), r.Y + (h - Theme.Pf(22)) / 2, Theme.Pf(52), Theme.Pf(22));
            Theme.FillRound(g, yr, Theme.Pf(11), Theme.A(accent, newest ? 230 : 110));
            Theme.TextIn(g, ev.Year.ToString(), fYear, Color.FromArgb(alpha, 12, 14, 24), yr, StringAlignment.Center);
            using var tb = new SolidBrush(Color.FromArgb(alpha, Theme.Text));
            g.DrawString(ev.Text, fText, tb, new RectangleF(r.X + Theme.Pf(68), r.Y + Theme.Pf(7), w - Theme.Pf(78), h - Theme.Pf(10)));
            y -= Theme.Pf(6);
        }
    }

    void TodayCard(Graphics g)
    {
        var c = Chronicle!;
        float w = Math.Min(mapRect.Width * 0.72f, Theme.Pf(600)), h = Theme.Pf(226);
        var r = new RectangleF(mapRect.X + (mapRect.Width - w) / 2, mapRect.Y + mapRect.Height * 0.16f, w, h);
        Theme.Card(g, r, Theme.Pf(16), Color.FromArgb(225, 10, 15, 32), Theme.A(Theme.Gold, 160));
        Theme.TextIn(g, "СЕГОДНЯ · " + c.Today, Theme.F(22, FontStyle.Regular, Theme.Display), Theme.Gold,
            new RectangleF(r.X, r.Y + Theme.Pf(14), r.Width, Theme.Pf(40)), StringAlignment.Center);
        Theme.TextIn(g, "История закончилась. Будущее — это выбор. Каким оно будет для села «" + c.Name + "»?",
            Theme.F(10), Theme.Text, new RectangleF(r.X + Theme.Pf(20), r.Y + Theme.Pf(56), r.Width - Theme.Pf(40), Theme.Pf(40)),
            StringAlignment.Center, StringAlignment.Center, ellipsis: false);
        float bw = (r.Width - Theme.Pf(56)) / 3;
        for (int s = 0; s < 3; s++)
        {
            var br = new RectangleF(r.X + Theme.Pf(20) + s * (bw + Theme.Pf(8)), r.Y + Theme.Pf(104), bw, Theme.Pf(102));
            todayButtons[s] = br;
            bool sel = s == c.Scenario;
            var col = Theme.ScenarioColors[s];
            Theme.FillRound(g, br, Theme.Pf(10), Theme.A(col, sel ? 60 : 18));
            Theme.StrokeRound(g, br, Theme.Pf(10), Theme.A(col, sel ? 230 : 90), sel ? 2 : 1);
            Theme.TextIn(g, $"{s + 1} · {Texts.ScenarioNames[s]}", Theme.F(11, FontStyle.Bold), col,
                new RectangleF(br.X, br.Y + Theme.Pf(8), br.Width, Theme.Pf(22)), StringAlignment.Center);
            Theme.TextIn(g, Texts.ScenarioTaglines[s], Theme.F(8), Theme.Muted,
                new RectangleF(br.X + Theme.Pf(8), br.Y + Theme.Pf(32), br.Width - Theme.Pf(16), Theme.Pf(64)),
                StringAlignment.Center, StringAlignment.Near, ellipsis: false);
        }
    }
}

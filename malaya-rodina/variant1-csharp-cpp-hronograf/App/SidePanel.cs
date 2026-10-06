using System.Drawing.Drawing2D;

namespace Hronograf;

/// <summary>Правая панель: год и эпоха, показатели, технологии, график трёх сценариев.</summary>
public sealed class SidePanel : Control
{
    public Chronicle? Chronicle { get; set; }
    public float YearF { get; set; } = 1900;
    int Year => (int)Math.Floor(YearF);

    public SidePanel()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Bg);
        Theme.Quality(g);
        if (Chronicle == null) return;
        var c = Chronicle;
        var st = c.Stats(Year);
        var prev = c.Stats(Year - 10);
        int sc = c.Scenario;
        var scCol = Year > c.Today ? Theme.ScenarioColors[sc] : Theme.Gold;

        float pad = Theme.Pf(14), x = pad, w = Width - pad * 2, y = Theme.Pf(10);

        // ---- Год и эпоха ----
        var head = new RectangleF(x, y, w, Theme.Pf(92));
        Theme.Card(g, head, Theme.Pf(14), Theme.Panel, Theme.Border);
        using (var glow = new LinearGradientBrush(head, Theme.A(scCol, 46), Theme.A(scCol, 0), LinearGradientMode.Horizontal))
        using (var path = Theme.Round(head, Theme.Pf(14)))
            g.FillPath(glow, path);
        Theme.Str(g, Year.ToString(), Theme.F(36, FontStyle.Regular, Theme.DisplayLight), Theme.Text, head.X + Theme.Pf(14), head.Y + Theme.Pf(8));
        Theme.TextIn(g, Texts.Era(Year, sc), Theme.F(11.5f, FontStyle.Bold), scCol,
            new RectangleF(head.X + Theme.Pf(132), head.Y + Theme.Pf(16), head.Width - Theme.Pf(144), Theme.Pf(24)));
        string sub = Year > c.Today ? $"Итог: «{Texts.ScenarioFates[sc]}»"
                                    : $"История · до «сегодня» {c.Today - Year} лет";
        Theme.TextIn(g, sub, Theme.F(8.5f), Theme.Muted,
            new RectangleF(head.X + Theme.Pf(132), head.Y + Theme.Pf(42), head.Width - Theme.Pf(144), Theme.Pf(20)));
        // прогресс по шкале времени
        float t = (YearF - c.FirstYear) / (c.LastYear - c.FirstYear);
        var bar = new RectangleF(head.X + Theme.Pf(14), head.Bottom - Theme.Pf(16), head.Width - Theme.Pf(28), Theme.Pf(4));
        Theme.FillRound(g, bar, bar.Height / 2, Theme.A(Theme.Text, 24));
        Theme.FillRound(g, new RectangleF(bar.X, bar.Y, Math.Max(bar.Height, bar.Width * t), bar.Height), bar.Height / 2, scCol);
        y = head.Bottom + Theme.Pf(10);

        // ---- Показатели ----
        var cards = new (string label, string value, string delta, float level, Color color)[]
        {
            ("Население", st.Population.ToString(), Delta(st.Population - prev.Population), Math.Min(1f, st.Population / 650f), Theme.Gold),
            ("Дворы", $"{st.Houses}", st.Abandoned > 0 ? $"брошено {st.Abandoned}" : "все жилые", st.Sites > 0 ? st.Houses / (float)st.Sites : 0, Theme.Coral),
            ("Качество жизни", $"{st.Quality:0}", Delta((int)Math.Round(st.Quality - prev.Quality)), (float)st.Quality / 100f, Theme.Green),
            ("Связь", $"{st.Connectivity:0}", Delta((int)Math.Round(st.Connectivity - prev.Connectivity)), (float)st.Connectivity / 100f, Theme.Cyan),
            ("Память", $"{st.Memory:0}", Delta((int)Math.Round(st.Memory - prev.Memory)), (float)st.Memory / 100f, Theme.Violet),
            ("Экология", $"{st.Ecology:0}", Delta((int)Math.Round(st.Ecology - prev.Ecology)), (float)st.Ecology / 100f, Theme.Green),
        };
        float gap = Theme.Pf(8), cw = (w - gap * 2) / 3, ch = Theme.Pf(64);
        for (int i = 0; i < cards.Length; i++)
        {
            var (label, value, delta, level, color) = cards[i];
            var r = new RectangleF(x + (i % 3) * (cw + gap), y + (i / 3) * (ch + gap), cw, ch);
            Theme.Card(g, r, Theme.Pf(10), Theme.Panel, Theme.Border);
            Theme.TextIn(g, label, Theme.F(7.5f), Theme.Muted, new RectangleF(r.X + Theme.Pf(10), r.Y + Theme.Pf(6), r.Width - Theme.Pf(14), Theme.Pf(16)));
            Theme.Str(g, value, Theme.F(16, FontStyle.Regular, Theme.Display), Theme.Text, r.X + Theme.Pf(9), r.Y + Theme.Pf(21));
            var dc = delta.StartsWith("▲") ? Theme.Green : delta.StartsWith("▼") ? Theme.Coral : Theme.Muted;
            Theme.TextIn(g, delta, Theme.F(7.5f, FontStyle.Bold), dc, new RectangleF(r.X + Theme.Pf(8), r.Y + Theme.Pf(24), r.Width - Theme.Pf(16), Theme.Pf(18)), StringAlignment.Far);
            var lb = new RectangleF(r.X + Theme.Pf(10), r.Bottom - Theme.Pf(10), r.Width - Theme.Pf(20), Theme.Pf(3));
            Theme.FillRound(g, lb, 2, Theme.A(Theme.Text, 20));
            Theme.FillRound(g, new RectangleF(lb.X, lb.Y, Math.Max(3, lb.Width * Math.Clamp(level, 0, 1)), lb.Height), 2, color);
        }
        y += ch * 2 + gap + Theme.Pf(12);

        // ---- Технологии ----
        Theme.Str(g, "ТЕХНОЛОГИИ В СЕЛЕ", Theme.F(8, FontStyle.Bold), Theme.Muted, x + 2, y);
        Theme.TextIn(g, "доля жителей, которым доступно", Theme.F(7.5f), Theme.A(Theme.Muted, 160), new RectangleF(x, y, w, Theme.Pf(14)), StringAlignment.Far, StringAlignment.Near);
        y += Theme.Pf(18);
        float colW = (w - gap) / 2, rowH = Theme.Pf(25);
        for (int i = 0; i < Native.TechCount; i++)
        {
            int col = i < 7 ? 0 : 1, row = i < 7 ? i : i - 7;
            var r = new RectangleF(x + col * (colW + gap), y + row * rowH, colW, rowH - Theme.Pf(4));
            double v = st.Tech[i];
            var tc = Theme.TechColors[i];
            bool active = v > 0.02;
            using (var db = new SolidBrush(active ? tc : Theme.A(Theme.Muted, 80)))
                g.FillEllipse(db, r.X + Theme.Pf(2), r.Y + Theme.Pf(4), Theme.Pf(7), Theme.Pf(7));
            Theme.TextIn(g, Texts.TechNames[i], Theme.F(8), active ? Theme.Text : Theme.A(Theme.Muted, 170),
                new RectangleF(r.X + Theme.Pf(14), r.Y, r.Width - Theme.Pf(50), Theme.Pf(15)));
            Theme.TextIn(g, $"{v * 100:0}%", Theme.F(8, FontStyle.Bold), active ? tc : Theme.A(Theme.Muted, 140),
                new RectangleF(r.X, r.Y, r.Width, Theme.Pf(15)), StringAlignment.Far);
            var tb = new RectangleF(r.X + Theme.Pf(14), r.Y + Theme.Pf(16), r.Width - Theme.Pf(14), Theme.Pf(3));
            Theme.FillRound(g, tb, 2, Theme.A(Theme.Text, 16));
            if (v > 0.005) Theme.FillRound(g, new RectangleF(tb.X, tb.Y, Math.Max(3, (float)(tb.Width * v)), tb.Height), 2, tc);
        }
        y += 7 * rowH + Theme.Pf(8);

        // ---- График: население по трём сценариям ----
        var chart = new RectangleF(x, y, w, Math.Max(Theme.Pf(110), Height - y - Theme.Pf(12)));
        Theme.Card(g, chart, Theme.Pf(12), Theme.Panel, Theme.Border);
        Theme.Str(g, "НАСЕЛЕНИЕ · 1900–2060", Theme.F(8, FontStyle.Bold), Theme.Muted, chart.X + Theme.Pf(12), chart.Y + Theme.Pf(9));
        DrawChart(g, new RectangleF(chart.X + Theme.Pf(12), chart.Y + Theme.Pf(30), chart.Width - Theme.Pf(24), chart.Height - Theme.Pf(52)), c);
    }

    static string Delta(int d) => d > 0 ? $"▲ {d}" : d < 0 ? $"▼ {-d}" : "—";

    void DrawChart(Graphics g, RectangleF r, Chronicle c)
    {
        int maxPop = 0;
        for (int s = 0; s < Chronicle.Scenarios; s++)
            for (int yv = c.FirstYear; yv <= c.LastYear; yv++) maxPop = Math.Max(maxPop, c.Stats(yv, s).Population);
        maxPop = (int)(Math.Ceiling(maxPop / 100.0) * 100);
        float X(float yr) => r.X + (yr - c.FirstYear) / (c.LastYear - c.FirstYear) * r.Width;
        float Y(float p) => r.Bottom - p / maxPop * r.Height;

        using var gridPen = new Pen(Theme.A(Theme.Text, 18), 1f);
        var small = Theme.F(7);
        for (int k = 0; k <= 2; k++)
        {
            float gy = r.Bottom - r.Height * k / 2;
            g.DrawLine(gridPen, r.X, gy, r.Right, gy);
            if (k > 0) Theme.Str(g, (maxPop * k / 2).ToString(), small, Theme.A(Theme.Muted, 170), r.X + 2, gy + 1);
        }
        foreach (var yr in new[] { 1900, 1941, 1991, c.Today, 2060 })
        {
            float gx = X(yr);
            var s = yr.ToString();
            var sz = Theme.Measure(g, s, small);
            float tx = Math.Clamp(gx - sz.Width / 2, r.X, r.Right - sz.Width);
            Theme.Str(g, s, small, yr == c.Today ? Theme.Gold : Theme.A(Theme.Muted, 190), tx, r.Bottom + Theme.Pf(4));
        }
        // будущее подсвечено
        using (var fut = new SolidBrush(Theme.A(Theme.ScenarioColors[c.Scenario], 14)))
            g.FillRectangle(fut, X(c.Today), r.Y, r.Right - X(c.Today), r.Height);
        using (var todayPen = new Pen(Theme.A(Theme.Gold, 120), 1f) { DashStyle = DashStyle.Dash })
            g.DrawLine(todayPen, X(c.Today), r.Y, X(c.Today), r.Bottom);

        PointF[] Line(int s, int from, int to)
        {
            var pts = new List<PointF>();
            for (int yv = from; yv <= to; yv++) pts.Add(new PointF(X(yv), Y(c.Stats(yv, s).Population)));
            return pts.ToArray();
        }
        // Заливка истории.
        var hist = Line(c.Scenario, c.FirstYear, c.Today);
        using (var area = new GraphicsPath())
        {
            area.AddLines(hist);
            area.AddLine(hist[^1], new PointF(hist[^1].X, r.Bottom));
            area.AddLine(new PointF(hist[^1].X, r.Bottom), new PointF(hist[0].X, r.Bottom));
            area.CloseFigure();
            using var ab = new LinearGradientBrush(r, Theme.A(Theme.Gold, 70), Theme.A(Theme.Gold, 0), LinearGradientMode.Vertical);
            g.FillPath(ab, area);
        }
        using (var hp = new Pen(Theme.Gold, Theme.Pf(2)) { LineJoin = LineJoin.Round }) g.DrawLines(hp, hist);
        for (int s = 0; s < Chronicle.Scenarios; s++)
        {
            bool sel = s == c.Scenario;
            using var p = new Pen(Theme.A(Theme.ScenarioColors[s], sel ? 255 : 120), sel ? Theme.Pf(2.4f) : Theme.Pf(1.2f))
            { DashStyle = sel ? DashStyle.Solid : DashStyle.Dash, LineJoin = LineJoin.Round };
            g.DrawLines(p, Line(s, c.Today, c.LastYear));
            var end = new PointF(X(c.LastYear), Y(c.Stats(c.LastYear, s).Population));
            using var eb = new SolidBrush(Theme.A(Theme.ScenarioColors[s], sel ? 255 : 150));
            g.FillEllipse(eb, end.X - 3, end.Y - 3, 6, 6);
        }
        // Курсор текущего года.
        float cx = X(YearF);
        using (var cp = new Pen(Theme.A(Theme.Text, 120), 1f)) g.DrawLine(cp, cx, r.Y, cx, r.Bottom);
        var cur = new PointF(cx, Y(c.Stats(Year).Population));
        var col = Year > c.Today ? Theme.ScenarioColors[c.Scenario] : Theme.Gold;
        using (var halo = new SolidBrush(Theme.A(col, 70))) g.FillEllipse(halo, cur.X - 7, cur.Y - 7, 14, 14);
        using (var dot = new SolidBrush(col)) g.FillEllipse(dot, cur.X - 3.5f, cur.Y - 3.5f, 7, 7);
    }
}

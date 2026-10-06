using System.Drawing.Drawing2D;

namespace Hronograf;

/// <summary>Нижняя шкала времени: эпохи, события, «сегодня», ползунок года, кнопка воспроизведения.</summary>
public sealed class TimelineBar : Control
{
    public Chronicle? Chronicle { get; set; }
    public float YearF { get; set; } = 1900;
    public bool Playing { get; set; }
    public float Speed { get; set; } = 1;
    int Year => (int)Math.Floor(YearF);

    public event Action<float>? Seek;
    public event Action? PlayToggled;
    public event Action? SpeedToggled;

    RectangleF track, playBtn, speedBtn;
    bool dragging;
    Point mouse = new(-1, -1);

    static readonly (int from, int to, string name, Color color)[] Eras =
    {
        (1900, 1917, "Старая деревня", Color.FromArgb(150, 120, 80)),
        (1917, 1930, "Новое время", Color.FromArgb(170, 110, 70)),
        (1930, 1941, "Колхозы", Color.FromArgb(190, 140, 60)),
        (1941, 1946, "Война", Color.FromArgb(200, 70, 60)),
        (1946, 1960, "Восстановление", Color.FromArgb(180, 160, 90)),
        (1960, 1986, "Расцвет", Color.FromArgb(120, 170, 110)),
        (1986, 2000, "Перелом", Color.FromArgb(150, 110, 130)),
        (2000, 2014, "Мобильная эпоха", Color.FromArgb(90, 150, 200)),
        (2014, 2026, "Цифра", Color.FromArgb(70, 190, 230)),
    };

    public TimelineBar()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;
        Cursor = Cursors.Hand;
    }

    float X(float year) => Chronicle == null ? 0 :
        track.X + (year - Chronicle.FirstYear) / (Chronicle.LastYear - Chronicle.FirstYear) * track.Width;

    float YearAt(float x) => Chronicle == null ? 1900 :
        Math.Clamp(Chronicle.FirstYear + (x - track.X) / track.Width * (Chronicle.LastYear - Chronicle.FirstYear), Chronicle.FirstYear, Chronicle.LastYear);

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Bg);
        Theme.Quality(g);
        if (Chronicle == null) return;
        var c = Chronicle;

        var panel = new RectangleF(Theme.Pf(14), Theme.Pf(6), Width - Theme.Pf(28), Height - Theme.Pf(16));
        Theme.Card(g, panel, Theme.Pf(16), Theme.Panel, Theme.Border);

        // Кнопки.
        float bs = Theme.Pf(46);
        playBtn = new RectangleF(panel.X + Theme.Pf(16), panel.Y + (panel.Height - bs) / 2, bs, bs);
        speedBtn = new RectangleF(playBtn.Right + Theme.Pf(10), playBtn.Y + Theme.Pf(9), Theme.Pf(44), Theme.Pf(28));
        bool hp = playBtn.Contains(mouse);
        var scCol = Theme.ScenarioColors[c.Scenario];
        using (var pg = new LinearGradientBrush(playBtn, Theme.Gold, Theme.Mix(Theme.Gold, scCol, 0.6f), 45f))
            g.FillEllipse(pg, playBtn);
        if (hp) using (var hb = new SolidBrush(Color.FromArgb(40, 255, 255, 255))) g.FillEllipse(hb, playBtn);
        using (var ib = new SolidBrush(Theme.Bg))
        {
            float cx = playBtn.X + bs / 2, cy = playBtn.Y + bs / 2, s = bs * 0.2f;
            if (Playing)
            {
                g.FillRectangle(ib, cx - s * 0.95f, cy - s, s * 0.7f, s * 2);
                g.FillRectangle(ib, cx + s * 0.25f, cy - s, s * 0.7f, s * 2);
            }
            else g.FillPolygon(ib, new[] { new PointF(cx - s * 0.7f, cy - s * 1.1f), new PointF(cx + s * 1.2f, cy), new PointF(cx - s * 0.7f, cy + s * 1.1f) });
        }
        Theme.FillRound(g, speedBtn, speedBtn.Height / 2, speedBtn.Contains(mouse) ? Theme.Panel2 : Theme.Bg);
        Theme.StrokeRound(g, speedBtn, speedBtn.Height / 2, Theme.Border);
        Theme.TextIn(g, $"{Speed:0}×", Theme.F(9, FontStyle.Bold), Theme.Text, speedBtn, StringAlignment.Center);

        track = new RectangleF(speedBtn.Right + Theme.Pf(28), panel.Y + panel.Height * 0.52f, panel.Right - speedBtn.Right - Theme.Pf(52), Theme.Pf(6));

        // Полосы эпох.
        var eraFont = Theme.F(7.5f, FontStyle.Bold);
        foreach (var (from, to, name, color) in Eras)
        {
            var er = new RectangleF(X(from) + 1, track.Y, X(to) - X(from) - 2, track.Height);
            Theme.FillRound(g, er, track.Height / 2, Theme.A(color, YearF >= from ? 230 : 90));
            var sz = Theme.Measure(g, name, eraFont);
            if (sz.Width < er.Width - 4)
                Theme.Str(g, name, eraFont, Theme.A(color, YearF >= from ? 255 : 150), er.X + (er.Width - sz.Width) / 2, track.Y - Theme.Pf(30));
        }
        // Будущее — цвет выбранного сценария.
        var fut = new RectangleF(X(c.Today) + 1, track.Y, track.Right - X(c.Today) - 1, track.Height);
        Theme.FillRound(g, fut, track.Height / 2, Theme.A(scCol, YearF > c.Today ? 220 : 70));
        var futName = "Будущее: " + Texts.ScenarioNames[c.Scenario];
        var fsz = Theme.Measure(g, futName, eraFont);
        Theme.Str(g, futName, eraFont, scCol, fut.X + (fut.Width - fsz.Width) / 2, track.Y - Theme.Pf(30));

        // «Сегодня».
        float tx = X(c.Today);
        using (var tp = new Pen(Theme.A(Theme.Gold, 200), 1.5f) { DashStyle = DashStyle.Dot })
            g.DrawLine(tp, tx, track.Y - Theme.Pf(14), tx, track.Bottom + Theme.Pf(10));

        // События — ромбики.
        ChronicleEvent? hover = null;
        foreach (var ev in c.Events())
        {
            float ex = X(ev.Year + 0.5f), ey = track.Y - Theme.Pf(9);
            var col = ev.Scenario >= 0 ? Theme.ScenarioColors[ev.Scenario] : Theme.Gold;
            bool past = ev.Year <= YearF;
            float s = Theme.Pf(3.2f);
            bool hv = Math.Abs(mouse.X - ex) < s + 2 && Math.Abs(mouse.Y - ey) < s + 6;
            if (hv) hover = ev;
            using var eb = new SolidBrush(Theme.A(col, past ? 255 : 90));
            g.FillPolygon(eb, new[] { new PointF(ex, ey - s), new PointF(ex + s, ey), new PointF(ex, ey + s), new PointF(ex - s, ey) });
        }

        // Подписи десятилетий.
        var small = Theme.F(7.5f);
        for (int yr = 1900; yr <= c.LastYear; yr += 10)
        {
            float gx = X(yr);
            using var tick = new Pen(Theme.A(Theme.Muted, 90), 1f);
            g.DrawLine(tick, gx, track.Bottom + Theme.Pf(4), gx, track.Bottom + Theme.Pf(8));
            var s = yr.ToString();
            var sz = Theme.Measure(g, s, small);
            Theme.Str(g, s, small, Theme.A(Theme.Muted, 200), gx - sz.Width / 2, track.Bottom + Theme.Pf(10));
        }

        // Ползунок текущего года.
        float hx = X(YearF);
        using (var halo = new SolidBrush(Theme.A(Year > c.Today ? scCol : Theme.Gold, 60)))
            g.FillEllipse(halo, hx - Theme.Pf(12), track.Y + track.Height / 2 - Theme.Pf(12), Theme.Pf(24), Theme.Pf(24));
        using (var knob = new SolidBrush(Theme.Text))
            g.FillEllipse(knob, hx - Theme.Pf(7), track.Y + track.Height / 2 - Theme.Pf(7), Theme.Pf(14), Theme.Pf(14));
        var yl = Year.ToString();
        var yf = Theme.F(9, FontStyle.Bold, Theme.Display);
        var ysz = Theme.Measure(g, yl, yf);
        var yb = new RectangleF(hx - ysz.Width / 2 - Theme.Pf(7), track.Bottom + Theme.Pf(8), ysz.Width + Theme.Pf(14), ysz.Height + Theme.Pf(6));
        Theme.FillRound(g, yb, yb.Height / 2, Year > c.Today ? scCol : Theme.Gold);
        Theme.Str(g, yl, yf, Theme.Bg, yb.X + Theme.Pf(7), yb.Y + Theme.Pf(3));

        if (hover != null)
        {
            var f = Theme.F(8.5f);
            var text = $"{hover.Year} · {hover.Text}";
            var sz = g.MeasureString(text, f, Theme.Px(360));
            float ex = X(hover.Year + 0.5f);
            var tr = new RectangleF(Math.Clamp(ex - sz.Width / 2 - 8, 4, Width - sz.Width - 20), Theme.Pf(2), sz.Width + 16, sz.Height + 10);
            Theme.FillRound(g, tr, Theme.Pf(8), Color.FromArgb(245, 22, 30, 52));
            Theme.StrokeRound(g, tr, Theme.Pf(8), Theme.A(Theme.Gold, 140));
            using var tb = new SolidBrush(Theme.Text);
            g.DrawString(text, f, tb, new RectangleF(tr.X + 8, tr.Y + 5, sz.Width + 2, sz.Height + 2));
        }
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (playBtn.Contains(e.Location)) { PlayToggled?.Invoke(); return; }
        if (speedBtn.Contains(e.Location)) { SpeedToggled?.Invoke(); return; }
        if (e.X >= track.X - 10 && e.X <= track.Right + 10) { dragging = true; Seek?.Invoke(YearAt(e.X)); }
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        mouse = e.Location;
        if (dragging) Seek?.Invoke(YearAt(e.X));
        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e) { base.OnMouseUp(e); dragging = false; }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); mouse = new Point(-1, -1); Invalidate(); }
}

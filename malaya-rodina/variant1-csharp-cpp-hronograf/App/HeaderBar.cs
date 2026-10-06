using System.Drawing.Drawing2D;

namespace Hronograf;

/// <summary>Плоская кнопка с закруглением, подсветкой и состоянием «выбрано».</summary>
public sealed class FlatButton : Control
{
    public Color Accent { get; set; } = Theme.Gold;
    public bool Selected { get; set; }
    public bool Filled { get; set; }
    bool hover, down;

    public FlatButton()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw | ControlStyles.SupportsTransparentBackColor, true);
        Cursor = Cursors.Hand;
        BackColor = Theme.Bg;
        TabStop = false;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Parent?.BackColor ?? Theme.Bg);
        Theme.Quality(g);
        var r = new RectangleF(1, 1, Width - 3, Height - 3);
        Color fill = Filled ? Accent : Selected ? Theme.A(Accent, 52) : hover ? Theme.Panel2 : Theme.Panel;
        if (down) fill = Theme.Scale3(fill, 0.85f);
        Theme.FillRound(g, r, r.Height / 2, fill);
        Theme.StrokeRound(g, r, r.Height / 2, Selected || Filled ? Accent : Theme.Border, Selected ? 1.6f : 1f);
        var tc = Filled ? Theme.Bg : Selected ? Accent : Theme.Text;
        Theme.TextIn(g, Text, Theme.F(9, FontStyle.Bold), tc, r, StringAlignment.Center);
    }

    protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { hover = down = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnMouseDown(MouseEventArgs e) { down = true; Invalidate(); base.OnMouseDown(e); }
    protected override void OnMouseUp(MouseEventArgs e) { down = false; Invalidate(); base.OnMouseUp(e); }
}

/// <summary>Шапка: логотип-объектив, название, поле «малая родина», сценарии, ночь, статус C++ движка.</summary>
public sealed class HeaderBar : Control
{
    public readonly TextBox NameBox;
    public readonly FlatButton Generate, Night;
    public readonly FlatButton[] Scenario = new FlatButton[3];
    public string EngineStatus { get; set; } = "C++";
    public float Time { get; set; }

    public HeaderBar()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;

        NameBox = new TextBox
        {
            BorderStyle = BorderStyle.None, BackColor = Theme.Panel2, ForeColor = Theme.Text,
            Font = Theme.F(11), Text = "Берёзовка", MaxLength = 40
        };
        Generate = new FlatButton { Text = "Создать мир", Accent = Theme.Gold, Filled = true };
        Night = new FlatButton { Text = "☾", Accent = Theme.Violet };
        for (int i = 0; i < 3; i++)
            Scenario[i] = new FlatButton { Text = $"{i + 1} · {Texts.ScenarioNames[i]}", Accent = Theme.ScenarioColors[i] };
        Controls.Add(NameBox);
        Controls.Add(Generate);
        Controls.Add(Night);
        foreach (var b in Scenario) Controls.Add(b);
    }

    protected override void OnLayout(LayoutEventArgs e)
    {
        base.OnLayout(e);
        int h = Theme.Px(34), y = (Height - h) / 2 + Theme.Px(2);
        int x = Width - Theme.Px(18);
        int chip = Theme.Px(ChipWidth);
        x -= chip + Theme.Px(10);               // место под статус движка (рисуется)
        x -= Theme.Px(44); Night.SetBounds(x, y, Theme.Px(44), h);
        x -= Theme.Px(12);
        for (int i = 2; i >= 0; i--) { x -= Theme.Px(112); Scenario[i].SetBounds(x, y, Theme.Px(106), h); }
        x -= Theme.Px(8);
        x -= Theme.Px(118); Generate.SetBounds(x, y, Theme.Px(112), h);
        int boxW = Theme.Px(164);
        x -= boxW + Theme.Px(8);
        NameBox.SetBounds(x + Theme.Px(14), y + (h - NameBox.PreferredHeight) / 2, boxW - Theme.Px(28), NameBox.PreferredHeight);
        nameRect = new RectangleF(x, y, boxW, h);
    }

    RectangleF nameRect;
    const float ChipWidth = 150;

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Bg);
        Theme.Quality(g);

        // Логотип: объектив с лепестками диафрагмы.
        float s = Theme.Pf(44), lx = Theme.Pf(20), ly = (Height - s) / 2;
        var lens = new RectangleF(lx, ly, s, s);
        using (var lg = new LinearGradientBrush(lens, Theme.Gold, Theme.Cyan, 45f)) g.FillEllipse(lg, lens);
        var inner = RectangleF.Inflate(lens, -s * 0.12f, -s * 0.12f);
        using (var ib = new SolidBrush(Theme.Bg)) g.FillEllipse(ib, inner);
        using (var blade = new Pen(Theme.A(Theme.Gold, 200), Theme.Pf(1.6f)))
            for (int k = 0; k < 6; k++)
            {
                double a = k * Math.PI / 3 + Time * 0.4;
                float cx = lens.X + s / 2, cy = lens.Y + s / 2, r1 = s * 0.36f, r2 = s * 0.12f;
                g.DrawLine(blade, cx + (float)Math.Cos(a) * r1, cy + (float)Math.Sin(a) * r1,
                    cx + (float)Math.Cos(a + 1.9) * r2, cy + (float)Math.Sin(a + 1.9) * r2);
            }
        using (var glint = new SolidBrush(Color.FromArgb(200, 255, 255, 255)))
            g.FillEllipse(glint, lens.X + s * 0.3f, lens.Y + s * 0.26f, s * 0.12f, s * 0.12f);

        float tx = lens.Right + Theme.Pf(14);
        float tw = nameRect.X - tx - Theme.Pf(16);
        var title = Theme.F(tw > Theme.Pf(330) ? 17 : 14, FontStyle.Regular, Theme.Display);
        Theme.TextIn(g, "ХРОНОГРАФ МАЛОЙ РОДИНЫ", title, Theme.Text, new RectangleF(tx, ly - Theme.Pf(2), tw, Theme.Pf(30)), StringAlignment.Near, StringAlignment.Near);
        Theme.TextIn(g, "Судьба малой родины в объективе технологий", Theme.F(8.5f), Theme.Muted,
            new RectangleF(tx, ly + Theme.Pf(28), tw, Theme.Pf(18)), StringAlignment.Near, StringAlignment.Near);

        // Поле ввода названия.
        Theme.FillRound(g, nameRect, nameRect.Height / 2, Theme.Panel2);
        Theme.StrokeRound(g, nameRect, nameRect.Height / 2, NameBox.Focused ? Theme.Gold : Theme.Border, NameBox.Focused ? 1.6f : 1f);
        Theme.Str(g, "МАЛАЯ РОДИНА", Theme.F(6.5f, FontStyle.Bold), Theme.Muted, nameRect.X + Theme.Pf(16), nameRect.Y - Theme.Pf(12));

        // Статус движка.
        var chip = new RectangleF(Width - Theme.Pf(18) - Theme.Pf(ChipWidth), (Height - Theme.Pf(34)) / 2 + Theme.Pf(2), Theme.Pf(ChipWidth), Theme.Pf(34));
        Theme.FillRound(g, chip, chip.Height / 2, Theme.Panel);
        Theme.StrokeRound(g, chip, chip.Height / 2, Theme.A(Theme.Green, 120));
        using (var dot = new SolidBrush(Theme.Green)) g.FillEllipse(dot, chip.X + Theme.Pf(12), chip.Y + chip.Height / 2 - Theme.Pf(4), Theme.Pf(8), Theme.Pf(8));
        Theme.TextIn(g, EngineStatus, Theme.F(8, FontStyle.Bold, Theme.Mono), Theme.Green,
            new RectangleF(chip.X + Theme.Pf(26), chip.Y, chip.Width - Theme.Pf(30), chip.Height));
        Theme.Str(g, "C# + C++ ДВИЖОК", Theme.F(6.5f, FontStyle.Bold), Theme.Muted, chip.X + Theme.Pf(16), chip.Y - Theme.Pf(12));
    }
}

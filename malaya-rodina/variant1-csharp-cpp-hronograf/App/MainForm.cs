using System.Diagnostics;

namespace Hronograf;

public sealed class MainForm : Form
{
    readonly HeaderBar header = new();
    readonly MapView map = new();
    readonly SidePanel side = new();
    readonly TimelineBar timeline = new();
    readonly System.Windows.Forms.Timer timer = new() { Interval = 33 };
    readonly Stopwatch clock = Stopwatch.StartNew();

    Chronicle? chronicle;
    float year;
    bool playing = true;
    float speed = 1f;
    float night, nightTarget;
    double lastTick;
    double holdUntil = -1;
    int frame, lastEraIndex = -1;
    float flash;
    double lensAvg;

    const float YearsPerSecond = 7f;

    readonly StartOptions options;

    public MainForm(StartOptions options)
    {
        this.options = options;
        Text = "Хронограф малой родины — судьба малой родины в объективе технологий";
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        AutoScaleMode = AutoScaleMode.None;
        Theme.Scale = DeviceDpi / 96f;
        StartPosition = FormStartPosition.CenterScreen;
        var wa = Screen.PrimaryScreen?.WorkingArea ?? new Rectangle(0, 0, 1600, 900);
        Size = new Size(Math.Min(Theme.Px(1480), wa.Width - 40), Math.Min(Theme.Px(920), wa.Height - 40));
        MinimumSize = new Size(Theme.Px(1280), Theme.Px(760));
        KeyPreview = true;
        DoubleBuffered = true;

        header.Dock = DockStyle.Top; header.Height = Theme.Px(76);
        timeline.Dock = DockStyle.Bottom; timeline.Height = Theme.Px(118);
        side.Dock = DockStyle.Right; side.Width = Theme.Px(430);
        map.Dock = DockStyle.Fill;
        Controls.Add(map);
        Controls.Add(side);
        Controls.Add(timeline);
        Controls.Add(header);

        header.Generate.Click += (_, _) => CreateWorld(header.NameBox.Text);
        header.NameBox.KeyDown += (_, e) =>
        {
            if (e.KeyCode == Keys.Enter) { e.SuppressKeyPress = true; CreateWorld(header.NameBox.Text); map.Focus(); }
        };
        header.NameBox.GotFocus += (_, _) => header.Invalidate();
        header.NameBox.LostFocus += (_, _) => header.Invalidate();
        header.Night.Click += (_, _) => ToggleNight();
        for (int i = 0; i < 3; i++)
        {
            int s = i;
            header.Scenario[i].Click += (_, _) => SetScenario(s);
        }
        timeline.Seek += y => { year = y; holdUntil = -1; map.ShowTodayCard = false; SyncViews(); };
        timeline.PlayToggled += TogglePlay;
        timeline.SpeedToggled += () => { speed = speed >= 4 ? 1 : speed * 2; timeline.Speed = speed; timeline.Invalidate(); };
        map.MouseClick += (_, _) => map.Focus();
        map.ScenarioPicked += SetScenario;

        timer.Tick += (_, _) => Tick();
        Shown += (_, _) =>
        {
            CreateWorld(options.Name, options.Scenario);
            if (options.Year is int y && chronicle != null) year = Math.Clamp(y, chronicle.FirstYear, chronicle.LastYear);
            if (options.Paused) playing = false;
            if (options.Night) { ToggleNight(); night = 1; }
            if (options.Today && chronicle != null)
            {
                // Сразу к развилке «сегодня» — карточка ждёт выбора сценария.
                year = chronicle.Today + 0.99f;
                map.ShowTodayCard = true;
            }
            SyncViews();
            timer.Start();
        };
    }

    void CreateWorld(string name, int? scenario = null)
    {
        try
        {
            Cursor = Cursors.WaitCursor;
            var sw = Stopwatch.StartNew();
            var fresh = new Chronicle(name, scenario ?? chronicle?.Scenario ?? 2);
            sw.Stop();
            chronicle?.Dispose();
            chronicle = fresh;
            header.NameBox.Text = fresh.Name;
            map.Chronicle = side.Chronicle = timeline.Chronicle = fresh;
            map.ResetScene();
            year = fresh.FirstYear;
            playing = true;
            holdUntil = -1;
            map.ShowTodayCard = false;
            header.EngineStatus = $"мир за {sw.ElapsedMilliseconds} мс";
            UpdateScenarioButtons();
            SyncViews();
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, "Не удалось запустить C++ движок.\n\n" + ex.Message +
                "\n\nСоберите проект Engine (Visual Studio → Hronograf.sln → Сборка) или запустите build.bat.",
                "Хронограф", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally { Cursor = Cursors.Default; }
    }

    void SetScenario(int s)
    {
        if (chronicle == null) return;
        chronicle.Scenario = s;
        map.ResetScene();
        if (map.ShowTodayCard)
        {
            // Выбор сделан на развилке — сразу идём в будущее.
            map.ShowTodayCard = false;
            holdUntil = -1;
            year = chronicle.Today + 1;
            playing = true;
            flash = 0.8f;
        }
        UpdateScenarioButtons();
        SyncViews();
    }

    void UpdateScenarioButtons()
    {
        for (int i = 0; i < 3; i++) { header.Scenario[i].Selected = chronicle?.Scenario == i; header.Scenario[i].Invalidate(); }
    }

    void TogglePlay()
    {
        if (chronicle == null) return;
        if (map.ShowTodayCard) { map.ShowTodayCard = false; holdUntil = -1; year = chronicle.Today + 1; playing = true; }
        else if (!playing && year >= chronicle.LastYear) { year = chronicle.FirstYear; playing = true; }
        else playing = !playing;
        SyncViews();
    }

    void ToggleNight()
    {
        nightTarget = nightTarget > 0.5f ? 0 : 1;
        header.Night.Selected = nightTarget > 0.5f;
        header.Night.Text = nightTarget > 0.5f ? "☀" : "☾";
        header.Night.Invalidate();
    }

    static int EraIndex(int y) => y < 1960 ? 0 : y < 1985 ? 1 : y < 2006 ? 2 : y <= 2026 ? 3 : 4;

    void Tick()
    {
        double now = clock.Elapsed.TotalSeconds;
        float dt = (float)Math.Min(0.1, now - lastTick);
        lastTick = now;
        frame++;
        if (chronicle == null) return;

        if (holdUntil > 0 && now >= holdUntil) { holdUntil = -1; }
        if (playing && holdUntil < 0 && !map.ShowTodayCard)
        {
            float before = year;
            year += dt * YearsPerSecond * speed;
            if (before <= chronicle.Today && year > chronicle.Today)
            {
                // Развилка «сегодня»: пауза и выбор сценария.
                year = chronicle.Today + 0.99f;
                map.ShowTodayCard = true;
                holdUntil = now + 7;
            }
            if (year >= chronicle.LastYear) { year = chronicle.LastYear; playing = false; }
        }
        if (map.ShowTodayCard && holdUntil > 0 && now >= holdUntil - 0.05)
        {
            map.ShowTodayCard = false;
            year = chronicle.Today + 1;
            flash = 0.8f;
        }

        int era = EraIndex((int)year);
        if (lastEraIndex >= 0 && era != lastEraIndex) flash = 0.9f;   // вспышка — смена «объектива»
        lastEraIndex = era;
        flash = Math.Max(0, flash - dt * 2.2f);
        night += (nightTarget - night) * Math.Min(1, dt * 3.5f);

        SyncViews();
        lensAvg = lensAvg * 0.9 + map.LastLensMs * 0.1;
        if (frame % 15 == 0)
        {
            header.EngineStatus = $"кадр {lensAvg:0.0} мс";
        }
        header.Time = (float)now;
        header.Invalidate();
    }

    void SyncViews()
    {
        if (chronicle == null) return;
        map.YearF = side.YearF = timeline.YearF = year;
        map.Night = night;
        map.Time = (float)clock.Elapsed.TotalSeconds;
        map.Frame = frame;
        map.FlashAlpha = flash;
        timeline.Playing = playing && !map.ShowTodayCard;
        map.Invalidate();
        side.Invalidate();
        timeline.Invalidate();
    }

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        if (header.NameBox.Focused) return base.ProcessCmdKey(ref msg, keyData);
        switch (keyData)
        {
            case Keys.Space: TogglePlay(); return true;
            case Keys.Left: year = Math.Max(chronicle?.FirstYear ?? 1900, (int)year - 1); SyncViews(); return true;
            case Keys.Right: year = Math.Min(chronicle?.LastYear ?? 2060, (int)year + 1); SyncViews(); return true;
            case Keys.Home: year = chronicle?.FirstYear ?? 1900; SyncViews(); return true;
            case Keys.End: year = chronicle?.LastYear ?? 2060; SyncViews(); return true;
            case Keys.N: ToggleNight(); return true;
            case Keys.D1: case Keys.NumPad1: SetScenario(0); return true;
            case Keys.D2: case Keys.NumPad2: SetScenario(1); return true;
            case Keys.D3: case Keys.NumPad3: SetScenario(2); return true;
        }
        return base.ProcessCmdKey(ref msg, keyData);
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        timer.Stop();
        chronicle?.Dispose();
        base.OnFormClosed(e);
    }
}

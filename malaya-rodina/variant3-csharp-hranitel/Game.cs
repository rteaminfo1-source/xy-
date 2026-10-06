namespace Hranitel;

public sealed record ChronicleEntry(int Year, string Era, string Choice, string Result, int[] Delta);

/// <summary>Игра «Хранитель малой родины»: девять эпох — девять решений.</summary>
public sealed class Game
{
    readonly string name;
    readonly int[] stats = (int[])Story.Start.Clone();
    readonly HashSet<string> flags = new();
    public readonly List<ChronicleEntry> Chronicle = new();

    public Game(string name) => this.name = name;

    /// <summary>Поле ввода в рамке поверх панорамы. Esc — null.</summary>
    public static string? AskName(string title, string prompt, string def, bool digits = false)
    {
        string text = "";
        while (true)
        {
            var key = Term.Animate(t =>
            {
                var s = new Screen(Term.Width, Term.Height);
                int rows = Math.Clamp(s.H - 14, 8, 16);
                var px = Scene.Render(SceneState.ForYear(1925, Mood.Dusk), s.W, rows * 2, t);
                px.BlitTo(s, 0, 0);
                int bw = Math.Min(64, s.W - 4), bx = (s.W - bw) / 2, by = rows + 1;
                s.Box(bx, by, bw, 8, Pal.Gold, Pal.Panel, title, Pal.Gold);
                s.Put(bx + 3, by + 2, prompt, Pal.Muted, Pal.Panel);
                s.Fill(bx + 3, by + 4, bw - 6, 1, Pal.Panel2);
                bool blink = (int)(t * 2) % 2 == 0;
                if (text.Length == 0 && def.Length > 0) s.Put(bx + 5, by + 4, def + "  (Enter — оставить)", Pal.Muted, Pal.Panel2);
                s.Put(bx + 4, by + 4, text + (blink ? "▌" : " "), Pal.Text, Pal.Panel2);
                s.Put(bx + 3, by + 6, "Enter — продолжить · Esc — назад", Pal.Muted, Pal.Panel);
                s.Flush();
            });
            if (key.Key == ConsoleKey.Enter) return string.IsNullOrWhiteSpace(text) ? def : text.Trim();
            if (key.Key == ConsoleKey.Escape) return null;
            if (key.Key == ConsoleKey.Backspace) { if (text.Length > 0) text = text[..^1]; continue; }
            if (digits && !char.IsDigit(key.KeyChar)) continue;
            if (!char.IsControl(key.KeyChar) && text.Length < (digits ? 7 : 28)) text += key.KeyChar;
        }
    }

    SceneState State(int year, Mood mood)
    {
        var st = new SceneState { Year = year, Mood = mood, People = stats[Stat.People], Flags = new HashSet<string>(flags) };
        st.Decline = Math.Clamp((60 - stats[Stat.People]) / 80.0, 0, 0.7);
        if (year >= 1952 || flags.Contains("hydro")) st.Flags.Add("electric");
        if (year >= 1965) st.Flags.Add("club");
        return st;
    }

    string Fill(string s) => s.Replace("{name}", name).Replace("{pop}", Story.Population(stats[Stat.People]).ToString());

    // ------------------------------------------------------------ экран эпохи
    void DrawFrame(Screen s, Era era, int eraIndex, SceneState scene, double t, out int panelY, out int leftW)
    {
        int rows = Math.Clamp(s.H - 20, 8, 15);
        var px = Scene.Render(scene, s.W, rows * 2, t);
        px.BlitTo(s, 0, 0);
        // полоса эпохи
        s.Fill(0, rows, s.W, 1, Pal.Panel2);
        s.PutGradient(2, rows, $"{era.Year} · {era.Title}", Pal.Gold, Pal.Cyan, Pal.Panel2);
        var dots = string.Join(" ", Story.Eras.Select((e, i) => i < eraIndex ? "■" : i == eraIndex ? "☼" : "·"));
        s.Put(s.W - dots.Length - 2, rows, dots, Pal.Gold, Pal.Panel2);
        panelY = rows + 1;
        leftW = s.W - 40;
        // показатели
        int sx = s.W - 38, sw = 37, sh = s.H - panelY - 1;
        s.Box(sx, panelY, sw, sh, Pal.Border, Pal.Panel, "СУДЬБА СЕЛА", Pal.Muted);
        s.Put(sx + 2, panelY + 2, name, Pal.Text, Pal.Panel);
        var pop = $"{Story.Population(stats[Stat.People])} жит.";
        s.Put(sx + sw - pop.Length - 2, panelY + 2, pop, Pal.Gold, Pal.Panel);
        for (int i = 0; i < Stat.Count; i++)
        {
            int y = panelY + 4 + i * 2;
            if (y >= panelY + sh - 1) break;
            s.Put(sx + 2, y, Stat.Names[i], Pal.Muted, Pal.Panel);
            s.Bar(sx + 13, y, 17, stats[i] / 100.0, Stat.Colors[i], Pal.Panel2);
            s.Put(sx + 31, y, $"{stats[i],3}", Stat.Colors[i], Pal.Panel);
        }
        s.Fill(0, s.H - 1, s.W, 1, Pal.Bg);
    }

    int ChooseInEra(Era era, int idx)
    {
        int sel = 0;
        double revealStart = -1;
        while (true)
        {
            var key = Term.Animate(t =>
            {
                if (revealStart < 0) revealStart = t;
                var s = new Screen(Term.Width, Term.Height);
                DrawFrame(s, era, idx, State(era.Year, era.Mood), t, out int py, out int lw);
                int reveal = (int)((t - revealStart) * 140);
                var ctx = Fill(era.Context);
                int y = py + 1;
                y += s.Wrap(2, y, lw - 2, ctx, Pal.Text, null, 6, reveal);
                if (!string.IsNullOrEmpty(era.DriftNote)) { y++; s.Put(2, y, "» " + era.DriftNote, Pal.Coral); y++; }
                y++;
                for (int i = 0; i < era.Choices.Length; i++)
                {
                    var c = era.Choices[i];
                    bool on = i == sel;
                    int lines = Screen.WrapCount(lw - 9, c.Title);
                    if (on) s.Fill(1, y, lw - 1, lines, Pal.Panel2);
                    s.Put(2, y, on ? "►" : " ", Pal.Gold, on ? Pal.Panel2 : null);
                    s.Put(4, y, $"{i + 1}", on ? Pal.Gold : Pal.Muted, on ? Pal.Panel2 : null);
                    s.Wrap(7, y, lw - 9, c.Title, on ? Pal.Text : Pal.Muted, on ? Pal.Panel2 : null);
                    y += lines + 1;
                }
                s.Put(2, s.H - 1, "↑↓ или 1–3 — выбор   Enter — решить   Esc — в меню", Pal.Muted);
                s.Flush();
            });
            switch (key.Key)
            {
                case ConsoleKey.UpArrow: sel = (sel + era.Choices.Length - 1) % era.Choices.Length; break;
                case ConsoleKey.DownArrow: sel = (sel + 1) % era.Choices.Length; break;
                case ConsoleKey.D1: case ConsoleKey.NumPad1: return 0;
                case ConsoleKey.D2: case ConsoleKey.NumPad2: return 1;
                case ConsoleKey.D3: case ConsoleKey.NumPad3: return 2;
                case ConsoleKey.Enter: return sel;
                case ConsoleKey.Escape: return -1;
            }
        }
    }

    bool ShowResult(Era era, int idx, Choice c, int[] before)
    {
        while (true)
        {
            var key = Term.Animate(t =>
            {
                var s = new Screen(Term.Width, Term.Height);
                var anim = Math.Clamp(t / 0.8, 0, 1);
                var shown = stats.Select((v, i) => (int)Math.Round(before[i] + (v - before[i]) * anim)).ToArray();
                var real = (int[])stats.Clone();
                Array.Copy(shown, stats, stats.Length);
                DrawFrame(s, era, idx, State(era.Year + 3, era.Mood == Mood.Overcast ? Mood.Day : era.Mood), t, out int py, out int lw);
                Array.Copy(real, stats, stats.Length);
                int y = py + 1;
                s.Put(2, y, "ВАШЕ РЕШЕНИЕ", Pal.Muted); y++;
                y += s.Wrap(2, y, lw - 2, c.Title, Pal.Gold); y++;
                y += s.Wrap(2, y, lw - 2, Fill(c.Result), Pal.Text, null, 5, (int)(t * 160)); y++;
                int x = 2;
                for (int i = 0; i < Stat.Count; i++)
                {
                    if (c.Delta[i] == 0) continue;
                    var tag = $" {Stat.Names[i]} {(c.Delta[i] > 0 ? "+" : "")}{c.Delta[i]} ";
                    if (x + tag.Length > lw) { x = 2; y++; }
                    s.Put(x, y, tag, Pal.Bg, c.Delta[i] > 0 ? Pal.Green : Pal.Coral);
                    x += tag.Length + 1;
                }
                s.Put(2, s.H - 1, "Enter — дальше, в следующую эпоху", Pal.Muted);
                s.Flush();
            });
            if (key.Key == ConsoleKey.Enter || key.Key == ConsoleKey.Spacebar) return true;
            if (key.Key == ConsoleKey.Escape) return false;
        }
    }

    // ------------------------------------------------------------ игра
    public bool Run()
    {
        for (int i = 0; i < Story.Eras.Length; i++)
        {
            var era = Story.Eras[i];
            for (int k = 0; k < Stat.Count; k++) stats[k] = Math.Clamp(stats[k] + era.Drift[k], 0, 100);
            int pick = ChooseInEra(era, i);
            if (pick < 0) return false;
            var c = era.Choices[pick];
            var before = (int[])stats.Clone();
            for (int k = 0; k < Stat.Count; k++) stats[k] = Math.Clamp(stats[k] + c.Delta[k], 0, 100);
            if (c.Flag != null) flags.Add(c.Flag);
            Chronicle.Add(new ChronicleEntry(era.Year, era.Title, c.Title, Fill(c.Result), c.Delta));
            if (!ShowResult(era, i, c, before)) return false;
        }
        Finale();
        return true;
    }

    void Finale()
    {
        var end = Story.Final(stats, name);
        string? saved = null;
        int scroll = 0;
        while (true)
        {
            var key = Term.Animate(t =>
            {
                var s = new Screen(Term.Width, Term.Height);
                int rows = Math.Clamp(s.H - 18, 8, 14);
                var st = State(2050, end.Mood);
                if (end.Color == Pal.Coral) { st.People = Math.Min(st.People, 25); st.Decline = 0.6; }
                Scene.Render(st, s.W, rows * 2, t).BlitTo(s, 0, 0);
                s.Fill(0, rows, s.W, 1, Pal.Panel2);
                s.PutGradient(2, rows, $"2050 · ИТОГ · {name.ToUpperInvariant()}", Pal.Gold, end.Color, Pal.Panel2);
                int y = rows + 2, lw = s.W - 40;
                s.Put(2, y, "СУДЬБА СЕЛА:", Pal.Muted);
                s.PutGradient(15, y, end.Title, end.Color, Pal.Text);
                y += 2;
                y += s.Wrap(2, y, lw - 2, end.Text, Pal.Text) + 1;
                s.Put(2, y++, "ЛЕТОПИСЬ ВАШИХ РЕШЕНИЙ", Pal.Muted);
                int room = s.H - 2 - y;
                foreach (var e in Chronicle.Skip(scroll).Take(room))
                {
                    s.Put(2, y, $"{e.Year}", Pal.Gold);
                    var line = e.Choice.Length > lw - 9 ? e.Choice[..(lw - 10)] + "…" : e.Choice;
                    s.Put(8, y, line, Pal.Text);
                    y++;
                }
                // показатели
                int sx = s.W - 38, sw = 37;
                s.Box(sx, rows + 1, sw, s.H - rows - 2, end.Color, Pal.Panel, "ИТОГ 2050", end.Color);
                s.Put(sx + 2, rows + 3, $"Население: {Story.Population(stats[Stat.People])} жит.", Pal.Gold, Pal.Panel);
                for (int i = 0; i < Stat.Count; i++)
                {
                    int yy = rows + 5 + i * 2;
                    s.Put(sx + 2, yy, Stat.Names[i], Pal.Muted, Pal.Panel);
                    s.Bar(sx + 13, yy, 17, stats[i] / 100.0, Stat.Colors[i], Pal.Panel2);
                    s.Put(sx + 31, yy, $"{stats[i],3}", Stat.Colors[i], Pal.Panel);
                }
                string hint = saved != null ? $"Сохранено: {saved}" : "S — сохранить летопись в HTML   ↑↓ — листать   Enter — в меню";
                s.Put(2, s.H - 1, hint.Length > s.W - 4 ? hint[..(s.W - 4)] : hint, saved != null ? Pal.Green : Pal.Muted);
                s.Flush();
            });
            if (key.Key == ConsoleKey.Enter || key.Key == ConsoleKey.Escape) return;
            if (key.Key == ConsoleKey.DownArrow) scroll = Math.Min(Chronicle.Count - 1, scroll + 1);
            if (key.Key == ConsoleKey.UpArrow) scroll = Math.Max(0, scroll - 1);
            if (key.Key == ConsoleKey.S || key.KeyChar is 'ы' or 'Ы' or 's')
                saved = Report.SaveChronicle(name, end, stats, Chronicle);
        }
    }
}

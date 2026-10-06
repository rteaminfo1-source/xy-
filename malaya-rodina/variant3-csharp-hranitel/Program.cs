namespace Hranitel;

/// <summary>
/// ХРАНИТЕЛЬ МАЛОЙ РОДИНЫ — консольная игра на C#.
/// Судьба малой родины в объективе технологий: 1925 → 2050.
/// </summary>
public static class Program
{
    static readonly string[] Menu =
    {
        "Новая игра «Хранитель малой родины»",
        "Цифровой паспорт моей малой родины",
        "Музей технологий",
        "Об идее проекта",
        "Выход",
    };

    static readonly int[] ShowYears = { 1925, 1937, 1946, 1965, 1978, 1995, 2008, 2019, 2026, 2035, 2050 };

    public static void Main()
    {
        Term.Init();
        try
        {
            int sel = 0;
            while (true)
            {
                var key = Term.Animate(t => DrawMenu(sel, t));
                switch (key.Key)
                {
                    case ConsoleKey.UpArrow: sel = (sel + Menu.Length - 1) % Menu.Length; break;
                    case ConsoleKey.DownArrow: sel = (sel + 1) % Menu.Length; break;
                    case ConsoleKey.D1: case ConsoleKey.NumPad1: sel = 0; goto case ConsoleKey.Enter;
                    case ConsoleKey.D2: case ConsoleKey.NumPad2: sel = 1; goto case ConsoleKey.Enter;
                    case ConsoleKey.D3: case ConsoleKey.NumPad3: sel = 2; goto case ConsoleKey.Enter;
                    case ConsoleKey.D4: case ConsoleKey.NumPad4: sel = 3; goto case ConsoleKey.Enter;
                    case ConsoleKey.Escape: return;
                    case ConsoleKey.Enter:
                        if (sel == 0)
                        {
                            var name = Game.AskName("ХРАНИТЕЛЬ МАЛОЙ РОДИНЫ", "Как называется ваша малая родина?", "Берёзовка");
                            if (name != null) new Game(name).Run();
                        }
                        else if (sel == 1) Passport.Run();
                        else if (sel == 2) Museum.Run();
                        else if (sel == 3) About();
                        else return;
                        break;
                }
            }
        }
        finally
        {
            Term.Restore();
        }
    }

    static void DrawMenu(int sel, double t)
    {
        var s = new Screen(Term.Width, Term.Height);
        int rows = Math.Clamp(s.H - Menu.Length - 9, 10, 22);
        int yi = (int)(t / 2.6) % ShowYears.Length;
        int year = ShowYears[yi];
        var px = Scene.Render(SceneState.ForYear(year), s.W, rows * 2, t);
        const string title = "МАЛАЯ РОДИНА";
        int tw = Pixels.TextWidth(title);
        px.Text((px.W - tw) / 2, 3, title, Pal.Gold, Pal.Cyan, Rgb.Hex("0a0e1c"));
        px.BlitTo(s, 0, 0);
        s.Center(rows - 2 < 6 ? 6 : 6, "", Pal.Text);
        var sub = " ХРАНИТЕЛЬ · судьба малой родины в объективе технологий ";
        s.Center(Math.Min(rows - 1, 7), sub, Pal.Text, Rgb.Hex("0b1122"));
        var stamp = $" ☼ {year} · {Lens(year)} ";
        s.Put(s.W - stamp.Length - 1, rows - 1, stamp, Pal.Gold, Rgb.Hex("0b1122"));

        int bw = Math.Min(56, s.W - 4), bx = (s.W - bw) / 2, by = rows + 1;
        s.Box(bx, by, bw, Menu.Length + 4, Pal.Border, Pal.Panel, "МЕНЮ", Pal.Muted);
        for (int i = 0; i < Menu.Length; i++)
        {
            bool on = i == sel;
            int y = by + 2 + i;
            if (on) s.Fill(bx + 1, y, bw - 2, 1, Pal.Panel2);
            s.Put(bx + 3, y, on ? "►" : " ", Pal.Gold, on ? Pal.Panel2 : Pal.Panel);
            s.Put(bx + 5, y, Menu[i], on ? Pal.Text : Pal.Muted, on ? Pal.Panel2 : Pal.Panel);
        }
        s.Center(s.H - 1, "↑↓ — выбор · Enter — открыть · Esc — выход", Pal.Muted);
        s.Flush();
    }

    public static string Lens(int year) => year switch
    {
        < 1950 => "объектив: старая фотоплёнка",
        < 1985 => "объектив: цветная плёнка «Свема»",
        < 2005 => "объектив: плёночная «мыльница»",
        <= 2026 => "объектив: камера смартфона",
        _ => "объектив: AR-очки"
    };

    static void About()
    {
        const string text =
            "Почти у каждого есть малая родина — деревня бабушки, посёлок родителей. Многие из них пустеют, хотя технологии " +
            "в село приходят: электричество, радио, телевидение, связь, интернет. Почему одни сёла живут, а другие угасают?\n\n" +
            "В игре вы — хранитель своего села. Девять эпох с 1925 по 2035 год, в каждой — трудный выбор. Каждое решение меняет " +
            "шесть показателей: жители, молодёжь, экономика, связь, память и природа. А панорама села меняется вместе с " +
            "решениями: появляются провода и антенны, вышки и солнечные панели — или пустеют дома.\n\n" +
            "Главная мысль: технологии сами по себе не спасают и не губят село. Решает то, работают ли они на людей и " +
            "сохраняют ли память. Пять финалов: «Возрождение», «Живое село», «Технополис без корней», «Музей под открытым " +
            "небом», «Тихое угасание».\n\n" +
            "«Цифровой паспорт» оценивает вашу настоящую малую родину и подсказывает, какие технологии ей помогут. " +
            "«Музей технологий» рассказывает о 13 изобретениях, изменивших деревню.";
        Term.Animate(t =>
        {
            var s = new Screen(Term.Width, Term.Height);
            int rows = Math.Clamp(s.H - 20, 6, 10);
            Scene.Render(SceneState.ForYear(2026, Mood.Dusk), s.W, rows * 2, t).BlitTo(s, 0, 0);
            int bw = Math.Min(100, s.W - 4), bx = (s.W - bw) / 2;
            s.Box(bx, rows + 1, bw, s.H - rows - 3, Pal.Gold, Pal.Panel, "ОБ ИДЕЕ ПРОЕКТА", Pal.Gold);
            s.Wrap(bx + 3, rows + 3, bw - 6, text, Pal.Text, Pal.Panel, s.H - rows - 6);
            s.Center(s.H - 1, "Любая клавиша — назад", Pal.Muted);
            s.Flush();
        });
    }
}

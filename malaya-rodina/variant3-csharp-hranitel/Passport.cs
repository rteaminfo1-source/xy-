namespace Hranitel;

/// <summary>«Цифровой паспорт малой родины»: анкета → индекс устойчивости → советы → HTML.</summary>
public static class Passport
{
    public sealed class Result
    {
        public string Name = "", Region = "";
        public int PopThen, PopNow, Trend, Index;
        public string Verdict = "";
        public Rgb Color;
        public List<(string label, int value, Rgb color)> Parts = new();
        public List<(string item, bool has)> Items = new();
        public List<string> Recommendations = new();
    }

    static readonly (string Item, string Advice)[] Checklist =
    {
        ("Быстрый интернет (оптоволокно или 4G)", "Интернет: подать заявку в программу устранения цифрового неравенства — оптоволокно или вышка 4G."),
        ("Устойчивая мобильная связь", "Связь: антенну можно разместить на водонапорной башне — так не испортится вид села."),
        ("Школа в селе", "Образование: онлайн-кружки и «цифровая школа» в библиотеке помогут детям учиться рядом с домом."),
        ("ФАП, врач или телемедицина", "Здоровье: телемедицина — консультации врача по видеосвязи из ФАПа или библиотеки."),
        ("Магазин", "Торговля: онлайн-заказы с доставкой автолавкой, а в будущем — дронами."),
        ("Дорога с покрытием и автобус", "Транспорт: автобус по заявке через приложение; ремонт дороги — через инициативное бюджетирование."),
        ("Дом культуры или библиотека", "Культура: библиотека как цифровой центр — интернет, кружки, кино, мастер-классы."),
        ("Музей, архив или книга по истории села", "Память: отсканируйте семейные фото и письма, запишите воспоминания старожилов — создайте цифровой музей."),
        ("Местные фермы и предприятия", "Экономика: умное фермерство и кооперация — датчики, дроны, продажа продуктов через интернет."),
        ("Жители, работающие удалённо", "Работа: коворкинг в клубе или школе — чтобы молодёжь могла работать, не уезжая."),
        ("Фестивали, ярмарки, туризм", "Туризм: экотропы и экскурсии с AR-гидом по истории села, фестиваль земляков."),
        ("Сайт или группа села в соцсетях", "Сообщество: группа села в соцсетях — новости, объявления, связь с уехавшими земляками."),
    };

    public static void Run()
    {
        var name = Game.AskName("ЦИФРОВОЙ ПАСПОРТ МАЛОЙ РОДИНЫ", "Как называется ваше село, деревня или посёлок?", "Берёзовка");
        if (name == null) return;
        var region = Game.AskName("ЦИФРОВОЙ ПАСПОРТ · " + name.ToUpperInvariant(), "Район, область (можно пропустить):", "");
        if (region == null) return;
        var then = Game.AskName("ЦИФРОВОЙ ПАСПОРТ · " + name.ToUpperInvariant(), "Сколько жителей было примерно в 1990 году?", "600", digits: true);
        if (then == null) return;
        var now = Game.AskName("ЦИФРОВОЙ ПАСПОРТ · " + name.ToUpperInvariant(), "Сколько жителей сейчас?", "350", digits: true);
        if (now == null) return;

        var has = new bool[Checklist.Length];
        int sel = 0;
        while (true)
        {
            var key = Term.Animate(t =>
            {
                var s = new Screen(Term.Width, Term.Height);
                int rows = Math.Clamp(s.H - Checklist.Length - 9, 6, 12);
                Scene.Render(SceneState.ForYear(2026, Mood.Day), s.W, rows * 2, t).BlitTo(s, 0, 0);
                int bw = Math.Min(78, s.W - 4), bx = (s.W - bw) / 2, by = rows + 1;
                s.Box(bx, by, bw, Checklist.Length + 5, Pal.Cyan, Pal.Panel, "ЧТО ЕСТЬ В СЕЛЕ «" + name.ToUpperInvariant() + "»?", Pal.Cyan);
                for (int i = 0; i < Checklist.Length; i++)
                {
                    bool on = i == sel;
                    int y = by + 2 + i;
                    if (on) s.Fill(bx + 1, y, bw - 2, 1, Pal.Panel2);
                    s.Put(bx + 3, y, has[i] ? "[√]" : "[ ]", has[i] ? Pal.Green : Pal.Muted, on ? Pal.Panel2 : Pal.Panel);
                    s.Put(bx + 8, y, Checklist[i].Item, on ? Pal.Text : Pal.Muted, on ? Pal.Panel2 : Pal.Panel);
                }
                s.Put(bx + 3, by + Checklist.Length + 3, "Пробел — отметить   Enter — рассчитать паспорт   Esc — меню", Pal.Muted, Pal.Panel);
                s.Flush();
            });
            if (key.Key == ConsoleKey.UpArrow) sel = (sel + Checklist.Length - 1) % Checklist.Length;
            else if (key.Key == ConsoleKey.DownArrow) sel = (sel + 1) % Checklist.Length;
            else if (key.Key == ConsoleKey.Spacebar) has[sel] = !has[sel];
            else if (key.Key == ConsoleKey.Escape) return;
            else if (key.Key == ConsoleKey.Enter) break;
        }

        var r = Compute(name, region, int.Parse(then), int.Parse(now), has);
        ShowResult(r);
    }

    public static Result Compute(string name, string region, int popThen, int popNow, bool[] h)
    {
        int B(int i) => h[i] ? 1 : 0;
        var r = new Result { Name = name, Region = region, PopThen = Math.Max(1, popThen), PopNow = popNow };
        r.Trend = (int)Math.Round((popNow / (double)r.PopThen - 1) * 100);
        int link = 50 * B(0) + 35 * B(1) + 15 * B(11);
        int services = 25 * B(2) + 30 * B(3) + 20 * B(4) + 25 * B(5);
        int economy = 45 * B(8) + 30 * B(9) + 25 * B(10);
        int memory = 45 * B(7) + 30 * B(6) + 15 * B(10) + 10 * B(11);
        int demo = (int)Math.Clamp(50 + r.Trend * 1.25, 0, 100);
        r.Parts.Add(("Связь", link, Pal.Cyan));
        r.Parts.Add(("Сервисы", services, Pal.Gold));
        r.Parts.Add(("Экономика", economy, Pal.Green));
        r.Parts.Add(("Память", memory, Pal.Violet));
        r.Parts.Add(("Демография", demo, Pal.Pink));
        r.Index = (int)Math.Round(0.22 * link + 0.22 * services + 0.20 * economy + 0.16 * memory + 0.20 * demo);
        (r.Verdict, r.Color) = r.Index switch
        {
            >= 75 => ("Устойчивое цифровое село", Pal.Green),
            >= 55 => ("Село с хорошим потенциалом", Pal.Cyan),
            >= 35 => ("Село в зоне риска", Pal.Gold),
            _ => ("Селу нужна срочная поддержка", Pal.Coral)
        };
        for (int i = 0; i < Checklist.Length; i++)
        {
            r.Items.Add((Checklist[i].Item, h[i]));
            if (!h[i]) r.Recommendations.Add(Checklist[i].Advice);
        }
        return r;
    }

    static void ShowResult(Result r)
    {
        string? saved = null;
        while (true)
        {
            var key = Term.Animate(t =>
            {
                var s = new Screen(Term.Width, Term.Height);
                int rows = Math.Clamp(s.H - 22, 6, 10);
                var st = SceneState.ForYear(r.Index >= 55 ? 2035 : 2015, r.Index >= 55 ? Mood.Night : Mood.Overcast);
                st.People = Math.Clamp(r.Index, 15, 90);
                Scene.Render(st, s.W, rows * 2, t).BlitTo(s, 0, 0);
                int y = rows + 1;
                s.Box(1, y, 34, s.H - y - 1, r.Color, Pal.Panel, "ПАСПОРТ", r.Color);
                s.Put(3, y + 2, r.Name.Length > 30 ? r.Name[..30] : r.Name, Pal.Text, Pal.Panel);
                if (r.Region.Length > 0) s.Put(3, y + 3, r.Region.Length > 30 ? r.Region[..30] : r.Region, Pal.Muted, Pal.Panel);
                s.Put(3, y + 5, "Индекс устойчивости", Pal.Muted, Pal.Panel);
                double anim = Math.Clamp(t / 1.0, 0, 1);
                s.Put(3, y + 6, $"{(int)(r.Index * anim),3} / 100", r.Color, Pal.Panel);
                s.Bar(3, y + 7, 30, r.Index * anim / 100.0, r.Color, Pal.Panel2);
                s.Wrap(3, y + 9, 30, r.Verdict, r.Color, Pal.Panel);
                s.Put(3, y + 11, $"Жителей: {r.PopThen} → {r.PopNow} ({r.Trend:+0;-0;0}%)", Pal.Text, Pal.Panel);
                for (int i = 0; i < r.Parts.Count; i++)
                {
                    var (label, value, color) = r.Parts[i];
                    int yy = y + 13 + i;
                    if (yy >= s.H - 2) break;
                    s.Put(3, yy, label, Pal.Muted, Pal.Panel);
                    s.Bar(15, yy, 14, value * anim / 100.0, color, Pal.Panel2);
                    s.Put(30, yy, $"{value,3}", color, Pal.Panel);
                }
                int rx = 37, rw = s.W - rx - 1;
                s.Box(rx, y, rw, s.H - y - 1, Pal.Border, Pal.Panel, "КАКИЕ ТЕХНОЛОГИИ ПОМОГУТ", Pal.Cyan);
                int ry = y + 2;
                if (r.Recommendations.Count == 0) s.Put(rx + 2, ry, "Все ключевые направления уже работают — так держать!", Pal.Green, Pal.Panel);
                foreach (var rec in r.Recommendations)
                {
                    if (ry >= s.H - 3) { s.Put(rx + 2, ry, "… остальное — в HTML-паспорте (клавиша S)", Pal.Muted, Pal.Panel); break; }
                    s.Put(rx + 2, ry, "♦", Pal.Cyan, Pal.Panel);
                    ry += s.Wrap(rx + 4, ry, rw - 6, rec, Pal.Text, Pal.Panel, 3);
                }
                string hint = saved != null ? "Сохранено: " + saved : "S — сохранить паспорт в HTML (откроется в браузере)   Enter — меню";
                s.Put(2, s.H - 1, hint.Length > s.W - 4 ? hint[..(s.W - 4)] : hint, saved != null ? Pal.Green : Pal.Muted);
                s.Flush();
            });
            if (key.Key == ConsoleKey.Enter || key.Key == ConsoleKey.Escape) return;
            if (key.Key == ConsoleKey.S || key.KeyChar is 'ы' or 'Ы') saved = Report.SavePassport(r);
        }
    }
}

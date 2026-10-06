namespace Hranitel;

/// <summary>Музей технологий: 13 залов — 13 технологий, изменивших село.</summary>
public static class Museum
{
    sealed record Hall(string Title, string Years, int Year, Mood Mood, string Gain, string Cost, string[] Extra);

    static readonly Hall[] Halls =
    {
        new("Электричество", "1920–1960", 1955, Mood.Night,
            "Свет в каждом доме: можно читать вечером, на ферме работают моторы и доильные аппараты, в домах появляются холодильники.",
            "Сёла, до которых не дотянули линию, начали пустеть первыми.", new string[0]),
        new("Радио", "1930-е", 1937, Mood.Day,
            "Новости, музыка и сигналы точного времени — в каждом доме. Село впервые слышит всю страну в тот же день.",
            "Один голос на всех: вещание было централизованным, а своё, местное, звучало всё реже.", new string[0]),
        new("Трактор и механизация", "1930–1960", 1940, Mood.Day,
            "Один трактор заменил десятки людей и лошадей. Урожаи выросли, тяжёлый труд стал легче.",
            "Рабочих рук стало нужно меньше — и молодёжь потянулась в города.", new string[0]),
        new("Телевидение", "1960–1980", 1968, Mood.Night,
            "Кино, новости и футбол — дома. Мир стал ближе, на крышах выросли «ёлочки» антенн.",
            "Городская жизнь на экране манила сильнее, чем родная улица.", new string[0]),
        new("Асфальт и автобус", "1960–1980", 1975, Mood.Dusk,
            "До райцентра — час вместо дня. Врачи, школа, рынок стали ближе, продукцию легче вывезти.",
            "По хорошей дороге легче не только приезжать, но и уезжать.", new string[0]),
        new("Телефон", "1970–1990", 1984, Mood.Night,
            "Можно вызвать врача, позвонить детям в город, договориться о делах — не выходя из сельсовета.",
            "Телефон долго был один на всю улицу — очередь к нему знала все новости.", new string[0]),
        new("Мобильная связь", "2000-е", 2009, Mood.Dusk,
            "Связь в кармане у каждого: безопасность, работа, общение с близкими.",
            "Без вышки село словно выпадает из жизни — семьи выбирают места со связью.", new string[0]),
        new("Интернет", "2010-е", 2019, Mood.Night,
            "Госуслуги, онлайн-учёба, интернет-магазины и видеосвязь с земляками по всему миру.",
            "Где интернета нет, молодые семьи не задерживаются.", new string[0]),
        new("Цифровые сервисы", "2020-е", 2024, Mood.Day,
            "Телемедицина, онлайн-школа, банк и документы — не выезжая из села.",
            "Пожилым людям нужна помощь, чтобы освоить новые сервисы.", new string[0]),
        new("Удалённая работа", "2020-е", 2028, Mood.Night,
            "Городская зарплата — сельская жизнь. Коворкинг в старом клубе возвращает молодёжь.",
            "Нужен быстрый и надёжный интернет — без него удалёнка невозможна.", new[] { "cowork" }),
        new("Умное земледелие", "2030-е", 2032, Mood.Day,
            "Датчики, дроны и ИИ экономят воду, удобрения и время. Сельское хозяйство снова даёт рабочие места.",
            "Нужны новые знания: фермер будущего — это и агроном, и программист.", new[] { "smartfarm" }),
        new("Зелёная энергия", "2030-е", 2038, Mood.Dusk,
            "Солнце и ветер дают селу собственную энергию, излишки можно продавать соседям.",
            "Ветряки и панели меняют привычный пейзаж — важно вписать их бережно.", new[] { "solar" }),
        new("Цифровая память", "2020–2050", 2042, Mood.Dusk,
            "Оцифрованные фото, письма и голоса прадедов; цифровой двойник села в AR. Память, которую не сжечь и не потерять.",
            "Оцифровать — мало: важно, чтобы историю продолжали рассказывать живые люди.", new[] { "museum", "twin" }),
    };

    public static void Run()
    {
        int i = 0;
        while (true)
        {
            var hall = Halls[i];
            var key = Term.Animate(t =>
            {
                var s = new Screen(Term.Width, Term.Height);
                int rows = Math.Clamp(s.H - 13, 8, 18);
                var st = SceneState.ForYear(hall.Year, hall.Mood);
                foreach (var f in hall.Extra) st.Flags.Add(f);
                Scene.Render(st, s.W, rows * 2, t).BlitTo(s, 0, 0);
                s.Fill(0, rows, s.W, 1, Pal.Panel2);
                s.PutGradient(2, rows, $"МУЗЕЙ ТЕХНОЛОГИЙ · ЗАЛ {i + 1} ИЗ {Halls.Length}", Pal.Gold, Pal.Cyan, Pal.Panel2);
                var yrs = $"{hall.Year} г. на панораме";
                s.Put(s.W - yrs.Length - 2, rows, yrs, Pal.Muted, Pal.Panel2);
                int y = rows + 2;
                s.PutGradient(2, y, hall.Title.ToUpperInvariant(), Pal.Gold, Pal.Text);
                s.Put(4 + hall.Title.Length, y, "· " + hall.Years, Pal.Muted);
                y += 2;
                s.Put(2, y, "Что изменилось", Pal.Green);
                y += 1 + s.Wrap(2, y + 1, s.W - 4, hall.Gain, Pal.Text, null, 3);
                y++;
                s.Put(2, y, "Обратная сторона", Pal.Coral);
                s.Wrap(2, y + 1, s.W - 4, hall.Cost, Pal.Text, null, 2);
                // лента залов
                var dots = string.Concat(Enumerable.Range(0, Halls.Length).Select(k => k == i ? "☼ " : "· "));
                s.Put(2, s.H - 1, "← → — листать залы   Esc — меню", Pal.Muted);
                s.Put(s.W - dots.Length - 1, s.H - 1, dots, Pal.Gold);
                s.Flush();
            });
            if (key.Key == ConsoleKey.RightArrow || key.Key == ConsoleKey.Enter || key.Key == ConsoleKey.Spacebar) i = (i + 1) % Halls.Length;
            else if (key.Key == ConsoleKey.LeftArrow) i = (i + Halls.Length - 1) % Halls.Length;
            else if (key.Key == ConsoleKey.Escape) return;
        }
    }
}

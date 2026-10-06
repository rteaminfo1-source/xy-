namespace Hronograf;

// Константы типов, общие с C++ движком (engine.cpp).
public static class T
{
    public const byte Water = 0, Sand = 1, Meadow = 2, Field = 3, Forest = 4, Road = 5, Asphalt = 6, Garden = 7,
                      Fallow = 8, YoungForest = 9, Bridge = 10, Plaza = 11, Marsh = 12, SmartField = 13;
}

public static class B
{
    public const int None = 0, House = 1, HouseAbandoned = 2, Ruin = 3, Church = 4, School = 5, Club = 6, Farm = 7,
                     Mts = 8, Tower = 9, Memorial = 10, Shop = 11, Fap = 12, Solar = 13, Wind = 14, Greenhouse = 15,
                     Hub = 16, Museum = 17, DronePort = 18, Mill = 19;
}

public static class F
{
    public const int Electric = 1, Tv = 2, Internet = 4, SolarRoof = 8, Modern = 16, Restored = 32, Closed = 64, Lit = 128;
}

public static class Te
{
    public const int Electric = 0, Radio = 1, Mech = 2, Phone = 3, Tv = 4, Roads = 5, Mobile = 6, Internet = 7,
                     Digital = 8, SmartAgro = 9, Green = 10, Ai = 11, Memory = 12;
}

public sealed record ChronicleEvent(int Year, int Code, int Value, int Scenario, string Text);

/// <summary>
/// Летопись села: три параллельных движка (по сценарию на каждый),
/// кэш показателей и тексты событий.
/// </summary>
public sealed class Chronicle : IDisposable
{
    public const int Scenarios = 3;
    public readonly int MapW, MapH, FirstYear, LastYear, Today;
    public string Name { get; }
    public int Scenario { get; set; } = 2;
    public byte[] Shade { get; }

    readonly IntPtr[] engines = new IntPtr[Scenarios];
    readonly HgStats[][] stats = new HgStats[Scenarios][];
    readonly List<ChronicleEvent>[] events = new List<ChronicleEvent>[Scenarios];
    readonly byte[] terrainBuf;

    public Chronicle(string name, int scenario)
    {
        Name = string.IsNullOrWhiteSpace(name) ? "Берёзовка" : name.Trim();
        Scenario = scenario;
        MapW = Native.hg_info(0); MapH = Native.hg_info(1);
        FirstYear = Native.hg_info(2); LastYear = Native.hg_info(3); Today = Native.hg_info(4);
        terrainBuf = new byte[MapW * MapH];

        // Три сценария считаются параллельно — каждый в своём экземпляре движка.
        Parallel.For(0, Scenarios, s => engines[s] = Native.hg_create(Name, s));
        if (engines.Any(e => e == IntPtr.Zero)) throw new InvalidOperationException("Движок не смог создать мир.");

        Shade = new byte[MapW * MapH];
        Native.hg_shade(engines[0], Shade);

        for (int s = 0; s < Scenarios; s++)
        {
            stats[s] = new HgStats[LastYear - FirstYear + 1];
            for (int y = FirstYear; y <= LastYear; y++) Native.hg_stats(engines[s], y, out stats[s][y - FirstYear]);

            int n = Native.hg_events(engines[s], null, 0);
            var raw = new HgEvent[n];
            Native.hg_events(engines[s], raw, n);
            events[s] = raw.Select(e => new ChronicleEvent(e.Year, e.Code, e.Value, e.Scenario,
                                                           Texts.Event(e.Code, e.Value, s, Name))).ToList();
        }
    }

    public IntPtr Engine => engines[Scenario];

    public HgStats Stats(int year, int scenario = -1)
    {
        if (scenario < 0) scenario = Scenario;
        year = Math.Clamp(year, FirstYear, LastYear);
        return stats[scenario][year - FirstYear];
    }

    public IReadOnlyList<ChronicleEvent> Events(int scenario = -1) => events[scenario < 0 ? Scenario : scenario];

    public byte[] Terrain(int year)
    {
        Native.hg_terrain(Engine, year, terrainBuf);
        return terrainBuf;
    }

    public HgBuilding[] Buildings(int year)
    {
        int n = Native.hg_buildings(Engine, year, null, 0);
        var arr = new HgBuilding[n];
        Native.hg_buildings(Engine, year, arr, n);
        return arr;
    }

    public void Dispose()
    {
        for (int i = 0; i < engines.Length; i++)
            if (engines[i] != IntPtr.Zero) { Native.hg_destroy(engines[i]); engines[i] = IntPtr.Zero; }
    }
}

public static class Texts
{
    public static readonly string[] TechNames =
    {
        "Электричество", "Радио", "Механизация", "Телефон", "Телевидение", "Асфальт и автобус",
        "Мобильная связь", "Интернет", "Цифровые сервисы", "Умное земледелие", "Зелёная энергия",
        "ИИ и цифр. двойник", "Цифровая память"
    };

    public static readonly string[] ScenarioNames = { "Инерция", "Рывок", "Гармония" };

    public static readonly string[] ScenarioTaglines =
    {
        "Ничего не меняем: технологии приходят, но не работают на село",
        "Максимум технологий: умные фермы, 5G, ИИ — старое сносим",
        "Технологии + память: цифровой музей, реставрация, умная экономика"
    };

    public static readonly string[] ScenarioFates = { "Тихое угасание", "Технополис без корней", "Возрождение" };

    public static string Era(int year, int scenario)
    {
        if (year < 1917) return "Старая деревня";
        if (year < 1930) return "Новое время";
        if (year < 1941) return "Колхозы и МТС";
        if (year < 1946) return "Великая Отечественная";
        if (year < 1960) return "Восстановление";
        if (year < 1986) return "Расцвет и урбанизация";
        if (year < 2000) return "Перелом";
        if (year < 2014) return "Мобильная эпоха";
        if (year <= 2026) return "Цифровой поворот";
        return scenario switch
        {
            0 => "Будущее · угасание",
            1 => "Будущее · рывок",
            _ => "Будущее · гармония"
        };
    }

    public static string Lens(int year) => year switch
    {
        < 1960 => "Фотопластинка и ч/б плёнка",
        < 1985 => "Цветная плёнка «Свема»",
        < 2006 => "Плёночная «мыльница»",
        <= 2026 => "Камера смартфона",
        _ => "AR-объектив дрона"
    };

    public static string Event(int code, int v, int scenario, string name) => code switch
    {
        1 => $"{name}: {v} жителей. Избы, лучина и керосиновая лампа — всё вручную.",
        3 => "Открыта школа — изба-читальня. Ликбез: учатся и дети, и взрослые.",
        4 => "Создан колхоз: поля объединены, построена общая ферма.",
        5 => "Храм закрыт, в здании устроили склад.",
        6 => "Машинно-тракторная станция: в село пришли первые тракторы.",
        7 => "Радио! На площади — громкоговоритель, новости слушает всё село.",
        8 => "«Лампочка Ильича»: первые дома подключены к электричеству.",
        9 => $"Началась Великая Отечественная война. На фронт ушли {v} земляков.",
        10 => $"Победа! Не вернулись с войны {v} человек.",
        11 => "Электрификация завершена: свет горит в каждом доме.",
        12 => "Построен Дом культуры: кино, танцы, библиотека.",
        13 => "Открылся сельский магазин.",
        14 => "Открыт фельдшерско-акушерский пункт (ФАП).",
        15 => $"Открыт обелиск: на нём {v} имён земляков, павших в годы войны.",
        16 => "Первый телевизор в селе — смотреть собираются всей улицей.",
        17 => "Главную улицу покрыли асфальтом, ходит автобус до райцентра.",
        18 => "Проведён телефон: в сельсовете и у первых абонентов.",
        19 => $"Пик послевоенной численности: {v} жителей.",
        20 => "Колхоз распался. Зарплату платят продуктами, молодёжь уезжает.",
        21 => "Закрылась ферма. Поля начинают зарастать.",
        22 => "Храм восстановлен силами жителей и земляков.",
        23 => "Появилась мобильная связь: на холме — вышка сотовой сети.",
        24 => "Школу закрыли: дети ездят на автобусе в райцентр.",
        25 => "Пришёл интернет: оптоволокно и точка доступа Wi‑Fi.",
        26 => "Цифровые сервисы: Госуслуги, телемедицина в ФАПе, онлайн-уроки.",
        27 => $"Сегодня: {v} жителей. Дальше — выбор сценария будущего.",
        28 => "ФАП закрыт. До ближайшего врача — 40 км.",
        29 => "Закрылся последний магазин. Автолавка — раз в неделю.",
        30 => $"Жителей стало меньше ста: {v} человек.",
        31 => "Дорога разрушается: рейсовый автобус отменён.",
        32 => v == 1 ? "Старая школа стала цифровым хабом: коворкинг для удалённой работы."
                     : "Открыт цифровой хаб: коворкинг и IT-мастерская для молодёжи.",
        33 => "Запущена сеть 5G: быстрый интернет даже в поле.",
        34 => "Старая школа стала Центром памяти — цифровым музеем села.",
        35 => "Солнечная электростанция: село само производит энергию.",
        36 => "На месте старой фермы — умные теплицы с датчиками и ИИ.",
        37 => "Дроны доставляют лекарства и следят за полями.",
        38 => "Открыта новая школа: робототехника и проектное обучение.",
        39 => "Построен первый современный эко-дом.",
        40 => $"Восстановлено {v} старинных домов — в них снова живут семьи.",
        41 => "Создан цифровой двойник села: ИИ помогает планировать развитие.",
        42 => "На холмах закрутились ветряные турбины.",
        43 => $"Население растёт — впервые за {v} лет!",
        44 => $"2060: {v} жителей. Итог — «{ScenarioFates[Math.Clamp(scenario, 0, 2)]}».",
        45 => "Дом культуры закрыт: нет средств на отопление.",
        46 => v == 1 ? "Дом культуры открылся вновь: медиастудия и киберспорт."
                     : "Дом культуры восстановлен: фольклорный ансамбль и медиастудия.",
        48 => "Мельница остановилась — зерно теперь сдают государству.",
        49 => "Оцифрован архив: фотографии, письма и голоса старожилов.",
        50 => "Брошенное здание школы обрушилось.",
        51 => "Брошенные поля заросли берёзовым лесом.",
        _ => $"Событие #{code}"
    };
}

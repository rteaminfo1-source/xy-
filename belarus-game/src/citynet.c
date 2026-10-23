/* Городской транспорт: схемы сетей и правила стройки перегонов.
 * Метро Минска (1984) и электротранспорт областных центров: витебский трамвай (1898),
 * троллейбусы Гомеля (1962), Могилёва (1970), Гродно (1974) и Бреста (1981).
 * Узлы — настоящие станции и районы городов; схемы условные (как на табличках в вагонах):
 * взаимное расположение примерно верное, расстояния — нет. Индексы сетей хранятся
 * в сохранении (Game.nets[]): новые сети — только в конец. */
#define BY_DATA
#include "citynet.h"
#include <string.h>
#include <math.h>

CityNetDef CITY_NETS[] = {
/* ===== Минское метро ===== */
/* Все 36 станций трёх линий, названия — по-белорусски, как на табличках в метро.
 * Узлы 0–23 и перегоны 0–20 — те же, что в сохранениях версии 7 (длинные перегоны разбиты на части:
 * старый перегон теперь первый из своих частей, остальные добавлены в конец — см. citynet_upgrade). */
{ "minsk_metro", C_MINSK, NT_METRO, L"Метрополитен", L"Минское метро", 1984, "metro", 0x1E6FD9,
  36, 33, 3,
  { { L"Інстытут культуры", 0.315f, 0.68f, 1984, L"Рядом — главный железнодорожный вокзал Минска." },
    { L"Плошча Леніна", 0.425f, 0.62f, 1984, L"Под площадью Независимости (раньше — площадь Ленина): Дом правительства и Красный костёл. Пересадка на станцию «Вакзальная»." },
    { L"Кастрычніцкая", 0.515f, 0.56f, 1984, L"Пересадка на станцию «Купалаўская». Рядом Дворец Республики." },
    { L"Плошча Перамогі", 0.5719f, 0.4963f, 1984, L"Монумент Победы и Вечный огонь в память о героях Великой Отечественной войны." },
    { L"Плошча Якуба Коласа", 0.6288f, 0.4325f, 1984, L"Памятник народному поэту Беларуси Якубу Коласу." },
    { L"Акадэмія навук", 0.6856f, 0.3688f, 1984, L"Рядом Национальная академия наук Беларуси." },
    { L"Парк Чалюскінцаў", 0.7425f, 0.305f, 1984, L"Большой парк и Центральный ботанический сад." },
    { L"Маскоўская", 0.7994f, 0.2413f, 1984, L"Конечная станция первой очереди 1984 года." },
    { L"Усход", 0.8563f, 0.1775f, 1986, L"Жилой район Восток на северо-востоке Минска." },
    { L"Уручча", 0.97f, 0.05f, 2007, L"Конечная на северо-востоке, у кольцевой дороги." },
    { L"Грушаўка", 0.24f, 0.74f, 2012, L"Старый район Грушевка на юго-западе Минска." },
    { L"Малінаўка", 0.02f, 0.92f, 2014, L"Конечная: один из самых больших жилых районов столицы." },
    { L"Каменная Горка", 0.045f, 0.070f, 2005, L"Конечная на западе Минска." },
    { L"Пушкінская", 0.210f, 0.226f, 1995, L"Названа по проспекту Пушкина." },
    { L"Фрунзенская", 0.320f, 0.330f, 1990, L"Пересадка на станцию «Юбілейная плошча»." },
    { L"Няміга", 0.44f, 0.415f, 1990, L"Рядом Минское замчище — отсюда начинался город." },
    { L"Купалаўская", 0.49f, 0.495f, 1990, L"Пересадка на станцию «Кастрычніцкая». Рядом театр имени Янки Купалы." },
    { L"Першамайская", 0.605f, 0.585f, 1990, NULL },
    { L"Трактарны завод", 0.735f, 0.725f, 1990, L"Минский тракторный завод: здесь делают тракторы «Беларус»." },
    { L"Аўтазаводская", 0.865f, 0.865f, 1997, L"Минский автомобильный завод — грузовики МАЗ." },
    { L"Магілёўская", 0.93f, 0.935f, 2001, L"Конечная на юго-востоке Минска." },
    { L"Юбілейная плошча", 0.357f, 0.285f, 2020, L"Пересадка на станцию «Фрунзенская»." },
    { L"Вакзальная", 0.357f, 0.575f, 2020, L"Под Привокзальной площадью; пересадка на станцию «Плошча Леніна»." },
    { L"Кавальская Слабада", 0.357f, 0.745f, 2020, L"Названа по старинной слободе ковалей — кузнецов." },
    { L"Барысаўскі тракт", 0.9131f, 0.1138f, 2007, L"Названа по старой дороге на Борисов." },
    { L"Міхалова", 0.165f, 0.8f, 2012, NULL },
    { L"Пятроўшчына", 0.09f, 0.86f, 2012, NULL },
    { L"Кунцаўшчына", 0.100f, 0.122f, 2005, NULL },
    { L"Спартыўная", 0.155f, 0.174f, 2005, NULL },
    { L"Маладзёжная", 0.265f, 0.278f, 1995, NULL },
    { L"Пралетарская", 0.67f, 0.655f, 1990, NULL },
    { L"Партызанская", 0.8f, 0.795f, 1997, L"Названа по Партизанскому проспекту — в честь партизан Беларуси." },
    { L"Плошча Францішка Багушэвіча", 0.357f, 0.430f, 2020, L"Названа в честь поэта Франтишка Богушевича — одного из зачинателей новой белорусской литературы." },
    { L"Аэрадромная", 0.357f, 0.820f, 2024, L"Рядом бывший аэропорт «Минск-1»: на его месте строят новый район." },
    { L"Неморшанскі сад", 0.357f, 0.895f, 2024, NULL },
    { L"Слуцкі гасцінец", 0.357f, 0.970f, 2024, L"Конечная на юге: отсюда шла старая дорога на Слуцк." } },
  { { 0, 1, 0 }, { 1, 2, 0 }, { 2, 3, 0 }, { 3, 4, 0 }, { 4, 5, 0 }, { 5, 6, 0 }, { 6, 7, 0 },   /* первая очередь */
    { 7, 8, 0 }, { 8, 24, 0 }, { 0, 10, 0 }, { 10, 25, 0 },
    { 15, 16, 1 }, { 14, 15, 1 }, { 16, 17, 1 }, { 17, 30, 1 }, { 18, 31, 1 }, { 19, 20, 1 }, { 29, 14, 1 }, { 28, 13, 1 },
    { 21, 32, 2 }, { 22, 23, 2 },
    /* версия 8: остальные части разбитых перегонов и вторая очередь Зеленолужской линии (2024) */
    { 24, 9, 0 }, { 25, 26, 0 }, { 26, 11, 0 },
    { 30, 18, 1 }, { 31, 19, 1 }, { 13, 29, 1 }, { 27, 28, 1 }, { 12, 27, 1 },
    { 32, 22, 2 }, { 23, 33, 2 }, { 33, 34, 2 }, { 34, 35, 2 } },
  { { L"Московская линия", 0x1E6FD9, 1984 }, { L"Автозаводская линия", 0xE0393E, 1990 }, { L"Зеленолужская линия", 0x2E9F4F, 2020 } },
  2, 3, { { 2, 16 }, { 1, 22 }, { 14, 21 } },
  L"Свислочь", 12,
  { { 0.22f, 0.00f }, { 0.30f, 0.08f }, { 0.40f, 0.17f }, { 0.475f, 0.27f }, { 0.515f, 0.36f }, { 0.535f, 0.45f },
    { 0.545f, 0.53f }, { 0.56f, 0.62f }, { 0.59f, 0.72f }, { 0.615f, 0.82f }, { 0.63f, 0.91f }, { 0.64f, 1.00f } },
  45, 30, FX(G(26), I(4), T(2), C(2)),
  "minsk_metro", 7, L"Первая очередь «Інстытут культуры» — «Маскоўская»",
  L"Девятое метро Советского Союза. Первая очередь открылась 30 июня 1984 года: восемь станций — от «Інстытут культуры» "
  L"до «Маскоўская». Вторая линия пошла в 1990 году, третья — в 2020-м. Это единственное метро в Беларуси." },

/* ===== Витебский трамвай ===== */
{ "vitebsk_tram", C_VITEBSK, NT_TRAM, L"Трамвай", L"Витебский трамвай", 1898, "tram", 0xC8402F,
  10, 12, 1,
  { { L"Вокзал", 0.20f, 0.74f, 0, NULL },
    { L"Ратуша", 0.42f, 0.60f, 0, L"Ратуша 1775 года — символ Витебска." },
    { L"Летний амфитеатр", 0.22f, 0.50f, 0, L"Здесь проходит фестиваль «Славянский базар в Витебске»." },
    { L"Смоленский рынок", 0.64f, 0.52f, 0, NULL },
    { L"Площадь Победы", 0.50f, 0.18f, 0, L"Мемориал «Три штыка» в честь воинов, партизан и подпольщиков." },
    { L"Улица Гагарина", 0.82f, 0.22f, 0, NULL },
    { L"Песковатик", 0.16f, 0.22f, 0, L"Старинный район на берегу Западной Двины." },
    { L"Билево", 0.86f, 0.68f, 0, NULL },
    { L"Улица Чкалова", 0.62f, 0.86f, 0, NULL },
    { L"Юг", 0.34f, 0.90f, 0, L"Новые микрорайоны на юге города." } },
  { { 0, 1, 0 }, { 1, 4, 0 }, { 1, 3, 0 },     /* первая очередь — линии 1898 года через центр и мост */
    { 1, 2, 0 }, { 4, 5, 0 }, { 3, 5, 0 }, { 2, 6, 0 }, { 3, 7, 0 }, { 1, 8, 0 }, { 8, 7, 0 }, { 0, 9, 0 }, { 9, 8, 0 } },
  { { L"Трамвайные пути", 0xC8402F, 1898 } },
  0, 0, { { 0, 0 } },
  L"Западная Двина", 8,
  { { 0.00f, 0.39f }, { 0.12f, 0.36f }, { 0.26f, 0.38f }, { 0.40f, 0.36f }, { 0.54f, 0.39f }, { 0.70f, 0.40f }, { 0.86f, 0.37f }, { 1.00f, 0.40f } },
  40, 22, FX(G(15), I(5), T(2), C(2)),
  "vitebsk_tram", 3, L"Первая очередь трамвая 1898 года",
  L"Первый электрический трамвай на белорусских землях пустили в Витебске в 1898 году — раньше, чем в Москве (1899) "
  L"и Петербурге (1907). Линии строила бельгийская компания. Витебский трамвай ходит и сегодня." },

/* ===== Гомельский троллейбус ===== */
{ "gomel_trolley", C_GOMEL, NT_TROLLEY, L"Троллейбус", L"Гомельский троллейбус", 1962, "tram", 0x1A9686,
  11, 12, 1,
  { { L"Вокзал", 0.28f, 0.40f, 0, NULL },
    { L"Площадь Ленина", 0.46f, 0.50f, 0, NULL },
    { L"Дворцовый парк", 0.58f, 0.62f, 0, L"Дворец Румянцевых и Паскевичей на высоком берегу Сожа." },
    { L"Новобелица", 0.88f, 0.72f, 0, L"Район на левом берегу Сожа." },
    { L"Университет", 0.40f, 0.76f, 0, L"Гомельский университет имени Франциска Скорины." },
    { L"Гомсельмаш", 0.34f, 0.18f, 0, L"Завод, где делают комбайны «Палессе»." },
    { L"Кленковский", 0.56f, 0.26f, 0, NULL },
    { L"Волотова", 0.70f, 0.12f, 0, NULL },
    { L"Мельников Луг", 0.10f, 0.56f, 0, NULL },
    { L"Химзавод", 0.14f, 0.86f, 0, NULL },
    { L"Костюковка", 0.14f, 0.08f, 0, NULL } },
  { { 0, 1, 0 }, { 1, 2, 0 }, { 2, 3, 0 }, { 1, 4, 0 }, { 0, 5, 0 }, { 5, 10, 0 }, { 1, 6, 0 }, { 6, 7, 0 },
    { 0, 8, 0 }, { 8, 9, 0 }, { 4, 9, 0 }, { 5, 6, 0 } },
  { { L"Троллейбусные линии", 0x1A9686, 1962 } },
  0, 0, { { 0, 0 } },
  L"Сож", 7,
  { { 0.82f, 0.00f }, { 0.80f, 0.18f }, { 0.76f, 0.36f }, { 0.72f, 0.52f }, { 0.74f, 0.68f }, { 0.70f, 0.84f }, { 0.72f, 1.00f } },
  30, 18, FX(G(22), I(3), T(2), C(1)),
  NULL, 0, NULL,
  L"Троллейбус пришёл в Гомель в 1962 году — вторым в Белоруссии после Минска (1952). Линии протянулись от вокзала "
  L"к заводам, университету и новым районам на обоих берегах Сожа." },

/* ===== Могилёвский троллейбус ===== */
{ "mogilev_trolley", C_MOGILEV, NT_TROLLEY, L"Троллейбус", L"Могилёвский троллейбус", 1970, "tram", 0xE0841A,
  9, 10, 1,
  { { L"Вокзал", 0.30f, 0.72f, 0, NULL },
    { L"Площадь Славы", 0.46f, 0.46f, 0, L"Ратуша на высоком берегу Днепра." },
    { L"Площадь Ленина", 0.36f, 0.28f, 0, NULL },
    { L"Луполово", 0.86f, 0.50f, 0, L"Район на левом берегу Днепра." },
    { L"Химволокно", 0.86f, 0.16f, 0, L"Завод «Могилёвхимволокно»." },
    { L"Юбилейный", 0.12f, 0.14f, 0, NULL },
    { L"Казимировка", 0.08f, 0.46f, 0, NULL },
    { L"Автозавод", 0.52f, 0.88f, 0, L"Могилёвский автозавод (МоАЗ): самосвалы для карьеров." },
    { L"Лифтмаш", 0.16f, 0.88f, 0, L"Завод лифтов «Могилёвлифтмаш»." } },
  { { 0, 1, 0 }, { 1, 2, 0 }, { 1, 3, 0 }, { 3, 4, 0 }, { 2, 5, 0 }, { 2, 6, 0 }, { 0, 7, 0 }, { 0, 8, 0 },
    { 6, 8, 0 }, { 2, 4, 0 } },
  { { L"Троллейбусные линии", 0xE0841A, 1970 } },
  0, 0, { { 0, 0 } },
  L"Днепр", 6,
  { { 0.64f, 0.00f }, { 0.62f, 0.20f }, { 0.64f, 0.40f }, { 0.68f, 0.58f }, { 0.66f, 0.78f }, { 0.70f, 1.00f } },
  30, 18, FX(G(22), I(3), T(2), C(1)),
  NULL, 0, NULL,
  L"Первые могилёвские троллейбусы вышли на линию в 1970 году и повезли пассажиров от вокзала к центру и заводам. "
  L"Мосты через Днепр соединили старый город с левобережным Луполовом." },

/* ===== Гродненский троллейбус ===== */
{ "grodno_trolley", C_GRODNO, NT_TROLLEY, L"Троллейбус", L"Гродненский троллейбус", 1974, "tram", 0x8A4FC7,
  8, 9, 1,
  { { L"Вокзал", 0.62f, 0.20f, 0, NULL },
    { L"Советская площадь", 0.46f, 0.38f, 0, L"Главная площадь Гродно; рядом Фарный костёл." },
    { L"Замки", 0.26f, 0.46f, 0, L"Старый и Новый замки над Неманом." },
    { L"Девятовка", 0.10f, 0.24f, 0, NULL },
    { L"Вишневец", 0.32f, 0.10f, 0, NULL },
    { L"Ольшанка", 0.88f, 0.10f, 0, NULL },
    { L"Азот", 0.88f, 0.40f, 0, L"Химический завод «Гродно Азот»." },
    { L"Южный", 0.50f, 0.86f, 0, L"Жилой район на левом берегу Немана." } },
  { { 1, 0, 0 }, { 1, 2, 0 }, { 1, 7, 0 }, { 2, 3, 0 }, { 3, 4, 0 }, { 4, 0, 0 }, { 0, 5, 0 }, { 0, 6, 0 }, { 6, 7, 0 } },
  { { L"Троллейбусные линии", 0x8A4FC7, 1974 } },
  1, 0, { { 0, 0 } },
  L"Неман", 7,
  { { 0.00f, 0.62f }, { 0.14f, 0.66f }, { 0.30f, 0.60f }, { 0.44f, 0.58f }, { 0.60f, 0.62f }, { 0.80f, 0.60f }, { 1.00f, 0.64f } },
  30, 18, FX(G(22), I(3), T(2), C(1)),
  NULL, 0, NULL,
  L"Гродненский троллейбус открыт в 1974 году. Линии связали старый центр у замков над Неманом с вокзалом, "
  L"новыми жилыми районами и химическим заводом «Азот»." },

/* ===== Брестский троллейбус ===== */
{ "brest_trolley", C_BREST, NT_TROLLEY, L"Троллейбус", L"Брестский троллейбус", 1981, "tram", 0x2F9E57,
  10, 11, 1,
  { { L"Площадь Ленина", 0.48f, 0.42f, 0, NULL },
    { L"Вокзал", 0.42f, 0.14f, 0, L"Брестский вокзал — западные ворота страны." },
    { L"Брестская крепость", 0.10f, 0.58f, 0, L"Крепость-герой: её защитники первыми приняли бой 22 июня 1941 года." },
    { L"Восток", 0.86f, 0.42f, 0, NULL },
    { L"Ковалёво", 0.74f, 0.14f, 0, NULL },
    { L"Вулька", 0.82f, 0.86f, 0, NULL },
    { L"Южный городок", 0.38f, 0.88f, 0, NULL },
    { L"Киевка", 0.62f, 0.72f, 0, NULL },
    { L"Тришин", 0.14f, 0.14f, 0, NULL },
    { L"Дубровка", 0.94f, 0.66f, 0, NULL } },
  { { 0, 1, 0 }, { 0, 2, 0 }, { 0, 3, 0 }, { 1, 4, 0 }, { 4, 3, 0 }, { 0, 7, 0 }, { 7, 6, 0 }, { 7, 5, 0 },
    { 1, 8, 0 }, { 3, 9, 0 }, { 5, 9, 0 } },
  { { L"Троллейбусные линии", 0x2F9E57, 1981 } },
  0, 0, { { 0, 0 } },
  L"Мухавец", 7,
  { { 0.00f, 0.70f }, { 0.12f, 0.66f }, { 0.24f, 0.60f }, { 0.42f, 0.58f }, { 0.60f, 0.56f }, { 0.80f, 0.58f }, { 1.00f, 0.56f } },
  30, 18, FX(G(22), I(3), T(2), C(1)),
  NULL, 0, NULL,
  L"Брестский троллейбус открылся 18 апреля 1981 года. Он соединил центр с вокзалом, новыми районами "
  L"и мемориалом «Брестская крепость-герой»." },
};
const int CITY_NET_COUNT = (int)(sizeof CITY_NETS / sizeof CITY_NETS[0]);

static int proj_find(const char *id)
{
    if (!id) return -1;
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (strcmp(PROJECTS[p].id, id) == 0) return p;
    return -1;
}

void citynet_init(void)
{
    for (int n = 0; n < CITY_NET_COUNT; n++) {
        CITY_NETS[n].techi = tech_index(CITY_NETS[n].tech);
        CITY_NETS[n].proji = proj_find(CITY_NETS[n].proj);
    }
}

int citynet_of_city(int c)
{
    for (int n = 0; n < CITY_NET_COUNT; n++)
        if (CITY_NETS[n].city == c) return n;
    return -1;
}

static int net_ok(int n) { return n >= 0 && n < CITY_NET_COUNT && n < MAX_NETS; }

/* перегоны первой очереди (стройка PROJECTS) */
static unsigned long long first_mask(const CityNetDef *d)
{
    return d->firstEdges > 0 ? (d->firstEdges >= 64 ? ~0ull : (1ull << d->firstEdges) - 1) : 0;
}

/* построенные перегоны: свои и первая очередь, если стройка построена из вкладки «Стройки» */
static unsigned long long net_edges(const Game *g, int n)
{
    const CityNetDef *d = &CITY_NETS[n];
    unsigned long long m = g->nets[n].edges;
    if (d->proji >= 0 && g->proj[d->proji]) m |= first_mask(d);
    if (d->nedges < 64) m &= (1ull << d->nedges) - 1;
    return m;
}

unsigned long long citynet_station_mask(const Game *g, int n)
{
    if (!net_ok(n)) return 0;
    const CityNetDef *d = &CITY_NETS[n];
    unsigned long long m = net_edges(g, n), s = 0;
    for (int e = 0; e < d->nedges; e++)
        if (m >> e & 1) s |= 1ull << d->edges[e].a | 1ull << d->edges[e].b;
    return s;
}

static void store_nodes(Game *g, int n)
{
    unsigned long long s = citynet_station_mask(g, n);
    g->nets[n].nodes = (unsigned)s;
    g->nets[n].nodesHi = (unsigned)(s >> 32);
}

/* сохранение версии 7: в Минском метро было 24 станции и 21 перегон. Длинные перегоны с тех пор
 * разбиты на части (старый номер — первая часть, остальные — в конце списка): достраиваем части */
void citynet_upgrade(Game *g, int from)
{
    if (from < 8) {
        static const struct { const char *net; unsigned char old, add[3]; } SPLIT[] = {
            { "minsk_metro", 8, { 21 } },          /* Усход — Уручча: через «Барысаўскі тракт» */
            { "minsk_metro", 10, { 22, 23 } },     /* Грушаўка — Малінаўка: через «Міхалова» и «Пятроўшчына» */
            { "minsk_metro", 14, { 24 } },         /* Першамайская — Трактарны завод: через «Пралетарская» */
            { "minsk_metro", 15, { 25 } },         /* Трактарны завод — Аўтазаводская: через «Партызанская» */
            { "minsk_metro", 17, { 26 } },         /* Пушкінская — Фрунзенская: через «Маладзёжная» */
            { "minsk_metro", 18, { 27, 28 } },     /* Каменная Горка — Пушкінская: через «Кунцаўшчына» и «Спартыўная» */
            { "minsk_metro", 19, { 29 } },         /* Юбілейная плошча — Вакзальная: через «Плошча Францішка Багушэвіча» */
        };
        for (unsigned k = 0; k < sizeof SPLIT / sizeof SPLIT[0]; k++)
            for (int n = 0; n < CITY_NET_COUNT && n < MAX_NETS; n++) {
                if (strcmp(CITY_NETS[n].id, SPLIT[k].net) != 0 || !(g->nets[n].edges >> SPLIT[k].old & 1)) continue;
                for (int i = 0; i < 3 && SPLIT[k].add[i]; i++) g->nets[n].edges |= 1ull << SPLIT[k].add[i];
            }
    }
    for (int n = 0; n < CITY_NET_COUNT && n < MAX_NETS; n++) store_nodes(g, n);
}

int citynet_state(const Game *g, int n)
{
    if (!net_ok(n)) return NS_FUTURE;
    const CityNetDef *d = &CITY_NETS[n];
    if (g->year < d->year && !citynet_station_mask(g, n)) return NS_FUTURE;   /* стройка раньше срока открывает сеть досрочно */
    if (d->techi >= 0 && !g->tech[d->techi]) return NS_LOCKED;
    return NS_AVAILABLE;
}

int citynet_edge_built(const Game *g, int n, int e)
{
    if (!net_ok(n) || e < 0 || e >= CITY_NETS[n].nedges) return 0;
    return (int)(net_edges(g, n) >> e & 1);
}

int citynet_node_station(const Game *g, int n, int v)
{
    if (!net_ok(n) || v < 0 || v >= CITY_NETS[n].nnodes) return 0;
    return (int)(citynet_station_mask(g, n) >> v & 1);
}

int citynet_stations(const Game *g, int n)
{
    unsigned long long s = citynet_station_mask(g, n);
    int k = 0;
    for (; s; s &= s - 1) k++;
    return k;
}

int citynet_edges_count(const Game *g, int n)
{
    if (!net_ok(n)) return 0;
    unsigned long long m = net_edges(g, n);
    int k = 0;
    for (; m; m &= m - 1) k++;
    return k;
}

/* к узлу можно подвести перегон: в нём станция, это первая станция пустой сети или пересадка со станции */
static int node_open(const CityNetDef *d, unsigned long long st, int v)
{
    if (st >> v & 1) return 1;
    if (!st) return v == d->startNode;
    for (int k = 0; k < d->nxfer; k++) {
        if (d->xfer[k][0] == v && st >> d->xfer[k][1] & 1) return 1;
        if (d->xfer[k][1] == v && st >> d->xfer[k][0] & 1) return 1;
    }
    return 0;
}

int citynet_can_build(const Game *g, int n, int e)
{
    if (!net_ok(n) || e < 0 || e >= CITY_NETS[n].nedges) return 0;
    const CityNetDef *d = &CITY_NETS[n];
    if (citynet_state(g, n) != NS_AVAILABLE || citynet_edge_built(g, n, e)) return 0;
    if (g->year < d->lines[d->edges[e].line].year) return 0;
    unsigned long long st = citynet_station_mask(g, n);
    return node_open(d, st, d->edges[e].a) || node_open(d, st, d->edges[e].b);
}

const wchar_t *citynet_cannot_build(const Game *g, int n, int e)
{
    static wchar_t buf[200];
    if (!net_ok(n) || e < 0 || e >= CITY_NETS[n].nedges) return L"Такого перегона нет.";
    const CityNetDef *d = &CITY_NETS[n];
    const NetEdge *ed = &d->edges[e];
    if (citynet_edge_built(g, n, e)) return L"Перегон уже построен.";
    int st = citynet_state(g, n);
    if (st == NS_FUTURE) { swprintf(buf, 200, L"%ls появится в %d году.", d->title, d->year); return buf; }
    if (st == NS_LOCKED) { swprintf(buf, 200, L"Сначала изучите технологию «%ls».", TECHS[d->techi].name); return buf; }
    if (g->year < d->lines[ed->line].year) {
        swprintf(buf, 200, L"%ls строится с %d года.", d->lines[ed->line].name, d->lines[ed->line].year);
        return buf;
    }
    unsigned long long sm = citynet_station_mask(g, n);
    if (!node_open(d, sm, ed->a) && !node_open(d, sm, ed->b)) {
        if (!sm) { swprintf(buf, 200, L"Начните со станции «%ls».", d->nodes[d->startNode].name); return buf; }
        return L"Перегон должен начинаться от построенной станции.";
    }
    if (g->gold < citynet_edge_gold(g, n, e) || g->ind < citynet_edge_ind(g, n, e)) return L"Не хватает казны или производства.";
    return NULL;
}

/* множитель цены: длинный перегон дороже, в поздние эпохи всё дороже, сложность — как у строек */
static double edge_price(const Game *g, int n, int e)
{
    const CityNetDef *d = &CITY_NETS[n];
    const NetNode *a = &d->nodes[d->edges[e].a], *b = &d->nodes[d->edges[e].b];
    double dx = (a->x - b->x) * 1.45, dy = a->y - b->y;   /* схема шире, чем выше */
    double len = sqrt(dx * dx + dy * dy) / 0.13;
    if (len < 0.7) len = 0.7;
    if (len > 1.6) len = 1.6;
    int e0 = era_of_year(d->year), era = g->era > e0 ? g->era : e0;
    return len * era_scale(era) / era_scale(e0) * (1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0);
}

int citynet_edge_gold(const Game *g, int n, int e)
{
    if (!net_ok(n) || e < 0 || e >= CITY_NETS[n].nedges) return 0;
    return (int)lround(CITY_NETS[n].goldPerEdge * edge_price(g, n, e));
}

int citynet_edge_ind(const Game *g, int n, int e)
{
    if (!net_ok(n) || e < 0 || e >= CITY_NETS[n].nedges) return 0;
    return (int)lround(CITY_NETS[n].indPerEdge * edge_price(g, n, e));
}

int citynet_build(Game *g, int n, int e)
{
    if (citynet_cannot_build(g, n, e)) return 0;
    const CityNetDef *d = &CITY_NETS[n];
    unsigned long long before = citynet_station_mask(g, n);
    g->gold -= citynet_edge_gold(g, n, e);
    g->ind -= citynet_edge_ind(g, n, e);
    g->nets[n].edges |= 1ull << e;
    store_nodes(g, n);
    g->erasBuilt |= 1u << (g->era - 1);
    unsigned long long fresh = citynet_station_mask(g, n) & ~before;
    const NetEdge *ed = &d->edges[e];
    int v = fresh >> ed->b & 1 ? ed->b : fresh >> ed->a & 1 ? ed->a : -1;
    if (v >= 0) game_log(g, LOG_BUILD, L"%ls: открыта станция «%ls»", d->title, d->nodes[v].name);
    else game_log(g, LOG_BUILD, L"%ls: открыт перегон «%ls» — «%ls»", d->title, d->nodes[ed->a].name, d->nodes[ed->b].name);
    /* вся первая очередь проложена своими руками — стройка засчитана (задания, достижения, бонус) */
    unsigned long long fm = first_mask(d);
    if (d->proji >= 0 && !g->proj[d->proji] && fm && (g->nets[n].edges & fm) == fm) {
        g->proj[d->proji] = 1;
        game_log(g, LOG_BUILD, L"Построено: %ls (%ls)", PROJECTS[d->proji].name, CITIES[PROJECTS[d->proji].city].name);
    }
    game_check_achievements(g);
    return 1;
}

void citynet_project_built(Game *g, int proj, int *gold, int *ind)
{
    int rg = 0, ri = 0;
    for (int n = 0; n < CITY_NET_COUNT && n < MAX_NETS; n++) {
        const CityNetDef *d = &CITY_NETS[n];
        if (d->proji != proj || proj < 0) continue;
        unsigned long long fm = first_mask(d), own = g->nets[n].edges & fm;
        for (int e = 0; e < d->firstEdges; e++)
            if (own >> e & 1) { rg += citynet_edge_gold(g, n, e); ri += citynet_edge_ind(g, n, e); }
        /* возвращаем не больше, чем стоила сама стройка */
        int pg = game_proj_gold(g, proj), pi = game_proj_ind(g, proj);
        if (rg > pg) rg = pg;
        if (ri > pi) ri = pi;
        g->nets[n].edges |= fm;
        store_nodes(g, n);
    }
    g->gold += rg;
    g->ind += ri;
    if (gold) *gold = rg;
    if (ind) *ind = ri;
}

void citynet_net_income(const Game *g, int n, double out[R_COUNT])
{
    memset(out, 0, sizeof(double) * R_COUNT);
    if (!net_ok(n)) return;
    const CityNetDef *d = &CITY_NETS[n];
    int st = citynet_stations(g, n);
    if (!st) return;
    int e0 = era_of_year(d->year), era = g->era > e0 ? g->era : e0;
    double grow = era_scale(era) / era_scale(e0);   /* казна, знания и производство растут с эпохой */
    for (int r = 0; r < R_COUNT; r++) {
        double k = (r == R_GOLD || r == R_SCI || r == R_IND) ? grow : 1.0;
        out[r] = d->fxPerStation.v[r] / 10.0 * st * k;
    }
}

void citynet_income(const Game *g, double out[R_COUNT])
{
    double one[R_COUNT];
    memset(out, 0, sizeof(double) * R_COUNT);
    for (int n = 0; n < CITY_NET_COUNT && n < MAX_NETS; n++) {
        citynet_net_income(g, n, one);
        for (int r = 0; r < R_COUNT; r++) out[r] += one[r];
    }
}

// =====================================================================
//  ХРОНОГРАФ МАЛОЙ РОДИНЫ — нативный движок (C++17)
//  См. engine.h — там описан внешний интерфейс.
// =====================================================================
#include "engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

// ---------------------------------------------------------------------
//  Константы мира
// ---------------------------------------------------------------------
constexpr int W = 120, H = 80;
constexpr int Y0 = 1900, Y1 = 2060, NY = Y1 - Y0 + 1;
constexpr int TODAY = 2026;   // точка развилки: дальше — сценарии

enum Terrain : uint8_t {
    T_WATER, T_SAND, T_MEADOW, T_FIELD, T_FOREST, T_ROAD, T_ASPHALT,
    T_GARDEN, T_FALLOW, T_YOUNG_FOREST, T_BRIDGE, T_PLAZA, T_MARSH,
    T_SMART_FIELD
};

enum Building {
    B_NONE, B_HOUSE, B_HOUSE_ABANDONED, B_RUIN, B_CHURCH, B_SCHOOL, B_CLUB,
    B_FARM, B_MTS, B_TOWER, B_MEMORIAL, B_SHOP, B_FAP, B_SOLAR, B_WIND,
    B_GREENHOUSE, B_HUB, B_MUSEUM, B_DRONEPORT, B_MILL
};

enum Flag {
    F_ELECTRIC = 1, F_TV = 2, F_INTERNET = 4, F_SOLAR_ROOF = 8,
    F_MODERN = 16, F_RESTORED = 32, F_CLOSED = 64, F_LIT = 128
};

enum Tech {
    TE_ELECTRIC, TE_RADIO, TE_MECH, TE_PHONE, TE_TV, TE_ROADS, TE_MOBILE,
    TE_INTERNET, TE_DIGITAL, TE_SMARTAGRO, TE_GREEN, TE_AI, TE_MEMORY,
    TE_COUNT
};
static_assert(TE_COUNT == HG_TECH_COUNT, "tech count mismatch");

enum Scenario { SC_INERTIA = 0, SC_LEAP = 1, SC_HARMONY = 2 };

// Коды событий летописи (тексты — на стороне C#).
enum Event {
    EV_START = 1, EV_SCHOOL_OPEN = 3, EV_KOLKHOZ = 4, EV_CHURCH_CLOSED = 5,
    EV_MTS = 6, EV_RADIO = 7, EV_ELECTRIC_FIRST = 8, EV_WAR_START = 9,
    EV_WAR_END = 10, EV_ELECTRIC_ALL = 11, EV_CLUB = 12, EV_SHOP = 13,
    EV_FAP = 14, EV_MEMORIAL = 15, EV_TV = 16, EV_ASPHALT = 17, EV_PHONE = 18,
    EV_POP_PEAK = 19, EV_KOLKHOZ_END = 20, EV_FARM_CLOSED = 21,
    EV_CHURCH_RESTORED = 22, EV_MOBILE = 23, EV_SCHOOL_CLOSED = 24,
    EV_INTERNET = 25, EV_DIGITAL = 26, EV_TODAY = 27, EV_FAP_CLOSED = 28,
    EV_SHOP_CLOSED = 29, EV_POP_BELOW_100 = 30, EV_ROADS_DECAY = 31,
    EV_HUB = 32, EV_5G = 33, EV_MUSEUM = 34, EV_SOLAR = 35,
    EV_GREENHOUSE = 36, EV_DRONES = 37, EV_NEW_SCHOOL = 38,
    EV_FIRST_MODERN = 39, EV_RESTORED = 40, EV_TWIN = 41, EV_WIND = 42,
    EV_POP_GROWTH = 43, EV_FINAL = 44, EV_CLUB_CLOSED = 45,
    EV_CLUB_REOPEN = 46, EV_MILL_STOP = 48, EV_DIGITAL_ARCHIVE = 49,
    EV_SCHOOL_RUIN = 50, EV_FIELDS_FOREST = 51
};

// ---------------------------------------------------------------------
//  Утилиты
// ---------------------------------------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    double range(double a, double b) { return a + (b - a) * uni(); }
};

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return h;
}

// Детерминированный хеш → [0,1). Нужен, чтобы история не зависела от сценария.
static inline float hash01(uint32_t a, uint32_t b = 0, uint32_t c = 0) {
    uint32_t h = a * 0x8DA6B343u ^ b * 0xD8163841u ^ c * 0xCB1AB31Fu;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (h & 0xFFFFFF) / 16777216.0f;
}

static inline double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
static inline double logistic(double t, double mid, double w) { return 1.0 / (1.0 + std::exp(-(t - mid) / w)); }
static inline double smooth01(double e0, double e1, double x) {
    double t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3 - 2 * t);
}

// Кусочно-линейная интерполяция по таблице {год, значение}.
template <size_t N>
static double interp(const double (&kv)[N][2], double t) {
    if (t <= kv[0][0]) return kv[0][1];
    for (size_t i = 1; i < N; ++i)
        if (t <= kv[i][0]) {
            double k = (t - kv[i - 1][0]) / (kv[i][0] - kv[i - 1][0]);
            return kv[i - 1][1] + (kv[i][1] - kv[i - 1][1]) * k;
        }
    return kv[N - 1][1];
}

// Value-noise с фрактальным сложением октав (fBm) — для рельефа и влажности.
struct Noise {
    static constexpr int N = 64;
    float lat[N * N];
    explicit Noise(Rng& r) { for (float& v : lat) v = (float)r.uni(); }
    float at(int x, int y) const { return lat[(y & (N - 1)) * N + (x & (N - 1))]; }
    float sample(float x, float y) const {
        int xi = (int)std::floor(x), yi = (int)std::floor(y);
        float fx = x - xi, fy = y - yi;
        fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
        float a = at(xi, yi), b = at(xi + 1, yi), c = at(xi, yi + 1), d = at(xi + 1, yi + 1);
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }
    float fbm(float x, float y, int oct) const {
        float sum = 0, amp = 0.5f, f = 1, norm = 0;
        for (int i = 0; i < oct; ++i) { sum += amp * sample(x * f, y * f); norm += amp; amp *= 0.5f; f *= 2.03f; }
        return sum / norm;
    }
};

// ---------------------------------------------------------------------
//  Исторические таблицы (типичное село средней полосы России)
// ---------------------------------------------------------------------
// Рождаемость на одного человека 0–29 лет.
static const double FERT[][2] = {
    {1900, .074}, {1914, .072}, {1918, .056}, {1925, .074}, {1935, .062},
    {1942, .030}, {1946, .040}, {1950, .060}, {1960, .052}, {1970, .044},
    {1980, .042}, {1988, .042}, {1993, .028}, {2000, .028}, {2012, .034},
    {2026, .030}};
// Смертность по когортам.
static const double DY[][2] = {{1900, .024}, {1930, .016}, {1950, .006}, {1970, .002}, {2026, .0012}};
static const double DM[][2] = {{1900, .012}, {1950, .008}, {1985, .008}, {1995, .011}, {2010, .009}, {2026, .007}};
static const double DO[][2] = {{1900, .085}, {1950, .068}, {1990, .066}, {2000, .072}, {2026, .060}};
// «Притяжение города» — доля молодёжи, уезжающей за год.
static const double PULL[][2] = {
    {1900, .004}, {1925, .006}, {1930, .016}, {1940, .012}, {1946, .008},
    {1955, .026}, {1965, .034}, {1975, .034}, {1985, .026}, {1991, .010},
    {1996, .024}, {2005, .034}, {2015, .030}, {2026, .030}};
// Размер домохозяйства.
static const double HH[][2] = {{1900, 7.2}, {1930, 6.0}, {1950, 5.0}, {1970, 4.0}, {1990, 3.1}, {2010, 2.6}, {2026, 2.4}, {2060, 2.6}};
// Экономика (рабочие места, доходы) — историческая база.
static const double ECON[][2] = {
    {1900, 30}, {1930, 46}, {1940, 52}, {1943, 30}, {1950, 46}, {1965, 66},
    {1980, 72}, {1988, 70}, {1993, 42}, {2000, 26}, {2010, 28}, {2026, 30}, {2060, 20}};
// Доля обрабатываемых полей.
static const double AGRO[][2] = {
    {1900, .55}, {1925, .60}, {1932, .80}, {1941, .70}, {1945, .50}, {1950, .75},
    {1965, .92}, {1988, .95}, {1993, .70}, {1998, .42}, {2005, .30}, {2026, .28}};

// Технологии: историческая S-кривая + будущее по сценариям.
struct TechDef { double mid, w, cap; double fmid[3], fw[3], fcap[3]; };
static const TechDef TECH[TE_COUNT] = {
    /* электричество */ {1951, 4.5, 1.00, {2030, 2030, 2030}, {3, 3, 3}, {1, 1, 1}},
    /* радио         */ {1940, 3.5, 1.00, {2030, 2030, 2030}, {3, 3, 3}, {1, 1, 1}},
    /* механизация   */ {1947, 5.0, 0.95, {2034, 2030, 2030}, {4, 3, 3}, {.55, 1, 1}},
    /* телефон       */ {1979, 6.0, 0.80, {2030, 2030, 2030}, {4, 4, 4}, {.8, .8, .8}},
    /* телевидение   */ {1969, 4.0, 1.00, {2034, 2032, 2032}, {4, 4, 4}, {.9, .8, .85}},
    /* дороги        */ {1978, 5.0, 0.80, {2040, 2030, 2031}, {4, 3, 3}, {.45, 1, 1}},
    /* мобильная     */ {2008, 2.5, 0.95, {2030, 2028, 2028}, {2, 2, 2}, {.95, 1, 1}},
    /* интернет      */ {2018, 2.2, 0.88, {2030, 2027, 2027}, {3, 2, 2}, {.75, 1, 1}},
    /* цифр. сервисы */ {2022, 2.5, 0.75, {2032, 2028, 2028}, {4, 2.5, 2.5}, {.55, .98, .95}},
    /* умное земл.   */ {2045, 4.0, 0.02, {2045, 2032, 2033}, {4, 2.5, 3}, {.15, .95, .85}},
    /* зелёная энерг.*/ {2040, 4.0, 0.03, {2046, 2032, 2033}, {4, 2.5, 3}, {.2, .95, .9}},
    /* ИИ и двойник  */ {2045, 4.0, 0.02, {2050, 2038, 2038}, {4, 3, 3}, {.1, .95, .85}},
    /* цифр. память  */ {2021, 3.0, 0.15, {2035, 2032, 2030}, {4, 3, 2.5}, {.18, .4, 1}},
};

static double techLevel(int i, double t, int sc) {
    const TechDef& d = TECH[i];
    double hist = d.cap * logistic(t, d.mid, d.w);
    if (t <= TODAY) return hist;
    double v0 = d.cap * logistic(TODAY, d.mid, d.w);
    double l0 = logistic(TODAY, d.fmid[sc], d.fw[sc]);
    double g = (logistic(t, d.fmid[sc], d.fw[sc]) - l0) / (1 - l0);
    return v0 + (d.fcap[sc] - v0) * clamp01(g);
}

// ---------------------------------------------------------------------
//  Мир
// ---------------------------------------------------------------------
struct Rect { int x = 0, y = 0, w = 0, h = 0; };

enum SiteState { S_EMPTY, S_LIVED, S_ABANDONED, S_RUIN };

struct Site {
    Rect house, garden;
    double order = 0;
    float h1 = 0, h2 = 0, h3 = 0, h4 = 0;
    int state = S_EMPTY, since = 0, keepFlags = 0;
    bool modern = false, restored = false, everLived = false;
};

enum Slot {
    SL_CHURCH, SL_RADIO, SL_SHOP, SL_MEMORIAL, SL_SCHOOL, SL_CLUB, SL_FAP,
    SL_MILL, SL_MTS, SL_FARM, SL_HUB, SL_NEWSCHOOL, SL_TVTOWER, SL_CELLTOWER,
    SL_GREEN2, SL_GREEN3, SL_SOLAR, SL_WIND
};

struct Pub { Rect r; int slot = 0, index = 0; int type = B_NONE, flags = 0, variant = 0; };

struct Snap {
    std::vector<uint8_t> terr;
    std::vector<HgBuilding> b;
    HgStats st{};
};

class World {
public:
    World(const std::string& name, int scenario)
        : sc(clampScenario(scenario)), rng(fnv1a(name.empty() ? std::string("Berezovka") : name)) {
        generate();
        simulate();
    }

    std::vector<uint8_t> shade;
    std::vector<Snap> snaps;
    std::vector<HgEvent> events;

private:
    int sc;
    Rng rng;
    int cx = 0, cy = 0;
    std::vector<float> elev, moist, roadTh, fieldTh;
    std::vector<float> riverX;
    std::vector<uint8_t> base, reserved;    // reserved: 1 дорога, 2 здание, 3 огород
    std::vector<int16_t> fallow;
    std::vector<Site> sites;
    std::vector<Pub> pubs;

    // демография
    double young = 0, middle = 0, old = 0;
    double warDeaths = 0, warMobilized = 0;
    int lastGrowthYear = Y0, prevPop = 0;
    bool emitted[64] = {};
    double prevTech[TE_COUNT] = {};
    int restoredCount = 0, demolishedOld = 0;

    static int clampScenario(int s) { return s < 0 ? 0 : (s > 2 ? 2 : s); }
    static int idx(int x, int y) { return y * W + x; }
    static bool inside(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }

    // -----------------------------------------------------------------
    //  Генерация карты
    // -----------------------------------------------------------------
    void generate() {
        Noise ne(rng), nm(rng), nd(rng);
        elev.assign(W * H, 0); moist.assign(W * H, 0);
        base.assign(W * H, T_MEADOW); reserved.assign(W * H, 0);
        roadTh.assign(W * H, 9.f); fieldTh.assign(W * H, 9.f);
        fallow.assign(W * H, 0); shade.assign(W * H, 128);

        float ox = (float)rng.range(0, 40), oy = (float)rng.range(0, 40);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                elev[idx(x, y)] = ne.fbm((x + ox) / 24.f, (y + oy) / 24.f, 5);
                moist[idx(x, y)] = nm.fbm((x + oy) / 18.f + 7, (y + ox) / 18.f + 3, 4);
            }

        // Река: плавный меандр сверху вниз.
        riverX.assign(H, 0);
        double rx0 = W * rng.range(0.22, 0.30), a1 = rng.range(3, 6), f1 = rng.range(0.05, 0.08),
               p1 = rng.range(0, 6.28), a2 = rng.range(1, 2.5), f2 = rng.range(0.15, 0.22),
               p2 = rng.range(0, 6.28);
        for (int y = 0; y < H; ++y) riverX[y] = (float)(rx0 + a1 * std::sin(y * f1 + p1) + a2 * std::sin(y * f2 + p2));

        cy = (int)(H * rng.range(0.44, 0.56));
        cx = (int)(riverX[cy] + rng.range(17, 21));

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int i = idx(x, y);
                double dR = std::fabs(x + 0.5 - riverX[y]);
                double hw = 1.7 + 0.6 * std::sin(y * 0.13 + p2);
                elev[i] *= (float)(0.5 + 0.5 * smooth01(0, 16, dR));    // речная долина
                double dist = std::hypot(x - cx, (y - cy) * 1.15);
                if (dR < hw) { base[i] = T_WATER; continue; }
                if (dR < hw + 1.0 && nd.sample(x * 0.4f, y * 0.4f) > 0.45f) { base[i] = T_SAND; continue; }
                if (dR < 6 && moist[i] > 0.56f && elev[i] < 0.36f) { base[i] = T_MARSH; continue; }
                bool edge = x < 4 || x > W - 5 || y < 3 || y > H - 4;
                if (dist > 34 && (moist[i] > 0.56f || elev[i] > 0.64f || (edge && moist[i] > 0.46f))) { base[i] = T_FOREST; continue; }
                if (dist > 50 && moist[i] > 0.50f) { base[i] = T_FOREST; continue; }
                if (dist > 14 && dist < 60 && elev[i] < 0.66f && dR > 4) { base[i] = T_FIELD; continue; }
                base[i] = T_MEADOW;
            }

        // Рельефная отмывка (свет с северо-запада).
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int xa = std::max(0, x - 1), ya = std::max(0, y - 1), xb = std::min(W - 1, x + 1), yb = std::min(H - 1, y + 1);
                double g = (elev[idx(xa, ya)] - elev[idx(xb, yb)]) * 900.0 + (elev[idx(x, y)] - 0.45) * 60.0;
                shade[idx(x, y)] = (uint8_t)clampd(128 + g, 40, 230);
            }

        // Дороги.
        int bankE = (int)std::ceil(riverX[cy] + 3);
        addRoad(0, cy, W - 1, cy, 0.04f, true);               // главная улица (через мост)
        addRoad(cx, 0, cx, cy, 0.10f, true);                  // дорога в райцентр
        addRoad(cx, cy, cx, std::min(H - 4, cy + 27), 0.35f, false);
        int nX0 = std::max(cx - 13, (int)std::ceil(riverX[std::max(0, cy - 13)] + 4));
        int sX0 = std::max(cx - 11, (int)std::ceil(riverX[std::min(H - 1, cy + 13)] + 4));
        if (cy - 13 > 3) addRoad(nX0, cy - 13, cx + 30, cy - 13, 0.45f, false);
        if (cy + 13 < H - 4) addRoad(sX0, cy + 13, cx + 28, cy + 13, 0.5f, false);
        addRoad(cx + 24, std::max(3, cy - 13), cx + 24, std::min(H - 4, cy + 26), 0.55f, false);
        if (cy - 26 > 2) addRoad(cx - 6, cy - 26, cx + 16, cy - 26, 0.75f, false);
        if (cy + 26 < H - 3) addRoad(cx - 4, cy + 26, cx + 24, cy + 26, 0.8f, false);
        addRoad(cx + 44, cy, cx + 44, H - 1, 9.f, false);     // полевая дорога (грунт)
        (void)bankE;

        // Общественные здания — резервируем места заранее (одинаково для всех сценариев).
        reservePub(SL_CHURCH, 3, 3, cx + 2, cy - 5, 8, true);
        reservePub(SL_RADIO, 1, 1, cx - 2, cy + 2, 4, true);
        reservePub(SL_SHOP, 2, 2, cx - 5, cy - 4, 8, true);
        reservePub(SL_MEMORIAL, 3, 3, cx - 6, cy + 3, 8, true);
        reservePub(SL_SCHOOL, 4, 3, cx + 8, cy - 5, 10, true);
        reservePub(SL_CLUB, 4, 3, cx + 8, cy + 3, 10, true);
        reservePub(SL_FAP, 3, 2, cx + 15, cy + 3, 10, true);
        reservePub(SL_MILL, 2, 2, (int)(riverX[std::max(0, cy - 6)] + 3), cy - 6, 6, false);
        reservePub(SL_MTS, 5, 3, cx + 33, cy + 4, 10, true);
        reservePub(SL_FARM, 6, 3, cx + 38, cy + 15, 10, true);
        reservePub(SL_HUB, 4, 3, cx + 16, cy - 5, 10, true);
        reservePub(SL_NEWSCHOOL, 5, 3, cx + 16, cy + 16, 12, true);
        reservePub(SL_GREEN2, 5, 2, cx + 47, cy + 15, 8, false);
        reservePub(SL_GREEN3, 5, 2, cx + 47, cy + 19, 8, false);
        int sx = std::min(W - 16, cx + 50), sy = std::max(4, cy - 18);
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 4; ++i) reservePub(SL_SOLAR, 3, 2, sx + i * 4, sy + j * 3, 3, false, j * 4 + i);
        placeHighest(SL_TVTOWER, 10, 30, -1, -1);
        const Pub* tv = findPub(SL_TVTOWER);
        placeHighest(SL_CELLTOWER, 8, 30, tv ? tv->r.x : -1, tv ? tv->r.y : -1);
        for (int k = 0; k < 3; ++k) placeWind(k);

        // Участки под дома вдоль улиц.
        buildSites();

        // Пороги: асфальт — от центра к окраинам; поля — лоскутами.
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int i = idx(x, y);
                if (base[i] == T_FIELD) {
                    float plot = hash01((uint32_t)(x / 6 + 1), (uint32_t)(y / 4 + 1), (uint32_t)rng.s);
                    float d = (float)std::hypot(x - cx, y - cy) / 60.f;
                    fieldTh[i] = 0.62f * plot + 0.38f * d;
                }
            }
    }

    void addRoad(int x0, int y0, int x1, int y1, float th, bool primary) {
        int dx = (x1 > x0) - (x1 < x0), dy = (y1 > y0) - (y1 < y0);
        int x = x0, y = y0;
        while (true) {
            if (inside(x, y)) {
                int i = idx(x, y);
                bool water = base[i] == T_WATER || base[i] == T_SAND || base[i] == T_MARSH;
                if (water && !primary) break;   // второстепенные улицы не заходят в реку
                base[i] = (base[i] == T_WATER) ? T_BRIDGE : T_ROAD;
                reserved[i] = 1;
                float d = (float)std::hypot(x - cx, y - cy);
                float t = th >= 9.f ? 9.f : th + d / (primary ? 220.f : 120.f) + 0.08f * hash01(x, y, 7);
                roadTh[i] = std::min(roadTh[i], t);
            }
            if (x == x1 && y == y1) break;
            x += dx; y += dy;
        }
    }

    bool fits(int x, int y, int w, int h) const {
        if (x < 1 || y < 1 || x + w > W - 1 || y + h > H - 1) return false;
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx) {
                int i = idx(xx, yy);
                if (reserved[i]) return false;
                uint8_t b = base[i];
                if (b == T_WATER || b == T_SAND || b == T_MARSH || b == T_BRIDGE) return false;
            }
        for (int yy = y - 1; yy <= y + h; ++yy)
            for (int xx = x - 1; xx <= x + w; ++xx)
                if (inside(xx, yy) && reserved[idx(xx, yy)] == 2) return false;
        return true;
    }

    bool nearRoad(int x, int y, int w, int h, int d) const {
        for (int yy = y - d; yy < y + h + d; ++yy)
            for (int xx = x - d; xx < x + w + d; ++xx)
                if (inside(xx, yy) && reserved[idx(xx, yy)] == 1) return true;
        return false;
    }

    void mark(const Rect& r, uint8_t v) {
        for (int yy = r.y; yy < r.y + r.h; ++yy)
            for (int xx = r.x; xx < r.x + r.w; ++xx) reserved[idx(xx, yy)] = v;
    }

    void reservePub(int slot, int w, int h, int tx, int ty, int maxR, bool needRoad, int index = 0) {
        for (int r = 0; r <= maxR; ++r)
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = tx + dx, y = ty + dy;
                    if (!fits(x, y, w, h)) continue;
                    if (needRoad && !nearRoad(x, y, w, h, 2)) continue;
                    Pub p; p.r = {x, y, w, h}; p.slot = slot; p.index = index;
                    mark(p.r, 2);
                    pubs.push_back(p);
                    return;
                }
    }

    const Pub* findPub(int slot) const {
        for (const Pub& p : pubs) if (p.slot == slot) return &p;
        return nullptr;
    }

    void placeHighest(int slot, int minDist, int maxDist, int avoidX, int avoidY) {
        int bx = -1, by = -1; float best = -1;
        for (int y = 2; y < H - 2; ++y)
            for (int x = 2; x < W - 2; ++x) {
                double d = std::hypot(x - cx, y - cy);
                if (d < minDist || d > maxDist) continue;
                if (avoidX >= 0 && std::hypot(x - avoidX, y - avoidY) < 9) continue;
                if (!fits(x, y, 1, 1)) continue;
                if (elev[idx(x, y)] > best) { best = elev[idx(x, y)]; bx = x; by = y; }
            }
        if (bx < 0) return;
        Pub p; p.r = {bx, by, 1, 1}; p.slot = slot; mark(p.r, 2); pubs.push_back(p);
    }

    void placeWind(int k) {
        int bx = -1, by = -1; float best = -1;
        for (int y = 3; y < H - 3; ++y)
            for (int x = 3; x < W - 3; ++x) {
                if (std::hypot(x - cx, y - cy) < 30) continue;
                bool far = true;
                for (const Pub& p : pubs)
                    if (p.slot == SL_WIND && std::hypot(x - p.r.x, y - p.r.y) < 10) far = false;
                if (!far || !fits(x, y, 1, 1)) continue;
                if (base[idx(x, y)] == T_FOREST) continue;
                if (elev[idx(x, y)] > best) { best = elev[idx(x, y)]; bx = x; by = y; }
            }
        if (bx < 0) return;
        Pub p; p.r = {bx, by, 1, 1}; p.slot = SL_WIND; p.index = k; mark(p.r, 2); pubs.push_back(p);
    }

    void buildSites() {
        struct Seg { int x0, y0, x1, y1; };
        std::vector<Seg> segs;
        // Собираем отрезки улиц из размеченных дорог (только внутри села).
        auto scanH = [&](int y) {
            int x = 0;
            while (x < W) {
                while (x < W && !(reserved[idx(x, y)] == 1 && base[idx(x, y)] != T_BRIDGE)) ++x;
                int s = x;
                while (x < W && reserved[idx(x, y)] == 1 && base[idx(x, y)] != T_BRIDGE) ++x;
                if (x - s >= 6) segs.push_back({s, y, x - 1, y});
            }
        };
        auto scanV = [&](int x) {
            int y = 0;
            while (y < H) {
                while (y < H && reserved[idx(x, y)] != 1) ++y;
                int s = y;
                while (y < H && reserved[idx(x, y)] == 1) ++y;
                if (y - s >= 6) segs.push_back({x, s, x, y - 1});
            }
        };
        scanH(cy);
        if (cy - 13 > 3) scanH(cy - 13);
        if (cy + 13 < H - 4) scanH(cy + 13);
        if (cy - 26 > 2) scanH(cy - 26);
        if (cy + 26 < H - 3) scanH(cy + 26);
        scanV(cx); scanV(cx + 24);

        float rx = riverX[cy];
        for (const Seg& s : segs) {
            bool horiz = s.y0 == s.y1;
            int len = horiz ? s.x1 - s.x0 : s.y1 - s.y0;
            for (int k = 1; k + 2 <= len; k += 3)
                for (int side = 0; side < 2; ++side) {
                    Site st;
                    if (horiz) {
                        int x = s.x0 + k, ry = s.y0;
                        if (side == 0) { st.house = {x, ry - 3, 2, 2}; st.garden = {x, ry - 6, 2, 3}; }
                        else           { st.house = {x, ry + 2, 2, 2}; st.garden = {x, ry + 4, 2, 3}; }
                    } else {
                        int y = s.y0 + k, rxx = s.x0;
                        if (side == 0) { st.house = {rxx - 3, y, 2, 2}; st.garden = {rxx - 6, y, 3, 2}; }
                        else           { st.house = {rxx + 2, y, 2, 2}; st.garden = {rxx + 4, y, 3, 2}; }
                    }
                    int hx = st.house.x, hy = st.house.y;
                    bool zarechye = hx < rx - 3;
                    if (!zarechye && (std::abs(hx - cx) > 40 || std::abs(hy - cy) > 31)) continue;
                    if (zarechye && hx < 3) continue;
                    if (!fits(st.house.x, st.house.y, 2, 2)) continue;
                    if (!fits(st.garden.x, st.garden.y, st.garden.w, st.garden.h)) continue;
                    // огород не должен лезть на дорогу
                    mark(st.house, 2); mark(st.garden, 3);
                    uint32_t n = (uint32_t)sites.size() + 1;
                    st.h1 = hash01(n, 11); st.h2 = hash01(n, 23); st.h3 = hash01(n, 37); st.h4 = hash01(n, 51);
                    st.order = std::hypot(hx - cx, (hy - cy) * 1.1) + rng.uni() * 8 + (zarechye ? 10 : 0);
                    sites.push_back(st);
                }
        }
        std::sort(sites.begin(), sites.end(), [](const Site& a, const Site& b) { return a.order < b.order; });
    }

    // -----------------------------------------------------------------
    //  Моделирование
    // -----------------------------------------------------------------
    double tech[TE_COUNT] = {};

    void computeTech(int y) {
        for (int i = 0; i < TE_COUNT; ++i) tech[i] = techLevel(i, y, sc);
        // Технологии вытесняют друг друга: мобильная связь — стационарный телефон,
        // интернет-видео — эфирное телевидение.
        tech[TE_PHONE] *= 1 - 0.6 * tech[TE_MOBILE];
        tech[TE_TV] *= 1 - 0.3 * tech[TE_INTERNET] * tech[TE_DIGITAL];
    }

    double attract() const {
        return clampd(0.40 * tech[TE_INTERNET] * tech[TE_DIGITAL] + 0.22 * tech[TE_SMARTAGRO] +
                      0.10 * tech[TE_GREEN] + 0.12 * tech[TE_AI] + 0.10 * tech[TE_MEMORY], 0, 0.92);
    }

    int pop() const { return (int)std::lround(young + middle + old); }

    void demography(int y) {
        bool future = y > TODAY;
        double fy = interp(FERT, y), dy = interp(DY, y), dm = interp(DM, y), dO = interp(DO, y);
        double A = attract();
        if (future) {
            fy = 0.029 + 0.016 * A;
            dm *= 1 - 0.25 * tech[TE_DIGITAL];
            dO *= 1 - 0.18 * tech[TE_DIGITAL] - 0.07 * tech[TE_AI];
        }
        double warM = 0, warY = 0;
        if (y >= 1941 && y <= 1945) { warM = 0.06; warY = 0.012; }
        if (y >= 1914 && y <= 1921) { warM = 0.014; }
        if (y == 1941) warMobilized = 0.42 * middle + 0.10 * young;

        double births = fy * young;
        double dY = (dy + warY) * young, dM = (dm + warM) * middle, dOl = dO * old;
        if (y >= 1941 && y <= 1945) warDeaths += warM * middle + warY * young;
        double ym = young / 30.0, mo = middle / 30.0;
        double U = interp(PULL, std::min(y, TODAY));
        if (future) U = 0.030;
        double outY = U * (1 - A) * young, outM = 0.3 * U * (1 - A) * middle;

        double in = 0;
        if (future && sc != SC_INERTIA) {
            double k = sc == SC_LEAP ? 1.0 : 0.9;
            in = k * std::max(0.0, A - 0.22) * 34.0;
            if (sc == SC_HARMONY) in += 6.0 * tech[TE_MEMORY];
            double cap = sites.size() * interp(HH, y) * 1.02;
            in *= clamp01((cap - pop()) / 40.0);
        }
        young += births - dY - ym - outY + 0.55 * in;
        middle += ym - dM - mo - outM + 0.38 * in;
        old += mo - dOl + 0.07 * in;
        young = std::max(0.0, young); middle = std::max(0.0, middle); old = std::max(0.0, old);
    }

    int countState(int s) const {
        int n = 0;
        for (const Site& st : sites) n += st.state == s;
        return n;
    }

    void housing(int y) {
        bool future = y > TODAY;
        double hh = interp(HH, y);
        int target = std::min((int)sites.size(), (int)std::lround(pop() / hh));
        int lived = countState(S_LIVED);

        if (y == Y0) {
            for (int k = 0; k < target; ++k) { sites[k].state = S_LIVED; sites[k].since = 1880; sites[k].everLived = true; }
            lived = target;
        } else if (target > lived) {
            int add = std::min(target - lived, future ? 6 : 4);
            for (int k = 0; k < add; ++k) occupyOne(y);
        } else if (lived > target + 1) {
            int rem = std::min(lived - target, 5);
            for (int k = 0; k < rem; ++k) abandonOne(y);
        }

        // Старение брошенных домов.
        for (Site& st : sites) {
            if (st.state == S_ABANDONED && y - st.since > ((future && sc == SC_HARMONY) ? 24 : 14)) { st.state = S_RUIN; st.since = y; }
            else if (st.state == S_RUIN && y - st.since > 22) { st.state = S_EMPTY; st.since = y; }
        }
        // В сценарии «Рывок» руины расчищают под новую застройку.
        if (future && sc == SC_LEAP && y >= 2028) {
            int cleared = 0;
            for (Site& st : sites)
                if (cleared < 3 && st.state == S_RUIN) { st.state = S_EMPTY; st.since = y; ++cleared; ++demolishedOld; }
        }

        for (Site& st : sites) {
            if (st.state != S_LIVED) continue;
            int f = 0;
            if (st.h1 < tech[TE_ELECTRIC]) f |= F_ELECTRIC;
            if (st.h2 < tech[TE_TV] + (st.keepFlags & F_TV ? 0.2 : 0)) f |= F_TV;
            if (st.h3 < tech[TE_INTERNET]) f |= F_INTERNET;
            if (future && st.h4 < 0.75 * tech[TE_GREEN]) f |= F_SOLAR_ROOF;
            if (st.modern) f |= F_MODERN;
            if (st.restored) f |= F_RESTORED;
            st.keepFlags = f;
        }
    }

    void occupyOne(int y) {
        bool future = y > TODAY;
        Site* pick = nullptr;
        auto first = [&](int state) -> Site* {
            for (Site& st : sites) if (st.state == state) return &st;
            return nullptr;
        };
        if (future && sc == SC_HARMONY) {
            pick = first(S_ABANDONED);
            if (!pick) pick = first(S_RUIN);
            if (pick) { pick->restored = true; ++restoredCount; }
            else if ((pick = first(S_EMPTY))) pick->modern = pick->everLived ? false : true;
        } else if (future && sc == SC_LEAP) {
            pick = first(S_EMPTY);
            if (!pick) { pick = first(S_RUIN); if (pick) ++demolishedOld; }
            if (!pick) { pick = first(S_ABANDONED); if (pick) ++demolishedOld; }
            if (pick) { pick->modern = true; pick->restored = false; }
        } else {
            pick = first(S_EMPTY);
            if (!pick) pick = first(S_ABANDONED);
        }
        if (!pick) return;
        if (pick->modern && future && !emitted[EV_FIRST_MODERN]) emit(y, EV_FIRST_MODERN, 0);
        pick->state = S_LIVED; pick->since = y; pick->everLived = true;
    }

    void abandonOne(int y) {
        Site* pick = nullptr; double best = -1;
        for (Site& st : sites)
            if (st.state == S_LIVED) {
                double s = st.order + st.h1 * 14;
                if (s > best) { best = s; pick = &st; }
            }
        if (!pick) return;
        pick->state = S_ABANDONED; pick->since = y;
        pick->keepFlags &= ~(F_ELECTRIC | F_INTERNET | F_SOLAR_ROOF);
    }

    void setSlot(int slot, int type, int flags = 0, int variant = 0) {
        for (Pub& p : pubs) if (p.slot == slot) { p.type = type; p.flags = flags; p.variant = variant; }
    }

    void publics(int y) {
        bool future = y > TODAY;
        bool leap = future && sc == SC_LEAP, harm = future && sc == SC_HARMONY, iner = future && sc == SC_INERTIA;

        setSlot(SL_CHURCH, B_CHURCH, (y >= 1932 && y < 2001) ? F_CLOSED : F_LIT);
        if (y < 1934) setSlot(SL_MILL, B_MILL);
        else if (y < 1962) setSlot(SL_MILL, B_MILL, F_CLOSED);
        else setSlot(SL_MILL, B_NONE);
        setSlot(SL_RADIO, (y >= 1936 && y < 2008) ? B_TOWER : B_NONE, 0, 0);

        // Школа
        if (y < 1924) setSlot(SL_SCHOOL, B_NONE);
        else if (y < 2009) setSlot(SL_SCHOOL, B_SCHOOL, F_ELECTRIC * (tech[TE_ELECTRIC] > 0.3));
        else if (iner) setSlot(SL_SCHOOL, y >= 2038 ? B_RUIN : B_SCHOOL, F_CLOSED);
        else if (leap && y >= 2028) setSlot(SL_SCHOOL, B_HUB, F_MODERN | F_LIT);
        else if (harm && y >= 2028) setSlot(SL_SCHOOL, B_MUSEUM, F_RESTORED | F_LIT);
        else setSlot(SL_SCHOOL, B_SCHOOL, F_CLOSED);

        // Дом культуры
        if (y < 1955) setSlot(SL_CLUB, B_NONE);
        else if (y < 1998) setSlot(SL_CLUB, B_CLUB, F_ELECTRIC);
        else if (leap && y >= 2033) setSlot(SL_CLUB, B_CLUB, F_MODERN | F_LIT);
        else if (harm && y >= 2029) setSlot(SL_CLUB, B_CLUB, F_RESTORED | F_LIT);
        else if (iner && y >= 2046) setSlot(SL_CLUB, B_RUIN);
        else setSlot(SL_CLUB, B_CLUB, F_CLOSED);

        setSlot(SL_SHOP, y < 1958 ? B_NONE : B_SHOP, (iner && y >= 2041) ? F_CLOSED : F_ELECTRIC);
        setSlot(SL_FAP, y < 1962 ? B_NONE : B_FAP, (iner && y >= 2034) ? F_CLOSED : F_ELECTRIC);
        setSlot(SL_MEMORIAL, y < 1967 ? B_NONE : B_MEMORIAL, ((harm && y >= 2030) || (leap && y >= 2035)) ? F_LIT : 0);

        if (y < 1933) setSlot(SL_MTS, B_NONE);
        else if (y < 1996) setSlot(SL_MTS, B_MTS);
        else if (leap && y >= 2033) setSlot(SL_MTS, B_DRONEPORT, F_MODERN | F_LIT);
        else if (harm && y >= 2034) setSlot(SL_MTS, B_DRONEPORT, F_MODERN | F_LIT);
        else if (iner && y >= 2040) setSlot(SL_MTS, B_RUIN);
        else setSlot(SL_MTS, B_MTS, F_CLOSED);

        if (y < 1930) setSlot(SL_FARM, B_NONE);
        else if (y < 1996) setSlot(SL_FARM, B_FARM);
        else if (leap && y >= 2031) setSlot(SL_FARM, B_GREENHOUSE, F_MODERN | F_LIT);
        else if (harm && y >= 2032) setSlot(SL_FARM, B_GREENHOUSE, F_MODERN | F_LIT);
        else if (iner && y >= 2030) setSlot(SL_FARM, B_RUIN);
        else setSlot(SL_FARM, B_FARM, F_CLOSED);

        setSlot(SL_GREEN2, ((leap && y >= 2033) || (harm && y >= 2034)) ? B_GREENHOUSE : B_NONE, F_MODERN | F_LIT);
        setSlot(SL_GREEN3, ((leap && y >= 2035) || (harm && y >= 2037)) ? B_GREENHOUSE : B_NONE, F_MODERN | F_LIT);
        setSlot(SL_HUB, (harm && y >= 2030) ? B_HUB : B_NONE, F_MODERN | F_LIT);
        setSlot(SL_NEWSCHOOL, ((leap && y >= 2034) || (harm && y >= 2035)) ? B_SCHOOL : B_NONE, F_MODERN | F_LIT);
        setSlot(SL_TVTOWER, y >= 1972 ? B_TOWER : B_NONE, 0, 1);
        setSlot(SL_CELLTOWER, y >= 2007 ? B_TOWER : B_NONE, 0, ((leap || harm) && y >= 2029) ? 3 : 2);

        for (Pub& p : pubs) {
            if (p.slot == SL_SOLAR) {
                bool on = false;
                if (leap) on = y >= 2031 + p.index / 2;
                else if (harm) on = y >= 2032 + p.index / 2 && p.index < 6;
                else if (iner) on = y >= 2046 && p.index < 2;
                p.type = on ? B_SOLAR : B_NONE; p.flags = F_MODERN;
            } else if (p.slot == SL_WIND) {
                bool on = false;
                if (leap) on = y >= 2036 + p.index * 2;
                else if (harm) on = y >= 2038 + p.index * 2 && p.index < 2;
                p.type = on ? B_WIND : B_NONE; p.flags = F_MODERN | F_LIT;
            }
        }

        // События с фиксированной датой.
        switch (y) {
            case 1924: emit(y, EV_SCHOOL_OPEN, 0); break;
            case 1930: emit(y, EV_KOLKHOZ, 0); break;
            case 1932: emit(y, EV_CHURCH_CLOSED, 0); break;
            case 1933: emit(y, EV_MTS, 0); break;
            case 1934: emit(y, EV_MILL_STOP, 0); break;
            case 1936: emit(y, EV_RADIO, 0); break;
            case 1955: emit(y, EV_CLUB, 0); break;
            case 1958: emit(y, EV_SHOP, 0); break;
            case 1962: emit(y, EV_FAP, 0); break;
            case 1967: emit(y, EV_MEMORIAL, (int)std::lround(warDeaths)); break;
            case 1992: emit(y, EV_KOLKHOZ_END, 0); break;
            case 1996: emit(y, EV_FARM_CLOSED, 0); break;
            case 1998: emit(y, EV_CLUB_CLOSED, 0); break;
            case 2001: emit(y, EV_CHURCH_RESTORED, 0); break;
            case 2009: emit(y, EV_SCHOOL_CLOSED, 0); break;
            default: break;
        }
        if (iner) {
            if (y == 2034) emit(y, EV_FAP_CLOSED, 0);
            if (y == 2038) emit(y, EV_SCHOOL_RUIN, 0);
            if (y == 2041) emit(y, EV_SHOP_CLOSED, 0);
            if (y == 2044) emit(y, EV_ROADS_DECAY, 0);
        }
        if (leap) {
            if (y == 2028) emit(y, EV_HUB, 1);
            if (y == 2029) emit(y, EV_5G, 0);
            if (y == 2031) emit(y, EV_SOLAR, 0);
            if (y == 2031) emit(y, EV_GREENHOUSE, 0);
            if (y == 2033) emit(y, EV_DRONES, 0);
            if (y == 2033) emit(y, EV_CLUB_REOPEN, 1);
            if (y == 2034) emit(y, EV_NEW_SCHOOL, 0);
            if (y == 2036) emit(y, EV_WIND, 0);
        }
        if (harm) {
            if (y == 2028) emit(y, EV_MUSEUM, 0);
            if (y == 2029) emit(y, EV_5G, 0);
            if (y == 2029) emit(y, EV_CLUB_REOPEN, 2);
            if (y == 2030) emit(y, EV_HUB, 2);
            if (y == 2032) emit(y, EV_SOLAR, 0);
            if (y == 2032) emit(y, EV_GREENHOUSE, 0);
            if (y == 2034) emit(y, EV_DRONES, 0);
            if (y == 2035) emit(y, EV_NEW_SCHOOL, 0);
            if (y == 2038) emit(y, EV_WIND, 0);
            if (y == 2042) emit(y, EV_RESTORED, restoredCount);
        }
    }

    void terrain(int y, std::vector<uint8_t>& t) {
        t = base;
        double agro = interp(AGRO, std::min(y, TODAY));
        if (y > TODAY) {
            if (sc == SC_INERTIA) agro = 0.28 - 0.18 * smooth01(TODAY, 2048, y);
            else agro = 0.28 + (sc == SC_LEAP ? 0.66 : 0.52) * tech[TE_SMARTAGRO];
        }
        int forestNew = 0, fieldsTotal = 0;
        for (int i = 0; i < W * H; ++i) {
            if (base[i] == T_FIELD) {
                ++fieldsTotal;
                bool active = fieldTh[i] < agro && fallow[i] < 26;
                if (active) {
                    fallow[i] = 0;
                    t[i] = (tech[TE_SMARTAGRO] > 0.45 && fieldTh[i] < agro * 0.9) ? T_SMART_FIELD : T_FIELD;
                } else if (y < 1990) {
                    t[i] = T_MEADOW;             // сенокосы и пастбища
                } else {
                    if (fallow[i] < 1000) ++fallow[i];
                    t[i] = fallow[i] > 17 ? T_YOUNG_FOREST : T_FALLOW;
                    if (fallow[i] > 17) ++forestNew;
                }
            } else if (base[i] == T_ROAD) {
                t[i] = (y >= 1958 && roadTh[i] < tech[TE_ROADS]) ? T_ASPHALT : T_ROAD;
            }
        }
        if (!emitted[EV_FIELDS_FOREST] && fieldsTotal > 0 && forestNew > fieldsTotal * 0.25) emit(y, EV_FIELDS_FOREST, 0);

        for (const Site& st : sites) {
            uint8_t g = T_MEADOW;
            if (st.state == S_LIVED) g = T_GARDEN;
            else if (st.state == S_ABANDONED) g = (y - st.since < 3) ? T_GARDEN : T_FALLOW;
            else if (st.state == S_RUIN) g = T_FALLOW;
            else if (st.everLived) g = (y - st.since > 12) ? T_YOUNG_FOREST : T_FALLOW;
            const Rect& r = st.garden;
            for (int yy = r.y; yy < r.y + r.h; ++yy)
                for (int xx = r.x; xx < r.x + r.w; ++xx) t[idx(xx, yy)] = g;
            if (st.state == S_EMPTY && st.everLived) {
                const Rect& hr = st.house;
                for (int yy = hr.y; yy < hr.y + hr.h; ++yy)
                    for (int xx = hr.x; xx < hr.x + hr.w; ++xx) t[idx(xx, yy)] = g;
            }
        }
        for (const Pub& p : pubs)
            if (p.slot == SL_MEMORIAL && p.type != B_NONE)
                for (int yy = p.r.y; yy < p.r.y + p.r.h; ++yy)
                    for (int xx = p.r.x; xx < p.r.x + p.r.w; ++xx) t[idx(xx, yy)] = T_PLAZA;
    }

    void emit(int y, int code, int value) {
        if (code > 0 && code < 64) emitted[code] = true;
        HgEvent e{y, code, value, y > TODAY ? sc : -1};
        events.push_back(e);
    }

    void techEvents(int y) {
        auto cross = [&](int t, double th, int code) {
            if (!emitted[code] && prevTech[t] < th && tech[t] >= th) emit(y, code, 0);
        };
        cross(TE_ELECTRIC, 0.05, EV_ELECTRIC_FIRST);
        cross(TE_ELECTRIC, 0.95, EV_ELECTRIC_ALL);
        cross(TE_TV, 0.05, EV_TV);
        cross(TE_ROADS, 0.30, EV_ASPHALT);
        cross(TE_PHONE, 0.10, EV_PHONE);
        cross(TE_MOBILE, 0.05, EV_MOBILE);
        cross(TE_INTERNET, 0.10, EV_INTERNET);
        cross(TE_DIGITAL, 0.30, EV_DIGITAL);
        cross(TE_MEMORY, 0.50, EV_DIGITAL_ARCHIVE);
        cross(TE_AI, 0.50, EV_TWIN);
        for (int i = 0; i < TE_COUNT; ++i) prevTech[i] = tech[i];
    }

    void stats(int y, HgStats& s) {
        s.year = y;
        s.population = pop();
        s.young = (int)std::lround(young); s.middle = (int)std::lround(middle); s.old = (int)std::lround(old);
        s.houses = countState(S_LIVED);
        s.abandoned = countState(S_ABANDONED) + countState(S_RUIN);
        s.sites = (int)sites.size();
        for (int i = 0; i < TE_COUNT; ++i) s.tech[i] = tech[i];

        auto pubOn = [&](int slot) {
            for (const Pub& p : pubs)
                if (p.slot == slot) return p.type != B_NONE && p.type != B_RUIN && !(p.flags & F_CLOSED);
            return false;
        };
        bool school = pubOn(SL_SCHOOL) && y < 2009;
        bool newSchool = pubOn(SL_NEWSCHOOL);
        double services = (school || newSchool ? 24 : 0) + (pubOn(SL_FAP) ? 22 : 0) + (pubOn(SL_SHOP) ? 14 : 0) +
                          (pubOn(SL_CLUB) ? 12 : 0) + 10 * tech[TE_ROADS] + 30 * tech[TE_DIGITAL] * tech[TE_INTERNET];
        s.services = clampd(services, 0, 100);

        s.connectivity = 100 * clamp01(0.08 * tech[TE_RADIO] + 0.14 * tech[TE_PHONE] / 0.8 + 0.12 * tech[TE_TV] +
                                       0.28 * tech[TE_MOBILE] + 0.38 * tech[TE_INTERNET]);

        double econ = interp(ECON, y);
        if (y > TODAY) econ = interp(ECON, y) + 48 * tech[TE_SMARTAGRO] + 14 * tech[TE_DIGITAL] * tech[TE_INTERNET] +
                              8 * tech[TE_GREEN] + (sc == SC_HARMONY ? 6 * tech[TE_MEMORY] : 0) - 8;
        s.economy = clampd(econ, 0, 100);

        double agro = interp(AGRO, std::min(y, TODAY));
        int lostOld = s.abandoned + demolishedOld;
        int ever = 0;
        for (const Site& st : sites) ever += st.everLived;
        double lost = ever > 0 ? (double)lostOld / ever : 0;
        double mem = 40 + (pubOn(SL_MEMORIAL) || (y >= 1967) ? 12 : 0) + (pubOn(SL_CHURCH) ? 6 : 0) +
                     (pubOn(SL_CLUB) ? 5 : 0) + (school ? 5 : 0) + 24 * tech[TE_MEMORY] - 34 * lost -
                     8 * (y >= 1941 && y <= 1945);
        for (const Pub& p : pubs) if (p.type == B_MUSEUM) mem += 9;
        s.memory = clampd(mem, 0, 100);

        double eco = 88 - 18 * tech[TE_MECH] * agro - 6 * tech[TE_ROADS] + 10 * tech[TE_GREEN] + 6 * tech[TE_SMARTAGRO];
        if (y > TODAY && sc == SC_INERTIA) eco += 6 * smooth01(TODAY, 2050, y);
        s.ecology = clampd(eco, 0, 100);

        s.quality = clampd(0.18 * tech[TE_ELECTRIC] * 100 + 0.24 * s.services + 0.18 * s.economy +
                           0.16 * s.connectivity + 0.10 * s.ecology + 0.14 * tech[TE_ROADS] * 100, 0, 100);
    }

    void simulate() {
        snaps.resize(NY);
        double P0 = 310 + rng.uni() * 100;
        young = 0.50 * P0; middle = 0.34 * P0; old = 0.16 * P0;
        int peakPop = 0, peakYear = 0;
        for (int y = Y0; y <= Y1; ++y) {
            computeTech(y);
            if (y > Y0) demography(y);
            housing(y);
            publics(y);
            Snap& s = snaps[y - Y0];
            terrain(y, s.terr);
            stats(y, s.st);
            techEvents(y);

            int p = s.st.population;
            if (y == Y0) emit(y, EV_START, p);
            if (y == 1941) emit(y, EV_WAR_START, (int)std::lround(warMobilized));
            if (y == 1945) emit(y, EV_WAR_END, (int)std::lround(warDeaths));
            if (y == TODAY) emit(y, EV_TODAY, p);
            if (y == Y1) emit(y, EV_FINAL, p);
            if (y >= 1947 && y <= 1990 && p > peakPop) { peakPop = p; peakYear = y; }
            if (y > Y0 && y <= TODAY && p > prevPop) lastGrowthYear = y;
            if (y > TODAY + 1 && p > prevPop + 1 && !emitted[EV_POP_GROWTH]) emit(y, EV_POP_GROWTH, y - lastGrowthYear);
            if (p < 100 && !emitted[EV_POP_BELOW_100]) emit(y, EV_POP_BELOW_100, p);
            prevPop = p;

            s.b.clear();
            int id = 0;
            for (const Site& st : sites) {
                ++id;
                if (st.state == S_EMPTY) continue;
                int type = st.state == S_LIVED ? B_HOUSE : (st.state == S_ABANDONED ? B_HOUSE_ABANDONED : B_RUIN);
                int fl = st.keepFlags;
                if (st.state != S_LIVED) fl &= ~(F_ELECTRIC | F_INTERNET);
                if (st.modern) fl |= F_MODERN;
                if (st.restored) fl |= F_RESTORED;
                s.b.push_back({st.house.x, st.house.y, st.house.w, st.house.h, type, fl, id, (int)(st.h4 * 4)});
            }
            for (const Pub& pb : pubs) {
                ++id;
                if (pb.type == B_NONE) continue;
                s.b.push_back({pb.r.x, pb.r.y, pb.r.w, pb.r.h, pb.type, pb.flags, id, pb.variant});
            }
        }
        if (peakYear) emit(peakYear, EV_POP_PEAK, peakPop);
        std::stable_sort(events.begin(), events.end(), [](const HgEvent& a, const HgEvent& b) { return a.year < b.year; });
    }
};

// ---------------------------------------------------------------------
//  «Объектив эпохи»: цветокоррекция и плёночные эффекты
// ---------------------------------------------------------------------
enum LensKey { LK_YEAR, LK_SEPIA, LK_BW, LK_FADE, LK_WARM, LK_SAT, LK_CONTRAST, LK_GRAIN,
               LK_VIGNETTE, LK_SCRATCH, LK_CHROMA, LK_SCAN, LK_TEAL, LK_FLICKER, LK_N };

static const float LENS[][LK_N] = {
    // год   сепия ч/б  выцв. тепл. насыщ контр. зерно винь. царап хрома скан  бирюза мерцание
    {1900, 1.00, 0.00, 0.00, 0.00, 0.00, 1.16, 0.130, 0.85, 1.00, 0.0, 0.00, 0.00, 0.060},
    {1936, 0.85, 0.15, 0.00, 0.00, 0.00, 1.12, 0.110, 0.75, 0.80, 0.0, 0.00, 0.00, 0.050},
    {1950, 0.15, 0.85, 0.00, 0.00, 0.00, 1.10, 0.090, 0.60, 0.45, 0.0, 0.00, 0.00, 0.030},
    {1962, 0.00, 0.20, 0.75, 0.60, 0.65, 1.00, 0.060, 0.45, 0.15, 0.0, 0.00, 0.00, 0.010},
    {1982, 0.00, 0.00, 0.60, 0.55, 0.80, 1.00, 0.045, 0.38, 0.05, 0.0, 0.00, 0.00, 0.000},
    {1993, 0.00, 0.00, 0.30, 0.35, 0.90, 1.02, 0.035, 0.30, 0.00, 1.6, 0.10, 0.00, 0.000},
    {2006, 0.00, 0.00, 0.05, 0.10, 0.98, 1.03, 0.020, 0.20, 0.00, 0.5, 0.00, 0.00, 0.000},
    {2018, 0.00, 0.00, 0.00, 0.00, 1.06, 1.05, 0.010, 0.16, 0.00, 0.0, 0.00, 0.00, 0.000},
    {2027, 0.00, 0.00, 0.00, 0.00, 1.10, 1.05, 0.006, 0.18, 0.00, 0.0, 0.00, 0.15, 0.000},
    {2036, 0.00, 0.00, 0.00, 0.00, 1.12, 1.06, 0.006, 0.26, 0.00, 0.3, 0.05, 0.55, 0.000},
    {2060, 0.00, 0.00, 0.00, 0.00, 1.15, 1.08, 0.006, 0.28, 0.00, 0.4, 0.06, 0.65, 0.000},
};

static void lensParams(float year, float out[LK_N]) {
    const int n = (int)(sizeof(LENS) / sizeof(LENS[0]));
    if (year <= LENS[0][0]) { std::memcpy(out, LENS[0], sizeof(float) * LK_N); return; }
    for (int i = 1; i < n; ++i)
        if (year <= LENS[i][0]) {
            float k = (year - LENS[i - 1][0]) / (LENS[i][0] - LENS[i - 1][0]);
            for (int j = 0; j < LK_N; ++j) out[j] = LENS[i - 1][j] + (LENS[i][j] - LENS[i - 1][j]) * k;
            return;
        }
    std::memcpy(out, LENS[n - 1], sizeof(float) * LK_N);
}

static inline uint32_t ihash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

static inline float smooth01f(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return t * t * (3 - 2 * t);
}

static inline uint8_t to8(float v) {
    v *= 255.f;
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f));
}

struct LensJob {
    uint8_t* px; int w, h, stride;
    const float* light;    // накопленный свет, w*h*3
    float P[LK_N];
    float night, lightGain, flicker;
    uint32_t frame;
};

static void lensRows(const LensJob& J, int y0, int y1) {
    const float* P = J.P;
    const float sepia = P[LK_SEPIA], bw = P[LK_BW], fade = P[LK_FADE], warm = P[LK_WARM];
    const float sat = P[LK_SAT], contrast = P[LK_CONTRAST], grain = P[LK_GRAIN], vig = P[LK_VIGNETTE];
    const float scan = P[LK_SCAN], teal = P[LK_TEAL];
    const int shift = (int)std::lround(P[LK_CHROMA] * J.w / 900.0f);
    const float nr = 1 - 0.86f * J.night, ng = 1 - 0.80f * J.night, nb = 1 - 0.60f * J.night;
    const float cxf = J.w * 0.5f, cyf = J.h * 0.5f;
    std::vector<uint8_t> row((size_t)J.w * 4);

    for (int y = y0; y < y1; ++y) {
        uint8_t* line = J.px + (size_t)y * J.stride;
        std::memcpy(row.data(), line, (size_t)J.w * 4);
        const float* L = J.light + (size_t)y * J.w * 3;
        float dyv = (y - cyf) / cyf;
        float rowScan = (scan > 0 && (y & 1)) ? 1 - scan : 1.f;
        for (int x = 0; x < J.w; ++x) {
            int xr = std::min(J.w - 1, std::max(0, x - shift)), xb = std::min(J.w - 1, std::max(0, x + shift));
            float b = row[x * 4 + 0] / 255.f, g = row[x * 4 + 1] / 255.f, r = row[x * 4 + 2] / 255.f;
            if (shift) { r = row[xr * 4 + 2] / 255.f; b = row[xb * 4 + 0] / 255.f; }

            // Ночь + свет окон и фонарей.
            r = r * nr + L[x * 3 + 0] * J.lightGain;
            g = g * ng + L[x * 3 + 1] * J.lightGain;
            b = b * nb + L[x * 3 + 2] * J.lightGain;

            float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            if (sat != 1.f && sepia < 0.999f) { r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat; }
            if (bw > 0) { r += (lum - r) * bw; g += (lum - g) * bw; b += (lum - b) * bw; }
            if (sepia > 0) {
                float sr = lum * 1.07f + 0.05f, sg = lum * 0.87f + 0.025f, sb = lum * 0.62f + 0.01f;
                r += (sr - r) * sepia; g += (sg - g) * sepia; b += (sb - b) * sepia;
            }
            if (fade > 0) {
                r = 0.07f * fade + r * (1 - 0.20f * fade);
                g = 0.06f * fade + g * (1 - 0.20f * fade);
                b = 0.05f * fade + b * (1 - 0.24f * fade);
            }
            if (warm > 0) { r *= 1 + 0.09f * warm; g *= 1 + 0.02f * warm; b *= 1 - 0.10f * warm; }
            if (teal > 0) {
                float sh = 1 - std::min(1.f, lum * 1.4f);
                r += teal * (0.05f * lum - 0.05f * sh);
                g += teal * (0.025f * sh);
                b += teal * (0.10f * sh - 0.035f * lum);
            }
            r = (r - 0.5f) * contrast + 0.5f; g = (g - 0.5f) * contrast + 0.5f; b = (b - 0.5f) * contrast + 0.5f;

            float dxv = (x - cxf) / cxf;
            float d = (dxv * dxv + dyv * dyv) * 0.5f;
            float v = 1 - vig * smooth01f(0.18f, 1.05f, d);
            float n = grain > 0 ? ((ihash((uint32_t)(x * 73856093u) ^ (uint32_t)(y * 19349663u) ^ (J.frame * 83492791u)) & 1023) / 1023.f - 0.5f) * 2 * grain : 0;
            float m = v * J.flicker * rowScan;
            r = r * m + n; g = g * m + n; b = b * m + n;

            line[x * 4 + 2] = to8(r); line[x * 4 + 1] = to8(g); line[x * 4 + 0] = to8(b);
        }
    }
}

// Царапины и пылинки старой плёнки.
static void scratches(uint8_t* px, int w, int h, int stride, float amount, uint32_t frame) {
    if (amount <= 0.01f) return;
    uint32_t slow = frame / 3;
    int lines = 1 + (int)(ihash(slow * 31u) % 3);
    for (int k = 0; k < lines; ++k) {
        uint32_t hs = ihash(slow * 977u + k * 131u);
        if ((hs & 255) > 255 * amount) continue;
        int x = (int)(hs % (uint32_t)w);
        bool bright = (hs >> 9) & 1;
        float a = 0.25f + 0.25f * ((hs >> 12) & 255) / 255.f;
        int y0 = (int)(((hs >> 3) % 100) / 100.f * h * 0.3f);
        for (int y = y0; y < h; ++y) {
            uint8_t* p = px + (size_t)y * stride + x * 4;
            for (int c = 0; c < 3; ++c) p[c] = (uint8_t)(bright ? p[c] + (255 - p[c]) * a : p[c] * (1 - a));
        }
    }
    int specks = (int)(amount * 26);
    for (int k = 0; k < specks; ++k) {
        uint32_t hs = ihash(frame * 7919u + k * 104729u);
        int x = (int)(hs % (uint32_t)w), y = (int)((hs >> 11) % (uint32_t)h);
        int r = 1 + (int)((hs >> 5) % 3);
        bool bright = (hs >> 21) & 1;
        for (int yy = y - r; yy <= y + r; ++yy)
            for (int xx = x - r; xx <= x + r; ++xx) {
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                if ((xx - x) * (xx - x) + (yy - y) * (yy - y) > r * r) continue;
                uint8_t* p = px + (size_t)yy * stride + xx * 4;
                for (int c = 0; c < 3; ++c) p[c] = bright ? (uint8_t)(p[c] + (255 - p[c]) * 0.6f) : (uint8_t)(p[c] * 0.35f);
            }
    }
}

}  // namespace

// =====================================================================
//  C ABI
// =====================================================================
HG_API int32_t hg_version(void) { return 100; }

HG_API int32_t hg_info(int32_t what) {
    switch (what) {
        case 0: return W;
        case 1: return H;
        case 2: return Y0;
        case 3: return Y1;
        case 4: return TODAY;
        case 5: return TE_COUNT;
        default: return 0;
    }
}

HG_API void* hg_create(const char* nameUtf8, int32_t scenario) {
    try {
        return new World(nameUtf8 ? std::string(nameUtf8) : std::string(), scenario);
    } catch (...) {
        return nullptr;
    }
}

HG_API void hg_destroy(void* engine) { delete static_cast<World*>(engine); }

HG_API int32_t hg_shade(void* engine, uint8_t* out) {
    if (!engine || !out) return 0;
    auto* w = static_cast<World*>(engine);
    std::memcpy(out, w->shade.data(), w->shade.size());
    return (int32_t)w->shade.size();
}

static const Snap* snapOf(void* engine, int32_t year) {
    if (!engine) return nullptr;
    auto* w = static_cast<World*>(engine);
    int i = std::min(std::max(year, (int32_t)Y0), (int32_t)Y1) - Y0;
    return &w->snaps[i];
}

HG_API int32_t hg_terrain(void* engine, int32_t year, uint8_t* out) {
    const Snap* s = snapOf(engine, year);
    if (!s || !out) return 0;
    std::memcpy(out, s->terr.data(), s->terr.size());
    return (int32_t)s->terr.size();
}

HG_API int32_t hg_buildings(void* engine, int32_t year, HgBuilding* out, int32_t max) {
    const Snap* s = snapOf(engine, year);
    if (!s) return 0;
    int n = (int)s->b.size();
    if (!out) return n;
    n = std::min(n, (int)max);
    std::memcpy(out, s->b.data(), sizeof(HgBuilding) * n);
    return n;
}

HG_API int32_t hg_stats(void* engine, int32_t year, HgStats* out) {
    const Snap* s = snapOf(engine, year);
    if (!s || !out) return 0;
    *out = s->st;
    return 1;
}

HG_API int32_t hg_events(void* engine, HgEvent* out, int32_t max) {
    if (!engine) return 0;
    auto* w = static_cast<World*>(engine);
    int n = (int)w->events.size();
    if (!out) return n;
    n = std::min(n, (int)max);
    std::memcpy(out, w->events.data(), sizeof(HgEvent) * n);
    return n;
}

HG_API double hg_lens(uint8_t* bgra, int32_t w, int32_t h, int32_t stride,
                      const HgLight* lights, int32_t count, const HgLens* p) {
    auto t0 = std::chrono::high_resolution_clock::now();
    if (!bgra || w <= 0 || h <= 0 || !p) return 0;

    // 1) Накопление света (аддитивные радиальные ореолы).
    static thread_local std::vector<float> acc;
    acc.assign((size_t)w * h * 3, 0.f);
    for (int k = 0; k < count && lights; ++k) {
        const HgLight& L = lights[k];
        float I = L.intensity;
        if (L.blink) {
            float ph = std::fmod(p->time * 0.9f + (L.x * 0.013f + L.y * 0.007f), 1.0f);
            I *= ph < 0.18f ? 1.0f : 0.08f;
        }
        if (I <= 0.001f || L.radius < 0.5f) continue;
        int x0 = std::max(0, (int)(L.x - L.radius)), x1 = std::min(w - 1, (int)(L.x + L.radius));
        int y0 = std::max(0, (int)(L.y - L.radius)), y1 = std::min(h - 1, (int)(L.y + L.radius));
        float inv = 1.f / L.radius;
        for (int y = y0; y <= y1; ++y) {
            float* row = acc.data() + (size_t)y * w * 3;
            float dy = (y + 0.5f - L.y) * inv;
            for (int x = x0; x <= x1; ++x) {
                float dx = (x + 0.5f - L.x) * inv;
                float d2 = dx * dx + dy * dy;
                if (d2 >= 1.f) continue;
                float f = 1.f - std::sqrt(d2);
                f = f * f * (0.6f + 0.4f * f) * I;
                row[x * 3 + 0] += L.r * f; row[x * 3 + 1] += L.g * f; row[x * 3 + 2] += L.b * f;
            }
        }
    }

    // 2) Плёнка/матрица эпохи — параллельно по полосам кадра.
    LensJob J;
    J.px = bgra; J.w = w; J.h = h; J.stride = stride; J.light = acc.data();
    lensParams(p->year, J.P);
    J.night = std::min(1.f, std::max(0.f, p->night));
    J.lightGain = 0.10f + 0.90f * J.night;
    J.frame = (uint32_t)p->frame;
    J.flicker = 1.f + (J.P[LK_FLICKER] > 0 ? ((ihash(J.frame * 2654435761u) & 255) / 255.f - 0.5f) * 2 * J.P[LK_FLICKER] : 0);

    unsigned hw = std::thread::hardware_concurrency();
    int nt = (int)std::max(1u, std::min(8u, hw ? hw : 1u));
    if ((long long)w * h < 120000) nt = 1;
    if (nt == 1) {
        lensRows(J, 0, h);
    } else {
        std::vector<std::thread> pool;
        int band = (h + nt - 1) / nt;
        for (int t = 0; t < nt; ++t) {
            int a = t * band, b = std::min(h, a + band);
            if (a < b) pool.emplace_back(lensRows, std::cref(J), a, b);
        }
        for (auto& th : pool) th.join();
    }

    // 3) Царапины и пыль (эпоха немого кино).
    scratches(bgra, w, h, stride, J.P[LK_SCRATCH], J.frame);

    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

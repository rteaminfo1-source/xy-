/* Армия и война: набор войск, походы за земли, набеги и осады, оккупация 1941–1944, специализации городов.
 * Как и весь движок, не зависит от ОС. */
#include "war.h"
#include <string.h>
#include <math.h>

#define OCC_FROM 1941
#define OCC_TO   1944
#define LEGACY_YEAR 1795      /* разделы Речи Посполитой: присоединённые земли становятся наследием */

/* ---------- случайные числа (тот же генератор, что в движке) ---------- */
static unsigned wrnd(Game *g)
{
    unsigned x = g->rng ? g->rng : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x;
    return x;
}
static double wrnd01(Game *g) { return (wrnd(g) % 10000u) / 10000.0; }
static int wrnd_n(Game *g, int n) { return n > 0 ? (int)(wrnd(g) % (unsigned)n) : 0; }
static double clampw(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static double cost_k(const Game *g) { return 1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0; }

void war_data_init(void)
{
    for (int u = 0; u < UNIT_COUNT; u++) UNIT_TYPES[u].techi = tech_index(UNIT_TYPES[u].tech);
}

int capital_of_era(int era)
{
    if (era == 1) return C_POLOTSK;
    if (era == 2) return C_NOVOGRUDOK;
    if (era >= 5) return C_MINSK;
    return -1;
}

/* ---------- войско ---------- */
double war_tech_mult(const Game *g)
{
    int n = 0;
    for (int t = 0; t < TECH_COUNT; t++)
        if (g->tech[t] && TECHS[t].branch == BR_WAR) n++;
    return n * 0.05 + 1.0 > 2.0 ? 2.0 : n * 0.05 + 1.0;
}

double war_unit_eff(const Game *g, int u)
{
    int lag = g->era - UNIT_TYPES[u].era - 1;
    return lag > 0 ? 1.0 / (1.0 + 0.3 * lag) : 1.0;
}

void war_army(const Game *g, Army *a)
{
    memset(a, 0, sizeof *a);
    double tm = war_tech_mult(g);
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) {
        int n = g->units[u];
        if (n <= 0) continue;
        double k = war_unit_eff(g, u) * tm;
        a->attack  += n * UNIT_TYPES[u].atk * k;
        a->defense += n * UNIT_TYPES[u].def * k;
        a->units   += n;
        a->upkeep  += n * UNIT_TYPES[u].upkeep;
    }
}

int war_army_size(const Game *g)
{
    int n = 0;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) n += g->units[u];
    return n;
}

int war_army_cap(const Game *g) { return 5 + (int)(g->pop / 250.0); }

int war_unit_state(const Game *g, int u)
{
    const UnitType *t = &UNIT_TYPES[u];
    if (t->era > g->era) return US_FUTURE;
    if (t->techi >= 0 && !g->tech[t->techi]) return US_LOCKED;
    return US_AVAILABLE;
}

int war_unit_cost_gold(const Game *g, int u) { return (int)lround(UNIT_TYPES[u].gold * cost_k(g)); }
int war_unit_cost_ind(const Game *g, int u)  { return (int)lround(UNIT_TYPES[u].ind  * cost_k(g)); }

int war_recruit(Game *g, int u)
{
    if (g->over || u < 0 || u >= UNIT_COUNT || u >= MAX_UNITS) return 0;
    if (war_unit_state(g, u) != US_AVAILABLE) return 0;
    if (war_army_size(g) >= war_army_cap(g)) return 0;
    int cg = war_unit_cost_gold(g, u), ci = war_unit_cost_ind(g, u);
    if (g->gold < cg || g->ind < ci) return 0;
    g->gold -= cg;
    g->ind -= ci;
    g->units[u]++;
    game_check_achievements(g);   /* задание «войско не меньше N» засчитывается сразу */
    return 1;
}

int war_disband(Game *g, int u)
{
    if (g->over || u < 0 || u >= UNIT_COUNT || u >= MAX_UNITS || g->units[u] <= 0) return 0;
    g->units[u]--;
    return 1;
}

double war_attack_power(const Game *g, int kind)
{
    Army a;
    war_army(g, &a);
    double atk = a.attack;
    if (kind == WAR_LIBERATION) {   /* партизаны знают каждую тропу */
        int p = -1;
        for (int u = 0; u < UNIT_COUNT; u++) if (strcmp(UNIT_TYPES[u].id, "partisans") == 0) p = u;
        if (p >= 0 && p < MAX_UNITS && g->units[p] > 0)
            atk += 0.5 * g->units[p] * UNIT_TYPES[p].atk * war_unit_eff(g, p) * war_tech_mult(g);
    }
    return atk;
}

double war_defense_power(const Game *g)
{
    Army a;
    war_army(g, &a);
    return a.defense * 0.75 + game_def(g);   /* game_def уже учитывает четверть обороны войска */
}

/* ---------- земли ---------- */
static int group_owner(const Game *g, int p)
{
    for (int q = 0; q < PROVINCE_COUNT && q < MAX_PROVINCES; q++)
        if (g->provOwned[q] && strcmp(PROVINCES[q].group, PROVINCES[p].group) == 0) return q;
    return -1;
}

int war_group_owner(const Game *g, int p)
{
    return p >= 0 && p < PROVINCE_COUNT ? group_owner(g, p) : -1;
}

int war_province_state(const Game *g, int p)
{
    if (p < 0 || p >= PROVINCE_COUNT || p >= MAX_PROVINCES) return PV_CLOSED;
    if (g->provOwned[p] == 1) return PV_OWNED;
    if (g->provOwned[p] == 2) return PV_LEGACY;
    if (g->year < PROVINCES[p].from) return PV_FUTURE;
    if (g->year > PROVINCES[p].to) return PV_CLOSED;
    if (group_owner(g, p) >= 0) return PV_CLOSED;
    return PV_AVAILABLE;
}

int war_province_strength(const Game *g, int p)
{
    /* противник тоже не стоит на месте: в поздние эпохи та же земля защищена сильнее */
    int era = g->era > PROVINCES[p].era ? g->era : PROVINCES[p].era;
    return (int)lround(PROVINCES[p].strength * era_scale(era) / era_scale(PROVINCES[p].era) * cost_k(g));
}

int war_province_count(const Game *g)
{
    int n = 0;
    for (int p = 0; p < PROVINCE_COUNT && p < MAX_PROVINCES; p++) n += g->provOwned[p] == 1;
    return n;
}

int war_region_strength(const Game *g)
{
    int y = g->year < OCC_FROM ? OCC_FROM : g->year > OCC_TO ? OCC_TO : g->year;
    return (int)lround(280.0 * (1.0 - 0.2 * (y - OCC_FROM)) * cost_k(g));
}

double war_enemy_power(const Game *g, int kind, int target)
{
    if (kind == WAR_CONQUEST && target >= 0 && target < PROVINCE_COUNT) return war_province_strength(g, target);
    if (kind == WAR_LIBERATION) return war_region_strength(g);
    return 0;
}

/* Сколько боёв осталось до закрытия окна: поход — до года PROVINCES[].to, освобождение — до 1944 года. */
int war_battles_left(const Game *g, int kind, int target)
{
    int ongoing = g->warKind == kind && g->warTarget == target;
    int cap = 10 - (ongoing ? g->warTurns : 0), n = 0;
    int last = kind == WAR_CONQUEST && target >= 0 && target < PROVINCE_COUNT ? PROVINCES[target].to : OCC_TO;
    for (int t = g->turn; t < TURN_COUNT && n < cap; t++) {
        if (TURN_YEARS[t] > last) break;
        n++;
    }
    return n;
}

/* Ожидаемое число ходов до победы: прогноз по ходам с учётом силы врага в каждом году,
 * закрытия окна похода (или конца оккупации) и предела в 10 ходов. 99 — не успеть. */
int war_eta(const Game *g, int kind, int target)
{
    double A = war_attack_power(g, kind);
    if (A <= 0) return 99;
    if (kind == WAR_CONQUEST && (target < 0 || target >= PROVINCE_COUNT)) return 99;
    int ongoing = g->warKind == kind && g->warTarget == target;
    double prog = ongoing ? g->warProgress : 0;
    int cap = 10 - (ongoing ? g->warTurns : 0), n = 0;
    int first = kind == WAR_CONQUEST ? PROVINCES[target].from : OCC_FROM;
    int last  = kind == WAR_CONQUEST ? PROVINCES[target].to   : OCC_TO;
    for (int t = g->turn; t < TURN_COUNT && n < cap; t++) {
        int y = TURN_YEARS[t];
        if (y > last) break;
        if (y < first) continue;
        double E;
        if (kind == WAR_CONQUEST) {
            int e = era_of_year(y), pe = PROVINCES[target].era;
            if (e < g->era) e = g->era;
            if (e < pe) e = pe;
            E = lround(PROVINCES[target].strength * era_scale(e) / era_scale(pe) * cost_k(g));
        } else E = lround(280.0 * (1.0 - 0.2 * (y - OCC_FROM)) * cost_k(g));
        prog += 200.0 * A / (A + E) - 60.0;
        n++;
        if (prog >= 100) return n;
    }
    return 99;
}

/* ---------- объявление войны ---------- */
const wchar_t *war_cannot_declare(const Game *g, int kind, int target)
{
    if (g->over) return L"Летопись завершена.";
    if (g->warKind != WAR_NONE) return L"Уже идёт война — сначала закончите её.";
    if (kind == WAR_CONQUEST) {
        if (target < 0 || target >= PROVINCE_COUNT || target >= MAX_PROVINCES) return L"Нет такой земли.";
        if (g->occupied) return L"Страна оккупирована: сейчас не до походов.";
        switch (war_province_state(g, target)) {
            case PV_OWNED:  return L"Эта земля уже ваша.";
            case PV_LEGACY: return L"Эта земля уже была вашей.";
            case PV_FUTURE: return L"Поход за эту землю ещё не возможен.";
            case PV_CLOSED: return group_owner(g, target) >= 0 ? L"Эта земля уже ваша." : L"Время для этого похода прошло.";
        }
    } else if (kind == WAR_LIBERATION) {
        if (target < 0 || target >= REGION_COUNT) return L"Нет такой области.";
        if (!(g->occupied & (1u << target))) return L"Эта область свободна.";
    } else return L"Неизвестная война.";
    if (war_attack_power(g, kind) <= 0) return L"Нет войска: наберите войска на этой вкладке.";
    return 0;
}

int war_declare(Game *g, int kind, int target)
{
    if (war_cannot_declare(g, kind, target)) return 0;
    g->warKind = kind;
    g->warTarget = target;
    g->warProgress = 0;
    g->warTurns = 0;
    g->warReport[0] = 0;
    if (kind == WAR_CONQUEST) {
        g->stab = clampw(g->stab - 2, 0, 100);
        game_log(g, LOG_WAR, L"Начат поход: %ls (владелец — %ls)", PROVINCES[target].name, PROVINCES[target].owner);
    } else {
        game_log(g, LOG_WAR, L"Начата операция по освобождению: %ls", REGION_NAMES[target]);
    }
    return 1;
}

void war_peace(Game *g)
{
    if (g->warKind == WAR_NONE) return;
    if (g->warKind == WAR_CONQUEST) {
        g->stab = clampw(g->stab - 1, 0, 100);
        game_log(g, LOG_WAR, L"Мир без победы: войска вернулись из похода на землю «%ls»", PROVINCES[g->warTarget].name);
    } else {
        game_log(g, LOG_WAR, L"Операция остановлена: %ls остаётся под оккупацией", REGION_NAMES[g->warTarget]);
    }
    g->warKind = WAR_NONE;
    g->warTarget = -1;
    g->warProgress = 0;
    g->warTurns = 0;
}

/* Потери: каждое соединение гибнет с вероятностью frac. Возвращает число потерянных. */
static int lose_units(Game *g, double frac)
{
    int lost = 0;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) {
        int n = g->units[u];
        for (int i = 0; i < n; i++)
            if (wrnd01(g) < frac) { g->units[u]--; lost++; }
    }
    g->unitsLost += lost;
    return lost;
}

/* ---------- доходы городов и земель ---------- */
int city_level(const Game *g, int c)
{
    int n = 0;
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (g->proj[p] && PROJECTS[p].city == c) n++;
    if (capital_of_era(g->era) == c) n += 2;
    return n;
}

void city_bonus(const Game *g, int c, double out[R_COUNT])
{
    memset(out, 0, sizeof(double) * R_COUNT);
    if (c < 0 || c >= CITY_COUNT) return;
    int lvl = city_level(g, c);
    if (lvl <= 0) return;
    double k = lvl * (0.5 + 0.25 * g->era);
    const Fx *fx = &SPEC_FX[CITY_PROFILES[c].spec];
    for (int r = 0; r < R_COUNT; r++) out[r] = fx->v[r] * k;
}

void cities_income(const Game *g, double out[R_COUNT])
{
    double b[R_COUNT];
    memset(out, 0, sizeof(double) * R_COUNT);
    for (int c = 0; c < CITY_COUNT; c++) {
        city_bonus(g, c, b);
        for (int r = 0; r < R_COUNT; r++) out[r] += b[r];
    }
}

void provinces_income(const Game *g, double out[R_COUNT])
{
    memset(out, 0, sizeof(double) * R_COUNT);
    for (int p = 0; p < PROVINCE_COUNT && p < MAX_PROVINCES; p++) {
        if (!g->provOwned[p]) continue;
        double k = era_scale(g->era) / era_scale(PROVINCES[p].era) * (g->provOwned[p] == 2 ? 0.3 : 1.0);
        for (int r = 0; r < R_COUNT; r++) out[r] += PROVINCES[p].fx.v[r] * k;
    }
}

/* ---------- оккупация ---------- */
int war_occupied_count(const Game *g)
{
    int n = 0;
    for (int r = 0; r < REGION_COUNT; r++) n += (g->occupied >> r) & 1u;
    return n;
}

double war_occupation_mult(const Game *g) { return 1.0 - 0.08 * war_occupied_count(g); }

double war_occupation_harm(const Game *g)
{
    if (g->year <= OCC_FROM) return 1.0;     /* удар 22 июня уже учтён в событии */
    return 1.0 + 0.04 * war_occupied_count(g);
}

/* место боя для карты; ahead = 1, если игрок увидит его в следующем ходу (бой в конце хода) */
static void mark_battle(Game *g, double lon, double lat, int won, int kind, int ahead)
{
    g->battleLon = (float)lon;
    g->battleLat = (float)lat;
    g->battleTurn = g->turn + ahead;
    g->battleWon = won;
    g->battleKind = kind;
}

/* ---------- набеги ---------- */
static const double RAID_P[ERA_COUNT + 1] = { 0, 0.25, 0.25, 0.25, 0.10, 0.20, 0.0, 0.06, 0.06 };
static const double RAID_E[ERA_COUNT + 1] = { 0, 18, 50, 110, 120, 220, 0, 300, 320 };
static const double RAID_POP[ERA_COUNT + 1] = { 0, 0.006, 0.006, 0.006, 0.001, 0.002, 0, 0, 0 };   /* доля погибших */
static const wchar_t *RAIDERS[ERA_COUNT + 1][4] = {
    { 0 },
    { L"Набег половцев", L"Поход меченосцев", L"Набег ятвягов", L"Рать соседнего князя" },
    { L"Поход Тевтонского ордена", L"Татарский набег", L"Ордынский набег", L"Московская рать" },
    { L"Набег крымских татар", L"Шведский рейд", L"Московское войско", L"Казацкий загон" },
    { L"Разбойничья шайка", L"Мародёры на тракте", L"Разбойничья шайка", L"Мародёры на тракте" },
    { L"Вооружённая банда", L"Конные налётчики", L"Вооружённая банда", L"Конные налётчики" },
    { 0 },
    { L"Нарушители границы", L"Контрабандисты", L"Нарушители границы", L"Контрабандисты" },
    { L"Контрабандисты", L"Незаконный провоз грузов", L"Контрабандисты", L"Незаконный провоз грузов" },
};
static const int RAID_CITIES[ERA_COUNT + 1][4] = {
    { 0 },
    { C_TUROV, C_POLOTSK, C_MINSK, C_VITEBSK },
    { C_GRODNO, C_BREST, C_NOVOGRUDOK, C_ORSHA },
    { C_MOGILEV, C_GRODNO, C_MSTISLAVL, C_PINSK },
    { C_BREST, C_GRODNO, C_VITEBSK, C_GOMEL },
    { C_MINSK, C_POLOTSK, C_MOZYR, C_BOBRUISK },
    { C_MINSK, C_MINSK, C_MINSK, C_MINSK },
    { C_BREST, C_GRODNO, C_BREST, C_GRODNO },
    { C_BREST, C_GRODNO, C_OSTROVETS, C_GOMEL },
};

double war_raid_chance(const Game *g)
{
    if (g->era < 1 || g->era > ERA_COUNT || g->occupied || g->warKind != WAR_NONE) return 0;
    return RAID_P[g->era];
}

double war_raid_strength(const Game *g)
{
    if (g->era < 1 || g->era > ERA_COUNT) return 0;
    return RAID_E[g->era] * (1.0 + DIFFICULTIES[g->difficulty].harmPct / 200.0);
}

static void raid(Game *g)
{
    double p = war_raid_chance(g);
    if (p <= 0 || wrnd01(g) >= p) return;
    int k = wrnd_n(g, 4);
    const wchar_t *who = RAIDERS[g->era][k];
    const wchar_t *city = CITIES[RAID_CITIES[g->era][k]].name;
    int peace = g->era >= 7;   /* после 1945 года боёв на границе нет: нарушителей задерживают пограничники */
    double E = war_raid_strength(g) * (0.7 + 0.6 * wrnd01(g));
    double D = war_defense_power(g) * (0.8 + 0.4 * wrnd01(g));
    g->warNews = 1;
    if (!peace) mark_battle(g, CITIES[RAID_CITIES[g->era][k]].lon, CITIES[RAID_CITIES[g->era][k]].lat, D >= E, WAR_NONE, 1);
    if (D >= E) {
        long loot = lround(5 * era_scale(g->era));
        g->gold += loot;
        g->stab = clampw(g->stab + 1, 0, 100);
        if (!peace) g->battlesWon++;
        int lost = !peace && war_army_size(g) > 0 && E > D * 0.7 ? lose_units(g, 0.05) : 0;
        if (peace) swprintf(g->warReport, 200, L"%ls у города %ls: пограничники пресекли нарушение; штрафы и конфискат — казна +%ld.",
                            who, city, loot);
        else if (lost) swprintf(g->warReport, 200, L"%ls. Бой у города %ls выигран! Трофеи: казна +%ld, потеряно соединений: %d.",
                           who, city, loot, lost);
        else swprintf(g->warReport, 200, L"%ls. Бой у города %ls выигран! Трофеи: казна +%ld.", who, city, loot);
    } else {
        double goldLoss = g->gold * (peace ? 0.05 : 0.15);
        double popLoss = g->pop * RAID_POP[g->era] * (1.0 + DIFFICULTIES[g->difficulty].harmPct / 100.0);
        g->gold -= goldLoss;
        g->ind *= 0.9;
        g->pop -= popLoss;
        g->popLostWar += popLoss;
        g->stab = clampw(g->stab - (peace ? 1 : 2), 0, 100);
        if (!peace) g->battlesLost++;
        int lost = !peace && war_army_size(g) > 0 ? lose_units(g, 0.15) : 0;
        wchar_t ps[32];
        fmt_pop(popLoss, ps, 32);
        if (peace)
            swprintf(g->warReport, 200, L"%ls у города %ls: нарушение не пресечено, ущерб — %ld казны, производство −10%%.",
                     who, city, lround(goldLoss));
        else if (popLoss >= 0.5)
            swprintf(g->warReport, 200, L"%ls. Враг прорвался к городу %ls: разграблено %ld казны, погибших — %ls%ls", who, city,
                     lround(goldLoss), ps, lost ? L" Войско понесло потери." : L" Нужно больше войск и крепостей.");
        else
            swprintf(g->warReport, 200, L"%ls. Граница у города %ls не удержана: ущерб — %ld казны, производство −10%%.%ls", who, city,
                     lround(goldLoss), lost ? L" Войско понесло потери." : L" Нужно больше войск и крепостей.");
    }
    game_log(g, LOG_WAR, L"%ls", g->warReport);
}

/* ---------- ход войны ---------- */
static void end_war(Game *g)
{
    g->warKind = WAR_NONE;
    g->warTarget = -1;
    g->warProgress = 0;
    g->warTurns = 0;
}

static void battle(Game *g)
{
    int kind = g->warKind, t = g->warTarget;
    const wchar_t *name = kind == WAR_CONQUEST ? PROVINCES[t].name : REGION_NAMES[t];
    double A = war_attack_power(g, kind), E = war_enemy_power(g, kind, t);
    g->warNews = 1;
    g->warTurns++;
    int gain = A > 0 ? (int)lround(200.0 * A / (A + E) * (0.8 + 0.4 * wrnd01(g)) - 60.0) : -40;
    g->warProgress += gain;
    int lost = lose_units(g, 0.10 * E / (A + E + 1e-9));
    if (kind == WAR_CONQUEST) g->stab = clampw(g->stab - 1.5, 0, 100);
    if (kind == WAR_CONQUEST) mark_battle(g, PROVINCES[t].lon, PROVINCES[t].lat, gain > 0, kind, 1);
    else mark_battle(g, CITIES[REGION_CITY[t]].lon, CITIES[REGION_CITY[t]].lat, gain > 0, kind, 1);

    if (g->warProgress >= 100) {
        g->battlesWon++;
        if (kind == WAR_CONQUEST) {
            const Province *P = &PROVINCES[t];
            long loot = lround(10 * era_scale(g->era));
            g->provOwned[t] = 1;
            g->pop += P->popK;
            g->gold += loot;
            g->cult += 5;
            g->stab = clampw(g->stab + 4, 0, 100);
            wchar_t ps[32];
            fmt_pop(P->popK, ps, 32);
            swprintf(g->warReport, 200, L"Победа! Земля «%ls» присоединена: +%ls жителей, трофеи +%ld казны, новый доход каждый ход.",
                     name, ps, loot);
        } else {
            g->occupied &= ~(1u << t);
            g->liberated |= 1u << t;
            g->stab = clampw(g->stab + 5, 0, 100);
            g->cult += 4;
            swprintf(g->warReport, 200, L"%ls освобождена своими силами! Доходы растут, каратели больше не тронут эти деревни.", name);
        }
        game_log(g, LOG_WAR, L"%ls", g->warReport);
        end_war(g);
        game_check_achievements(g);
        return;
    }
    if (g->warProgress <= -40 || g->warTurns >= 10) {
        g->battlesLost++;
        g->stab = clampw(g->stab - (kind == WAR_CONQUEST ? 6 : 3), 0, 100);
        g->gold *= 0.85;
        if (kind == WAR_CONQUEST)
            swprintf(g->warReport, 200, g->warProgress <= -40
                     ? L"Поражение: земля «%ls» не взята, войско отступило. Казна −15%%, стабильность падает."
                     : L"Затяжная война: земля «%ls» не взята за 10 ходов, заключено перемирие.", name);
        else
            swprintf(g->warReport, 200, L"Наступление захлебнулось: %ls остаётся под оккупацией. Пополните войско и попробуйте снова.", name);
        game_log(g, LOG_WAR, L"%ls", g->warReport);
        end_war(g);
        return;
    }
    int pr = g->warProgress < 0 ? 0 : g->warProgress;
    swprintf(g->warReport, 200, L"%ls%ls%ls: продвижение %d%% (за ход %ls%d%ls). %ls", kind == WAR_CONQUEST ? L"Поход на землю «" : L"",
             name, kind == WAR_CONQUEST ? L"»" : L"",
             pr, gain < 0 ? L"\x2212" : L"+", gain < 0 ? -gain : gain, gain < 0 ? L" — войско отступило" : L"",
             lost ? L"Есть потери — пополните войско." : L"Войско цело.");
}

void war_end_turn(Game *g)
{
    g->warNews = 0;
    if (g->over) return;
    /* оккупация: каратели и голод уносят людей */
    int occ = war_occupied_count(g);
    if (occ > 0) {
        double loss = g->pop * 0.0025 * occ;
        g->pop -= loss;
        g->popLostWar += loss;
    }
    /* пустая казна: часть войска расходится */
    if (g->gold < 0) {
        g->gold = 0;
        int n = war_army_size(g);
        if (n > 0) {
            int lost = lose_units(g, 0.25);
            if (lost == 0) {
                for (int u = UNIT_COUNT - 1; u >= 0; u--)
                    if (u < MAX_UNITS && g->units[u] > 0) { g->units[u]--; g->unitsLost++; lost = 1; break; }
            }
            game_log(g, LOG_WAR, L"Казна пуста: жалованья нет, разошлось соединений: %d", lost);
        }
    }
    if (g->warKind != WAR_NONE) battle(g);
    else raid(g);
}

/* ---------- смена года ---------- */
void war_year_changed(Game *g, int oldYear)
{
    int y = g->year;
    /* поход, у которого закрылось окно, заканчивается перемирием */
    if (g->warKind == WAR_CONQUEST && y > PROVINCES[g->warTarget].to) {
        swprintf(g->warReport, 200, L"Перемирие: время похода на землю «%ls» прошло — земля не взята.", PROVINCES[g->warTarget].name);
        g->warNews = 1;
        game_log(g, LOG_WAR, L"%ls", g->warReport);
        end_war(g);
    }
    /* осады: прежний владелец пытается вернуть землю, когда для неё открывается новая эпоха */
    for (int p = 0; p < PROVINCE_COUNT && p < MAX_PROVINCES; p++) {
        if (g->provOwned[p] != 1) continue;
        for (int q = 0; q < PROVINCE_COUNT; q++) {
            if (q == p || strcmp(PROVINCES[q].group, PROVINCES[p].group) != 0) continue;
            if (!(PROVINCES[q].from > oldYear && PROVINCES[q].from <= y)) continue;
            double E = war_province_strength(g, q) * (0.8 + 0.4 * wrnd01(g));
            double D = war_defense_power(g) * (0.8 + 0.4 * wrnd01(g));
            g->warNews = 1;
            mark_battle(g, PROVINCES[p].lon, PROVINCES[p].lat, D >= E, WAR_NONE, 0);
            if (D >= E) {
                g->battlesWon++;
                swprintf(g->warReport, 200, L"%ls пыталось отнять землю «%ls» — осада отбита!", PROVINCES[q].owner, PROVINCES[p].name);
            } else {
                g->battlesLost++;
                g->provOwned[p] = 0;
                double back = PROVINCES[p].popK < g->pop * 0.2 ? PROVINCES[p].popK : g->pop * 0.2;
                g->pop -= back;
                g->stab = clampw(g->stab - 4, 0, 100);
                lose_units(g, 0.2);
                swprintf(g->warReport, 200, L"%ls отняло землю «%ls». Её можно отвоевать — вкладка «Армия».",
                         PROVINCES[q].owner, PROVINCES[p].name);
            }
            game_log(g, LOG_WAR, L"%ls", g->warReport);
            break;
        }
    }
    /* разделы Речи Посполитой: свои земли становятся наследием */
    if (oldYear < LEGACY_YEAR && y >= LEGACY_YEAR) {
        int n = 0;
        for (int p = 0; p < PROVINCE_COUNT && p < MAX_PROVINCES; p++)
            if (g->provOwned[p] == 1) { g->provOwned[p] = 2; n++; }
        if (n) game_log(g, LOG_WAR, L"Разделы Речи Посполитой: присоединённые земли (%d) вошли в империю, остались торговые связи — 30%% дохода", n);
    }
    /* оккупация 1941–1944 */
    if (oldYear < OCC_FROM && y >= OCC_FROM && y <= OCC_TO) {
        g->occupied = (1u << REGION_COUNT) - 1;
        if (g->warKind != WAR_NONE) end_war(g);
        game_log(g, LOG_WAR, L"Оккупация: все шесть областей под властью захватчиков. Освобождайте их — вкладка «Армия»");
    }
    if (oldYear <= OCC_TO && y > OCC_TO) {
        int left = war_occupied_count(g);
        if (left) game_log(g, LOG_WAR, L"Операция «Багратион» освободила оставшиеся области: %d", left);
        g->occupied = 0;
        if (g->warKind == WAR_LIBERATION) {
            swprintf(g->warReport, 200, L"Операция «Багратион» завершила освобождение: %ls свободна.", REGION_NAMES[g->warTarget]);
            g->warNews = 1;
            end_war(g);
        }
    }
}

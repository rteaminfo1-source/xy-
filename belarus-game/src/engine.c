/* Игровой движок: экономика, исследования, стройки, события, счёт. Без зависимостей от ОС. */
#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>

static const double ERA_SCALE[ERA_COUNT + 1] = { 1, 1.0, 1.8, 2.8, 4.0, 5.5, 5.5, 8.0, 11.0 };
static const wchar_t *RES_NAMES[R_COUNT] = {
    L"казна", L"знания", L"производство", L"оборона", L"стабильность", L"культура", L"экология", L"рост"
};

double era_scale(int era) { return (era >= 1 && era <= ERA_COUNT) ? ERA_SCALE[era] : 1.0; }

int era_of_year(int year)
{
    for (int e = ERA_COUNT - 1; e >= 0; e--)
        if (year >= ERAS[e].from) return e + 1;
    return 1;
}

int tech_index(const char *id)
{
    if (!id) return -1;
    for (int i = 0; i < TECH_COUNT; i++)
        if (strcmp(TECHS[i].id, id) == 0) return i;
    return -1;
}

static int proj_index(const char *id)
{
    for (int i = 0; i < PROJECT_COUNT; i++)
        if (strcmp(PROJECTS[i].id, id) == 0) return i;
    return -1;
}

int game_has_tech(const Game *g, const char *id) { int i = tech_index(id); return i >= 0 && g->tech[i]; }
int game_has_proj(const Game *g, const char *id) { int i = proj_index(id); return i >= 0 && g->proj[i]; }

static void init_events(Event *ev, int n)
{
    for (int i = 0; i < n; i++) {
        ev[i].nch = 0;
        for (int c = 0; c < MAX_CHOICES; c++) {
            if (!ev[i].ch[c].label) break;
            ev[i].nch = c + 1;
            ev[i].ch[c].reqi = tech_index(ev[i].ch[c].reqTech);
            ev[i].ch[c].granti = tech_index(ev[i].ch[c].grant);
        }
    }
}

void data_init(void)
{
    static int ready;
    if (ready) return;
    ready = 1;
    for (int t = 0; t < TECH_COUNT; t++)
        for (int k = 0; k < 3; k++)
            TECHS[t].reqi[k] = tech_index(TECHS[t].req[k]);
    for (int p = 0; p < PROJECT_COUNT; p++) PROJECTS[p].techi = tech_index(PROJECTS[p].tech);
    for (int i = 0; i < PEOPLE_COUNT; i++) PEOPLE[i].disci = tech_index(PEOPLE[i].discount);
    init_events(HIST_EVENTS, HIST_COUNT);
    init_events(RAND_EVENTS, RAND_COUNT);
}

/* ---------- утилиты ---------- */
static unsigned rnd(Game *g)
{
    unsigned x = g->rng ? g->rng : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x;
    return x;
}
static int rnd_n(Game *g, int n) { return n > 0 ? (int)(rnd(g) % (unsigned)n) : 0; }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

void fmt_pop(double popK, wchar_t *buf, int n)
{
    if (popK >= 1000) {
        long m10 = lround(popK / 100.0);
        swprintf(buf, n, L"%ld,%ld млн", m10 / 10, m10 % 10);
    } else {
        swprintf(buf, n, L"%ld тыс.", lround(popK));
    }
}

static void cat(wchar_t *buf, int n, const wchar_t *s)
{
    size_t len = wcslen(buf);
    if ((int)len >= n - 1) return;
    wcsncat(buf, s, (size_t)n - 1 - len);
}

static void cat_delta(wchar_t *buf, int n, const wchar_t *name, long v, int *first)
{
    wchar_t tmp[96];
    if (v == 0) return;
    swprintf(tmp, 96, L"%ls%ls %ls%ld", *first ? L"" : L" · ", name, v < 0 ? L"\x2212" : L"+", v < 0 ? -v : v);
    cat(buf, n, tmp);
    *first = 0;
}

void game_log(Game *g, int kind, const wchar_t *fmt, ...)
{
    if (g->logN >= MAX_LOG) {
        memmove(&g->logs[0], &g->logs[1], sizeof(LogEntry) * (MAX_LOG - 1));
        g->logN = MAX_LOG - 1;
    }
    LogEntry *e = &g->logs[g->logN++];
    e->year = g->year;
    e->kind = kind;
    va_list ap;
    va_start(ap, fmt);
    vswprintf(e->text, LOG_LEN, fmt, ap);
    va_end(ap);
}

void fx_describe(const Fx *fx, wchar_t *buf, int n)
{
    buf[0] = 0;
    int first = 1;
    for (int r = 0; r < R_COUNT; r++) {
        int v = fx->v[r];
        if (!v) continue;
        wchar_t tmp[64];
        if (r == R_GROW)
            swprintf(tmp, 64, L"%ls%ls0,%02d%% рост", first ? L"" : L" · ", v < 0 ? L"\x2212" : L"+", abs(v));
        else
            swprintf(tmp, 64, L"%ls%ls%d %ls", first ? L"" : L" · ", v < 0 ? L"\x2212" : L"+", abs(v), RES_NAMES[r]);
        cat(buf, n, tmp);
        first = 0;
    }
    if (first) cat(buf, n, L"без постоянного эффекта");
}

void choice_describe(const Game *g, int isRandom, const Choice *c, wchar_t *buf, int n)
{
    double k = isRandom ? era_scale(g->era) : 1.0;
    int first = 1;
    buf[0] = 0;
    cat_delta(buf, n, L"Казна", lround(c->dGold * k), &first);
    cat_delta(buf, n, L"Знания", lround(c->dSci * k), &first);
    cat_delta(buf, n, L"Производство", lround(c->dInd * k), &first);
    cat_delta(buf, n, L"Стабильность", c->dStab, &first);
    cat_delta(buf, n, L"Культура", c->dCult, &first);
    cat_delta(buf, n, L"Экология", c->dEco, &first);
    wchar_t tmp[160];
    if (c->popPct) {
        swprintf(tmp, 160, L"%lsНаселение %ls%d%%", first ? L"" : L" · ", c->popPct < 0 ? L"\x2212" : L"+", abs(c->popPct));
        cat(buf, n, tmp); first = 0;
    }
    if (c->war) {
        swprintf(tmp, 160, L"%lsУдар: до \x2212%d%% населения, оборона смягчит", first ? L"" : L" · ", c->war);
        cat(buf, n, tmp); first = 0;
    }
    if (c->indLoss) {
        swprintf(tmp, 160, L"%lsПотеря до %d%% производства", first ? L"" : L" · ", c->indLoss);
        cat(buf, n, tmp); first = 0;
    }
    if (c->modName) {
        wchar_t fx[200];
        fx_describe(&c->mod, fx, 200);
        if (c->modTurns) swprintf(tmp, 160, L"%ls%d хода: %ls", first ? L"" : L" · ", c->modTurns, fx);
        else swprintf(tmp, 160, L"%lsНавсегда: %ls", first ? L"" : L" · ", fx);
        cat(buf, n, tmp); first = 0;
    }
    if (c->granti >= 0 && !g->tech[c->granti]) {
        swprintf(tmp, 160, L"%lsОткрытие: %ls", first ? L"" : L" · ", TECHS[c->granti].name);
        cat(buf, n, tmp); first = 0;
    }
    if (first) cat(buf, n, L"Без последствий");
}

/* ---------- доход ---------- */
static int person_active(const Game *g, int i) { return g->person[i] && PEOPLE[i].to >= g->year; }

void game_income(const Game *g, Income *inc)
{
    memset(inc, 0, sizeof *inc);
    inc->base[R_GOLD] = 8;  inc->base[R_SCI] = 4; inc->base[R_IND] = 3;
    inc->base[R_DEF] = 4;   inc->base[R_CULT] = 1;
    inc->base[R_GROW] = ERAS[g->era - 1].base_grow;

    inc->popb[R_GOLD] = g->pop / 120.0;
    inc->popb[R_IND]  = g->pop / (g->era >= 4 ? 220.0 : 450.0);
    inc->popb[R_SCI]  = g->pop / (g->era >= 5 ? 700.0 : 1500.0);
    inc->popb[R_DEF]  = g->pop / 400.0;

    for (int t = 0; t < TECH_COUNT; t++)
        if (g->tech[t]) for (int r = 0; r < R_COUNT; r++) inc->techs[r] += TECHS[t].fx.v[r];
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (g->proj[p]) for (int r = 0; r < R_COUNT; r++) inc->projs[r] += PROJECTS[p].fx.v[r];
    for (int i = 0; i < PEOPLE_COUNT; i++)
        if (g->person[i]) {
            double k = person_active(g, i) ? 1.0 : 0.5;
            for (int r = 0; r < R_COUNT; r++) inc->people[r] += PEOPLE[i].fx.v[r] * k;
        }
    for (int m = 0; m < g->modN; m++)
        if (g->mods[m].until == 0 || g->mods[m].until > g->turn)
            for (int r = 0; r < R_COUNT; r++) inc->mods[r] += g->mods[m].fx.v[r];

    const Priority *pr = &PRIORITIES[g->priority];
    const Difficulty *df = &DIFFICULTIES[g->difficulty];
    double stabK = g->stab < 25 ? 0.7 : g->stab < 40 ? 0.85 : g->stab > 75 ? 1.1 : 1.0;
    double diffK = 1.0 + df->incomePct / 100.0;

    for (int r = 0; r < R_COUNT; r++) inc->mult[r] = 1.0;
    inc->mult[R_GOLD] = (1.0 + pr->goldPct / 100.0) * stabK * diffK;
    inc->mult[R_SCI]  = (1.0 + pr->sciPct  / 100.0) * stabK * diffK;
    inc->mult[R_IND]  = (1.0 + pr->indPct  / 100.0) * stabK * diffK;
    inc->mult[R_DEF]  = (1.0 + pr->defPct  / 100.0);

    double raw[R_COUNT];
    for (int r = 0; r < R_COUNT; r++)
        raw[r] = inc->base[r] + inc->popb[r] + inc->techs[r] + inc->projs[r] + inc->people[r] + inc->mods[r];
    for (int r = 0; r < R_COUNT; r++) inc->total[r] = raw[r] * inc->mult[r];

    inc->total[R_ECO] = raw[R_ECO] * 0.3 + pr->ecoAdd;
    inc->stabTarget = clampd(42.0 + 52.0 * (1.0 - exp(-raw[R_STAB] / 50.0)) + pr->stabAdd, 5, 97);

    /* вклад технологий в рост насыщается: не больше +0,4% в год */
    double extra = raw[R_GROW] - inc->base[R_GROW];
    double grow = inc->base[R_GROW] + (extra > 0 ? 30.0 * (1.0 - exp(-extra / 80.0)) : extra)
                + pr->growAdd + (g->stab - 50.0) / 10.0;
    if (g->eco < 50) grow -= (50.0 - g->eco) / 2.0;
    inc->total[R_GROW] = grow;
    inc->growYear = clampd(grow / 10000.0, -0.02, 0.03);
}

int game_def(const Game *g)
{
    Income inc;
    game_income(g, &inc);
    return (int)lround(inc.total[R_DEF]);
}

/* ---------- технологии ---------- */
int game_tech_state(const Game *g, int t)
{
    if (g->tech[t]) return TS_DONE;
    if (TECHS[t].era > g->era) return TS_FUTURE;
    for (int k = 0; k < 3; k++)
        if (TECHS[t].reqi[k] >= 0 && !g->tech[TECHS[t].reqi[k]]) return TS_LOCKED;
    return TS_AVAILABLE;
}

int game_tech_cost(const Game *g, int t)
{
    double c = TECHS[t].cost * (1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0);
    for (int i = 0; i < PEOPLE_COUNT; i++)
        if (g->person[i] && PEOPLE[i].disci == t) { c *= 0.65; break; }
    if (TECHS[t].era < g->era) c *= 0.8;
    return (int)lround(c);
}

int game_tech_count(const Game *g)
{
    int n = 0;
    for (int t = 0; t < TECH_COUNT; t++) n += g->tech[t] ? 1 : 0;
    return n;
}

static void unlock_tech(Game *g, int t)
{
    g->tech[t] = 1;
    g->techTurn[t] = g->turn;
    if (TECHS[t].year - g->year >= 50) g->aheadOfTime = 1;
}

int game_research(Game *g, int t)
{
    if (game_tech_state(g, t) != TS_AVAILABLE) return 0;
    int cost = game_tech_cost(g, t);
    if (g->sci < cost) return 0;
    g->sci -= cost;
    unlock_tech(g, t);
    game_log(g, LOG_TECH, L"Открытие: %ls", TECHS[t].name);
    game_check_achievements(g);
    return 1;
}

/* ---------- стройки ---------- */
int game_proj_state(const Game *g, int p)
{
    const Project *pr = &PROJECTS[p];
    if (g->proj[p]) return PS_DONE;
    if (pr->era > g->era) return PS_FUTURE;
    if (pr->until && g->year > pr->until) return PS_MISSED;
    if (pr->techi >= 0 && !g->tech[pr->techi]) return PS_LOCKED;
    return PS_AVAILABLE;
}

int game_proj_gold(const Game *g, int p) { return (int)lround(PROJECTS[p].gold * (1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0)); }
int game_proj_ind(const Game *g, int p)  { return (int)lround(PROJECTS[p].ind  * (1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0)); }

int game_proj_count(const Game *g)
{
    int n = 0;
    for (int p = 0; p < PROJECT_COUNT; p++) n += g->proj[p] ? 1 : 0;
    return n;
}

int game_build(Game *g, int p)
{
    if (game_proj_state(g, p) != PS_AVAILABLE) return 0;
    int cg = game_proj_gold(g, p), ci = game_proj_ind(g, p);
    if (g->gold < cg || g->ind < ci) return 0;
    g->gold -= cg;
    g->ind -= ci;
    g->proj[p] = 1;
    g->erasBuilt |= 1u << (g->era - 1);
    game_log(g, LOG_BUILD, L"Построено: %ls (%ls)", PROJECTS[p].name, CITIES[PROJECTS[p].city].name);
    game_check_achievements(g);
    return 1;
}

/* ---------- торговля излишками ---------- */
int game_sell_amount(const Game *g) { return (int)lround(20 * era_scale(g->era)); }

int game_sell_ind(Game *g)
{
    int amount = game_sell_amount(g);
    if (g->ind < amount) return 0;
    int gain = amount / 2;
    g->ind -= amount;
    g->gold += gain;
    return gain;
}

/* ---------- деятели ---------- */
int game_next_year(const Game *g) { return g->turn + 1 < TURN_COUNT ? TURN_YEARS[g->turn + 1] : END_YEAR; }

int game_person_state(const Game *g, int i)
{
    const Person *p = &PEOPLE[i];
    if (g->person[i]) return p->to >= g->year ? HS_ACTIVE : HS_LEGACY;
    if (p->from >= game_next_year(g)) return HS_FUTURE;
    if (p->to < g->year) return HS_MISSED;
    return HS_AVAILABLE;
}

int game_person_cost(const Game *g, int i) { return (int)lround(PEOPLE[i].cost * (1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0)); }

int game_people_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < PEOPLE_COUNT; i++) n += g->person[i] ? 1 : 0;
    return n;
}

int game_hire(Game *g, int i)
{
    if (game_person_state(g, i) != HS_AVAILABLE) return 0;
    int c = game_person_cost(g, i);
    if (g->gold < c) return 0;
    g->gold -= c;
    g->person[i] = 1;
    game_log(g, LOG_PERSON, L"Приглашён деятель: %ls — %ls", PEOPLE[i].name, PEOPLE[i].role);
    game_check_achievements(g);
    return 1;
}

/* ---------- события ---------- */
const Event *game_event_ptr(const QItem *q)
{
    if (q->type == Q_HIST) return &HIST_EVENTS[q->idx];
    if (q->type == Q_RAND) return &RAND_EVENTS[q->idx];
    return 0;
}

int game_pending(const Game *g, QItem *out)
{
    if (g->qpos >= g->qn) return 0;
    if (out) *out = g->queue[g->qpos];
    return 1;
}

static void push_q(Game *g, int type, int idx)
{
    if (g->qn < (int)(sizeof g->queue / sizeof g->queue[0])) {
        g->queue[g->qn].type = type;
        g->queue[g->qn].idx = idx;
        g->qn++;
    }
}

static void queue_period(Game *g, int eraChanged)
{
    g->qn = g->qpos = 0;
    if (eraChanged) push_q(g, Q_ERA, g->era);
    int next = game_next_year(g);
    int found[32], nf = 0;
    for (int i = 0; i < HIST_COUNT && nf < 32; i++)
        if (!g->histDone[i] && HIST_EVENTS[i].year >= g->year && HIST_EVENTS[i].year < next)
            found[nf++] = i;
    for (int a = 1; a < nf; a++) {               /* по году */
        int v = found[a], b = a - 1;
        while (b >= 0 && HIST_EVENTS[found[b]].year > HIST_EVENTS[v].year) { found[b + 1] = found[b]; b--; }
        found[b + 1] = v;
    }
    int histHere = nf;
    for (int i = 0; i < nf; i++) push_q(g, Q_HIST, found[i]);

    /* случайное событие: чаще, когда исторических нет */
    int chance = histHere == 0 ? 75 : histHere == 1 ? 40 : 20;
    if (g->era == 6) chance = 0;
    if (rnd_n(g, 100) < chance) {
        int cand[MAX_RAND], w[MAX_RAND], nc = 0, sum = 0;
        for (int i = 0; i < RAND_COUNT; i++) {
            if (i == g->lastRand) continue;
            if (g->year < RAND_EVENTS[i].from || g->year > RAND_EVENTS[i].to) continue;
            cand[nc] = i;
            w[nc] = 6 / (1 + g->randSeen[i]) + 1;
            sum += w[nc++];
        }
        if (nc) {
            int r = rnd_n(g, sum);
            for (int i = 0; i < nc; i++) { if (r < w[i]) { push_q(g, Q_RAND, cand[i]); g->lastRand = cand[i]; break; } r -= w[i]; }
        }
    }
}

static int choice_affordable(const Game *g, const Event *e, int ch)
{
    const Choice *c = &e->ch[ch];
    double k = e->year == 0 ? era_scale(g->era) : 1.0;
    if (c->reqi >= 0 && !g->tech[c->reqi]) return 0;
    if (c->dGold < 0 && g->gold + c->dGold * k < 0) return 0;
    /* потеря знаний и производства не блокирует выбор: запас просто уходит в ноль */
    return 1;
}

int game_choice_ok(const Game *g, const Event *e, int ch)
{
    if (ch < 0 || ch >= e->nch) return 0;
    if (choice_affordable(g, e, ch)) return 1;
    /* если по карману ничего, доступны варианты без условий: казна уйдёт в ноль */
    if (e->ch[ch].reqi >= 0) return 0;
    for (int c = 0; c < e->nch; c++)
        if (choice_affordable(g, e, c)) return 0;
    return 1;
}

static double war_factor(const Game *g, int severity)
{
    double def = game_def(g);
    double mit = def / (def + 30.0 + g->era * 18.0);
    double harm = 1.0 + DIFFICULTIES[g->difficulty].harmPct / 100.0;
    return severity / 100.0 * (1.0 - 0.75 * mit) * harm;
}

void game_choose(Game *g, int ch, Outcome *o)
{
    QItem q;
    memset(o, 0, sizeof *o);
    if (!game_pending(g, &q) || (q.type != Q_HIST && q.type != Q_RAND)) return;
    const Event *e = game_event_ptr(&q);
    if (!game_choice_ok(g, e, ch)) return;
    const Choice *c = &e->ch[ch];
    double k = q.type == Q_RAND ? era_scale(g->era) : 1.0;
    double harm = 1.0 + DIFFICULTIES[g->difficulty].harmPct / 100.0;
    int first = 1;
    wchar_t *buf = o->text;
    const int n = (int)(sizeof o->text / sizeof o->text[0]);

    o->popBefore = g->pop;
    long dg = lround(c->dGold * k), ds = lround(c->dSci * k), di = lround(c->dInd * k);
    g->gold += dg; if (g->gold < 0) g->gold = 0;
    g->sci  += ds; if (g->sci  < 0) g->sci  = 0;
    g->ind  += di; if (g->ind  < 0) g->ind  = 0;
    g->stab = clampd(g->stab + c->dStab * (c->dStab < 0 ? harm : 1.0), 0, 100);
    g->cult += c->dCult; if (g->cult < 0) g->cult = 0;
    g->eco  = clampd(g->eco + c->dEco * (c->dEco < 0 ? harm : 1.0), 0, 100);
    cat_delta(buf, n, L"Казна", dg, &first);
    cat_delta(buf, n, L"Знания", ds, &first);
    cat_delta(buf, n, L"Производство", di, &first);
    cat_delta(buf, n, L"Стабильность", lround(c->dStab * (c->dStab < 0 ? harm : 1.0)), &first);
    cat_delta(buf, n, L"Культура", c->dCult, &first);
    cat_delta(buf, n, L"Экология", lround(c->dEco * (c->dEco < 0 ? harm : 1.0)), &first);

    if (c->popPct) {
        int harmful = e->kind == K_WAR || e->kind == K_DISASTER;
        double pct = c->popPct < 0 && harmful ? c->popPct * harm : c->popPct;
        g->pop *= 1.0 + pct / 100.0;
    }
    if (c->war || c->indLoss) {
        double f = war_factor(g, c->war);
        double popLoss = g->pop * f;
        g->pop -= popLoss;
        g->popLostWar += popLoss;
        if (c->indLoss) {
            double fi = war_factor(g, c->indLoss);
            g->ind *= 1.0 - clampd(fi, 0, 0.95);
        }
        g->gold *= 1.0 - clampd(f * 0.5, 0, 0.9);
        if (c->war) g->wars++;
    }
    if (g->pop < 50) g->pop = 50;
    o->popAfter = g->pop;
    if (lround(o->popAfter - o->popBefore) != 0) {
        wchar_t tmp[120], a[32];
        double d = o->popAfter - o->popBefore;
        fmt_pop(fabs(d), a, 32);
        swprintf(tmp, 120, L"%lsНаселение %ls%.1f%% (%ls%ls)", first ? L"" : L" · ", d < 0 ? L"\x2212" : L"+",
                 fabs(d) / o->popBefore * 100.0, d < 0 ? L"\x2212" : L"+", a);
        for (wchar_t *p = tmp; *p; p++) if (*p == L'.') *p = L',';
        cat(buf, n, tmp); first = 0;
    }
    if (c->modName && g->modN < MAX_MODS) {
        Mod *m = &g->mods[g->modN++];
        m->fx = c->mod;
        m->until = c->modTurns ? g->turn + 1 + c->modTurns : 0;
        wcsncpy(m->name, c->modName, MOD_NAME - 1);
        m->name[MOD_NAME - 1] = 0;
        wchar_t tmp[120];
        swprintf(tmp, 120, L"%lsНовый курс: «%ls»", first ? L"" : L" · ", c->modName);
        cat(buf, n, tmp); first = 0;
    }
    if (c->granti >= 0 && !g->tech[c->granti]) {
        unlock_tech(g, c->granti);
        wchar_t tmp[120];
        swprintf(tmp, 120, L"%lsОткрытие: %ls", first ? L"" : L" · ", TECHS[c->granti].name);
        cat(buf, n, tmp); first = 0;
    }
    g->flags |= (unsigned)c->setFlag;
    if (first) cat(buf, n, L"Всё осталось как было.");

    if (q.type == Q_HIST) g->histDone[q.idx] = 1;
    else if (g->randSeen[q.idx] < 250) g->randSeen[q.idx]++;
    game_log(g, c->war ? LOG_WAR : LOG_EVENT, L"%ls — %ls", e->title, c->label);
    if (q.type == Q_HIST && g->logN > 0) g->logs[g->logN - 1].year = e->year;
    g->qpos++;
    game_check_achievements(g);
}

void game_ack(Game *g)
{
    if (g->qpos < g->qn) g->qpos++;
}

static void record_history(Game *g)
{
    if (g->histN >= MAX_TURNS) return;
    int i = g->histN++;
    g->hYear[i] = (float)g->year;
    g->hPop[i]  = (float)g->pop;
    g->hGold[i] = (float)g->gold;
    g->hSci[i]  = (float)g->sci;
    g->hInd[i]  = (float)g->ind;
    g->hStab[i] = (float)g->stab;
    g->hEco[i]  = (float)g->eco;
    g->hTech[i] = (float)game_tech_count(g);
    g->hCult[i] = (float)g->cult;
}

void game_new(Game *g, int difficulty, unsigned seed)
{
    data_init();
    memset(g, 0, sizeof *g);
    g->version = GAME_VERSION;
    g->difficulty = difficulty < 0 || difficulty >= DIFFICULTY_COUNT ? 1 : difficulty;
    g->turn = 0;
    g->year = TURN_YEARS[0];
    g->era = 1;
    g->pop = 450;
    g->gold = 30;
    g->sci = 16;
    g->ind = 10;
    g->stab = 55;
    g->eco = 96;
    g->quizCur = -1;
    g->quizLastAns = -1;
    g->lastRand = -1;
    g->rng = seed ? seed : 0x1234567u;
    g->popPeak = g->pop;
    record_history(g);
    game_log(g, LOG_ERA, L"Начало летописи: %ls", ERAS[0].name);
    queue_period(g, 1);
}

void game_end_turn(Game *g)
{
    if (g->over) return;
    Income inc;
    game_income(g, &inc);
    int next = game_next_year(g);
    int years = next - g->year;

    g->gold += inc.total[R_GOLD];
    g->sci  += inc.total[R_SCI];
    g->ind  += inc.total[R_IND];
    g->cult += inc.total[R_CULT];
    if (g->cult < 0) g->cult = 0;
    g->stab += (inc.stabTarget - g->stab) * 0.25;
    g->stab = clampd(g->stab, 0, 100);
    g->eco += inc.total[R_ECO] + (100.0 - g->eco) * 0.1;
    g->eco = clampd(g->eco, 0, 100);
    g->pop *= pow(1.0 + inc.growYear, years);
    if (g->pop > g->popPeak) g->popPeak = g->pop;

    /* устаревшие временные модификаторы убираем */
    int w = 0;
    for (int m = 0; m < g->modN; m++)
        if (g->mods[m].until == 0 || g->mods[m].until > g->turn + 1) g->mods[w++] = g->mods[m];
    g->modN = w;

    g->turn++;
    g->quizCur = -1;
    g->quizAnswered = 0;
    g->quizLastAns = -1;

    if (g->turn >= TURN_COUNT) {
        g->year = END_YEAR;
        record_history(g);
        g->over = 1;
        g->qn = g->qpos = 0;
        push_q(g, Q_FINAL, 0);
        game_log(g, LOG_ERA, L"Летопись доведена до %d года", END_YEAR);
        game_check_achievements(g);
        return;
    }
    g->year = TURN_YEARS[g->turn];
    if (g->year == 1945) g->pop1945 = g->pop;
    record_history(g);
    int newEra = era_of_year(g->year);
    int changed = newEra != g->era;
    g->era = newEra;
    if (changed) game_log(g, LOG_ERA, L"Новая эпоха: %ls", ERAS[newEra - 1].name);
    queue_period(g, changed);
    game_check_achievements(g);
}

/* ---------- викторина ---------- */
int game_quiz_reward(const Game *g) { return (int)lround(4 + 6 * era_scale(g->era)); }

int game_quiz_pick(Game *g)
{
    if (g->quizCur >= 0) return g->quizCur;
    if (g->quizAnswered) return -1;
    int cand[MAX_QUIZ], nc = 0;
    for (int pass = 0; pass < 3 && nc == 0; pass++)
        for (int i = 0; i < QUIZ_COUNT; i++) {
            if (g->quizDone[i]) continue;
            int e = QUIZ[i].era;
            if (pass == 0 && e != g->era) continue;
            if (pass == 1 && e > g->era) continue;
            cand[nc++] = i;
        }
    if (!nc) return -1;
    int q = cand[rnd_n(g, nc)];
    if (q == g->quizLastPick && nc > 1) q = cand[(rnd_n(g, nc - 1) + 1) % nc];
    g->quizCur = q;
    g->quizLastPick = q;
    for (int i = 0; i < 4; i++) g->quizPerm[i] = i;
    for (int i = 3; i > 0; i--) { int j = rnd_n(g, i + 1), t = g->quizPerm[i]; g->quizPerm[i] = g->quizPerm[j]; g->quizPerm[j] = t; }
    return q;
}

int game_quiz_answer(Game *g, int shown)
{
    int q = g->quizCur;
    if (q < 0 || g->quizAnswered || shown < 0 || shown > 3) return 0;
    int ok = g->quizPerm[shown] == QUIZ[q].correct;
    g->quizDone[q] = ok ? 1 : 2;
    g->quizAnswered = 1;
    g->quizLastAns = shown;
    if (ok) {
        g->quizRight++;
        g->sci += game_quiz_reward(g);
        g->cult += 2;
        game_log(g, LOG_QUIZ, L"Летописец: верный ответ (+%d знаний)", game_quiz_reward(g));
    } else {
        g->quizWrong++;
    }
    game_check_achievements(g);
    return ok;
}

/* ---------- достижения ---------- */
static int all_era_techs(const Game *g, int era)
{
    for (int t = 0; t < TECH_COUNT; t++)
        if (TECHS[t].era == era && !g->tech[t]) return 0;
    return 1;
}

void game_check_achievements(Game *g)
{
    int got[MAX_ACH] = {0};
    int manuf = game_has_proj(g, "naliboki_glass") + game_has_proj(g, "urechye") +
                game_has_proj(g, "slutsk_persiyarnya") + game_has_proj(g, "grodno_manufactories");
    got[0]  = game_has_tech(g, "printing");
    got[1]  = all_era_techs(g, 1);
    got[2]  = game_has_proj(g, "kamenets_tower");
    got[3]  = game_has_tech(g, "rocketry");
    got[4]  = manuf >= 3;
    got[5]  = game_has_tech(g, "electricity") && game_has_tech(g, "goelro");
    got[6]  = game_has_proj(g, "maz") && game_has_proj(g, "mtz") && game_has_proj(g, "belaz");
    got[7]  = game_has_tech(g, "space") && game_has_tech(g, "satellite");
    got[8]  = game_has_tech(g, "hitech_park") && game_has_tech(g, "digital_economy");
    got[9]  = game_has_proj(g, "belnpp");
    got[10] = g->quizRight >= 25;
    got[11] = game_people_count(g) >= 10;
    got[12] = game_proj_count(g) >= 25;
    got[13] = g->over && g->eco >= 60;
    got[14] = g->pop1945 > 6000;
    got[15] = game_tech_count(g) >= 80;
    got[16] = g->cult >= 4000;
    got[17] = g->over && g->stab >= 70;
    got[18] = g->aheadOfTime;
    got[19] = g->erasBuilt == 0xFFu;
    for (int a = 0; a < ACH_COUNT; a++)
        if (got[a] && !g->ach[a]) {
            g->ach[a] = 1;
            game_log(g, LOG_ACH, L"Достижение: «%ls»", ACHIEVEMENTS[a].name);
        }
}

/* ---------- итог ---------- */
static int count_branch(const Game *g, int br)
{
    int n = 0;
    for (int t = 0; t < TECH_COUNT; t++) n += (g->tech[t] && TECHS[t].branch == br) ? 1 : 0;
    return n;
}

void game_score(const Game *g, Score *s)
{
    memset(s, 0, sizeof *s);
    int ach = 0;
    for (int a = 0; a < ACH_COUNT; a++) ach += g->ach[a];
    s->techs  = game_tech_count(g) * 10;
    s->projs  = game_proj_count(g) * 12;
    s->people = game_people_count(g) * 8;
    s->pop    = (int)lround(g->pop / 40.0);
    s->stab   = (int)lround(g->stab * 2);
    s->eco    = (int)lround(g->eco * 2);
    s->cult   = (int)lround(g->cult / 10.0);
    s->ach    = ach * 25;
    s->quiz   = g->quizRight * 5;
    s->total  = s->techs + s->projs + s->people + s->pop + s->stab + s->eco + s->cult + s->ach + s->quiz;
    s->rank = s->total >= 3400 ? L"Страна-лаборатория" :
              s->total >= 2800 ? L"Технологический узел Европы" :
              s->total >= 2200 ? L"Сборочный цех с собственным умом" :
              s->total >= 1600 ? L"Упорный край мастеров" :
                                 L"Хроника трудных времён";

    wchar_t *e = s->epilogue;
    const int n = (int)(sizeof s->epilogue / sizeof s->epilogue[0]);
    wchar_t tmp[700], pop[32];
    e[0] = 0;
    fmt_pop(g->pop, pop, 32);
    swprintf(tmp, 700, L"К %d году в стране живёт %ls человек; в реальной истории — около 9,1 млн. ", END_YEAR, pop);
    cat(e, n, tmp);
    if (g->pop > 9600) cat(e, n, L"Медицина, жильё и стабильность сделали своё дело: демографический спад обошёл вашу Беларусь стороной.\n\n");
    else if (g->pop < 8000) cat(e, n, L"Войны, эпидемии и отъезды оставили глубокий след: людей меньше, чем могло бы быть.\n\n");
    else cat(e, n, L"Демография близка к исторической.\n\n");

    int digi = count_branch(g, BR_DIGI);
    if (digi >= 14) cat(e, n, L"Минск — одна из IT-столиц Европы: от ЭВМ «Минск» и ЕС ЭВМ до мобильных приложений и искусственного интеллекта прослеживается одна непрерывная инженерная школа. ");
    else if (digi >= 8) cat(e, n, L"Цифровая отрасль стала заметной частью экономики: программисты и вычислители продолжают традицию «Минска» и ЕС ЭВМ. ");
    else cat(e, n, L"Цифровая революция прошла по касательной: страна осталась прежде всего промышленной. ");
    if (game_has_proj(g, "maz") || game_has_proj(g, "mtz") || game_has_proj(g, "belaz"))
        cat(e, n, L"Тракторы «Беларус» и самосвалы БелАЗ работают на всех континентах. ");
    if (game_has_tech(g, "satellite")) cat(e, n, L"Свои спутники снимают Землю, а белорусы снова летают в космос. ");
    if (game_has_tech(g, "nuclear_power")) cat(e, n, L"Атомная станция в Островце дала стране собственную энергию. ");
    cat(e, n, L"\n\n");

    if (g->eco >= 70) cat(e, n, L"Природа сохранена: Беловежская пуща, Полесье и Ельня остаются заповедными уголками Европы, несмотря на Чернобыль и мелиорацию. ");
    else if (g->eco >= 45) cat(e, n, L"Природа пострадала от мелиорации, химии и Чернобыля, но восстанавливается. ");
    else cat(e, n, L"За индустриальный рывок природа заплатила высокую цену: осушенные болота, солеотвалы, загрязнённые реки. ");
    if (g->cult >= 4000) cat(e, n, L"Культура и память сильны: от креста Евфросинии до книг Скорины и «Нашай Нівы» — всё это живёт. ");
    else cat(e, n, L"Многое из культурного наследия утрачено, и его ещё предстоит восстановить. ");
    if (g->stab >= 70) cat(e, n, L"Общество спокойно и уверено в завтрашнем дне.");
    else if (g->stab >= 45) cat(e, n, L"Общество проходит через напряжение, но держится.");
    else cat(e, n, L"Общество расколото и устало от кризисов.");
    cat(e, n, L"\n\nСудьба страны складывается из тысяч решений, и технологии — лишь один из объективов, через который на неё можно смотреть.");
}

/* ---------- сохранение ---------- */
int game_save(const Game *g, FILE *f)
{
    const char magic[4] = { 'B', 'Y', 'T', 'G' };
    unsigned sz = (unsigned)sizeof *g;
    if (fwrite(magic, 1, 4, f) != 4) return 0;
    if (fwrite(&sz, sizeof sz, 1, f) != 1) return 0;
    return fwrite(g, sizeof *g, 1, f) == 1;
}

int game_load(Game *g, FILE *f)
{
    char magic[4];
    unsigned sz = 0;
    Game tmp;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "BYTG", 4) != 0) return 0;
    if (fread(&sz, sizeof sz, 1, f) != 1 || sz != sizeof tmp) return 0;
    if (fread(&tmp, sizeof tmp, 1, f) != 1) return 0;
    if (tmp.version != GAME_VERSION) return 0;
    data_init();
    *g = tmp;
    return 1;
}

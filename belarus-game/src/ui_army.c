/* Вкладка «Армия»: войско, набеги и оборона, текущая война, походы за земли, освобождение 1941–1944. */
#include "ui.h"
#include "war.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void decimal_comma(wchar_t *s)
{
    for (; *s; s++) if (*s == L'.' && s[1] >= L'0' && s[1] <= L'9') *s = L',';
}

void unit_badge(int u, int x, int y, int w, int h, COLORREF border)
{
    d_rrect(x, y, w, h, SC(6), mix(P.surface2, P.redSoft, 90), border);
    int idx = u >= 0 && u < UNIT_COUNT ? art_unit_index(UNIT_TYPES[u].id) : -1;
    if (idx >= 0) { art_unit_draw(idx, x + 1, y + 1, w - 2, h - 2); return; }
    int s = (w < h ? w : h) * 5 / 10;
    d_icon(u >= 0 && UNIT_TYPES[u].def > UNIT_TYPES[u].atk ? IC_DEF : IC_WAR, x + (w - s) / 2, y + (h - s) / 2, s, P.red);
}

/* Доход земли сейчас (с учётом эпохи и наследия) — для описания. */
static void province_fx_now(int p, Fx *out)
{
    double k = era_scale(G.era) / era_scale(PROVINCES[p].era) * (G.provOwned[p] == 2 ? 0.3 : 1.0);
    for (int r = 0; r < R_COUNT; r++) out->v[r] = (int)lround(PROVINCES[p].fx.v[r] * k);
}

static void stat_box(int x, int y, int w, int h, int icon, const wchar_t *label, const wchar_t *value, COLORREF vc, int hoverArg)
{
    d_rrect(x, y, w, h, SC(6), tab_card_fill(), P.line);
    d_icon(icon, x + SC(10), y + SC(10), SC(16), P.muted);
    d_ellipsis(x + SC(32), y + SC(9), w - SC(40), label, F_TINY, P.muted);
    d_ellipsis(x + SC(10), y + SC(30), w - SC(20), value, F_H2, vc);
    hit(x, y, w, h, A_HOVER_STAT, hoverArg);
}

/* Цветная плашка с заголовком и текстом; возвращает высоту. */
static int notice(int x, int y, int w, COLORREF accent, int icon, const wchar_t *title, const wchar_t *text)
{
    int tw = w - SC(44);
    int h = SC(12) + d_font_h(F_SMALL_B) + SC(4) + d_wrap(0, 0, tw, text, F_SMALL, 0, 0) + SC(12);
    d_rrect(x, y, w, h, SC(6), mix(P.surface, accent, 26), accent);
    d_icon(icon, x + SC(12), y + SC(12), SC(18), accent);
    SetTextCharacterExtra(d_dc(), 1);
    d_text(x + SC(38), y + SC(12), title, F_SMALL_B, accent);
    SetTextCharacterExtra(d_dc(), 0);
    d_wrap(x + SC(38), y + SC(12) + d_font_h(F_SMALL_B) + SC(4), tw, text, F_SMALL, P.ink, 1);
    return h;
}

static void cost_line(int x, int y, int gold, int ind)
{
    wchar_t a[32], b[32];
    fmt_num(gold, a, 32); fmt_num(ind, b, 32);
    d_icon(IC_GOLD, x, y + SC(1), SC(16), P.gold);
    d_text(x + SC(20), y, a, F_SMALL_B, G.gold >= gold ? P.ink : P.red);
    int ox = x + SC(26) + d_text_w(a, F_SMALL_B);
    d_icon(IC_IND, ox, y + SC(1), SC(16), mix(P.gold, P.red, 90));
    d_text(ox + SC(20), y, b, F_SMALL_B, G.ind >= ind ? P.ink : P.red);
}

/* ---------- войска ---------- */
static int unit_shown(int u)
{
    const UnitType *t = &UNIT_TYPES[u];
    if (u < MAX_UNITS && G.units[u] > 0) return 1;
    if (t->era == G.era + 1) return 1;                         /* что появится дальше */
    return t->era <= G.era && t->era >= G.era - 1;
}

static void unit_card(int u, int x, int y, int w, int h)
{
    const UnitType *t = &UNIT_TYPES[u];
    int st = war_unit_state(&G, u), have = u < MAX_UNITS ? G.units[u] : 0;
    int hv = hover(A_RECRUIT, u) || hover(A_DISBAND, u) || hover(A_HOVER_UNIT, u);
    COLORREF fill = have ? mix(tab_card_fill(), P.red, 20) : tab_card_fill();
    d_rrect(x, y, w, h, SC(6), fill, hv ? P.muted : P.line);
    hit(x, y, w, h, A_HOVER_UNIT, u);
    int bs = h - SC(20);
    if (bs > SC(84)) bs = SC(84);
    unit_badge(u, x + SC(10), y + SC(10), bs, bs, have ? P.red : P.line);
    int px = x + bs + SC(20), pw = w - bs - SC(30);
    int faded = st != US_AVAILABLE;
    wchar_t buf[300], era[32];
    swprintf(era, 32, L"эпоха %ls", roman(t->era));
    int ew = d_text_w(era, F_TINY) + SC(14);
    d_ellipsis(px, y + SC(8), pw - ew - SC(6), t->name, F_H3, faded ? mix(P.ink, P.muted, 150) : P.ink);
    w_pill(x + w - SC(10) - ew, y + SC(10), era, P.muted, P.surface2);
    double k = war_unit_eff(&G, u) * war_tech_mult(&G);
    swprintf(buf, 300, L"Атака %.1f · оборона %.1f · содержание %.1f казны за ход", t->atk * k, t->def * k, t->upkeep / 10.0);
    decimal_comma(buf);
    d_ellipsis(px, y + SC(31), pw, buf, F_SMALL, faded ? P.muted : P.ink);
    d_ellipsis(px, y + SC(50), pw, t->desc, F_TINY, P.muted);
    int by = y + h - SC(36);
    if (st == US_AVAILABLE) {
        int cg = war_unit_cost_gold(&G, u), ci = war_unit_cost_ind(&G, u);
        cost_line(px, by + SC(5), cg, ci);
        int full = war_army_size(&G) >= war_army_cap(&G);
        int ok = !G.over && !full && G.gold >= cg && G.ind >= ci;
        int bx = x + w - SC(10) - SC(104);
        w_button(bx, by, SC(104), SC(30), L"Набрать", B_PRIMARY, ok, A_RECRUIT, u);
        if (have) {
            w_button(bx - SC(40), by, SC(34), SC(30), L"\x2212", B_SECONDARY, !G.over, A_DISBAND, u);
            swprintf(buf, 300, L"в строю: %d", have);
            d_text_r(bx - SC(48), by + SC(7), buf, F_SMALL_B, P.red);
        } else if (war_unit_eff(&G, u) < 0.999) {
            swprintf(buf, 300, L"устарели: ×%.2f", war_unit_eff(&G, u));
            decimal_comma(buf);
            d_text_r(bx - SC(8), by + SC(7), buf, F_TINY, P.muted);
        }
    } else {
        if (st == US_LOCKED) swprintf(buf, 300, L"Нужна технология «%ls»", TECHS[t->techi].name);
        else swprintf(buf, 300, L"Появится в эпоху %ls (с %d года)", roman(t->era), ERAS[t->era - 1].from);
        int bx = x + w - SC(10);
        if (have) {
            w_button(bx - SC(34), by, SC(34), SC(30), L"\x2212", B_SECONDARY, !G.over, A_DISBAND, u);
            wchar_t s2[32];
            swprintf(s2, 32, L"в строю: %d", have);
            d_text_r(bx - SC(42), by + SC(7), s2, F_SMALL_B, P.red);
            bx -= SC(42) + d_text_w(s2, F_SMALL_B) + SC(8);
        }
        d_ellipsis(px, by + SC(7), bx - px, buf, F_SMALL, P.muted);
    }
}

/* ---------- земли ---------- */
static int prov_rank(int p)
{
    int st = war_province_state(&G, p);
    if (G.warKind == WAR_CONQUEST && G.warTarget == p) return 0;
    return st == PV_AVAILABLE ? 1 : st == PV_OWNED ? 2 : st == PV_FUTURE ? 3 : st == PV_LEGACY ? 4 : 5;
}

/* та же земля в другую эпоху уже ваша — отдельной карточки не нужно */
static int prov_dup(int p) { return !G.provOwned[p] && war_group_owner(&G, p) >= 0; }

static int prov_shown(int p)
{
    int st = war_province_state(&G, p);
    if (st == PV_CLOSED || prov_dup(p)) return 0;
    if (st == PV_FUTURE) return PROVINCES[p].from <= G.year + 160;
    return 1;
}

static int province_card(int p, int x, int y, int w, int draw)
{
    const Province *P0 = &PROVINCES[p];
    int st = war_province_state(&G, p);
    int target = G.warKind == WAR_CONQUEST && G.warTarget == p;
    int tw = w - SC(24);
    int hd = d_wrap(0, 0, tw, P0->desc, F_TINY, 0, 0);
    int h = SC(10) + d_font_h(F_H3) + SC(4) + SC(18) + SC(20) + SC(20) + hd + SC(8) + SC(38);
    if (!draw) return h;
    COLORREF accent = target ? P.red : st == PV_OWNED ? P.gold : st == PV_AVAILABLE ? P.blue : P.line;
    COLORREF fill = st == PV_OWNED ? mix(tab_card_fill(), P.gold, 36) : target ? mix(tab_card_fill(), P.red, 26) : tab_card_fill();
    d_rrect(x, y, w, h, SC(6), fill, accent);
    int tx = x + SC(12), ty = y + SC(10);
    const wchar_t *stn = target ? L"идёт война" : st == PV_OWNED ? L"ваша земля" : st == PV_AVAILABLE ? L"можно в поход" :
                         st == PV_LEGACY ? L"наследие" : L"позже";
    int sw = d_text_w(stn, F_TINY) + SC(14);
    d_icon(st == PV_OWNED || st == PV_LEGACY ? IC_FLAG : IC_WAR, tx, ty + SC(3), SC(16), accent == P.line ? P.muted : accent);
    d_ellipsis(tx + SC(22), ty, tw - sw - SC(30), P0->name, F_H3, st == PV_FUTURE ? P.muted : P.ink);
    w_pill(x + w - SC(12) - sw, ty + SC(3), stn, target || st == PV_AVAILABLE || st == PV_OWNED ? P.onAccent : P.muted,
           target ? P.red : st == PV_AVAILABLE ? P.blue : st == PV_OWNED ? P.gold : P.surface2);
    ty += d_font_h(F_H3) + SC(4);
    wchar_t buf[320], fx[200], pop[32];
    swprintf(buf, 320, L"%ls: %ls · окно похода %d–%d", st == PV_OWNED || st == PV_LEGACY ? L"прежний владелец" : L"владелец",
             P0->owner, P0->from, P0->to);
    d_ellipsis(tx, ty, tw, buf, F_TINY, P.muted);
    ty += SC(18);
    Fx now;
    province_fx_now(p, &now);
    fx_describe(&now, fx, 200);
    fmt_pop(P0->popK, pop, 32);
    if (st == PV_OWNED || st == PV_LEGACY) swprintf(buf, 320, L"Доход за ход: %ls", fx);
    else swprintf(buf, 320, L"Даст: %ls · +%ls жителей", fx, pop);
    d_ellipsis(tx, ty, tw, buf, F_SMALL_B, st == PV_OWNED ? P.gold : P.ink);
    ty += SC(20);
    double A = war_attack_power(&G, WAR_CONQUEST);
    int E = war_province_strength(&G, p);
    if (st == PV_AVAILABLE || target || st == PV_FUTURE) {
        int eta = war_eta(&G, WAR_CONQUEST, p), bl = war_battles_left(&G, WAR_CONQUEST, p);
        wchar_t etas[96];
        if (A <= 0) swprintf(etas, 96, L"нужно войско");
        else if (eta >= 99 && st != PV_FUTURE && bl <= 2) swprintf(etas, 96, L"не успеть: окно закроется в %d г.", P0->to);
        else if (eta >= 99) swprintf(etas, 96, L"победа маловероятна");
        else swprintf(etas, 96, L"≈ %d %ls до победы", eta, eta == 1 ? L"ход" : eta < 5 ? L"хода" : L"ходов");
        if (st == PV_FUTURE) swprintf(buf, 320, L"Оборона противника %d · ваша сила %ld · %ls", E, lround(A), etas);
        else swprintf(buf, 320, L"Оборона %d · ваша сила %ld · %ls · боёв до %d г.: %d", E, lround(A), etas, P0->to, bl);
        d_ellipsis(tx, ty, tw, buf, F_SMALL, eta <= 3 ? P.green : eta >= 99 ? P.red : P.ink);
    } else if (st == PV_OWNED) {
        int q = -1;
        for (int k = 0; k < PROVINCE_COUNT; k++)
            if (k != p && !strcmp(PROVINCES[k].group, P0->group) && PROVINCES[k].from > G.year && (q < 0 || PROVINCES[k].from < PROVINCES[q].from)) q = k;
        if (q >= 0) {
            swprintf(buf, 320, L"%ls попытается отнять её в %d году — держите войско и крепости (осада %d).",
                     PROVINCES[q].owner, PROVINCES[q].from, war_province_strength(&G, q));
            d_ellipsis(tx, ty, tw, buf, F_SMALL, P.red);
        } else d_ellipsis(tx, ty, tw, L"Земля ваша: доход растёт с каждой эпохой.", F_SMALL, P.muted);
    } else {
        d_ellipsis(tx, ty, tw, L"После разделов 1795 года — 30% прежнего дохода через торговые связи.", F_SMALL, P.muted);
    }
    ty += SC(20);
    ty += d_wrap(tx, ty, tw, P0->desc, F_TINY, P.muted, 1) + SC(8);
    if (target) {
        swprintf(buf, 320, L"Поход идёт: %d%%", G.warProgress < 0 ? 0 : G.warProgress);
        d_text(tx, ty + SC(7), buf, F_SMALL_B, P.red);
    } else if (st == PV_AVAILABLE) {
        const wchar_t *why = war_cannot_declare(&G, WAR_CONQUEST, p);
        w_button(x + w - SC(12) - SC(170), ty, SC(170), SC(30), L"Объявить поход", B_DANGER, why == 0, A_DECLARE, p);
        if (why) d_ellipsis(tx, ty + SC(7), tw - SC(180), why, F_TINY, P.muted);
    } else if (st == PV_FUTURE) {
        swprintf(buf, 320, L"Поход станет возможен в %d году", P0->from);
        d_ellipsis(tx, ty + SC(7), tw, buf, F_SMALL, P.muted);
    }
    return h;
}

/* ---------- вкладка ---------- */
int tab_army(int x, int y, int w)
{
    int y0 = y;
    Army a;
    war_army(&G, &a);
    int cap = war_army_cap(&G);
    double defp = war_defense_power(&G);
    wchar_t buf[600], v[64];

    /* 1. сводка */
    int cols = w >= SC(560) ? 4 : 2, gap = SC(8), cw = (w - gap * (cols - 1)) / cols, ch = SC(66);
    for (int i = 0; i < 4; i++) {
        int cx = x + (i % cols) * (cw + gap), cy = y + (i / cols) * (ch + gap);
        switch (i) {
        case 0: swprintf(v, 64, L"%d / %d", a.units, cap); stat_box(cx, cy, cw, ch, IC_POP, L"Войско, соединений", v, P.ink, 20); break;
        case 1: swprintf(v, 64, L"%ld", lround(a.attack)); stat_box(cx, cy, cw, ch, IC_WAR, L"Сила наступления", v, P.red, 21); break;
        case 2: swprintf(v, 64, L"%ld", lround(defp)); stat_box(cx, cy, cw, ch, IC_DEF, L"Оборона с крепостями", v, P.blue, 22); break;
        case 3: {
            double up = a.upkeep / 10.0 * (G.warKind != WAR_NONE ? 1.5 : 1.0);
            swprintf(v, 64, up > 0 ? L"\x2212%.1f" : L"0", up);
            decimal_comma(v);
            stat_box(cx, cy, cw, ch, IC_GOLD, L"Содержание за ход", v, up > 0 ? P.gold : P.muted, 23);
            break;
        }
        }
    }
    y += ((4 + cols - 1) / cols) * (ch + gap) + SC(4);
    swprintf(buf, 600, L"Военные технологии усиливают войско: ×%.2f. Войска двух эпох назад и старше устаревают. "
                       L"Предел войска растёт с населением. Четверть обороны войска идёт в общую оборону — она смягчает удары войн в событиях.",
             war_tech_mult(&G));
    decimal_comma(buf);
    y += d_wrap(x, y, w, buf, F_SMALL, P.muted, 1) + SC(12);

    /* 2. угроза, оккупация, предупреждение */
    if (G.occupied) {
        int k = war_occupied_count(&G);
        swprintf(buf, 600, L"Оккупировано областей: %d из %d. Доходы ×%.2f, каждый ход гибнут люди, каратели жгут деревни — удары войны сильнее. "
                           L"Освобождайте области сами: сила оккупантов слабеет с каждым годом (сейчас %d). Остальное освободит операция «Багратион» в 1944 году.",
                 k, REGION_COUNT, war_occupation_mult(&G), war_region_strength(&G));
        decimal_comma(buf);
        y += notice(x, y, w, P.red, IC_WAR, L"ОККУПАЦИЯ 1941–1944", buf) + SC(10);
    } else if (war_raid_chance(&G) > 0) {
        double E = war_raid_strength(&G);
        const wchar_t *verdict = defp * 0.8 >= E * 1.3 ? L"вы отобьётесь наверняка." :
                                 defp * 1.2 < E * 0.7 ? L"враг сильнее — наберите войска или стройте крепости." :
                                                         L"исход боя не предрешён.";
        swprintf(buf, 600, L"Каждый ход с вероятностью %ld%% на границу нападают. Сила врага около %ld (от %ld до %ld), ваша оборона %ld — %ls "
                           L"Победа даёт трофеи, поражение — потери казны, производства и людей.",
                 lround(war_raid_chance(&G) * 100), lround(E), lround(E * 0.7), lround(E * 1.3), lround(defp), verdict);
        y += notice(x, y, w, defp * 1.2 < E * 0.7 ? P.red : P.gold, IC_DEF, L"УГРОЗА НАБЕГОВ", buf) + SC(10);
    }
    if (G.year >= 1921 && G.year < 1941) {
        y += notice(x, y, w, P.red, IC_WAR, L"ВПЕРЕДИ 1941 ГОД",
                    L"22 июня 1941 года начнётся Великая Отечественная война, и все шесть областей окажутся под оккупацией. "
                    L"Готовьте армию заранее: стрелковые дивизии и авиация понадобятся, чтобы освобождать области раньше «Багратиона».") + SC(10);
    }

    /* 3. текущая война */
    if (G.warKind != WAR_NONE) {
        int conquest = G.warKind == WAR_CONQUEST;
        const wchar_t *name = conquest ? PROVINCES[G.warTarget].name : REGION_NAMES[G.warTarget];
        double A = war_attack_power(&G, G.warKind), E = war_enemy_power(&G, G.warKind, G.warTarget);
        int eta = war_eta(&G, G.warKind, G.warTarget);
        int tw = w - SC(24);
        wchar_t etas[80], line[600];
        if (eta >= 99) swprintf(etas, 80, L"победа маловероятна");
        else swprintf(etas, 80, L"до победы ≈ %d %ls", eta, eta == 1 ? L"ход" : eta < 5 ? L"хода" : L"ходов");
        swprintf(line, 600, L"Продвижение %d%% · ход войны %d из 10 · ваша сила %ld против %ld · %ls · боёв до %d г.: %d",
                 G.warProgress, G.warTurns, lround(A), lround(E), etas,
                 conquest ? PROVINCES[G.warTarget].to : 1944, war_battles_left(&G, G.warKind, G.warTarget));
        int hl = d_wrap(0, 0, tw, line, F_SMALL_B, 0, 0);
        int hr = G.warReport[0] ? d_wrap(0, 0, tw, G.warReport, F_SMALL, 0, 0) + SC(6) : 0;
        int h = SC(12) + d_font_h(F_H2) + SC(10) + SC(14) + SC(10) + hl + SC(6) + hr + SC(22) + SC(36) + SC(12);
        d_rrect(x, y, w, h, SC(6), mix(P.surface, P.red, 30), P.red);
        int tx = x + SC(12), ty = y + SC(12);
        swprintf(buf, 600, L"%ls: %ls", conquest ? L"Поход" : L"Освобождение", name);
        d_icon(IC_WAR, tx, ty + SC(4), SC(20), P.red);
        d_ellipsis(tx + SC(28), ty, tw - SC(28), buf, F_H2, P.ink);
        ty += d_font_h(F_H2) + SC(10);
        int pr = G.warProgress < 0 ? 0 : G.warProgress > 100 ? 100 : G.warProgress;
        d_rrect(tx, ty, tw, SC(14), SC(7), P.surface2, CLR_INVALID);
        if (pr > 0) d_rrect(tx, ty, tw * pr / 100 > SC(14) ? tw * pr / 100 : SC(14), SC(14), SC(7), P.red, CLR_INVALID);
        ty += SC(14) + SC(10);
        ty += d_wrap(tx, ty, tw, line, F_SMALL_B, G.warProgress < 0 || eta >= 99 ? P.red : P.ink, 1) + SC(6);
        if (hr) ty += d_wrap(tx, ty, tw, G.warReport, F_SMALL, P.muted, 1) + SC(6);
        d_ellipsis(tx, ty, tw, conquest ? L"Бой идёт в конце хода. Во время похода содержание ×1,5, стабильность −1,5 за ход."
                                        : L"Бой идёт в конце хода. Партизанские бригады при освобождении сильнее в полтора раза.",
                   F_TINY, P.muted);
        ty += SC(22);
        w_button(tx, ty, SC(190), SC(32), conquest ? L"Заключить мир" : L"Остановить операцию", B_SECONDARY, !G.over, A_PEACE, 0);
        y += h + SC(14);
    } else if (G.warReport[0]) {
        tab_section(x, &y, L"ПОСЛЕДНИЙ БОЙ");
        y += d_wrap(x, y, w, G.warReport, F_SMALL, P.ink, 1) + SC(4);
        swprintf(buf, 600, L"Побед: %d · поражений: %d · потеряно соединений: %d", G.battlesWon, G.battlesLost, G.unitsLost);
        y += d_wrap(x, y, w, buf, F_TINY, P.muted, 1) + SC(14);
    }

    /* 4. освобождение */
    if (G.occupied || G.liberated) {
        tab_section(x, &y, L"ОСВОБОЖДЕНИЕ БЕЛАРУСИ");
        int rc = w >= SC(560) ? 2 : 1, rw = (w - gap * (rc - 1)) / rc, rh = SC(88);
        for (int r = 0; r < REGION_COUNT; r++) {
            int cx = x + (r % rc) * (rw + gap), cy = y + (r / rc) * (rh + gap);
            int occ = (G.occupied >> r) & 1u, lib = (G.liberated >> r) & 1u;
            int target = G.warKind == WAR_LIBERATION && G.warTarget == r;
            COLORREF ac = target ? P.red : occ ? mix(P.ink, P.red, 120) : P.green;
            d_rrect(cx, cy, rw, rh, SC(6), occ ? mix(tab_card_fill(), P.ink, 28) : mix(tab_card_fill(), P.green, 30), ac);
            d_icon(occ ? IC_WAR : IC_CHECK, cx + SC(10), cy + SC(10), SC(18), ac);
            d_ellipsis(cx + SC(34), cy + SC(8), rw - SC(44), REGION_NAMES[r], F_H3, P.ink);
            swprintf(buf, 600, L"центр — %ls · %ls", CITIES[REGION_CITY[r]].name,
                     target ? L"идёт операция" : occ ? L"под оккупацией" : lib ? L"освобождена своими силами" : L"освобождена");
            d_ellipsis(cx + SC(34), cy + SC(30), rw - SC(44), buf, F_TINY, occ ? P.muted : P.green);
            hit(cx, cy, rw, rh, A_HOVER_REGION, r);
            if (occ && !target) {
                int eta = war_eta(&G, WAR_LIBERATION, r);
                if (eta < 99) swprintf(buf, 600, L"≈ %d %ls", eta, eta == 1 ? L"ход" : eta < 5 ? L"хода" : L"ходов");
                else swprintf(buf, 600, L"до 1944 не успеть");
                d_text(cx + SC(34), cy + rh - SC(26), buf, F_TINY, eta <= 2 ? P.green : P.muted);
                w_button(cx + rw - SC(10) - SC(124), cy + rh - SC(38), SC(124), SC(30), L"Освободить", B_DANGER,
                         war_cannot_declare(&G, WAR_LIBERATION, r) == 0, A_LIBERATE, r);
            }
        }
        y += ((REGION_COUNT + rc - 1) / rc) * (rh + gap) + SC(10);
    }

    /* 5. войска */
    tab_section(x, &y, L"ВОЙСКА");
    {
        int ucols = w >= SC(760) ? 2 : 1, uw = (w - gap * (ucols - 1)) / ucols, uh = SC(104), n = 0;
        for (int u = 0; u < UNIT_COUNT; u++) {
            if (!unit_shown(u)) continue;
            int cx = x + (n % ucols) * (uw + gap), cy = y + (n / ucols) * (uh + gap);
            unit_card(u, cx, cy, uw, uh);
            n++;
        }
        y += ((n + ucols - 1) / ucols) * (uh + gap) + SC(10);
    }

    /* 6. походы */
    int anyProv = 0, closed = 0;
    for (int p = 0; p < PROVINCE_COUNT; p++) { anyProv |= prov_shown(p); closed += war_province_state(&G, p) == PV_CLOSED && !prov_dup(p); }
    if (anyProv) {
        tab_section(x, &y, L"ЗЕМЛИ ДЛЯ ПОХОДОВ");
        y += d_wrap(x, y, w, L"Присоединённая земля сразу даёт жителей и каждый ход — доход, который растёт с эпохой. Окна походов — годы, "
                             L"когда эти земли действительно переходили из рук в руки. В 1795 году, после разделов Речи Посполитой, "
                             L"земли становятся наследием: остаётся 30% дохода.", F_SMALL, P.muted, 1) + SC(10);
        int order[MAX_PROVINCES], n = 0;
        for (int p = 0; p < PROVINCE_COUNT && n < MAX_PROVINCES; p++) if (prov_shown(p)) order[n++] = p;
        for (int i = 1; i < n; i++) {
            int v0 = order[i], j = i - 1;
            while (j >= 0 && (prov_rank(order[j]) > prov_rank(v0) ||
                              (prov_rank(order[j]) == prov_rank(v0) && PROVINCES[order[j]].from > PROVINCES[v0].from))) { order[j + 1] = order[j]; j--; }
            order[j + 1] = v0;
        }
        for (int i = 0; i < n; i++) {
            int h = province_card(order[i], x, y, w, 0);
            if (y + h > U.panelY - SC(40) && y < U.panelY + U.panelH + SC(40)) province_card(order[i], x, y, w, 1);
            y += h + SC(10);
        }
        if (closed) {
            swprintf(buf, 600, L"Время для походов упущено: %d %ls.", closed, closed == 1 ? L"земля" : closed < 5 ? L"земли" : L"земель");
            y += d_wrap(x, y, w, buf, F_TINY, P.muted, 1) + SC(6);
        }
    } else if (G.era >= 4 && !G.occupied && !G.liberated) {
        y += d_wrap(x, y, w, L"Эпоха походов за земли закончилась в XVIII веке. Войско теперь — щит: оно отбивает набеги, "
                             L"смягчает удары войн в событиях и понадобится в 1941–1944 годах.", F_SMALL, P.muted, 1) + SC(10);
    }
    return y - y0 + SC(16);
}

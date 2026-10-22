/* Содержимое вкладок правой панели. Каждая функция рисует с учётом прокрутки и возвращает высоту содержимого. */
#include "ui.h"
#include "rulers.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static COLORREF card_fill(void) { return mix(P.surface2, P.surface, g_dark ? 90 : 60); }

static void section(int x, int *y, const wchar_t *s)
{
    SetTextCharacterExtra(d_dc(), 1);
    d_text(x, *y, s, F_SMALL_B, P.muted);
    SetTextCharacterExtra(d_dc(), 0);
    *y += SC(24);
}

void tab_section(int x, int *y, const wchar_t *s) { section(x, y, s); }
COLORREF tab_card_fill(void) { return card_fill(); }

/* карточка: мягкая тень (при наведении глубже), лёгкий градиент, рамка */
static void card(int x, int y, int w, int h, COLORREF fill, COLORREF border, int hv)
{
    d_shadow(x, y, w, h, SC(10), SC(hv ? 12 : 8), g_dark ? (hv ? 150 : 110) : (hv ? 70 : 45));
    d_rrect_grad(x, y, w, h, SC(10), mix(fill, P.ink, hv ? 18 : 8), fill, border);
}

static int chip(int x, int y, const wchar_t *s, int sel, int dim, int act, int arg)
{
    int w = d_text_w(s, F_SMALL_B) + SC(22), h = SC(28);
    int hv = hover(act, arg);
    COLORREF bg = sel ? P.red : hv ? P.surface2 : CLR_INVALID;
    d_rrect(x, y, w, h, SC(14), bg, sel ? CLR_INVALID : P.line);
    d_text_c(x, y + (h - d_font_h(F_SMALL_B)) / 2, w, s, F_SMALL_B, sel ? P.onAccent : dim ? P.muted : P.ink);
    hit(x, y, w, h, act, arg);
    return w;
}

/* ---------- технологии ---------- */
static void tech_card(int t, int x, int y, int w, int h)
{
    const Tech *T = &TECHS[t];
    int st = game_tech_state(&G, t), cost = game_tech_cost(&G, t);
    int hv = hover(A_TECH, t) || hover(A_RESEARCH, t);
    COLORREF fill = st == TS_DONE ? mix(card_fill(), P.green, 26) : card_fill();
    card(x, y, w, h, fill, hv ? P.muted : P.line, hv);
    d_rrect(x + SC(5), y + SC(12), SC(4), h - SC(24), SC(2), branch_color(T->branch), CLR_INVALID);
    hit(x, y, w, h, A_TECH, t);
    int px = x + SC(16), pw = w - SC(26);
    int faded = st == TS_FUTURE || st == TS_LOCKED;
    d_ellipsis(px, y + SC(8), pw, T->name, F_H3, faded ? mix(P.ink, P.muted, 150) : P.ink);
    wchar_t buf[300];
    swprintf(buf, 300, L"%d · %ls", T->year, BRANCH_NAMES[T->branch]);
    d_ellipsis(px, y + SC(31), pw, buf, F_TINY, P.muted);
    fx_describe(&T->fx, buf, 300);
    d_ellipsis(px, y + SC(49), pw, buf, F_SMALL, st == TS_DONE ? P.green : faded ? P.muted : P.ink);
    int by = y + h - SC(38);
    if (st == TS_DONE) {
        d_icon(IC_CHECK, px, by + SC(6), SC(16), P.green);
        d_text(px + SC(22), by + SC(5), L"Изучено", F_SMALL_B, P.green);
    } else if (st == TS_AVAILABLE) {
        d_icon(IC_SCI, px, by + SC(6), SC(16), P.blue);
        swprintf(buf, 300, L"%d знаний", cost);
        d_text(px + SC(22), by + SC(5), buf, F_SMALL_B, G.sci >= cost ? P.ink : P.red);
        w_button(x + w - SC(118), by, SC(106), SC(30), L"Изучить", B_PRIMARY, G.sci >= cost, A_RESEARCH, t);
    } else if (st == TS_LOCKED) {
        wcscpy(buf, L"Нужно: ");
        int first = 1;
        for (int k = 0; k < 3; k++) {
            int r = T->reqi[k];
            if (r < 0 || G.tech[r]) continue;
            if (!first) wcsncat(buf, L", ", 300 - wcslen(buf) - 1);
            wcsncat(buf, TECHS[r].name, 300 - wcslen(buf) - 1);
            first = 0;
        }
        d_icon(IC_LOCK, px, by + SC(6), SC(16), P.muted);
        d_ellipsis(px + SC(22), by + SC(6), pw - SC(22), buf, F_SMALL, P.muted);
    } else {
        swprintf(buf, 300, L"Откроется в эпоху %ls (с %d года)", roman(T->era), ERAS[T->era - 1].from);
        d_icon(IC_LOCK, px, by + SC(6), SC(16), P.muted);
        d_ellipsis(px + SC(22), by + SC(6), pw - SC(22), buf, F_SMALL, P.muted);
    }
}

static int tab_tech(int x, int y, int w)
{
    int y0 = y, cx = x;
    if (U.eraFilter < 1 || U.eraFilter > ERA_COUNT) U.eraFilter = G.era;
    for (int e = 1; e <= ERA_COUNT; e++) {
        int total = 0, done = 0;
        for (int t = 0; t < TECH_COUNT; t++) if (TECHS[t].era == e) { total++; done += G.tech[t]; }
        wchar_t s[32];
        swprintf(s, 32, L"%ls  %d/%d", roman(e), done, total);
        int cw = d_text_w(s, F_SMALL_B) + SC(22);
        if (cx + cw > x + w) { cx = x; y += SC(34); }
        cx += chip(cx, y, s, U.eraFilter == e, e > G.era, A_ERAF, e) + SC(6);
    }
    y += SC(40);
    Income inc;
    game_income(&G, &inc);
    wchar_t info[200], n1[32];
    fmt_num(G.sci, n1, 32);
    swprintf(info, 200, L"%ls · %d–%d · знаний: %ls, за ход +%ld", ERAS[U.eraFilter - 1].name, ERAS[U.eraFilter - 1].from,
             ERAS[U.eraFilter - 1].to, n1, lround(inc.total[R_SCI]));
    y += d_wrap(x, y, w, info, F_SMALL, P.muted, 1) + SC(4);
    if (U.eraFilter > G.era) y += d_wrap(x, y, w, L"Эта эпоха ещё не наступила: технологии можно рассмотреть, но изучить нельзя.", F_SMALL, P.red, 1) + SC(4);
    else if (U.eraFilter < G.era) y += d_wrap(x, y, w, L"Технологии прошлых эпох дешевле на 20%: догнать упущенное никогда не поздно.", F_SMALL, P.muted, 1) + SC(4);
    y += SC(6);
    int cols = w >= SC(600) ? 2 : 1, gap = SC(10), cw = (w - gap * (cols - 1)) / cols, ch = SC(112);
    int i = 0;
    for (int t = 0; t < TECH_COUNT; t++) {
        if (TECHS[t].era != U.eraFilter) continue;
        int cxx = x + (i % cols) * (cw + gap), cy = y + (i / cols) * (ch + gap);
        if (cy + ch > U.panelY - SC(20) && cy < U.panelY + U.panelH + SC(20)) tech_card(t, cxx, cy, cw, ch);
        i++;
    }
    y += ((i + cols - 1) / cols) * (ch + gap);
    return y - y0 + SC(12);
}

/* ---------- стройки ---------- */
static int proj_rank(int p)
{
    int st = game_proj_state(&G, p);
    if (st == PS_AVAILABLE) return G.gold >= game_proj_gold(&G, p) && G.ind >= game_proj_ind(&G, p) ? 0 : 1;
    return st == PS_LOCKED ? 2 : st == PS_DONE ? 3 : st == PS_MISSED ? 4 : 5;
}

static void proj_card(int p, int x, int y, int w, int h)
{
    const Project *R = &PROJECTS[p];
    int st = game_proj_state(&G, p);
    int hv = hover(A_PROJ, p) || hover(A_BUILD, p);
    COLORREF fill = st == PS_DONE ? mix(card_fill(), P.red, 22) : card_fill();
    card(x, y, w, h, fill, hv ? P.muted : P.line, hv);
    d_icon(IC_BUILD, x + SC(12), y + SC(10), SC(18), st == PS_DONE ? P.red : P.muted);
    hit(x, y, w, h, A_PROJ, p);
    int px = x + SC(38), pw = w - SC(48);
    int faded = st != PS_AVAILABLE && st != PS_DONE;
    d_ellipsis(px, y + SC(8), pw, R->name, F_H3, faded ? mix(P.ink, P.muted, 150) : P.ink);
    wchar_t buf[300];
    swprintf(buf, 300, L"%ls · %d · эпоха %ls", CITIES[R->city].name, R->year, roman(R->era));
    d_ellipsis(px, y + SC(31), pw, buf, F_TINY, P.muted);
    fx_describe(&R->fx, buf, 300);
    d_ellipsis(px, y + SC(49), pw, buf, F_SMALL, st == PS_DONE ? P.red : faded ? P.muted : P.ink);
    int by = y + h - SC(38), bx = x + SC(12);
    if (st == PS_AVAILABLE || st == PS_LOCKED) {
        int cg = game_proj_gold(&G, p), ci = game_proj_ind(&G, p);
        wchar_t a[32], b[32];
        fmt_num(cg, a, 32); fmt_num(ci, b, 32);
        d_icon(IC_GOLD, bx, by + SC(6), SC(16), P.gold);
        d_text(bx + SC(20), by + SC(5), a, F_SMALL_B, G.gold >= cg ? P.ink : P.red);
        int ox = bx + SC(26) + d_text_w(a, F_SMALL_B);
        d_icon(IC_IND, ox, by + SC(6), SC(16), mix(P.gold, P.red, 90));
        d_text(ox + SC(20), by + SC(5), b, F_SMALL_B, G.ind >= ci ? P.ink : P.red);
        if (st == PS_AVAILABLE)
            w_button(x + w - SC(124), by, SC(112), SC(30), L"Построить", B_PRIMARY, G.gold >= cg && G.ind >= ci, A_BUILD, p);
        else {
            swprintf(buf, 300, L"нужна технология «%ls»", TECHS[R->techi].name);
            int tx = ox + SC(26) + d_text_w(b, F_SMALL_B);
            d_ellipsis(tx, by + SC(6), x + w - tx - SC(10), buf, F_TINY, P.muted);
        }
    } else if (st == PS_DONE) {
        d_icon(IC_CHECK, bx, by + SC(6), SC(16), P.red);
        d_text(bx + SC(22), by + SC(5), L"Построено", F_SMALL_B, P.red);
    } else if (st == PS_MISSED) {
        swprintf(buf, 300, L"Время упущено: строить можно было до %d года", R->until);
        d_ellipsis(bx, by + SC(6), w - SC(24), buf, F_SMALL, P.muted);
    } else {
        swprintf(buf, 300, L"Станет доступна в эпоху %ls", roman(R->era));
        d_ellipsis(bx, by + SC(6), w - SC(24), buf, F_SMALL, P.muted);
    }
}

static int tab_proj(int x, int y, int w)
{
    int y0 = y, cx = x;
    static const wchar_t *F[3] = { L"Доступные и закрытые", L"Все стройки", L"Построенные" };
    for (int i = 0; i < 3; i++) cx += chip(cx, y, F[i], U.projFilter == i, 0, A_PFILTER, i) + SC(6);
    y += SC(40);
    wchar_t info[200], a[32], b[32];
    fmt_num(G.gold, a, 32); fmt_num(G.ind, b, 32);
    swprintf(info, 200, L"Казна: %ls · производство: %ls. Построенное даёт доход каждый ход и отмечается на карте.", a, b);
    y += d_wrap(x, y, w, info, F_SMALL, P.muted, 1) + SC(10);

    int idx[MAX_PROJ], n = 0;
    for (int p = 0; p < PROJECT_COUNT; p++) {
        int st = game_proj_state(&G, p);
        if (U.projFilter == 0 && !(st == PS_AVAILABLE || st == PS_LOCKED)) continue;
        if (U.projFilter == 2 && st != PS_DONE) continue;
        idx[n++] = p;
    }
    for (int i = 1; i < n; i++) {
        int v = idx[i], j = i - 1;
        while (j >= 0 && (proj_rank(idx[j]) > proj_rank(v) ||
               (proj_rank(idx[j]) == proj_rank(v) && PROJECTS[idx[j]].year > PROJECTS[v].year))) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = v;
    }
    if (!n) {
        y += d_wrap(x, y, w, U.projFilter == 2 ? L"Пока ничего не построено. Стройки открываются вместе с технологиями."
                                               : L"Сейчас нечего строить: изучите технологии, которые открывают стройки, или дождитесь новой эпохи.",
                    F_BODY, P.muted, 1);
        return y - y0 + SC(12);
    }
    int cols = w >= SC(600) ? 2 : 1, gap = SC(10), cw = (w - gap * (cols - 1)) / cols, ch = SC(112);
    for (int i = 0; i < n; i++) {
        int cxx = x + (i % cols) * (cw + gap), cy = y + (i / cols) * (ch + gap);
        if (cy + ch > U.panelY - SC(20) && cy < U.panelY + U.panelH + SC(20)) proj_card(idx[i], cxx, cy, cw, ch);
    }
    y += ((n + cols - 1) / cols) * (ch + gap);
    return y - y0 + SC(12);
}

/* ---------- деятели ---------- */
static void person_card(int i, int x, int y, int w, int h)
{
    const Person *R = &PEOPLE[i];
    int st = game_person_state(&G, i);
    int hv = hover(A_PERSON, i) || hover(A_HIRE, i);
    COLORREF fill = st == HS_ACTIVE ? mix(card_fill(), P.blue, 24) : card_fill();
    card(x, y, w, h, fill, hv ? P.muted : P.line, hv);
    hit(x, y, w, h, A_PERSON, i);
    /* 3D-портрет (или инициалы, если портретов нет) */
    portrait_box(R->id, R->name, x + SC(10), y + SC(10), SC(46), SC(58), st == HS_ACTIVE ? P.blue : P.line);
    int px = x + SC(66), pw = w - SC(76);
    d_ellipsis(px, y + SC(8), pw, R->name, F_H3, st == HS_MISSED ? P.muted : P.ink);
    wchar_t buf[300];
    swprintf(buf, 300, L"%ls · %ls", R->life, R->role);
    d_ellipsis(px, y + SC(30), pw, buf, F_TINY, P.muted);
    fx_describe(&R->fx, buf, 300);
    d_ellipsis(px, y + SC(48), pw, buf, F_SMALL, st == HS_LEGACY ? P.muted : P.ink);
    if (R->disci >= 0) swprintf(buf, 300, L"Удешевляет на 35%%: «%ls»", TECHS[R->disci].name);
    else swprintf(buf, 300, L"Окно приглашения: %d–%d", R->from, R->to);
    d_ellipsis(px, y + SC(68), pw, buf, F_TINY, P.muted);
    int by = y + h - SC(36);
    if (st == HS_AVAILABLE) {
        int c = game_person_cost(&G, i);
        wchar_t a[32];
        fmt_num(c, a, 32);
        d_icon(IC_GOLD, px, by + SC(6), SC(16), P.gold);
        d_text(px + SC(20), by + SC(5), a, F_SMALL_B, G.gold >= c ? P.ink : P.red);
        swprintf(buf, 300, L"до %d", R->to);
        d_text(px + SC(28) + d_text_w(a, F_SMALL_B), by + SC(6), buf, F_TINY, P.muted);
        w_button(x + w - SC(130), by - SC(2), SC(118), SC(30), L"Пригласить", B_PRIMARY, G.gold >= c, A_HIRE, i);
    } else {
        const wchar_t *s = st == HS_ACTIVE ? L"В команде: полный эффект" : st == HS_LEGACY ? L"Наследие: половина эффекта" :
                           st == HS_MISSED ? L"Время упущено" : L"Ещё не появился";
        swprintf(buf, 300, L"%ls · %d–%d", s, R->from, R->to);
        d_ellipsis(px, by + SC(6), pw, buf, F_SMALL_B, st == HS_ACTIVE ? P.blue : P.muted);
    }
}

static int people_list(int x, int *y, int w, int state, int limit)
{
    int cols = w >= SC(600) ? 2 : 1, gap = SC(10), cw = (w - gap * (cols - 1)) / cols, ch = SC(122);
    int n = 0;
    for (int i = 0; i < PEOPLE_COUNT && n < limit; i++) {
        if (game_person_state(&G, i) != state) continue;
        if (state == HS_FUTURE && PEOPLE[i].from > G.year + 120) continue;
        int cx = x + (n % cols) * (cw + gap), cy = *y + (n / cols) * (ch + gap);
        if (cy + ch > U.panelY - SC(20) && cy < U.panelY + U.panelH + SC(20)) person_card(i, cx, cy, cw, ch);
        n++;
    }
    *y += ((n + cols - 1) / cols) * (ch + gap);
    return n;
}

static int count_state(int st)
{
    int n = 0;
    for (int i = 0; i < PEOPLE_COUNT; i++) n += game_person_state(&G, i) == st;
    return n;
}

static int tab_people(int x, int y, int w)
{
    int y0 = y;
    y += d_wrap(x, y, w, L"Мастера, учёные и просветители приходят в своё время. Пригласите их, пока окно открыто: "
                         L"деятель даёт доход каждый ход и удешевляет свою технологию на 35%. После окна остаётся наследие — половина эффекта.",
                F_SMALL, P.muted, 1) + SC(14);
    {
        int rl[3], nr = rulers_in_period(G.year, game_next_year(&G), rl, 3);
        if (nr) {
            section(x, &y, L"ПРАВИТЕЛИ СЕЙЧАС");
            for (int i = 0; i < nr; i++) {
                const Ruler *R = &RULERS[rl[i]];
                int pw = SC(64), ph = SC(80), tx = x + pw + SC(26), tw = w - pw - SC(38);
                wchar_t s[200];
                swprintf(s, 200, L"%ls · %ls, %d–%d", R->title, STATE_NAMES[R->state], R->from, R->to);
                int hd = d_wrap(0, 0, tw, R->desc, F_SMALL, 0, 0);
                int ch = SC(12) + d_font_h(F_H3) + SC(2) + d_font_h(F_TINY) + SC(6) + hd + SC(12);
                if (ch < ph + SC(20)) ch = ph + SC(20);
                int hv = hover(A_RULER, rl[i]);
                d_rrect(x, y, w, ch, SC(6), mix(card_fill(), P.gold, 30), hv ? P.gold : P.line);
                portrait_box(R->id, R->name, x + SC(10), y + SC(10), pw, ph, P.gold);
                int ty = y + SC(10);
                d_ellipsis(tx, ty, tw, R->name, F_H3, P.ink); ty += d_font_h(F_H3) + SC(2);
                d_ellipsis(tx, ty, tw, s, F_TINY, P.gold); ty += d_font_h(F_TINY) + SC(6);
                d_wrap(tx, ty, tw, R->desc, F_SMALL, P.ink, 1);
                hit(x, y, w, ch, A_RULER, rl[i]);
                y += ch + SC(10);
            }
            y += SC(6);
        }
    }
    section(x, &y, L"МОЖНО ПРИГЛАСИТЬ СЕЙЧАС");
    if (!people_list(x, &y, w, HS_AVAILABLE, 99)) { y += d_wrap(x, y, w, L"Сейчас никого нет — новые деятели появятся в следующих ходах.", F_BODY, P.muted, 1) + SC(12); }
    if (count_state(HS_ACTIVE)) { section(x, &y, L"В КОМАНДЕ"); people_list(x, &y, w, HS_ACTIVE, 99); }
    if (count_state(HS_LEGACY)) { section(x, &y, L"НАСЛЕДИЕ"); people_list(x, &y, w, HS_LEGACY, 99); }
    if (count_state(HS_FUTURE)) { section(x, &y, L"СКОРО ПОЯВЯТСЯ"); people_list(x, &y, w, HS_FUTURE, 4); }
    int missed = count_state(HS_MISSED);
    if (missed) {
        section(x, &y, L"УПУЩЕНЫ");
        wchar_t buf[1200];
        buf[0] = 0;
        for (int i = 0; i < PEOPLE_COUNT; i++)
            if (game_person_state(&G, i) == HS_MISSED) {
                if (buf[0]) wcsncat(buf, L", ", 1199 - wcslen(buf));
                wcsncat(buf, PEOPLE[i].name, 1199 - wcslen(buf));
            }
        y += d_wrap(x, y, w, buf, F_SMALL, P.muted, 1) + SC(10);
    }
    {
        int any = 0;
        for (int k = 0; k < RULER_COUNT; k++) any |= RULERS[k].from <= G.year;
        if (any) {
            wchar_t t[80];
            swprintf(t, 80, L"ВСЕ ПРАВИТЕЛИ ДО %d ГОДА", G.year);
            section(x, &y, t);
            int cw = SC(66), chh = SC(82), gap = SC(10), cx = x;
            for (int k = 0; k < RULER_COUNT; k++) {
                if (RULERS[k].from > G.year) continue;
                if (cx + cw > x + w) { cx = x; y += chh + SC(34); }
                int hv = hover(A_RULER, k);
                portrait_box(RULERS[k].id, RULERS[k].name, cx, y, cw, chh, hv ? P.gold : P.line);
                const wchar_t *sp = wcsrchr(RULERS[k].name, L' ');
                d_ellipsis(cx - SC(4), y + chh + SC(3), cw + SC(8), sp ? sp + 1 : RULERS[k].name, F_TINY, P.ink);
                wchar_t yr[24];
                swprintf(yr, 24, L"%d–%d", RULERS[k].from, RULERS[k].to);
                d_ellipsis(cx - SC(4), y + chh + SC(17), cw + SC(8), yr, F_TINY, P.muted);
                hit(cx, y, cw, chh + SC(30), A_RULER, k);
                cx += cw + gap;
            }
            y += chh + SC(40);
        }
    }
    return y - y0 + SC(12);
}

/* ---------- курс и доходы ---------- */
static int tab_policy(int x, int y, int w)
{
    int y0 = y;
    section(x, &y, L"КУРС ПРАВЛЕНИЯ");
    y += d_wrap(x, y, w, L"Курс действует на все доходы. Менять его можно в любой момент, без затрат.", F_SMALL, P.muted, 1) + SC(10);
    int cols = w >= SC(640) ? 3 : w >= SC(420) ? 2 : 1, gap = SC(8), cw = (w - gap * (cols - 1)) / cols, ch = SC(86);
    for (int i = 0; i < PRIORITY_COUNT; i++) {
        int cx = x + (i % cols) * (cw + gap), cy = y + (i / cols) * (ch + gap);
        int sel = G.priority == i, hv = hover(A_PRIORITY, i);
        d_rrect(cx, cy, cw, ch, SC(6), sel ? P.redSoft : hv ? P.surface2 : card_fill(), sel ? P.red : P.line);
        d_circle(cx + SC(16), cy + SC(19), SC(6), sel ? P.red : CLR_INVALID, sel ? P.red : P.muted, 2);
        d_ellipsis(cx + SC(30), cy + SC(10), cw - SC(40), PRIORITIES[i].name, F_BODY_B, sel ? P.red : P.ink);
        d_wrap(cx + SC(12), cy + SC(34), cw - SC(22), PRIORITIES[i].desc, F_TINY, P.muted, 1);
        hit(cx, cy, cw, ch, A_PRIORITY, i);
    }
    y += ((PRIORITY_COUNT + cols - 1) / cols) * (ch + gap) + SC(14);

    section(x, &y, L"ДОХОД ЗА ХОД");
    Income inc;
    game_income(&G, &inc);
    static const wchar_t *colN[5] = { L"Казна", L"Знания", L"Произв.", L"Оборона", L"Культура" };
    static const int colR[5] = { R_GOLD, R_SCI, R_IND, R_DEF, R_CULT };
    int labW = w * 34 / 100, numW = (w - labW) / 5;
    for (int c = 0; c < 5; c++) d_text_r(x + labW + (c + 1) * numW - SC(6), y, colN[c], F_TINY, P.muted);
    y += SC(20);
    static const wchar_t *rowN[11] = { L"Основа", L"Население", L"Технологии", L"Стройки", L"Деятели", L"Города", L"Земли",
                                        L"Решения", L"Множитель", L"Армия", L"Итого" };
    for (int r = 0; r < 11; r++) {
        if (r == 10) d_fill(x, y - SC(3), w, 1, P.line);
        d_text(x, y, rowN[r], r == 10 ? F_SMALL_B : F_SMALL, r == 10 ? P.ink : P.muted);
        for (int c = 0; c < 5; c++) {
            int k = colR[c];
            wchar_t s[32];
            double v = r == 0 ? inc.base[k] : r == 1 ? inc.popb[k] : r == 2 ? inc.techs[k] : r == 3 ? inc.projs[k] :
                       r == 4 ? inc.people[k] : r == 5 ? inc.cities[k] : r == 6 ? inc.provs[k] : r == 7 ? inc.mods[k] :
                       r == 8 ? inc.mult[k] : r == 9 ? inc.army[k] : inc.total[k];
            if (r == 8) { swprintf(s, 32, L"\x00D7%.2f", v); for (wchar_t *p = s; *p; p++) if (*p == L'.') *p = L','; }
            else swprintf(s, 32, L"%ld", lround(v));
            d_text_r(x + labW + (c + 1) * numW - SC(6), y, s, r == 10 ? F_MONO_B : F_MONO, r == 10 ? P.ink : P.muted);
        }
        y += SC(21);
    }
    y += d_wrap(x, y, w, inc.occMult < 0.999 ? L"Множитель учитывает оккупацию. Содержание армии вычитается из казны после множителя; оборона войска — до него."
                                             : L"Содержание армии вычитается из казны после множителя; оборона войска — до него.", F_TINY, P.muted, 1) + SC(4);
    y += SC(6);
    wchar_t buf[320];
    swprintf(buf, 320, L"Рост населения: %ls%.2f%% в год. Стабильность стремится к %ld. Экология меняется на %+.1f за ход.",
             inc.growYear < 0 ? L"\x2212" : L"+", fabs(inc.growYear * 100), lround(inc.stabTarget),
             inc.total[R_ECO] + (100.0 - G.eco) * 0.1);
    for (wchar_t *p = buf; *p; p++) if (*p == L'.' && p[1] >= L'0' && p[1] <= L'9') *p = L',';
    y += d_wrap(x, y, w, buf, F_SMALL, P.ink, 1) + SC(16);

    section(x, &y, L"РЕШЕНИЯ, КОТОРЫЕ ДЕЙСТВУЮТ");
    int any = 0;
    for (int m = 0; m < G.modN; m++) {
        if (G.mods[m].until && G.mods[m].until <= G.turn) continue;
        wchar_t fx[200];
        fx_describe(&G.mods[m].fx, fx, 200);
        if (G.mods[m].until) swprintf(buf, 320, L"%ls — %ls (ещё %d х.)", G.mods[m].name, fx, G.mods[m].until - G.turn);
        else swprintf(buf, 320, L"%ls — %ls", G.mods[m].name, fx);
        d_fill(x, y + SC(7), SC(5), SC(5), P.red);
        y += d_wrap(x + SC(14), y, w - SC(14), buf, F_SMALL, P.ink, 1) + SC(4);
        any = 1;
    }
    if (!any) y += d_wrap(x, y, w, L"Пока нет. Решения в исторических событиях иногда дают постоянные прибавки.", F_SMALL, P.muted, 1) + SC(4);
    y += SC(14);

    section(x, &y, L"ИЗЛИШКИ ПРОИЗВОДСТВА");
    int amount = game_sell_amount(&G);
    swprintf(buf, 320, L"Продать %d производства за %d казны", amount, amount / 2);
    w_button(x, y, d_text_w(buf, F_BODY_B) + SC(36), SC(36), buf, B_SECONDARY, G.ind >= amount, A_SELL, 0);
    y += SC(50);
    swprintf(buf, 320, L"Сложность «%ls»: %ls", DIFFICULTIES[G.difficulty].name, DIFFICULTIES[G.difficulty].desc);
    y += d_wrap(x, y, w, buf, F_SMALL, P.muted, 1);
    return y - y0 + SC(16);
}

/* ---------- летописец (викторина) ---------- */
static int tab_quiz(int x, int y, int w)
{
    int y0 = y;
    wchar_t buf[300];
    swprintf(buf, 300, L"Верных ответов: %d · ошибок: %d · награда за верный ответ: +%d знаний и +2 культуры. Один вопрос за ход.",
             G.quizRight, G.quizWrong, game_quiz_reward(&G));
    y += d_wrap(x, y, w, buf, F_SMALL, P.muted, 1) + SC(14);
    int q = G.over ? -1 : game_quiz_pick(&G);
    if (q < 0) {
        y += d_wrap(x, y, w, G.over ? L"Летопись завершена." : L"Летописец ждёт следующего хода: новый вопрос появится после завершения хода.", F_BODY, P.muted, 1);
        return y - y0 + SC(12);
    }
    const Quiz *Q = &QUIZ[q];
    swprintf(buf, 300, L"ВОПРОС · ЭПОХА %ls", roman(Q->era));
    section(x, &y, buf);
    y += d_wrap(x, y, w, Q->q, F_H2, P.ink, 1) + SC(16);
    int answered = G.quizAnswered && G.quizCur == q;
    for (int i = 0; i < 4; i++) {
        int real = G.quizPerm[i];
        int isRight = real == Q->correct, picked = answered && G.quizLastAns == i;
        COLORREF bg = card_fill(), br = P.line, fg = P.ink;
        if (answered && isRight) { bg = mix(card_fill(), P.green, 60); br = P.green; }
        else if (picked) { bg = mix(card_fill(), P.red, 60); br = P.red; }
        else if (!answered && hover(A_QUIZ, i)) { bg = P.surface2; br = P.muted; }
        if (answered && !isRight && !picked) fg = P.muted;
        int h = SC(46);
        d_rrect(x, y, w, h, SC(6), bg, br);
        wchar_t num[4] = { (wchar_t)(L'A' + i), 0, 0, 0 };
        if (i < 4) num[0] = (wchar_t)(L"АБВГ"[i]);
        d_text(x + SC(14), y + (h - d_font_h(F_BODY_B)) / 2, num, F_BODY_B, P.red);
        d_ellipsis(x + SC(40), y + (h - d_font_h(F_BODY)) / 2, w - SC(56), Q->a[real], F_BODY, fg);
        if (!answered) hit(x, y, w, h, A_QUIZ, i);
        y += h + SC(8);
    }
    if (answered) {
        y += SC(6);
        int ok = G.quizPerm[G.quizLastAns] == Q->correct;
        d_text(x, y, ok ? L"Верно." : L"Неверно.", F_H3, ok ? P.green : P.red);
        y += d_font_h(F_H3) + SC(4);
        y += d_wrap(x, y, w, Q->expl, F_BODY, P.ink, 1) + SC(8);
        y += d_wrap(x, y, w, L"Следующий вопрос — в новом ходу.", F_SMALL, P.muted, 1);
    }
    return y - y0 + SC(12);
}

/* ---------- летопись ---------- */
static int tab_log(int x, int y, int w)
{
    int y0 = y;
    if (!G.logN) { y += d_wrap(x, y, w, L"Летопись пуста.", F_BODY, P.muted, 1); return y - y0; }
    for (int i = G.logN - 1; i >= 0; i--) {
        const LogEntry *e = &G.logs[i];
        int ic = IC_POLIT;
        COLORREF c = P.muted;
        switch (e->kind) {
        case LOG_TECH: ic = IC_TECH; c = P.blue; break;
        case LOG_BUILD: ic = IC_BUILD; c = P.red; break;
        case LOG_PERSON: ic = IC_PERSON; c = P.blue; break;
        case LOG_WAR: ic = IC_WAR; c = P.red; break;
        case LOG_ERA: ic = IC_CULT; c = P.red; break;
        case LOG_QUIZ: ic = IC_QUIZ; c = P.green; break;
        case LOG_ACH: ic = IC_CHECK; c = P.gold; break;
        case LOG_QUEST: ic = IC_FLAG; c = P.red; break;
        default: ic = IC_POLIT; c = P.muted; break;
        }
        int h = d_wrap(x + SC(84), y, w - SC(84), e->text, F_SMALL, P.ink, 0);
        if (h < SC(20)) h = SC(20);
        if (y + h > U.panelY - SC(40) && y < U.panelY + U.panelH + SC(40)) {
            wchar_t yr[16];
            swprintf(yr, 16, L"%d", e->year);
            d_text(x, y + SC(1), yr, F_MONO_B, e->kind == LOG_ERA ? P.red : P.muted);
            d_icon(ic, x + SC(52), y + SC(1), SC(16), c);
            d_wrap(x + SC(84), y, w - SC(84), e->text, e->kind == LOG_ERA ? F_SMALL_B : F_SMALL, P.ink, 1);
        }
        y += h + SC(8);
        if (e->kind == LOG_ERA) { d_fill(x, y - SC(4), w, 1, P.line); y += SC(6); }
    }
    return y - y0 + SC(10);
}

/* ---------- графики ---------- */
static double turn_pos_of_year(double year)
{
    if (year <= TURN_YEARS[0]) return 0;
    for (int t = 1; t < TURN_COUNT; t++)
        if (year < TURN_YEARS[t]) return t - 1 + (year - TURN_YEARS[t - 1]) / (TURN_YEARS[t] - TURN_YEARS[t - 1]);
    return TURN_COUNT - 1 + (year - TURN_YEARS[TURN_COUNT - 1]) / (END_YEAR - TURN_YEARS[TURN_COUNT - 1] + 0.0);
}

static void chart(int x, int y, int w, int h, const wchar_t *title, const float *a, const float *b, COLORREF ca, COLORREF cb,
                  int fixed100, int popRef)
{
    d_rrect(x, y, w, h, SC(6), card_fill(), P.line);
    d_text(x + SC(12), y + SC(8), title, F_SMALL_B, P.ink);
    int px = x + SC(46), py = y + SC(34), pw = w - SC(62), ph = h - SC(62);
    int n = G.histN;
    double mx = fixed100 ? 100 : 1;
    if (!fixed100) {
        for (int i = 0; i < n; i++) { if (a[i] > mx) mx = a[i]; if (b && b[i] > mx) mx = b[i]; }
        if (popRef && mx < 10500) mx = 10500;
        double mag = pow(10, floor(log10(mx)));
        mx = ceil(mx * 1.05 / mag * 2) * mag / 2;
    }
    for (int g = 0; g <= 4; g++) {
        int gy = py + ph - ph * g / 4;
        d_fill(px, gy, pw, 1, g == 0 ? P.muted : mix(card_fill(), P.line, 200));
        if (g == 0 || g == 2 || g == 4) {
            wchar_t s[32];
            if (popRef) { fmt_pop(mx * g / 4, s, 32); if (g == 0) wcscpy(s, L"0"); }
            else fmt_num(mx * g / 4, s, 32);
            d_text_r(px - SC(6), gy - d_font_h(F_TINY) / 2, s, F_TINY, P.muted);
        }
    }
    double span = TURN_COUNT;
    for (int e = 1; e <= ERA_COUNT; e++) {
        double tp = turn_pos_of_year(ERAS[e - 1].from);
        int ex = px + (int)(pw * tp / span);
        d_fill(ex, py + ph, 1, SC(4), P.muted);
        if (pw > SC(260) || e % 2 == 1) d_text(ex + SC(2), py + ph + SC(5), roman(e), F_TINY, P.muted);
    }
    if (popRef) {
        static const float RY[] = { 1945, 1950, 1959, 1970, 1979, 1989, 1994, 1999, 2009, 2019, 2024 };
        static const float RV[] = { 6300, 7700, 8056, 9002, 9533, 10152, 10243, 10045, 9504, 9413, 9155 };
        POINT rp[11];
        for (int i = 0; i < 11; i++) {
            rp[i].x = px + (int)(pw * turn_pos_of_year(RY[i]) / span);
            rp[i].y = py + ph - (int)(ph * RV[i] / mx);
        }
        d_poly(rp, 11, CLR_INVALID, mix(card_fill(), P.muted, 170), 1);
        for (int i = 0; i < 11; i += 2) d_circle(rp[i].x, rp[i].y, SC(2), P.muted, CLR_INVALID, 1);
        d_text(rp[1].x + SC(4), rp[1].y + SC(6), L"реальная история", F_TINY, P.muted);
    }
    for (int s = 0; s < 2; s++) {
        const float *v = s ? b : a;
        if (!v || n < 1) continue;
        COLORREF c = s ? cb : ca;
        POINT pts[MAX_TURNS];
        for (int i = 0; i < n; i++) {
            pts[i].x = px + (int)(pw * (double)i / span);
            pts[i].y = py + ph - (int)(ph * v[i] / mx);
        }
        if (n > 1) d_poly(pts, n, CLR_INVALID, c, SC(2) > 1 ? SC(2) : 2);
        d_circle(pts[n - 1].x, pts[n - 1].y, SC(4), c, card_fill(), 2);
        wchar_t s2[32];
        if (popRef) fmt_pop(v[n - 1], s2, 32); else fmt_num(v[n - 1], s2, 32);
        int tx = pts[n - 1].x + SC(8);
        if (tx + d_text_w(s2, F_TINY) > px + pw) tx = pts[n - 1].x - d_text_w(s2, F_TINY) - SC(8);
        d_text(tx, pts[n - 1].y - SC(16), s2, F_TINY, c);
    }
}

static int tab_charts(int x, int y, int w)
{
    int y0 = y;
    y += d_wrap(x, y, w, L"По горизонтали — ходы, римскими цифрами отмечены начала эпох. Серая линия на графике населения — реальная численность населения Беларуси с 1945 года.",
                F_SMALL, P.muted, 1) + SC(12);
    int cols = w >= SC(620) ? 2 : 1, gap = SC(10), cw = (w - gap * (cols - 1)) / cols, ch = SC(220);
    static float tmp[MAX_TURNS];
    for (int i = 0; i < G.histN; i++) tmp[i] = G.hStab[i];
    for (int i = 0; i < 4; i++) {
        int cx = x + (i % cols) * (cw + gap), cy = y + (i / cols) * (ch + gap);
        switch (i) {
        case 0: chart(cx, cy, cw, ch, L"Население", G.hPop, 0, P.red, 0, 0, 1); break;
        case 1: chart(cx, cy, cw, ch, L"Изученные технологии", G.hTech, 0, P.blue, 0, 0, 0); break;
        case 2: chart(cx, cy, cw, ch, L"Стабильность (красная) и экология (зелёная)", tmp, G.hEco, P.red, P.green, 1, 0); break;
        case 3: chart(cx, cy, cw, ch, L"Культура и память", G.hCult, 0, P.gold, 0, 0, 0); break;
        }
    }
    y += ((4 + cols - 1) / cols) * (ch + gap);
    return y - y0 + SC(10);
}

/* ---------- награды ---------- */
static int tab_ach(int x, int y, int w)
{
    int y0 = y;
    Score sc;
    game_score(&G, &sc);
    wchar_t buf[200];
    swprintf(buf, 200, L"Текущий счёт: %d · если бы летопись закончилась сейчас — «%ls»", sc.total, sc.rank);
    y += d_wrap(x, y, w, buf, F_BODY_B, P.ink, 1) + SC(12);
    int cols = w >= SC(560) ? 2 : 1, gap = SC(8), cw = (w - gap * (cols - 1)) / cols, ch = SC(66);
    for (int a = 0; a < ACH_COUNT; a++) {
        int cx = x + (a % cols) * (cw + gap), cy = y + (a / cols) * (ch + gap);
        int got = G.ach[a];
        d_rrect(cx, cy, cw, ch, SC(6), got ? mix(card_fill(), P.gold, 40) : card_fill(), got ? P.gold : P.line);
        d_icon(got ? IC_CHECK : IC_LOCK, cx + SC(12), cy + SC(14), SC(20), got ? P.gold : P.muted);
        d_ellipsis(cx + SC(44), cy + SC(10), cw - SC(54), ACHIEVEMENTS[a].name, F_BODY_B, got ? P.ink : P.muted);
        d_ellipsis(cx + SC(44), cy + SC(34), cw - SC(54), ACHIEVEMENTS[a].desc, F_TINY, P.muted);
        hit(cx, cy, cw, ch, A_HOVER_ACH, a);
    }
    y += ((ACH_COUNT + cols - 1) / cols) * (ch + gap);
    return y - y0 + SC(10);
}

/* ---------- задания ---------- */
static int tab_quests(int x, int y, int w)
{
    int y0 = y;
    int done = game_quest_count(&G, QS_DONE), act = game_quest_count(&G, QS_ACTIVE), fail = game_quest_count(&G, QS_FAILED);
    wchar_t buf[400];
    swprintf(buf, 400, L"Выполнено %d из %d · активных %d · провалено %d. Флажки на карте отмечают города, где ждут задания; награда приходит сразу.",
             done, QUEST_COUNT, act, fail);
    y += d_wrap(x, y, w, buf, F_SMALL, P.muted, 1) + SC(14);
    section(x, &y, L"АКТИВНЫЕ ЗАДАНИЯ");
    int next = game_next_year(&G), shown = 0;
    for (int q = 0; q < QUEST_COUNT; q++) {
        if (game_quest_state(&G, q) != QS_ACTIVE) continue;
        const Quest *Q = &QUESTS[q];
        wchar_t goal[200], rew[200];
        quest_describe_goal(q, goal, 200);
        quest_describe_reward(q, rew, 200);
        int tw = w - SC(32);
        int hd = d_wrap(0, 0, tw, Q->desc, F_SMALL, 0, 0);
        int hg = d_wrap(0, 0, tw, goal, F_SMALL_B, 0, 0);
        int ch = SC(12) + d_font_h(F_H3) + SC(4) + hg + SC(4) + hd + SC(10) + SC(8) + SC(8) + d_font_h(F_TINY) + SC(12);
        int urgent = Q->deadline < next;
        if (y + ch > U.panelY - SC(40) && y < U.panelY + U.panelH + SC(40)) {
            d_rrect(x, y, w, ch, SC(6), card_fill(), urgent ? P.red : P.line);
            d_fill(x + 1, y + SC(10), SC(4), ch - SC(20), urgent ? P.red : P.gold);
            int tx = x + SC(16), ty = y + SC(12);
            d_icon(IC_FLAG, tx, ty + SC(2), SC(16), P.red);
            d_ellipsis(tx + SC(22), ty, tw - SC(150), Q->title, F_H3, P.ink);
            /* срок */
            wchar_t dl[64];
            int left = Q->deadline - G.year;
            if (urgent) swprintf(dl, 64, L"срок в этом ходу (%d)", Q->deadline);
            else swprintf(dl, 64, L"до %d · %d лет", Q->deadline, left);
            int dw = w_pill(x + w - SC(16) - d_text_w(dl, F_TINY) - SC(12), ty + SC(2), dl, urgent ? P.onAccent : P.ink, urgent ? P.red : P.surface2);
            (void)dw;
            ty += d_font_h(F_H3) + SC(4);
            ty += d_wrap(tx, ty, tw, goal, F_SMALL_B, P.ink, 1) + SC(4);
            ty += d_wrap(tx, ty, tw, Q->desc, F_SMALL, P.muted, 1) + SC(10);
            /* прогресс */
            double cur = 0, tgt = 1;
            game_quest_progress(&G, q, &cur, &tgt);
            double fr = tgt > 0 ? cur / tgt : 0;
            if (fr > 1) fr = 1;
            if (fr < 0) fr = 0;
            int bw = tw - SC(150);
            d_rrect(tx, ty, bw, SC(8), SC(4), P.surface2, CLR_INVALID);
            if (fr > 0) d_rrect(tx, ty, (int)(bw * fr) > SC(8) ? (int)(bw * fr) : SC(8), SC(8), SC(4), P.green, CLR_INVALID);
            wchar_t pr[64];
            if (Q->type == QT_TECH || Q->type == QT_PROJ || Q->type == QT_PERSON || Q->type == QT_PROVINCE)
                swprintf(pr, 64, L"%ls", cur >= tgt ? L"готово" : L"ещё не сделано");
            else { wchar_t a[24], b[24]; fmt_num(cur, a, 24); fmt_num(tgt, b, 24); swprintf(pr, 64, L"%ls / %ls", a, b); }
            d_text(tx + bw + SC(10), ty - SC(4), pr, F_TINY, P.muted);
            ty += SC(16);
            swprintf(buf, 400, L"Награда: %ls", rew);
            d_ellipsis(tx, ty, tw - (Q->city >= 0 ? SC(120) : 0), buf, F_TINY, P.gold);
            if (Q->city >= 0) {
                int cx = x + w - SC(16) - SC(110);
                int hv = hover(A_CITY, Q->city);
                d_rrect(cx, ty - SC(4), SC(110), SC(22), SC(11), hv ? P.blueSoft : P.surface2, CLR_INVALID);
                d_text_c(cx, ty - SC(1), SC(110), CITIES[Q->city].name, F_TINY, P.blue);
                hit(cx, ty - SC(4), SC(110), SC(22), A_CITY, Q->city);
            }
        }
        y += ch + SC(10);
        shown++;
    }
    if (!shown) y += d_wrap(x, y, w, L"Активных заданий нет — новые откроются в следующей эпохе.", F_BODY, P.muted, 1) + SC(12);
    for (int pass = 0; pass < 2; pass++) {
        int st = pass == 0 ? QS_DONE : QS_FAILED;
        if (!game_quest_count(&G, st)) continue;
        y += SC(6);
        section(x, &y, pass == 0 ? L"ВЫПОЛНЕННЫЕ" : L"ПРОВАЛЕННЫЕ");
        for (int q = 0; q < QUEST_COUNT; q++) {
            if (game_quest_state(&G, q) != st) continue;
            wchar_t goal[200];
            quest_describe_goal(q, goal, 200);
            d_icon(pass == 0 ? IC_CHECK : IC_LOCK, x, y + SC(1), SC(16), pass == 0 ? P.green : P.muted);
            d_ellipsis(x + SC(24), y, w - SC(24), QUESTS[q].title, F_SMALL_B, pass == 0 ? P.ink : P.muted);
            d_ellipsis(x + SC(24), y + SC(18), w - SC(24), goal, F_TINY, P.muted);
            y += SC(40);
        }
    }
    int hidden = game_quest_count(&G, QS_HIDDEN);
    if (hidden) {
        swprintf(buf, 400, L"Ещё %d заданий откроются в следующих эпохах.", hidden);
        y += SC(4) + d_wrap(x, y + SC(4), w, buf, F_SMALL, P.muted, 1);
    }
    return y - y0 + SC(14);
}

int tab_draw(int tab, int x, int y, int w, int h)
{
    (void)h;
    switch (tab) {
    case TAB_TECH:   return tab_tech(x, y, w);
    case TAB_PROJ:   return tab_proj(x, y, w);
    case TAB_PEOPLE: return tab_people(x, y, w);
    case TAB_POLICY: return tab_policy(x, y, w);
    case TAB_QUIZ:   return tab_quiz(x, y, w);
    case TAB_LOG:    return tab_log(x, y, w);
    case TAB_ARMY:   return tab_army(x, y, w);
    case TAB_RESULTS: {
        int h1 = tab_charts(x, y, w);
        int yy = y + h1;
        section(x, &yy, L"НАГРАДЫ");
        return yy - y + tab_ach(x, yy, w);
    }
    case TAB_QUESTS: return tab_quests(x, y, w);
    }
    return 0;
}

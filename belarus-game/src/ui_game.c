/* Экраны: титульный и игровой (шапка, лента эпох, карта, вкладки), ввод с мыши и клавиатуры. */
#include "ui.h"
#include "mapview.h"
#include "citynet.h"
#include "rulers.h"
#include "war.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int tab_draw(int tab, int x, int y, int w, int h);   /* ui_tabs.c */

static const wchar_t *TAB_NAMES[TAB_COUNT] = {
    L"Технологии", L"Стройки", L"Деятели", L"Армия", L"Задания", L"Курс", L"Летописец", L"Летопись", L"Итоги"
};

/* ---------- общие помощники ---------- */
const wchar_t *roman(int e)
{
    static const wchar_t *R[] = { L"", L"I", L"II", L"III", L"IV", L"V", L"VI", L"VII", L"VIII" };
    return e >= 1 && e <= 8 ? R[e] : L"";
}

void fmt_num(double v, wchar_t *buf, int n)
{
    long x = lround(v);
    int neg = x < 0;
    if (neg) x = -x;
    wchar_t tmp[32];
    int len = 0, digits = 0;
    do {
        if (digits && digits % 3 == 0) tmp[len++] = L'\x00A0';
        tmp[len++] = (wchar_t)(L'0' + x % 10);
        x /= 10;
        digits++;
    } while (x && len < 30);
    if (neg) tmp[len++] = L'\x2212';
    int o = 0;
    while (len && o < n - 1) buf[o++] = tmp[--len];
    buf[o] = 0;
}

int kind_icon(int k)
{
    static const int M[K_COUNT] = { IC_POLIT, IC_WAR, IC_DISASTER, IC_CULT, IC_SCI, IC_GOLD, IC_IND, IC_SPACE };
    return k >= 0 && k < K_COUNT ? M[k] : IC_POLIT;
}

COLORREF kind_color(int k)
{
    switch (k) {
    case K_WAR: case K_DISASTER: return P.red;
    case K_CULTURE: case K_ECON: return P.gold;
    case K_SCIENCE: case K_TECH: case K_SPACE: return P.blue;
    default: return P.muted;
    }
}

COLORREF branch_color(int br)
{
    switch (br) {
    case BR_CRAFT:  return P.gold;
    case BR_KNOW:   return P.blue;
    case BR_BUILD:  return mix(P.red, P.gold, 120);
    case BR_AGRO:   return P.green;
    case BR_WAR:    return P.red;
    case BR_TRANS:  return mix(P.blue, P.green, 128);
    case BR_ENERGY: return mix(P.gold, P.red, 90);
    case BR_MED:    return mix(P.green, P.blue, 90);
    default:        return mix(P.blue, P.red, 100);
    }
}

static void spaced(int x, int y, const wchar_t *s, int f, COLORREF c)
{
    SetTextCharacterExtra(d_dc(), SC(1) > 1 ? SC(1) : 1);
    d_text(x, y, s, f, c);
    SetTextCharacterExtra(d_dc(), 0);
}

/* ---------- титульный экран ---------- */
static void draw_title(void)
{
    ui_set_era_font(1);
    d_fill(0, 0, g_w, g_h, P.bg);
    int cell = SC(4) > 2 ? SC(4) : 2;
    d_ornament(0, SC(26), g_w, cell, P.red, mix(P.red, P.bg, 140));
    d_ornament(0, g_h - SC(26) - 9 * cell, g_w, cell, mix(P.red, P.bg, 90), mix(P.red, P.bg, 170));

    int x0 = g_w / 16 > SC(40) ? g_w / 16 : SC(40);
    int wide = g_w > SC(1000);
    int colW = wide ? g_w * 46 / 100 : g_w - 2 * x0;
    /* Колонка подстраивается под высоту окна: при нехватке места сначала убираем вводный абзац,
       затем уменьшаем заголовок и кнопки — чтобы кнопки и издатели всегда оставались на экране. */
    static const wchar_t *TITLE = L"Судьба Республики Беларусь";
    static const wchar_t *PITCH =
        L"Девять веков от болотной руды и берестяных грамот до атомной станции и искусственного интеллекта. "
        L"Изучайте технологии, стройте замки и заводы, приглашайте мастеров и учёных, принимайте решения "
        L"в переломные моменты истории — и посмотрите, какой станет страна к 2026 году.";
    int top = SC(26) + 9 * cell + SC(36);
    int limit = g_h - SC(26) - 9 * cell - SC(12) - d_font_h(F_SMALL_B) - SC(12);
    int nb = U.hasSave ? 3 : 2;
    int titleF = F_HUGE, pitch = 1, bh = SC(44), gap = SC(10);
    int pw = colW < SC(520) ? colW : SC(520);
    for (int pass = 0; pass < 4; pass++) {
        int h = SC(30) + d_wrap(x0, 0, colW, TITLE, titleF, 0, 0) + SC(2) + d_font_h(F_ERA_L) + SC(6)
              + d_font_h(F_BODY) + SC(18) + (pitch ? d_wrap(x0, 0, pw, PITCH, F_BODY, 0, 0) + SC(26) : SC(4))
              + nb * (bh + gap) + SC(36);
        if (top + h <= limit) break;
        if (pass == 0) pitch = 0;
        else if (pass == 1) titleF = F_ERA_XL;
        else if (pass == 2) { bh = SC(36); gap = SC(6); top = SC(26) + 9 * cell + SC(18); }
    }
    int y = top;
    spaced(x0, y, L"СТРАТЕГИЯ · 1100–2026 · 8 ЭПОХ", F_SMALL_B, P.red);
    y += SC(30);
    y += d_wrap(x0, y, colW, TITLE, titleF, P.ink, 1);
    y += SC(2);
    d_text(x0, y, L"в объективе технологий", F_ERA_L, P.red);
    y += d_font_h(F_ERA_L) + SC(6);
    d_text(x0, y, L"Лёс Беларусі ў аб’ектыве тэхналогій", F_BODY, P.muted);
    y += d_font_h(F_BODY) + SC(18);
    if (pitch) y += d_wrap(x0, y, pw, PITCH, F_BODY, P.ink, 1) + SC(26);
    else y += SC(4);

    int bw = SC(300);
    if (U.hasSave) {
        w_button(x0, y, bw, bh, L"Продолжить летопись", B_PRIMARY, 1, A_CONTINUE, 0); y += bh + gap;
        w_button(x0, y, bw, bh, L"Новая игра", B_SECONDARY, 1, A_NEW, 0); y += bh + gap;
    } else {
        w_button(x0, y, bw, bh, L"Новая игра", B_PRIMARY, 1, A_NEW, 0); y += bh + gap;
    }
    w_button(x0, y, bw, bh, L"Как играть", B_SECONDARY, 1, A_HELP, 0); y += bh + gap;
    int tw3 = (bw - 2 * gap) / 3;
    w_button(x0, y, tw3, SC(36), L"Тема", B_GHOST, 1, A_THEME, 0);
    w_button(x0 + tw3 + gap, y, tw3, SC(36), L"Об игре", B_GHOST, 1, A_CREDITS, 0);
    w_button(x0 + 2 * (tw3 + gap), y, tw3, SC(36), L"Выход", B_GHOST, 1, A_EXIT, 0);
    int stackBottom = y + SC(36);   /* низ колонки кнопок */

    if (wide) {
        int mx = x0 + colW + SC(30), my = SC(26) + 9 * cell + SC(30);
        int mw = g_w - mx - x0 / 2, mh = g_h - my - SC(150);
        if (mw > SC(260) && mh > SC(220)) {
            d_rrect(mx, my, mw, mh, SC(8), P.surface, P.line);
            map_draw(mx + 1, my + 1, mw - 2, mh - 2, 0);
            /* восемь эпох */
            int ey = my + mh + SC(14), ew = mw / 8;
            for (int e = 1; e <= ERA_COUNT; e++) {
                int ex = mx + (e - 1) * ew;
                d_fill(ex, ey, ew - SC(4), SC(3), e % 2 ? P.red : mix(P.red, P.bg, 120));
                d_text(ex, ey + SC(8), roman(e), F_SMALL_B, P.ink);
                wchar_t yr[16];
                swprintf(yr, 16, L"%d", ERAS[e - 1].from);
                d_text(ex, ey + SC(26), yr, F_TINY, P.muted);
                hit(ex, ey, ew - SC(4), SC(44), A_HOVER_ERA, e);
            }
        }
    }
    wchar_t foot[200];
    swprintf(foot, 200, L"%d технологий · %d строек · %d деятелей · %d событий · %d вопросов летописца",
             TECH_COUNT, PROJECT_COUNT, PEOPLE_COUNT, HIST_COUNT + RAND_COUNT, QUIZ_COUNT);
    /* издатели — у нижнего орнамента, но никогда не выше низа колонки кнопок */
    int orn = g_h - SC(26) - 9 * cell;
    int lh = d_font_h(F_SMALL_B) + SC(3);
    int fy = orn - SC(26), cy = fy - SC(24) - lh;
    if (cy < stackBottom + SC(12)) { cy = stackBottom + SC(12); fy = cy + lh + SC(24); }
    wchar_t cr[240];
    swprintf(cr, 240, L"Издатели: %ls %ls", CREDIT_SCHOOL, CREDIT_HONOR);
    d_text(x0, cy, cr, F_SMALL_B, P.ink);
    swprintf(cr, 240, L"%ls, %ls", CREDIT_AUTHOR, CREDIT_CLASS);
    d_text(x0, cy + lh, cr, F_SMALL_B, P.ink);
    if (fy + d_font_h(F_SMALL) <= orn)   /* строка статистики — только если остаётся место */
        d_text(x0, fy, U.status[0] ? U.status : foot, F_SMALL, P.muted);
}

/* ---------- шапка ---------- */
static void draw_header(int *outH)
{
    Income inc;
    game_income(&G, &inc);
    int pad = SC(16), H = SC(88);
    d_fill(0, 0, g_w, H, P.surface);
    d_fill(0, H - 1, g_w, 1, P.line);

    wchar_t s[64];
    swprintf(s, 64, L"%d", G.year);
    d_text(pad, SC(4), s, F_ERA_XL, P.ink);
    /* часы минутного дохода справа от года */
    if (!G.over) {
        int px = pad + d_text_w(s, F_ERA_XL) + SC(26), py = SC(30), pr = SC(13);
        int left = (60000 - U.minuteMs + 999) / 1000;   /* секунды до выплаты, с округлением вверх */
        if (left < 1) left = 1;
        if (left > 60) left = 60;
        d_circle(px, py, pr, P.surface2, P.line, 1);
        d_pie(px, py, pr, U.minuteMs / 60000.0, P.gold);
        d_circle(px, py, pr * 45 / 100, P.surface, CLR_INVALID, 1);
        wchar_t mt[40];
        swprintf(mt, 40, L"%d:%02d", left / 60, left % 60);
        d_text(px + pr + SC(8), py - SC(16), L"доход через", F_TINY, P.muted);
        d_text(px + pr + SC(8), py - SC(2), mt, F_SMALL_B, P.gold);
        hit(px - pr, py - pr, pr * 2 + SC(80), pr * 2, A_HOVER_MIN, 0);
    }
    wchar_t era[160];
    swprintf(era, 160, L"%ls · %ls", roman(G.era), ERAS[G.era - 1].name);
    int yearW = SC(270);
    d_ellipsis(pad, SC(60), yearW - SC(10), era, F_SMALL_B, P.red);

    /* кнопки справа */
    int bw = SC(196), bx = g_w - pad - bw;
    if (G.over) w_button(bx, SC(12), bw, SC(40), L"Итоги летописи", B_PRIMARY, 1, A_FINAL, 0);
    else {
        wchar_t lbl[64];
        swprintf(lbl, 64, L"Завершить ход  \x2192 %d", game_next_year(&G));
        w_button(bx, SC(12), bw, SC(40), lbl, B_PRIMARY, U.modal == M_NONE, A_END_TURN, 0);
    }
    w_button(bx, SC(56), bw / 2 - SC(3), SC(26), L"Меню", B_GHOST, 1, A_MENU, 0);
    w_button(bx + bw / 2 + SC(3), SC(56), bw / 2 - SC(3), SC(26), L"Справка", B_GHOST, 1, A_HELP, 0);

    /* ресурсы */
    int rx = pad + yearW, rw = bx - SC(16) - rx;
    /* в одну строку — только если колонка вмещает самое длинное: «4,7 млн +0,27%/год» */
    int cols = rw >= 8 * SC(172) ? 8 : 4, rows = cols == 8 ? 1 : 2;
    int cw = rw / cols, ch = rows == 1 ? SC(52) : SC(38);
    int ry = rows == 1 ? SC(18) : SC(6);
    double ecoD = inc.total[R_ECO] + (100.0 - G.eco) * 0.1;
    for (int i = 0; i < 8; i++) {
        int cx = rx + (i % cols) * cw, cy = ry + (i / cols) * (ch + SC(4));
        static const int icons[8] = { IC_GOLD, IC_SCI, IC_IND, IC_POP, IC_STAB, IC_CULT, IC_ECO, IC_DEF };
        static const wchar_t *names[8] = { L"Казна", L"Знания", L"Производство", L"Население", L"Стабильность", L"Культура", L"Экология", L"Оборона" };
        COLORREF ic = i == 0 ? P.gold : i == 1 ? P.blue : i == 2 ? mix(P.gold, P.red, 90) : i == 6 ? P.green : i == 7 ? P.red : P.ink;
        wchar_t val[40], del[48];
        del[0] = 0;
        switch (i) {
        case 0: fmt_num(G.gold, val, 40); swprintf(del, 48, L"+%ld", lround(inc.total[R_GOLD])); break;
        case 1: fmt_num(G.sci, val, 40);  swprintf(del, 48, L"+%ld", lround(inc.total[R_SCI])); break;
        case 2: fmt_num(G.ind, val, 40);  swprintf(del, 48, L"+%ld", lround(inc.total[R_IND])); break;
        case 3: fmt_pop(G.pop, val, 40);
                swprintf(del, 48, L"%ls%.2f%%/год", inc.growYear < 0 ? L"\x2212" : L"+", fabs(inc.growYear * 100));
                for (wchar_t *p = del; *p; p++) if (*p == L'.') *p = L',';
                break;
        case 4: swprintf(val, 40, L"%ld", lround(G.stab)); swprintf(del, 48, L"\x2192 %ld", lround(inc.stabTarget)); break;
        case 5: fmt_num(G.cult, val, 40); swprintf(del, 48, L"+%ld", lround(inc.total[R_CULT])); break;
        case 6: swprintf(val, 40, L"%ld", lround(G.eco));
                swprintf(del, 48, L"%ls%ld", ecoD < -0.5 ? L"\x2212" : L"+", labs(lround(ecoD))); break;
        case 7: swprintf(val, 40, L"%ld", lround(inc.total[R_DEF])); break;
        }
        int hv = hover(A_HOVER_RES, i);
        if (hv) d_rrect(cx, cy, cw - SC(6), ch, SC(5), P.surface2, CLR_INVALID);
        d_icon(icons[i], cx + SC(6), cy + (ch - SC(18)) / 2, SC(18), ic);
        /* всё в своей колонке: прибавка не наезжает на соседа — сначала короче («+0,26%» без «/год»),
         * потом в строку названия, иначе не показывается (подсказка при наведении объясняет) */
        int tx = cx + SC(30), right = cx + cw - SC(10);
        int vf = rows == 1 ? F_H3 : F_BODY_B;
        int ny = cy + (rows == 1 ? SC(2) : SC(1)), vy = cy + (rows == 1 ? SC(17) : SC(15)), dy = cy + (rows == 1 ? SC(21) : SC(17));
        d_ellipsis(tx, ny, right - tx, names[i], F_TINY, P.muted);
        d_ellipsis(tx, vy, right - tx, val, vf, P.ink);
        if (del[0]) {
            COLORREF dc = del[0] == L'\x2212' ? P.red : (i == 4 ? P.muted : P.green);
            int dx = tx + d_text_w(val, vf) + SC(rows == 1 ? 6 : 5);
            if (dx + d_text_w(del, F_TINY) > right) {
                wchar_t *slash = wcschr(del, L'/');
                if (slash) *slash = 0;                   /* «+0,26%/год» → «+0,26%» */
            }
            if (dx + d_text_w(del, F_TINY) <= right) d_text(dx, dy, del, F_TINY, dc);
            else {
                int nx = tx + d_text_w(names[i], F_TINY) + SC(6);
                if (nx + d_text_w(del, F_TINY) <= right) d_text(nx, ny, del, F_TINY, dc);
            }
        }
        hit(cx, cy, cw - SC(6), ch, A_HOVER_RES, i);
    }
    *outH = H;
}

/* ---------- лента эпох: каждый ход — равная доля ---------- */
static void draw_timeline(int x, int y, int w, int h)
{
    int n = TURN_COUNT;
    for (int e = 1; e <= ERA_COUNT; e++) {
        int a = -1, b = -1;
        for (int t = 0; t < n; t++) if (era_of_year(TURN_YEARS[t]) == e) { if (a < 0) a = t; b = t; }
        if (a < 0) continue;
        int sx = x + w * a / n, ex = x + w * (b + 1) / n;
        int cur = e == G.era;
        COLORREF bg = cur ? P.redSoft : (e % 2 ? P.surface2 : mix(P.surface2, P.bg, 120));
        d_fill(sx, y, ex - sx - 1, h, bg);
        /* пройденные ходы */
        int done = 0;
        for (int t = a; t <= b; t++) if (t < G.turn) done++;
        if (done) d_fill(sx, y + h - SC(3), (ex - sx - 1) * done / (b - a + 1), SC(3), mix(P.red, P.surface2, 80));
        wchar_t lab[96];
        swprintf(lab, 96, L"%ls  %d", roman(e), ERAS[e - 1].from);
        int lw = d_text_w(lab, F_TINY);
        if (lw < ex - sx - SC(6)) d_text(sx + SC(5), y + (h - d_font_h(F_TINY)) / 2 - SC(1), lab, F_TINY, cur ? P.red : P.muted);
        else if (d_text_w(roman(e), F_TINY) < ex - sx - SC(2)) d_text(sx + SC(2), y + (h - d_font_h(F_TINY)) / 2 - SC(1), roman(e), F_TINY, P.muted);
        hit(sx, y, ex - sx, h, A_HOVER_ERA, e);
    }
    int t = G.turn < n ? G.turn : n - 1;
    int mx = x + w * t / n + (w / n) / 2;
    d_fill(mx - SC(1), y - SC(4), SC(3), h + SC(8), P.red);
    POINT tri[4] = { { mx - SC(5), y - SC(8) }, { mx + SC(6), y - SC(8) }, { mx, y - SC(2) }, { mx - SC(5), y - SC(8) } };
    d_poly(tri, 4, P.red, CLR_INVALID, 1);
}

/* ---------- панель карты ---------- */
static void draw_map_panel(int x, int y, int w, int h)
{
    d_rrect(x, y, w, h, SC(8), P.surface, P.line);
    int pad = SC(14);
    /* справа: кнопка «развернуть» и правитель эпохи */
    int bw = SC(96), bx = x + w - pad - bw;
    w_button(bx, y + SC(12), bw, SC(28), L"Развернуть", B_SECONDARY, 1, A_MAP_FULL, 0);
    int titleR = bx - SC(10);
    int ri = ruler_at(G.year);
    if (ri >= 0 && w > SC(520)) {
        const Ruler *R = &RULERS[ri];
        int rw = SC(176), rx = bx - SC(10) - rw, ry = y + SC(8);
        int ph = SC(44), pw = ph * 4 / 5;
        int hv = hover(A_RULER, ri);
        if (hv) d_rrect(rx - SC(4), ry - SC(2), rw + SC(8), ph + SC(4), SC(6), P.surface2, CLR_INVALID);
        portrait_box(R->id, R->name, rx, ry, pw, ph, P.gold);
        d_text(rx + pw + SC(8), ry + SC(4), L"Правит", F_TINY, P.muted);
        d_ellipsis(rx + pw + SC(8), ry + SC(18), rw - pw - SC(10), R->name, F_SMALL_B, P.ink);
        hit(rx - SC(4), ry - SC(2), rw + SC(8), ph + SC(4), A_RULER, ri);
        titleR = rx - SC(10);
    }
    /* название — целиком: шрифт поменьше, если не помещается (без «…» посреди слова) */
    int tw = titleR - x - pad, tf = F_ERA_M;
    static const int TF[] = { F_ERA_M, F_H2, F_H3, F_BODY_B };
    for (int i = 0; i < 4; i++) { tf = TF[i]; if (d_text_w(ERAS[G.era - 1].polity, tf) <= tw) break; }
    int th = d_font_h(F_ERA_M);
    d_ellipsis(x + pad, y + SC(10) + (th - d_font_h(tf)) / 2, tw, ERAS[G.era - 1].polity, tf, P.ink);
    static const wchar_t *SUB3D[] = { L"3D-карта: колесо — ближе, мышь — повернуть · флажки — задания",
                                      L"3D-карта: колесо — ближе, мышь — повернуть", L"колесо — ближе, мышь — повернуть" };
    const wchar_t *sub = map3d_on() ? SUB3D[0] :
                         art_map_available() ? L"3D-карта: постройки появляются у городов · флажки — задания" : L"Карта земель · контур современной границы для ориентира";
    for (int i = 1; map3d_on() && i < 3 && d_text_w(sub, F_TINY) > tw; i++) sub = SUB3D[i];
    d_ellipsis(x + pad, y + SC(10) + th, tw, sub, F_TINY, P.muted);
    int top = y + SC(16) + d_font_h(F_ERA_M) + d_font_h(F_TINY);
    int foot = SC(56);
    map_draw(x + 2, top, w - 4, h - (top - y) - foot, 1);

    /* сводка */
    int fy = y + h - foot + SC(6);
    d_fill(x + pad, fy - SC(6), w - 2 * pad, 1, P.line);
    wchar_t s[4][48];
    swprintf(s[0], 48, L"%d/%d", game_tech_count(&G), TECH_COUNT);
    swprintf(s[1], 48, L"%d/%d", game_proj_count(&G), PROJECT_COUNT);
    swprintf(s[2], 48, L"%d/%d", game_people_count(&G), PEOPLE_COUNT);
    int ach = 0;
    for (int a = 0; a < ACH_COUNT; a++) ach += G.ach[a];
    swprintf(s[3], 48, L"%d/%d", ach, ACH_COUNT);
    static const wchar_t *lab[4] = { L"Технологии", L"Стройки", L"Деятели", L"Награды" };
    int cw = (w - 2 * pad) / 4;
    for (int i = 0; i < 4; i++) {
        d_text(x + pad + i * cw, fy, lab[i], F_TINY, P.muted);
        d_text(x + pad + i * cw, fy + SC(15), s[i], F_BODY_B, P.ink);
    }
    /* легенда — в нижнем левом углу карты */
    int lx = x + pad, ly = fy - SC(32);
    {   /* отступы — по ширине подписей, на подложке, чтобы читалось поверх карты */
        int fh = d_font_h(F_TINY), w1 = d_text_w(L"можно строить", F_TINY), w2 = d_text_w(L"построено", F_TINY);
        int lw = SC(16) + w1 + SC(14) + SC(12) + w2 + SC(8);
        d_blend(lx - SC(6), ly - SC(3), lw + SC(6), fh + SC(6), P.surface, 190);
        d_circle(lx + SC(5), ly + fh / 2, SC(5), CLR_INVALID, P.blue, 2);
        d_text(lx + SC(16), ly, L"можно строить", F_TINY, P.muted);
        int qx = lx + SC(16) + w1 + SC(14);
        d_fill(qx, ly + fh / 2 - SC(4), SC(8), SC(8), P.red);
        d_text(qx + SC(12), ly, L"построено", F_TINY, P.muted);
    }
}

/* ---------- вкладки ---------- */
static void draw_tabs_panel(int x, int y, int w, int h)
{
    d_rrect(x, y, w, h, SC(8), P.surface, P.line);
    int tx = x + SC(8), ty = y + SC(8), th = SC(34);
    int total = 0;
    for (int i = 0; i < TAB_COUNT; i++) total += d_text_w(TAB_NAMES[i], F_SMALL_B) + SC(22);
    int f = total > w - SC(16) ? F_TINY : F_SMALL_B;
    for (int i = 0; i < TAB_COUNT; i++) {
        int tw = d_text_w(TAB_NAMES[i], f) + (f == F_TINY ? SC(14) : SC(22));
        int sel = U.tab == i, hv = hover(A_TAB, i);
        if (sel) d_rrect(tx, ty, tw, th, SC(5), P.redSoft, CLR_INVALID);
        else if (hv) d_rrect(tx, ty, tw, th, SC(5), P.surface2, CLR_INVALID);
        d_text_c(tx, ty + (th - d_font_h(f)) / 2, tw, TAB_NAMES[i], f, sel ? P.red : P.ink);
        /* метка «есть что сделать» */
        int dot = 0;
        if (i == TAB_QUIZ && !G.over && !G.quizAnswered) dot = 1;
        if (i == TAB_PEOPLE) for (int p = 0; p < PEOPLE_COUNT && !dot; p++) dot = game_person_state(&G, p) == HS_AVAILABLE;
        if (i == TAB_ARMY) dot = !G.over && ((G.occupied && G.warKind == WAR_NONE) || (G.warKind != WAR_NONE && G.warNews));
        if (i == TAB_QUESTS) for (int q = 0; q < QUEST_COUNT && !dot; q++)
            dot = game_quest_state(&G, q) == QS_ACTIVE && QUESTS[q].deadline < game_next_year(&G);
        if (dot) d_circle(tx + tw - SC(5), ty + SC(6), SC(3), P.red, CLR_INVALID, 1);
        hit(tx, ty, tw, th, A_TAB, i);
        tx += tw + SC(2);
    }
    int cy = ty + th + SC(8);
    d_fill(x + SC(8), cy - SC(4), w - SC(16), 1, P.line);
    int cx = x + SC(16), cw = w - SC(36), ch = y + h - cy - SC(8);
    U.panelX = x; U.panelY = cy; U.panelW = w; U.panelH = ch;
    U.view = ch;
    int maxS = U.content[U.tab] - ch;
    if (U.scroll[U.tab] > maxS) U.scroll[U.tab] = maxS > 0 ? maxS : 0;
    if (U.scroll[U.tab] < 0) U.scroll[U.tab] = 0;
    d_clip(x + 1, cy, w - 2, ch);
    hits_clip(x + 1, cy, w - 2, ch);
    U.content[U.tab] = tab_draw(U.tab, cx, cy + SC(4) - U.scroll[U.tab], cw, ch);
    hits_unclip();
    d_unclip();
    w_scrollbar(x + w - SC(12), cy + SC(2), ch - SC(4), U.content[U.tab], ch, U.scroll[U.tab]);
}

static void draw_game(void)
{
    ui_set_era_font(G.era);
    d_fill(0, 0, g_w, g_h, P.bg);
    int H;
    draw_header(&H);
    int cell = SC(2) > 2 ? SC(2) : 2;
    d_ornament(0, H + SC(4), g_w, cell, P.red, mix(P.red, P.bg, 150));
    int y = H + SC(4) + 9 * cell + SC(8);
    int pad = SC(16);
    draw_timeline(pad, y, g_w - 2 * pad, SC(22));
    y += SC(22) + SC(12);
    int statusH = SC(28);
    int bodyH = g_h - statusH - y - SC(6);
    int mapW = g_w * 42 / 100;
    if (mapW < SC(360)) mapW = SC(360);
    if (mapW > SC(760)) mapW = SC(760);
    draw_map_panel(pad, y, mapW, bodyH);
    draw_tabs_panel(pad + mapW + SC(12), y, g_w - 2 * pad - mapW - SC(12), bodyH);

    /* строка состояния */
    int sy = g_h - statusH;
    d_fill(0, sy, g_w, statusH, P.surface);
    d_fill(0, sy, g_w, 1, P.line);
    const wchar_t *keys = L"Enter — ход · 1–9 — вкладки · M — карта · Ctrl+S — сохранить · Ctrl+колесо — масштаб · F1 — справка";
    int kw = d_text_w(keys, F_TINY);
    d_text(g_w - pad - kw, sy + (statusH - d_font_h(F_TINY)) / 2, keys, F_TINY, P.muted);
    wchar_t def[200];
    if (G.over) swprintf(def, 200, L"Летопись завершена · сложность «%ls»", DIFFICULTIES[G.difficulty].name);
    else swprintf(def, 200, L"Ход %d из %d · сложность «%ls» · курс «%ls»", G.turn + 1, TURN_COUNT,
                  DIFFICULTIES[G.difficulty].name, PRIORITIES[G.priority].name);
    d_ellipsis(pad, sy + (statusH - d_font_h(F_SMALL)) / 2, g_w - 3 * pad - kw, U.status[0] ? U.status : def, F_SMALL, U.status[0] ? P.ink : P.muted);
}

static void draw_toast(void)
{
    if (!U.toast[0]) return;
    /* длинные сообщения (сводки боёв) переносятся по строкам */
    int maxw = g_w * 6 / 10 > SC(420) ? g_w * 6 / 10 : SC(420);
    int tw = d_text_w(U.toast, F_BODY_B);
    if (tw > maxw - SC(40)) tw = maxw - SC(40);
    int th = d_wrap(0, 0, tw, U.toast, F_BODY_B, 0, 0);
    int w = tw + SC(40), h = th + SC(24) > SC(42) ? th + SC(24) : SC(42);
    int x = g_w - w - SC(20), y = g_h - SC(28) - h - SC(14);
    d_rrect(x + SC(2), y + SC(3), w, h, SC(8), mix(P.bg, P.shadow, 120), CLR_INVALID);
    d_rrect(x, y, w, h, SC(8), P.ink, CLR_INVALID);
    d_fill(x + SC(12), y + SC(12), SC(4), h - SC(24), P.red);
    d_wrap(x + SC(24), y + (h - th) / 2, tw, U.toast, F_BODY_B, P.surface, 1);
}

void ui_paint(void)
{
    if (U.screen == SCR_TITLE) draw_title();
    else draw_game();
    if (U.modal != M_NONE) { hits_reset(); modal_draw(); }
    draw_toast();
}

/* ---------- логика переходов ---------- */
void ui_after_change(void)
{
    if (U.screen != SCR_GAME || U.modal != M_NONE) { ui_invalidate(); return; }
    QItem q;
    if (game_pending(&G, &q)) {
        U.modalScroll = 0;
        U.evResult = 0;
        if (q.type == Q_HIST || q.type == Q_RAND) { U.modal = M_EVENT; U.evShown = game_event_ptr(&q); }
        else if (q.type == Q_ERA) U.modal = M_ERA;
        else U.modal = M_FINAL;
    }
    /* событий нет — очередь заставок: взлёт ракеты, приближение к Минску в 1984 и т. п. */
    if (U.modal == M_NONE && !G.over && !fx_ui_idle()) citynet_ui_idle();
    ui_invalidate();
}

static void close_modal(void)
{
    if (U.modal == M_FINAL) { QItem q; if (game_pending(&G, &q) && q.type == Q_FINAL) game_ack(&G); }
    U.modal = M_NONE;
    U.modalScroll = 0;
    ui_after_change();
}

/* новая партия, загрузка, выход в меню: вид карты и выбор армии — с начала */
static void reset_view(void)
{
    U.camInit = 0;
    U.selArmy = -1;
}

static void start_game(void)
{
    game_new(&G, U.diffPick, GetTickCount() ^ 0x5bd1e995u);
    reset_view();
    U.minuteMs = 0;
    memset(U.scroll, 0, sizeof U.scroll);
    U.tab = TAB_TECH;
    U.eraFilter = 1;
    U.projFilter = 0;
    U.city = -1;
    U.screen = SCR_GAME;
    U.modal = M_NONE;
    map_invalidate();
    ui_save();
    ui_after_change();
}

void ui_click(int act, int arg)
{
    /* действия модулей: армии на карте, городской транспорт, анимации */
    if (front_ui_click(act, arg) || citynet_ui_click(act, arg) || fx_ui_click(act, arg)) { ui_invalidate(); return; }
    switch (act) {
    case A_NEW: U.modal = M_NEWGAME; U.modalScroll = 0; break;
    case A_CONTINUE:
        if (ui_load()) {
            reset_view();
            U.screen = SCR_GAME; U.modal = M_NONE; U.eraFilter = G.era; U.city = -1;
            memset(U.scroll, 0, sizeof U.scroll);
            map_invalidate();
            if (G.over) { U.modal = M_FINAL; }
            ui_after_change();
        } else ui_toast(L"Сохранение не найдено или повреждено");
        break;
    case A_DIFF: U.diffPick = arg; break;
    case A_START: start_game(); break;
    case A_HELP: U.modal = M_HELP; U.modalScroll = 0; break;
    case A_EXIT: PostMessageW(g_hwnd, WM_CLOSE, 0, 0); break;
    case A_THEME: ui_toggle_theme(); break;
    case A_TAB: U.tab = arg; break;
    case A_ERAF: U.eraFilter = arg; U.scroll[TAB_TECH] = 0; break;
    case A_PFILTER: U.projFilter = arg; U.scroll[TAB_PROJ] = 0; break;
    case A_RESEARCH:
        if (game_research(&G, arg)) ui_toast(L"Открыто: %ls", TECHS[arg].name);
        else ui_toast(L"Не хватает знаний");
        break;
    case A_TECH: U.modal = M_TECH; U.modalArg = arg; U.modalScroll = 0; break;
    case A_BUILD:
        if (game_build(&G, arg)) { ui_toast(L"Построено: %ls", PROJECTS[arg].name); citynet_ui_built(arg); }
        else ui_toast(L"Не хватает казны или производства");
        break;
    case A_PROJ: U.modal = M_PROJ; U.modalArg = arg; U.modalScroll = 0; break;
    case A_HIRE:
        if (game_hire(&G, arg)) { ui_toast(L"%ls с вами", PEOPLE[arg].name); fx_ui_hired(arg); }
        else ui_toast(L"Не хватает казны");
        break;
    case A_PERSON: U.modal = M_PERSON; U.modalArg = arg; U.modalScroll = 0; break;
    case A_PRIORITY: G.priority = arg; ui_toast(L"Курс: %ls", PRIORITIES[arg].name); break;
    case A_QUIZ:
        if (game_quiz_answer(&G, arg)) ui_toast(L"Верно! +%d знаний", game_quiz_reward(&G));
        else ui_toast(L"Неверно — загляните в пояснение");
        break;
    case A_END_TURN:
        if (U.modal == M_NONE && !G.over) {
            int era = G.era;
            game_end_turn(&G);
            if (G.era != era) { U.eraFilter = G.era; memset(U.scroll, 0, sizeof U.scroll); }
            if (G.warNews && G.warReport[0]) ui_toast(L"%ls", G.warReport);
            map_invalidate();
            if (!G.over) ui_save();
            ui_after_change();
        }
        break;
    case A_CHOICE:
        game_choose(&G, arg, &U.outcome);
        U.evChoice = arg;
        U.evResult = 1;
        U.modalScroll = 0;
        map_invalidate();
        break;
    case A_NEXT: U.modal = M_NONE; U.evResult = 0; ui_after_change(); break;
    case A_ERA_START: game_ack(&G); U.modal = M_NONE; ui_after_change(); break;
    case A_CLOSE: close_modal(); break;
    case A_FINAL: U.modal = M_FINAL; U.modalScroll = 0; break;
    case A_MENU: U.modal = M_MENU; break;
    case A_SAVE: if (ui_save()) ui_toast(L"Игра сохранена"); else ui_toast(L"Не удалось сохранить"); U.modal = M_NONE; break;
    case A_LOAD:
        if (ui_load()) { reset_view(); U.modal = M_NONE; U.eraFilter = G.era; map_invalidate(); ui_toast(L"Сохранение загружено"); ui_after_change(); }
        else ui_toast(L"Сохранение не найдено");
        break;
    case A_TO_TITLE:
        if (U.screen == SCR_GAME && !G.over) ui_save();
        U.hasSave = U.screen == SCR_GAME && !G.over;
        U.screen = SCR_TITLE; U.modal = M_NONE; reset_view(); map_invalidate();
        break;
    case A_CITY: U.city = arg; U.modal = M_CITY; U.modalArg = arg; U.modalScroll = 0; break;
    case A_SELL: {
        int gain = game_sell_ind(&G);
        if (gain) ui_toast(L"Продано производство: +%d казны", gain); else ui_toast(L"Мало производства для продажи");
        break;
    }
    case A_ZOOM: ui_zoom(arg); break;
    case A_CREDITS: U.modal = M_CREDITS; U.modalScroll = 0; break;
    case A_MAP_FULL: U.modal = M_MAP; break;
    case A_QUEST: U.modal = M_NONE; U.tab = TAB_QUESTS; U.scroll[TAB_QUESTS] = 0; break;
    case A_RULER: U.modal = M_RULER; U.modalArg = arg; U.modalScroll = 0; break;
    case A_RECRUIT:
        if (arg < 0 || arg >= UNIT_COUNT) break;
        if (war_recruit(&G, arg)) ui_toast(L"В строю: %ls (%d)", UNIT_TYPES[arg].name, G.units[arg]);
        else if (war_army_size(&G) >= war_army_cap(&G)) ui_toast(L"Войско достигло предела (%d) — предел растёт с населением", war_army_cap(&G));
        else ui_toast(L"Не хватает казны или производства");
        break;
    case A_DISBAND:
        if (arg >= 0 && arg < UNIT_COUNT && war_disband(&G, arg)) ui_toast(L"Распущено: %ls", UNIT_TYPES[arg].name);
        break;
    case A_DECLARE: {
        const wchar_t *why = war_cannot_declare(&G, WAR_CONQUEST, arg);
        if (why) ui_toast(L"%ls", why);
        else if (war_declare(&G, WAR_CONQUEST, arg)) { ui_toast(L"Начат поход: %ls. Бой — в конце хода", PROVINCES[arg].name); map_invalidate(); }
        break;
    }
    case A_LIBERATE: {
        const wchar_t *why = war_cannot_declare(&G, WAR_LIBERATION, arg);
        if (why) ui_toast(L"%ls", why);
        else if (war_declare(&G, WAR_LIBERATION, arg)) { ui_toast(L"Начата операция: %ls. Бой — в конце хода", REGION_NAMES[arg]); map_invalidate(); }
        break;
    }
    case A_PEACE: { int lib = G.warKind == WAR_LIBERATION; war_peace(&G); ui_toast(L"%ls", lib ? L"Операция остановлена: область остаётся под оккупацией" : L"Поход окончен: войска вернулись без победы"); map_invalidate(); break; }
    case A_PROVINCE: U.modal = M_NONE; U.tab = TAB_ARMY; break;
    case A_MAP_CAM: map_cam_action(arg); U.mapFast = 0; break;
    }
    ui_invalidate();
}

/* правая кнопка без перетаскивания (например, приказ армии идти в точку) */
void ui_rclick(int act, int arg)
{
    if (front_ui_rclick(act, arg)) ui_invalidate();
}

void ui_key(WPARAM vk)
{
    if (U.modal == M_CITYNET && citynet_ui_key(vk)) { ui_invalidate(); return; }
    if (U.modal == M_LAUNCH && fx_ui_key(vk)) { ui_invalidate(); return; }
    if (vk == VK_F1) { U.modal = M_HELP; U.modalScroll = 0; ui_invalidate(); return; }
    if (vk == VK_F2) { ui_toggle_theme(); return; }
    if (U.modal == M_EVENT) {
        if (U.evResult) { if (vk == VK_RETURN || vk == VK_SPACE || vk == VK_ESCAPE) ui_click(A_NEXT, 0); return; }
        if (vk >= '1' && vk <= '3') {
            int c = (int)(vk - '1');
            if (U.evShown && game_choice_ok(&G, U.evShown, c)) ui_click(A_CHOICE, c);
        }
        return;
    }
    if (U.modal == M_ERA) { if (vk == VK_RETURN || vk == VK_SPACE) ui_click(A_ERA_START, 0); return; }
    if (U.modal == M_NEWGAME) {
        if (vk >= '1' && vk <= '3') U.diffPick = (int)(vk - '1');
        else if (vk == VK_RETURN) ui_click(A_START, 0);
        else if (vk == VK_ESCAPE) close_modal();
        ui_invalidate();
        return;
    }
    if (U.modal == M_MAP && map3d_on()) {   /* клавиши камеры на большой карте */
        int a = vk == VK_LEFT ? MC_ROT_LEFT : vk == VK_RIGHT ? MC_ROT_RIGHT : vk == VK_UP ? MC_TILT_UP : vk == VK_DOWN ? MC_TILT_DOWN :
                vk == VK_OEM_PLUS || vk == VK_ADD ? MC_ZOOM_IN : vk == VK_OEM_MINUS || vk == VK_SUBTRACT ? MC_ZOOM_OUT :
                vk == VK_HOME ? MC_RESET : -1;
        if (a >= 0) { map_cam_action(a); ui_invalidate(); return; }
    }
    if (U.modal != M_NONE) {
        if (vk == VK_ESCAPE || vk == VK_RETURN || (U.modal == M_MAP && vk == 'M')) close_modal();
        else if (vk == VK_DOWN || vk == VK_NEXT) { U.modalScroll += vk == VK_NEXT ? SC(300) : SC(40); ui_invalidate(); }
        else if (vk == VK_UP || vk == VK_PRIOR) { U.modalScroll -= vk == VK_PRIOR ? SC(300) : SC(40); if (U.modalScroll < 0) U.modalScroll = 0; ui_invalidate(); }
        return;
    }
    if (U.screen == SCR_TITLE) {
        if (vk == VK_RETURN) ui_click(U.hasSave ? A_CONTINUE : A_NEW, 0);
        return;
    }
    if (vk == VK_RETURN) ui_click(G.over ? A_FINAL : A_END_TURN, 0);
    else if (vk == VK_ESCAPE || vk == VK_F10) ui_click(A_MENU, 0);
    else if (vk >= '1' && vk <= '9') { U.tab = (int)(vk - '1'); ui_invalidate(); }
    else if (vk == 'M') { U.modal = M_MAP; ui_invalidate(); }
    else if (vk == VK_TAB) { U.tab = (U.tab + (GetKeyState(VK_SHIFT) < 0 ? TAB_COUNT - 1 : 1)) % TAB_COUNT; ui_invalidate(); }
    else if (vk == VK_DOWN || vk == VK_NEXT || vk == VK_UP || vk == VK_PRIOR || vk == VK_HOME || vk == VK_END) {
        int d = vk == VK_DOWN ? SC(40) : vk == VK_UP ? -SC(40) : vk == VK_NEXT ? U.view - SC(40) : vk == VK_PRIOR ? -(U.view - SC(40)) : vk == VK_HOME ? -1000000 : 1000000;
        U.scroll[U.tab] += d;
        if (U.scroll[U.tab] < 0) U.scroll[U.tab] = 0;
        ui_invalidate();
    }
}

void ui_wheel(int delta, int x, int y)
{
    int step = -delta * SC(64) / 120;
    if (U.modal != M_NONE) {
        U.modalScroll += step;
        if (U.modalScroll > U.modalContent - U.modalView) U.modalScroll = U.modalContent - U.modalView;
        if (U.modalScroll < 0) U.modalScroll = 0;
    } else if (U.screen == SCR_GAME && x >= U.panelX && x < U.panelX + U.panelW && y >= U.panelY - SC(50) && y < U.panelY + U.panelH) {
        U.scroll[U.tab] += step;
        int maxS = U.content[U.tab] - U.view;
        if (U.scroll[U.tab] > maxS) U.scroll[U.tab] = maxS;
        if (U.scroll[U.tab] < 0) U.scroll[U.tab] = 0;
    }
    ui_invalidate();
}

static void first_sentence(const wchar_t *src, wchar_t *dst, int n)
{
    int i = 0;
    for (; src[i] && i < n - 1; i++) {
        dst[i] = src[i];
        if (src[i] == L'.' && (src[i + 1] == L' ' || !src[i + 1]) && i > 20) { i++; break; }
    }
    dst[i] = 0;
}

void ui_hover_status(int act, int arg)
{
    wchar_t buf[260];
    buf[0] = 0;
    /* подсказки модулей */
    front_ui_hover(act, arg, buf, 260);
    if (!buf[0]) citynet_ui_hover(act, arg, buf, 260);
    if (!buf[0]) fx_ui_hover(act, arg, buf, 260);
    if (buf[0]) { wcsncpy(U.status, buf, 259); U.status[259] = 0; return; }
    static const wchar_t *RES_HELP[8] = {
        L"Казна: оплачивает стройки, деятелей и решения. Растёт от населения, торговли и производства.",
        L"Знания: тратятся на открытия. Растут от школ, типографий, университетов и вопросов летописца.",
        L"Производство: второй ресурс для строек. Излишки можно продать во вкладке «Курс».",
        L"Население: растёт от медицины, земледелия и стабильности; падает от войн, эпидемий, низкой экологии. Зелёное число — рост за год.",
        L"Стабильность: каждый ход сдвигается к целевому уровню. Ниже 40 доходы падают, выше 75 — растут.",
        L"Культура: копится и даёт очки в итоговом счёте. Источник — храмы, книги, театры, газеты, кино.",
        L"Экология: падает от торфа, химии, нефти и мелиорации; ниже 50 тормозит рост населения.",
        L"Оборона: смягчает удары войн. Замки, бастионы, арсеналы, партизанская связь, авиация."
    };
    if (act == A_HOVER_RES && arg >= 0 && arg < 8) swprintf(buf, 260, L"%ls", RES_HELP[arg]);
    else if (act == A_HOVER_ERA && arg >= 1 && arg <= ERA_COUNT)
        swprintf(buf, 260, L"Эпоха %ls · %ls · %d–%d", roman(arg), ERAS[arg - 1].name, ERAS[arg - 1].from, ERAS[arg - 1].to);
    else if ((act == A_TECH || act == A_RESEARCH) && arg >= 0 && arg < TECH_COUNT) {
        wchar_t s[200]; first_sentence(TECHS[arg].desc, s, 200);
        swprintf(buf, 260, L"%ls (%d): %ls", TECHS[arg].name, TECHS[arg].year, s);
    } else if ((act == A_PROJ || act == A_BUILD) && arg >= 0 && arg < PROJECT_COUNT) {
        wchar_t s[200]; first_sentence(PROJECTS[arg].desc, s, 200);
        swprintf(buf, 260, L"%ls, %ls: %ls", PROJECTS[arg].name, CITIES[PROJECTS[arg].city].name, s);
    } else if ((act == A_PERSON || act == A_HIRE) && arg >= 0 && arg < PEOPLE_COUNT)
        swprintf(buf, 260, L"%ls (%ls) — %ls", PEOPLE[arg].name, PEOPLE[arg].life, PEOPLE[arg].role);
    else if (act == A_CITY && arg >= 0 && arg < CITY_COUNT) {
        int built = 0, avail = 0;
        for (int p = 0; p < PROJECT_COUNT; p++)
            if (PROJECTS[p].city == arg) { int st = game_proj_state(&G, p); built += st == PS_DONE; avail += st == PS_AVAILABLE; }
        swprintf(buf, 260, L"%ls · построено: %d · можно строить: %d — нажмите, чтобы открыть", CITIES[arg].name, built, avail);
    } else if (act == A_PRIORITY && arg >= 0 && arg < PRIORITY_COUNT)
        swprintf(buf, 260, L"%ls: %ls", PRIORITIES[arg].name, PRIORITIES[arg].desc);
    else if (act == A_HOVER_MIN) {
        MinuteBonus b;
        game_minute_preview(&G, &b);
        swprintf(buf, 260, L"Минутный доход: каждую минуту игры +%d казны, +%d знаний, +%d производства. Растёт от эпохи к эпохе.",
                 b.gold, b.sci, b.ind);
    }
    else if (act == A_QUEST && arg >= 0 && arg < QUEST_COUNT) {
        wchar_t goal[200];
        quest_describe_goal(arg, goal, 200);
        swprintf(buf, 260, L"Задание «%ls»: %ls", QUESTS[arg].title, goal);
    }
    else if (act == A_RULER && arg >= 0 && arg < RULER_COUNT)
        swprintf(buf, 260, L"%ls — %ls, %d–%d (%ls)", RULERS[arg].name, RULERS[arg].title, RULERS[arg].from, RULERS[arg].to, STATE_NAMES[RULERS[arg].state]);
    else if (act == A_HOVER_ACH && arg >= 0 && arg < ACH_COUNT)
        swprintf(buf, 260, L"%ls — %ls", ACHIEVEMENTS[arg].name, ACHIEVEMENTS[arg].desc);
    else if ((act == A_RECRUIT || act == A_DISBAND || act == A_HOVER_UNIT) && arg >= 0 && arg < UNIT_COUNT) {
        const UnitType *t = &UNIT_TYPES[arg];
        if (act == A_DISBAND) swprintf(buf, 260, L"Распустить одно соединение «%ls»: содержание уменьшится, деньги не вернутся.", t->name);
        else swprintf(buf, 260, L"%ls — %ls", t->name, t->desc);
    } else if ((act == A_DECLARE || act == A_PROVINCE) && arg >= 0 && arg < PROVINCE_COUNT) {
        wchar_t s[200]; first_sentence(PROVINCES[arg].desc, s, 200);
        swprintf(buf, 260, L"%ls (%ls, %d–%d): %ls", PROVINCES[arg].name, PROVINCES[arg].owner, PROVINCES[arg].from, PROVINCES[arg].to, s);
    } else if ((act == A_LIBERATE || act == A_HOVER_REGION) && arg >= 0 && arg < REGION_COUNT) {
        int occ = (G.occupied >> arg) & 1u;
        swprintf(buf, 260, L"%ls (центр — %ls): %ls", REGION_NAMES[arg], CITIES[REGION_CITY[arg]].name,
                 occ ? L"под оккупацией, сила врага слабеет с каждым годом" : L"свободна");
    } else if (act == A_PEACE) swprintf(buf, 260, L"%ls", G.warKind == WAR_LIBERATION ? L"Остановить операцию: область останется под оккупацией до новой попытки или операции «Багратион»." : L"Закончить поход без победы: войска вернутся, стабильность немного упадёт.");
    else if (act == A_HOVER_BATTLE && G.warReport[0]) swprintf(buf, 260, L"%ls", G.warReport);
    else if (act == A_MAP_CAM && arg >= 0 && arg <= MC_TILT_DOWN) {
        static const wchar_t *H[7] = { L"Отдалить карту (колесо мыши вниз)", L"Приблизить карту (колесо мыши вверх)",
            L"Повернуть карту влево (или тяните левой кнопкой мыши)", L"Повернуть карту вправо (или тяните левой кнопкой мыши)",
            L"Показать всю страну сверху-сбоку, как вначале", L"Смотреть сильнее сверху", L"Смотреть ниже, сбоку" };
        swprintf(buf, 260, L"%ls", H[arg]);
    }
    else if (act == A_HOVER_STAT && arg >= 20 && arg <= 23) {
        static const wchar_t *H[4] = {
            L"Сколько соединений в строю и сколько можно содержать: предел растёт с населением.",
            L"Сила наступления: решает исход походов и освобождения. Военные технологии усиливают все войска.",
            L"Оборона против набегов и осад: войско плюс крепости, замки и оборонные технологии.",
            L"Каждое соединение стоит казны каждый ход; во время войны (похода или освобождения) — в полтора раза больше. Пустая казна — дезертирство."
        };
        swprintf(buf, 260, L"%ls", H[arg - 20]);
    }
    wcsncpy(U.status, buf, 259);
    U.status[259] = 0;
}

/* Каждую секунду: копим реальное время партии; раз в минуту — доход. */
void ui_minute_tick(void)
{
    DWORD now = GetTickCount();
    DWORD dt = U.minuteLast ? now - U.minuteLast : 0;
    U.minuteLast = now;
    if (dt > 5000) dt = 1000;   /* после сна компьютера не начисляем «пропущенные» минуты */
    if (U.screen != SCR_GAME || G.over || IsIconic(g_hwnd)) return;
    front_ui_tick((int)dt);        /* армии идут и бьются сами, в реальном времени */
    U.minuteMs += (int)dt;
    if (U.minuteMs >= 60000) {
        U.minuteMs -= 60000;
        if (U.minuteMs >= 60000) U.minuteMs = 0;
        MinuteBonus b;
        if (game_minute_bonus(&G, &b))
            ui_toast(L"Минутный доход: +%d казны, +%d знаний, +%d производства", b.gold, b.sci, b.ind);
    }
    RECT r = { 0, 0, g_w, SC(90) };
    InvalidateRect(g_hwnd, &r, FALSE);
}

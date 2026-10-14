/* Модальные окна: события, эпохи, итоги, справка, меню, новая игра, карточки. */
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void eyebrow(int x, int *y, int icon, COLORREF ic, const wchar_t *s)
{
    int tx = x;
    if (icon >= 0) { d_icon(icon, x, *y, SC(20), ic); tx += SC(28); }
    SetTextCharacterExtra(d_dc(), 1);
    d_text(tx, *y + SC(2), s, F_SMALL_B, ic);
    SetTextCharacterExtra(d_dc(), 0);
    *y += SC(30);
}

static void upper(wchar_t *s)
{
    for (; *s; s++) {
        if (*s >= L'а' && *s <= L'я') *s = (wchar_t)(*s - 32);
        else if (*s == L'ё') *s = L'Ё';
    }
}

/* ---------- событие ---------- */
static int body_event(int x, int y, int w)
{
    const Event *e = U.evShown;
    if (!e) return 0;
    int y0 = y, isRand = e->year == 0;
    wchar_t eb[96], kn[32];
    wcsncpy(kn, KIND_NAMES[e->kind], 31); kn[31] = 0;
    upper(kn);
    if (isRand) swprintf(eb, 96, L"%d · %ls · СЛУЧАЙ", G.year, kn);
    else swprintf(eb, 96, L"%d · %ls", e->year, kn);
    eyebrow(x, &y, kind_icon(e->kind), kind_color(e->kind), eb);
    y += d_wrap(x, y, w, e->title, F_ERA_L, P.ink, 1) + SC(8);
    d_ornament(x, y, w < SC(260) ? w : SC(260), 2, P.red, mix(P.red, P.surface, 140));
    y += 18 + SC(12);
    y += d_wrap(x, y, w, e->text, F_BODY, P.ink, 1) + SC(18);

    if (!U.evResult) {
        for (int c = 0; c < e->nch; c++) {
            const Choice *ch = &e->ch[c];
            int ok = game_choice_ok(&G, e, c);
            wchar_t desc[400];
            choice_describe(&G, isRand, ch, desc, 400);
            wchar_t why[160];
            why[0] = 0;
            if (!ok) {
                if (ch->reqi >= 0 && !G.tech[ch->reqi]) swprintf(why, 160, L"Нужна технология «%ls»", TECHS[ch->reqi].name);
                else swprintf(why, 160, L"Не хватает казны");
            } else if (ch->reqi >= 0) swprintf(why, 160, L"Доступно благодаря технологии «%ls»", TECHS[ch->reqi].name);
            int tw = w - SC(60);
            int hL = d_wrap(0, 0, tw, ch->label, F_BODY_B, 0, 0);
            int hD = d_wrap(0, 0, tw, desc, F_SMALL, 0, 0);
            int hW = why[0] ? d_font_h(F_TINY) + SC(4) : 0;
            int h = SC(14) + hL + SC(4) + hD + hW + SC(14);
            int hv = ok && hover(A_CHOICE, c);
            d_rrect(x, y, w, h, SC(7), !ok ? mix(P.surface, P.surface2, 100) : hv ? P.redSoft : mix(P.surface, P.surface2, 160),
                    hv ? P.red : P.line);
            wchar_t num[4];
            swprintf(num, 4, L"%d", c + 1);
            d_circle(x + SC(24), y + SC(14) + d_font_h(F_BODY_B) / 2, SC(11), ok ? P.red : P.surface2, CLR_INVALID, 1);
            d_text_c(x + SC(13), y + SC(14) + (d_font_h(F_BODY_B) - d_font_h(F_SMALL_B)) / 2, SC(22), num, F_SMALL_B, ok ? P.onAccent : P.muted);
            int ty = y + SC(14);
            d_wrap(x + SC(48), ty, tw, ch->label, F_BODY_B, ok ? P.ink : P.muted, 1);
            ty += hL + SC(4);
            d_wrap(x + SC(48), ty, tw, desc, F_SMALL, P.muted, 1);
            ty += hD;
            if (why[0]) d_text(x + SC(48), ty + SC(3), why, F_TINY, ok ? P.blue : P.red);
            if (ok) hit(x, y, w, h, A_CHOICE, c);
            y += h + SC(10);
        }
        y += d_wrap(x, y, w, L"Клавиши 1–3 выбирают вариант.", F_TINY, P.muted, 1);
    } else {
        const Choice *ch = &e->ch[U.evChoice];
        wchar_t s[300];
        swprintf(s, 300, L"Ваше решение: %ls", ch->label);
        y += d_wrap(x, y, w, s, F_SMALL_B, P.red, 1) + SC(6);
        if (ch->result) y += d_wrap(x, y, w, ch->result, F_BODY, P.ink, 1) + SC(14);
        int oh = d_wrap(0, 0, w - SC(28), U.outcome.text, F_SMALL_B, 0, 0) + SC(24);
        d_rrect(x, y, w, oh, SC(6), mix(P.surface, P.surface2, 170), CLR_INVALID);
        d_fill(x, y + SC(8), SC(4), oh - SC(16), P.red);
        d_wrap(x + SC(16), y + SC(12), w - SC(28), U.outcome.text, F_SMALL_B, P.ink, 1);
        y += oh + SC(18);
        w_button(x + w - SC(180), y, SC(180), SC(42), L"Далее  (Enter)", B_PRIMARY, 1, A_NEXT, 0);
        y += SC(42);
    }
    return y - y0;
}

/* ---------- новая эпоха ---------- */
static int body_era(int x, int y, int w)
{
    int y0 = y, e = G.era;
    wchar_t eb[96];
    swprintf(eb, 96, L"ЭПОХА %ls · %d–%d", roman(e), ERAS[e - 1].from, ERAS[e - 1].to);
    eyebrow(x, &y, IC_CULT, P.red, eb);
    y += d_wrap(x, y, w, ERAS[e - 1].name, F_ERA_XL, P.ink, 1) + SC(10);
    d_ornament(x, y, w, SC(3) > 2 ? SC(3) : 3, P.red, mix(P.red, P.surface, 140));
    y += 9 * (SC(3) > 2 ? SC(3) : 3) + SC(16);
    y += d_wrap(x, y, w, ERAS[e - 1].intro, F_BODY, P.ink, 1) + SC(16);
    int nt = 0, np = 0, nh = 0, ne = 0, nturn = 0;
    for (int t = 0; t < TECH_COUNT; t++) nt += TECHS[t].era == e;
    for (int p = 0; p < PROJECT_COUNT; p++) np += PROJECTS[p].era == e;
    for (int i = 0; i < PEOPLE_COUNT; i++) nh += era_of_year(PEOPLE[i].from < ERAS[e - 1].from ? ERAS[e - 1].from : PEOPLE[i].from) == e && PEOPLE[i].to >= ERAS[e - 1].from;
    for (int i = 0; i < HIST_COUNT; i++) ne += era_of_year(HIST_EVENTS[i].year) == e;
    for (int t = 0; t < TURN_COUNT; t++) nturn += era_of_year(TURN_YEARS[t]) == e;
    wchar_t s[300];
    swprintf(s, 300, L"В эпохе: %d технологий · %d строек · %d деятелей · %d исторических событий · %d ходов", nt, np, nh, ne, nturn);
    y += d_wrap(x, y, w, s, F_SMALL_B, P.muted, 1) + SC(14);
    if (G.turn == 0) {
        static const wchar_t *tips[] = {
            L"Изучайте технологии во вкладке «Технологии»: они дают доход каждый ход и открывают стройки.",
            L"Стройте замки, мануфактуры и заводы — на карте они появятся красными квадратами у городов.",
            L"Приглашайте деятелей, пока открыто их окно, и отвечайте на вопрос летописца ради знаний.",
            L"В переломные годы история задаст вопрос: ваш выбор меняет население, казну и будущее.",
            L"Нажмите «Завершить ход» или Enter. Партия длится 69 ходов — до 2026 года."
        };
        for (int i = 0; i < 5; i++) {
            d_fill(x, y + SC(8), SC(6), SC(6), P.red);
            y += d_wrap(x + SC(18), y, w - SC(18), tips[i], F_SMALL, P.ink, 1) + SC(6);
        }
        y += SC(10);
    }
    w_button(x + w - SC(220), y, SC(220), SC(44), L"Начать эпоху  (Enter)", B_PRIMARY, 1, A_ERA_START, 0);
    y += SC(44);
    return y - y0;
}

/* ---------- итоги ---------- */
static int body_final(int x, int y, int w)
{
    int y0 = y;
    Score sc;
    game_score(&G, &sc);
    wchar_t s[200];
    swprintf(s, 200, L"ЛЕТОПИСЬ ЗАВЕРШЕНА · %d · %ls", END_YEAR, DIFFICULTIES[G.difficulty].name);
    eyebrow(x, &y, IC_CULT, P.red, s);
    y += d_wrap(x, y, w, sc.rank, F_ERA_L, P.red, 1) + SC(4);
    wchar_t tot[32];
    fmt_num(sc.total, tot, 32);
    swprintf(s, 200, L"Итоговый счёт: %ls", tot);
    d_text(x, y, s, F_H1, P.ink);
    y += d_font_h(F_H1) + SC(14);

    static const wchar_t *names[9] = { L"Технологии", L"Стройки", L"Деятели", L"Население", L"Стабильность", L"Экология", L"Культура", L"Награды", L"Летописец" };
    int vals[9] = { sc.techs, sc.projs, sc.people, sc.pop, sc.stab, sc.eco, sc.cult, sc.ach, sc.quiz };
    wchar_t det[9][64], pop[32];
    fmt_pop(G.pop, pop, 32);
    int ach = 0;
    for (int a = 0; a < ACH_COUNT; a++) ach += G.ach[a];
    swprintf(det[0], 64, L"%d из %d", game_tech_count(&G), TECH_COUNT);
    swprintf(det[1], 64, L"%d из %d", game_proj_count(&G), PROJECT_COUNT);
    swprintf(det[2], 64, L"%d из %d", game_people_count(&G), PEOPLE_COUNT);
    swprintf(det[3], 64, L"%ls", pop);
    swprintf(det[4], 64, L"%ld из 100", lround(G.stab));
    swprintf(det[5], 64, L"%ld из 100", lround(G.eco));
    fmt_num(G.cult, det[6], 64);
    swprintf(det[7], 64, L"%d из %d", ach, ACH_COUNT);
    swprintf(det[8], 64, L"%d верных ответов", G.quizRight);
    int cols = w >= SC(560) ? 3 : 2, cw = w / cols, ch = SC(54);
    for (int i = 0; i < 9; i++) {
        int cx = x + (i % cols) * cw, cy = y + (i / cols) * ch;
        d_text(cx, cy, names[i], F_TINY, P.muted);
        wchar_t v[32];
        fmt_num(vals[i], v, 32);
        d_text(cx, cy + SC(15), v, F_H3, P.ink);
        d_text(cx + d_text_w(v, F_H3) + SC(8), cy + SC(19), det[i], F_TINY, P.muted);
    }
    y += ((9 + cols - 1) / cols) * ch + SC(8);
    d_fill(x, y, w, 1, P.line);
    y += SC(14);
    y += d_wrap(x, y, w, sc.epilogue, F_BODY, P.ink, 1) + SC(20);
    int bw = (w - SC(20)) / 3;
    w_button(x, y, bw, SC(42), L"Новая игра", B_PRIMARY, 1, A_NEW, 0);
    w_button(x + bw + SC(10), y, bw, SC(42), L"Смотреть летопись", B_SECONDARY, 1, A_CLOSE, 0);
    w_button(x + 2 * (bw + SC(10)), y, bw, SC(42), L"Главное меню", B_SECONDARY, 1, A_TO_TITLE, 0);
    y += SC(42);
    return y - y0;
}

/* ---------- справка ---------- */
static int body_help(int x, int y, int w)
{
    int y0 = y;
    eyebrow(x, &y, IC_QUIZ, P.red, L"КАК ИГРАТЬ");
    y += d_wrap(x, y, w, L"Судьба Республики Беларусь в объективе технологий", F_ERA_M, P.ink, 1) + SC(12);
    static const wchar_t *H[][2] = {
        { L"Цель", L"Провести белорусские земли от 1100 до 2026 года через восемь эпох: от Полоцкого княжества до Республики Беларусь. "
                   L"Итоговый счёт складывается из технологий, строек, деятелей, населения, стабильности, экологии, культуры, наград и ответов летописцу." },
        { L"Ход", L"Каждый ход — от 25 лет в Средневековье до 1–3 лет в XX–XXI веках. В начале хода случаются исторические и случайные события. "
                  L"Затем вы изучаете технологии, строите, приглашаете деятелей, меняете курс и отвечаете летописцу. «Завершить ход» начисляет доходы." },
        { L"Ресурсы", L"Знания тратятся на технологии. Казна и производство — на стройки, казна — на деятелей и решения. "
                      L"Население даёт доход и оборону. Стабильность ниже 40 режет доходы, выше 75 — увеличивает. Экология ниже 50 тормозит рост населения." },
        { L"Технологии", L"112 открытий в девяти отраслях. Технология требует предшественников и наступления своей эпохи. "
                         L"Технологии прошлых эпох дешевле на 20%, а приглашённый деятель удешевляет «свою» на 35%." },
        { L"Стройки", L"69 исторических объектов, от Спасской церкви до Белорусской АЭС. Строятся в городах на карте; синее кольцо — можно строить, "
                      L"красные квадраты — построено. Каменецкую вежу нужно успеть возвести до 1400 года." },
        { L"Войны и бедствия", L"Удар войны смягчает оборона: замки, бастионы, арсеналы, партизанская связь, авиация. "
                               L"Некоторые варианты решений открываются только с нужной технологией — так знания спасают людей." },
        { L"Курс", L"Торговля, Просвещение, Индустрия, Оборона, Благосостояние, Бережливость к земле — курс меняет доходы и его можно менять в любой момент." },
        { L"Клавиши", L"Enter — завершить ход или продолжить · 1–3 — выбор в событии · 1–8 — вкладки · Tab — следующая вкладка · "
                      L"Esc — меню · F1 — справка · F2 — тема · Ctrl+S — сохранить · Ctrl+колесо или Ctrl+плюс/минус — масштаб." },
        { L"Сохранения", L"Игра сохраняется автоматически после каждого хода и при выходе в папку %APPDATA%\\BelarusTechFate." },
        { L"Об истории", L"Все технологии, стройки, деятели и события опираются на реальную историю. Решения в событиях — это развилки: "
                         L"иногда исторический вариант, иногда тот, которого история не знала. Числа в игре условны и служат балансу." },
    };
    for (int i = 0; i < (int)(sizeof H / sizeof H[0]); i++) {
        d_text(x, y, H[i][0], F_H3, P.red);
        y += d_font_h(F_H3) + SC(4);
        y += d_wrap(x, y, w, H[i][1], F_BODY, P.ink, 1) + SC(14);
    }
    w_button(x + w - SC(160), y, SC(160), SC(40), L"Понятно", B_PRIMARY, 1, A_CLOSE, 0);
    return y + SC(40) - y0;
}

/* ---------- меню ---------- */
static int body_menu(int x, int y, int w)
{
    int y0 = y;
    eyebrow(x, &y, IC_STAB, P.red, L"МЕНЮ");
    int bh = SC(40), gap = SC(8);
    w_button(x, y, w, bh, L"Продолжить", B_PRIMARY, 1, A_CLOSE, 0); y += bh + gap;
    if (U.screen == SCR_GAME) {
        w_button(x, y, w, bh, L"Сохранить  (Ctrl+S)", B_SECONDARY, 1, A_SAVE, 0); y += bh + gap;
        w_button(x, y, w, bh, L"Загрузить последнее сохранение", B_SECONDARY, 1, A_LOAD, 0); y += bh + gap;
    }
    w_button(x, y, w, bh, g_dark ? L"Светлая тема  (F2)" : L"Тёмная тема  (F2)", B_SECONDARY, 1, A_THEME, 0); y += bh + gap;
    wchar_t z[48];
    swprintf(z, 48, L"Масштаб %d%%", U.zoom);
    int hw = (w - gap) / 2;
    w_button(x, y, hw, bh, L"Мельче", B_SECONDARY, U.zoom > 70, A_ZOOM, -10);
    w_button(x + hw + gap, y, hw, bh, L"Крупнее", B_SECONDARY, U.zoom < 160, A_ZOOM, 10);
    y += bh + SC(4);
    d_text_c(x, y, w, z, F_TINY, P.muted);
    y += SC(22);
    w_button(x, y, w, bh, L"Справка  (F1)", B_SECONDARY, 1, A_HELP, 0); y += bh + gap;
    if (U.screen == SCR_GAME) { w_button(x, y, w, bh, L"В главное меню", B_GHOST, 1, A_TO_TITLE, 0); y += bh + gap; }
    w_button(x, y, w, bh, L"Выйти из игры", B_GHOST, 1, A_EXIT, 0); y += bh;
    return y - y0;
}

/* ---------- новая игра ---------- */
static int body_newgame(int x, int y, int w)
{
    int y0 = y;
    eyebrow(x, &y, IC_CULT, P.red, L"НОВАЯ ЛЕТОПИСЬ · 1100 ГОД");
    y += d_wrap(x, y, w, L"Выберите сложность", F_ERA_L, P.ink, 1) + SC(8);
    if (U.hasSave) y += d_wrap(x, y, w, L"Новая игра заменит текущее сохранение.", F_SMALL, P.red, 1) + SC(8);
    for (int i = 0; i < DIFFICULTY_COUNT; i++) {
        int sel = U.diffPick == i, hv = hover(A_DIFF, i);
        int hd = d_wrap(0, 0, w - SC(70), DIFFICULTIES[i].desc, F_SMALL, 0, 0);
        int h = SC(16) + d_font_h(F_H3) + SC(4) + hd + SC(14);
        d_rrect(x, y, w, h, SC(7), sel ? P.redSoft : hv ? P.surface2 : mix(P.surface, P.surface2, 160), sel ? P.red : P.line);
        d_circle(x + SC(24), y + SC(16) + d_font_h(F_H3) / 2, SC(8), sel ? P.red : CLR_INVALID, sel ? P.red : P.muted, 2);
        wchar_t nm[64];
        swprintf(nm, 64, L"%d. %ls", i + 1, DIFFICULTIES[i].name);
        d_text(x + SC(46), y + SC(14), nm, F_H3, sel ? P.red : P.ink);
        d_wrap(x + SC(46), y + SC(18) + d_font_h(F_H3), w - SC(70), DIFFICULTIES[i].desc, F_SMALL, P.muted, 1);
        hit(x, y, w, h, A_DIFF, i);
        y += h + SC(10);
    }
    y += SC(8);
    w_button(x, y, (w - SC(10)) * 2 / 3, SC(44), L"Начать летопись  (Enter)", B_PRIMARY, 1, A_START, 0);
    w_button(x + (w - SC(10)) * 2 / 3 + SC(10), y, (w - SC(10)) / 3, SC(44), L"Отмена", B_SECONDARY, 1, A_CLOSE, 0);
    return y + SC(44) - y0;
}

/* ---------- карточки ---------- */
static int body_tech(int x, int y, int w)
{
    int y0 = y, t = U.modalArg;
    const Tech *T = &TECHS[t];
    int st = game_tech_state(&G, t), cost = game_tech_cost(&G, t);
    wchar_t s[600];
    swprintf(s, 600, L"ТЕХНОЛОГИЯ · %d · ЭПОХА %ls", T->year, roman(T->era));
    eyebrow(x, &y, IC_TECH, branch_color(T->branch), s);
    y += d_wrap(x, y, w, T->name, F_ERA_L, P.ink, 1) + SC(4);
    d_text(x, y, BRANCH_NAMES[T->branch], F_SMALL_B, branch_color(T->branch));
    y += SC(26);
    y += d_wrap(x, y, w, T->desc, F_BODY, P.ink, 1) + SC(14);
    fx_describe(&T->fx, s, 600);
    d_text(x, y, L"Каждый ход", F_TINY, P.muted); y += SC(16);
    y += d_wrap(x, y, w, s, F_BODY_B, P.green, 1) + SC(12);
    /* требования */
    int any = 0;
    for (int k = 0; k < 3; k++) if (T->reqi[k] >= 0) any = 1;
    if (any) {
        d_text(x, y, L"Требует", F_TINY, P.muted); y += SC(16);
        for (int k = 0; k < 3; k++) {
            int r = T->reqi[k];
            if (r < 0) continue;
            d_icon(G.tech[r] ? IC_CHECK : IC_LOCK, x, y + SC(1), SC(16), G.tech[r] ? P.green : P.muted);
            d_text(x + SC(22), y, TECHS[r].name, F_SMALL, G.tech[r] ? P.ink : P.muted);
            y += SC(22);
        }
        y += SC(8);
    }
    /* что открывает */
    s[0] = 0;
    for (int u = 0; u < TECH_COUNT; u++)
        for (int k = 0; k < 3; k++)
            if (TECHS[u].reqi[k] == t) { if (s[0]) wcsncat(s, L", ", 599 - wcslen(s)); wcsncat(s, TECHS[u].name, 599 - wcslen(s)); }
    if (s[0]) { d_text(x, y, L"Ведёт к технологиям", F_TINY, P.muted); y += SC(16); y += d_wrap(x, y, w, s, F_SMALL, P.ink, 1) + SC(10); }
    s[0] = 0;
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (PROJECTS[p].techi == t) { if (s[0]) wcsncat(s, L", ", 599 - wcslen(s)); wcsncat(s, PROJECTS[p].name, 599 - wcslen(s)); }
    if (s[0]) { d_text(x, y, L"Открывает стройки", F_TINY, P.muted); y += SC(16); y += d_wrap(x, y, w, s, F_SMALL, P.ink, 1) + SC(10); }
    for (int i = 0; i < PEOPLE_COUNT; i++)
        if (PEOPLE[i].disci == t) {
            swprintf(s, 600, L"Дешевле на 35%%, если пригласить: %ls (%d–%d)", PEOPLE[i].name, PEOPLE[i].from, PEOPLE[i].to);
            y += d_wrap(x, y, w, s, F_SMALL, P.blue, 1) + SC(6);
        }
    y += SC(10);
    if (st == TS_AVAILABLE) {
        swprintf(s, 600, L"Изучить за %d знаний", cost);
        w_button(x, y, SC(260), SC(42), s, B_PRIMARY, G.sci >= cost, A_RESEARCH, t);
    } else {
        const wchar_t *msg = st == TS_DONE ? L"Изучено" : st == TS_LOCKED ? L"Сначала изучите требуемые технологии" : L"Эпоха ещё не наступила";
        d_text(x, y + SC(10), msg, F_BODY_B, st == TS_DONE ? P.green : P.muted);
    }
    w_button(x + w - SC(140), y, SC(140), SC(42), L"Закрыть", B_SECONDARY, 1, A_CLOSE, 0);
    return y + SC(42) - y0;
}

static int body_proj(int x, int y, int w)
{
    int y0 = y, p = U.modalArg;
    const Project *R = &PROJECTS[p];
    int st = game_proj_state(&G, p);
    wchar_t s[300];
    swprintf(s, 300, L"СТРОЙКА · %ls · %d", CITIES[R->city].name, R->year);
    upper(s);
    eyebrow(x, &y, IC_BUILD, P.red, s);
    y += d_wrap(x, y, w, R->name, F_ERA_L, P.ink, 1) + SC(10);
    y += d_wrap(x, y, w, R->desc, F_BODY, P.ink, 1) + SC(14);
    fx_describe(&R->fx, s, 300);
    d_text(x, y, L"Каждый ход", F_TINY, P.muted); y += SC(16);
    y += d_wrap(x, y, w, s, F_BODY_B, P.green, 1) + SC(12);
    swprintf(s, 300, L"Нужна технология «%ls» и эпоха %ls.%ls", TECHS[R->techi].name, roman(R->era),
             R->until ? L" Строить нужно успеть до назначенного срока." : L"");
    y += d_wrap(x, y, w, s, F_SMALL, P.muted, 1) + SC(4);
    if (R->until) { swprintf(s, 300, L"Срок: до %d года.", R->until); y += d_wrap(x, y, w, s, F_SMALL_B, P.red, 1) + SC(4); }
    y += SC(12);
    int cg = game_proj_gold(&G, p), ci = game_proj_ind(&G, p);
    if (st == PS_AVAILABLE) {
        swprintf(s, 300, L"Построить: %d казны и %d производства", cg, ci);
        w_button(x, y, d_text_w(s, F_BODY_B) + SC(40), SC(42), s, B_PRIMARY, G.gold >= cg && G.ind >= ci, A_BUILD, p);
    } else {
        const wchar_t *msg = st == PS_DONE ? L"Построено" : st == PS_MISSED ? L"Время упущено" : st == PS_LOCKED ? L"Нужна технология" : L"Эпоха ещё не наступила";
        d_text(x, y + SC(10), msg, F_BODY_B, st == PS_DONE ? P.red : P.muted);
    }
    w_button(x + w - SC(140), y, SC(140), SC(42), L"Закрыть", B_SECONDARY, 1, A_CLOSE, 0);
    return y + SC(42) - y0;
}

static int body_person(int x, int y, int w)
{
    int y0 = y, i = U.modalArg;
    const Person *R = &PEOPLE[i];
    int st = game_person_state(&G, i);
    wchar_t s[300];
    swprintf(s, 300, L"ДЕЯТЕЛЬ · %ls", R->life);
    upper(s);
    eyebrow(x, &y, IC_PERSON, P.blue, s);
    y += d_wrap(x, y, w, R->name, F_ERA_L, P.ink, 1) + SC(2);
    d_text(x, y, R->role, F_SMALL_B, P.blue);
    y += SC(26);
    y += d_wrap(x, y, w, R->desc, F_BODY, P.ink, 1) + SC(14);
    fx_describe(&R->fx, s, 300);
    d_text(x, y, L"Каждый ход, пока в команде (потом — половина)", F_TINY, P.muted); y += SC(16);
    y += d_wrap(x, y, w, s, F_BODY_B, P.green, 1) + SC(10);
    if (R->disci >= 0) { swprintf(s, 300, L"Удешевляет технологию «%ls» на 35%%.", TECHS[R->disci].name); y += d_wrap(x, y, w, s, F_SMALL, P.ink, 1) + SC(6); }
    swprintf(s, 300, L"Окно приглашения: %d–%d.", R->from, R->to);
    y += d_wrap(x, y, w, s, F_SMALL, P.muted, 1) + SC(16);
    if (st == HS_AVAILABLE) {
        int c = game_person_cost(&G, i);
        swprintf(s, 300, L"Пригласить за %d казны", c);
        w_button(x, y, d_text_w(s, F_BODY_B) + SC(40), SC(42), s, B_PRIMARY, G.gold >= c, A_HIRE, i);
    } else {
        const wchar_t *msg = st == HS_ACTIVE ? L"В команде" : st == HS_LEGACY ? L"Наследие" : st == HS_MISSED ? L"Время упущено" : L"Ещё не появился";
        d_text(x, y + SC(10), msg, F_BODY_B, st == HS_ACTIVE ? P.blue : P.muted);
    }
    w_button(x + w - SC(140), y, SC(140), SC(42), L"Закрыть", B_SECONDARY, 1, A_CLOSE, 0);
    return y + SC(42) - y0;
}

static int body_city(int x, int y, int w)
{
    int y0 = y, c = U.modalArg;
    eyebrow(x, &y, IC_BUILD, P.red, L"ГОРОД");
    y += d_wrap(x, y, w, CITIES[c].name, F_ERA_L, P.ink, 1) + SC(12);
    int any = 0;
    for (int p = 0; p < PROJECT_COUNT; p++) {
        if (PROJECTS[p].city != c) continue;
        int st = game_proj_state(&G, p);
        any = 1;
        int h = SC(58);
        d_rrect(x, y, w, h, SC(6), st == PS_DONE ? mix(P.surface, P.red, 24) : mix(P.surface, P.surface2, 160), P.line);
        wchar_t s[200];
        swprintf(s, 200, L"%ls · %d", PROJECTS[p].name, PROJECTS[p].year);
        d_ellipsis(x + SC(12), y + SC(8), w - SC(170), s, F_BODY_B, st == PS_FUTURE || st == PS_MISSED ? P.muted : P.ink);
        const wchar_t *msg = st == PS_DONE ? L"построено" : st == PS_AVAILABLE ? L"можно строить" : st == PS_LOCKED ? L"нужна технология" :
                             st == PS_MISSED ? L"время упущено" : L"в будущей эпохе";
        swprintf(s, 200, L"%ls · эпоха %ls", msg, roman(PROJECTS[p].era));
        d_text(x + SC(12), y + SC(32), s, F_TINY, st == PS_AVAILABLE ? P.blue : P.muted);
        hit(x, y, w - SC(150), h, A_PROJ, p);
        if (st == PS_AVAILABLE) {
            int ok = G.gold >= game_proj_gold(&G, p) && G.ind >= game_proj_ind(&G, p);
            w_button(x + w - SC(140), y + SC(12), SC(128), SC(34), L"Построить", B_PRIMARY, ok, A_BUILD, p);
        }
        y += h + SC(8);
    }
    if (!any) y += d_wrap(x, y, w, L"В этом городе нет строек в игре.", F_BODY, P.muted, 1) + SC(10);
    y += SC(8);
    w_button(x + w - SC(140), y, SC(140), SC(42), L"Закрыть", B_SECONDARY, 1, A_CLOSE, 0);
    return y + SC(42) - y0;
}

/* ---------- каркас: измерение, рамка, прокрутка ---------- */
static void modal_run(int width, int (*body)(int, int, int), int closable)
{
    d_blend(0, 0, g_w, g_h, P.shadow, g_dark ? 150 : 120);
    int pad = SC(28);
    int mw = width < g_w - SC(32) ? width : g_w - SC(32);
    int cw = mw - 2 * pad;
    d_clip(0, 0, 0, 0);
    hits_clip(0, 0, 0, 0);
    int ch = body(0, 0, cw);
    hits_unclip();
    d_unclip();
    int maxH = g_h - SC(40);
    int mh = ch + 2 * pad;
    if (mh > maxH) mh = maxH;
    int mx = (g_w - mw) / 2, my = (g_h - mh) / 2;
    U.modalContent = ch;
    U.modalView = mh - 2 * pad;
    if (U.modalScroll > ch - U.modalView) U.modalScroll = ch - U.modalView;
    if (U.modalScroll < 0) U.modalScroll = 0;
    d_rrect(mx + SC(3), my + SC(6), mw, mh, SC(12), mix(P.bg, P.shadow, 160), CLR_INVALID);
    d_rrect(mx, my, mw, mh, SC(12), P.surface, P.line);
    d_clip(mx + 1, my + SC(10), mw - 2, mh - SC(20));
    hits_clip(mx + 1, my + SC(10), mw - 2, mh - SC(20));
    body(mx + pad, my + pad - U.modalScroll, cw);
    hits_unclip();
    d_unclip();
    if (ch > U.modalView) {
        w_scrollbar(mx + mw - SC(12), my + SC(14), mh - SC(28), ch, U.modalView, U.modalScroll);
        if (U.modalScroll < ch - U.modalView) d_text_c(mx, my + mh - SC(18), mw, L"прокрутите колесом \x2193", F_TINY, P.muted);
    }
    if (closable) {
        int bx = mx + mw - SC(42), by = my + SC(10);
        int hv = hover(A_CLOSE, 0);
        if (hv) d_rrect(bx, by, SC(30), SC(30), SC(15), P.surface2, CLR_INVALID);
        d_line(bx + SC(10), by + SC(10), bx + SC(20), by + SC(20), P.muted, 2);
        d_line(bx + SC(20), by + SC(10), bx + SC(10), by + SC(20), P.muted, 2);
        hit(bx, by, SC(30), SC(30), A_CLOSE, 0);
    }
}

void modal_draw(void)
{
    switch (U.modal) {
    case M_EVENT:   modal_run(SC(760), body_event, 0); break;
    case M_ERA:     modal_run(SC(820), body_era, 0); break;
    case M_FINAL:   modal_run(SC(860), body_final, 1); break;
    case M_HELP:    modal_run(SC(820), body_help, 1); break;
    case M_MENU:    modal_run(SC(420), body_menu, 1); break;
    case M_NEWGAME: modal_run(SC(640), body_newgame, 1); break;
    case M_TECH:    modal_run(SC(700), body_tech, 1); break;
    case M_PROJ:    modal_run(SC(680), body_proj, 1); break;
    case M_PERSON:  modal_run(SC(680), body_person, 1); break;
    case M_CITY:    modal_run(SC(680), body_city, 1); break;
    }
}

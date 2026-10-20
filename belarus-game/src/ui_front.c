/* Армии на карте, фронт, железные дороги и поезда — интерфейс (движок — front.c).
 * Слой рисуется поверх любой проекции карты через mapv_*: дороги и узлы фронта (когда выбрана армия
 * или открыта вкладка «Армия»), рельсы со шпалами, поезда, стрелки маршей, фигурки армий, пушки у боя. */
#include "mapview.h"
#include "front.h"
#include "war.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

int front_tab_armies(int x, int y, int w);     /* разделы вкладки «Армия» (ui_army.c) */
int front_tab_rails(int x, int y, int w);

#define PI 3.14159265358979
#define ES 16                                   /* точек на ребро */

/* ---------- помощники ---------- */
static const wchar_t *w_turns(int n)
{
    n %= 100;
    if (n / 10 == 1) return L"ходов";
    switch (n % 10) { case 1: return L"ход"; case 2: case 3: case 4: return L"хода"; default: return L"ходов"; }
}

static const wchar_t *w_units(int n)
{
    n %= 100;
    if (n / 10 == 1) return L"соединений";
    switch (n % 10) { case 1: return L"соединение"; case 2: case 3: case 4: return L"соединения"; default: return L"соединений"; }
}

/* цвет армии: красный оставлен врагу и цели войны */
static COLORREF army_col(int a)
{
    switch (a & 3) {
    case 0:  return P.blue;
    case 1:  return P.green;
    case 2:  return mix(P.gold, RGB(220, 110, 30), 120);
    default: return RGB(150, 100, 205);
    }
}

static int sel_army(void)
{
    return U.selArmy >= 0 && U.selArmy < MAX_ARMIES && front_army_alive(&G, U.selArmy) ? U.selArmy : -1;
}

/* земля-узел, за которую сейчас можно объявить поход (первая доступная земля группы) или -1 */
static int node_available_prov(int n)
{
    if (n < 0 || n >= FRONT_NODE_COUNT || FRONT_NODES[n].prov < 0) return -1;
    const char *grp = PROVINCES[FRONT_NODES[n].prov].group;
    for (int p = 0; p < PROVINCE_COUNT; p++)
        if (!strcmp(PROVINCES[p].group, grp) && war_province_state(&G, p) == PV_AVAILABLE) return p;
    return -1;
}

static int node_region(int n)
{
    for (int r = 0; r < REGION_COUNT; r++) if (front_node_of_region(r) == n) return r;
    return -1;
}

static double geo_z(const MapProj *pj, double X, double Y, double lift)
{
    if (!mapv_view(pj)) return 0;
    return t3d_inside(X, Y) ? t3d_height(X, Y) + lift : 3.4 + lift;   /* за границей — террасы земель */
}

static int node_xy(const MapProj *pj, int n, double lift, int *x, int *y, double *k)
{
    double X, Y;
    t3d_lonlat(FRONT_NODES[n].lon, FRONT_NODES[n].lat, &X, &Y);
    return mapv_xyz(pj, X, Y, geo_z(pj, X, Y, lift), x, y, k);
}

/* точки ребра a→b на экране; arc — высота дуги над землёй (доля длины, только на 3D-карте) */
static int edge_pts(const MapProj *pj, int a, int b, POINT *pt, int *ok, double *kk, double lift, double arc, int skipFirst)
{
    double ax, ay, bx, by;
    t3d_lonlat(FRONT_NODES[a].lon, FRONT_NODES[a].lat, &ax, &ay);
    t3d_lonlat(FRONT_NODES[b].lon, FRONT_NODES[b].lat, &bx, &by);
    double len = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    int n = (int)(len / 14) + 3;
    if (n > ES) n = ES;
    int m = 0;
    for (int i = skipFirst ? 1 : 0; i < n; i++) {
        double t = (double)i / (n - 1), X = ax + (bx - ax) * t, Y = ay + (by - ay) * t;
        double Z = geo_z(pj, X, Y, lift) + (mapv_view(pj) ? arc * len * sin(PI * t) : 0);
        int x, y;
        double k = 1;
        ok[m] = mapv_xyz(pj, X, Y, Z, &x, &y, &k);
        pt[m].x = x; pt[m].y = y;
        if (kk) kk[m] = k;
        m++;
    }
    return m;
}

static void poly_line(const POINT *p, const int *ok, int n, COLORREF c, int t)
{
    for (int i = 0; i + 1 < n; i++)
        if (ok[i] && ok[i + 1]) d_line(p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, c, t);
}

/* штрихи вдоль ломаной: on пикселей линии, off — пропуск; phase сдвигает рисунок (бегущие штрихи) */
static void dash_line(const POINT *p, const int *ok, int n, COLORREF c, int t, double on, double off, double phase)
{
    double per = on + off, acc = fmod(phase, per);
    if (acc < 0) acc += per;
    for (int i = 0; i + 1 < n; i++) {
        double dx = p[i + 1].x - p[i].x, dy = p[i + 1].y - p[i].y, L = sqrt(dx * dx + dy * dy);
        if (!ok[i] || !ok[i + 1] || L < 0.5) { acc += L; continue; }
        double s = 0;
        while (s < L) {
            double pos = fmod(acc + s, per), seg;
            if (pos < on) {
                seg = on - pos < L - s ? on - pos : L - s;
                d_line(p[i].x + (int)(dx * s / L), p[i].y + (int)(dy * s / L),
                       p[i].x + (int)(dx * (s + seg) / L), p[i].y + (int)(dy * (s + seg) / L), c, t);
            } else seg = per - pos < L - s ? per - pos : L - s;
            s += seg > 0.01 ? seg : 0.01;
        }
        acc += L;
    }
}

/* наконечник стрелки в конце ломаной */
static void arrow_head(const POINT *p, const int *ok, int n, COLORREF fill, COLORREF border, int a)
{
    if (n < 2 || !ok[n - 1]) return;
    int j = n - 2;
    while (j > 0 && (!ok[j] || (abs(p[j].x - p[n - 1].x) + abs(p[j].y - p[n - 1].y)) < a)) j--;
    if (!ok[j]) return;
    double dx = p[n - 1].x - p[j].x, dy = p[n - 1].y - p[j].y, d = sqrt(dx * dx + dy * dy);
    if (d < 0.5) return;
    dx /= d; dy /= d;
    int ex = p[n - 1].x, ey = p[n - 1].y;
    POINT tri[4] = { { ex + (int)(dx * a * 0.5), ey + (int)(dy * a * 0.5) },
                     { ex - (int)(dx * a) - (int)(dy * a * 0.7), ey - (int)(dy * a) + (int)(dx * a * 0.7) },
                     { ex - (int)(dx * a) + (int)(dy * a * 0.7), ey - (int)(dy * a) - (int)(dx * a * 0.7) },
                     { ex + (int)(dx * a * 0.5), ey + (int)(dy * a * 0.5) } };
    d_poly(tri, 4, fill, border, 1);
}

/* подпись-плашка с центром по x */
static void tag(int cx, int y, const wchar_t *s, int f, COLORREF fg, COLORREF border)
{
    int tw = d_text_w(s, f), th = d_font_h(f);
    d_rrect(cx - tw / 2 - SC(6), y, tw + SC(12), th + SC(4), (th + SC(4)) / 2, P.surface, border);
    d_text(cx - tw / 2, y + SC(2), s, f, fg);
}

static void ellipse_ring(int cx, int cy, int rx, int ry, COLORREF c, int t)
{
    POINT p[25];
    int ok[25];
    for (int i = 0; i <= 24; i++) {
        double a = 2 * PI * i / 24;
        p[i].x = cx + (int)(cos(a) * rx);
        p[i].y = cy + (int)(sin(a) * ry);
        ok[i] = 1;
    }
    poly_line(p, ok, 25, c, t);
}

/* ---------- приказы ---------- */
static void say_order(int a, int node)
{
    int eta = front_eta(&G, G.armies[a].node, node);
    if (eta <= 1) ui_toast(L"Армия %d выступила: %ls → %ls, придёт к концу хода.", a + 1, FRONT_NODES[G.armies[a].node].name, FRONT_NODES[node].name);
    else ui_toast(L"Армия %d выступила: %ls → %ls, в пути %d %ls.", a + 1, FRONT_NODES[G.armies[a].node].name, FRONT_NODES[node].name,
                  eta, w_turns(eta));
}

/* приказ армии a идти к узлу: на чужую землю — объявить поход, к оккупированному центру области — операцию */
static void order(int a, int node)
{
    if (a < 0 || node < 0) return;
    const wchar_t *why = front_cannot_send(&G, a, node);
    int p = node_available_prov(node);
    if (why && p >= 0 && FRONT_NODES[node].kind == FN_LAND) {
        const wchar_t *no = war_cannot_declare(&G, WAR_CONQUEST, p);
        if (no) { ui_toast(L"%ls", no); return; }
        war_declare(&G, WAR_CONQUEST, p);
        if (front_send(&G, a, node)) {
            int eta = front_eta(&G, G.armies[a].node, node);
            if (eta <= 1) ui_toast(L"Объявлен поход на землю «%ls»! Армия %d придёт туда к концу хода, и сразу начнётся бой.", PROVINCES[p].name, a + 1);
            else ui_toast(L"Объявлен поход на землю «%ls»! Армия %d выступила: в пути %d %ls, бой начнётся, когда она дойдёт.",
                          PROVINCES[p].name, a + 1, eta, w_turns(eta));
        } else ui_toast(L"Объявлен поход на землю «%ls». Отправьте к ней армию.", PROVINCES[p].name);
        map_invalidate();
        return;
    }
    if (why) { ui_toast(L"%ls", why); return; }
    front_send(&G, a, node);
    int r = node_region(node);
    if (r >= 0 && ((G.occupied >> r) & 1u) && G.warKind == WAR_NONE && !war_cannot_declare(&G, WAR_LIBERATION, r)) {
        war_declare(&G, WAR_LIBERATION, r);
        int eta = front_eta(&G, G.armies[a].node, node);
        if (eta <= 1) ui_toast(L"Начата операция: %ls. Армия %d придёт в город %ls к концу хода, и сразу начнётся бой.", REGION_NAMES[r], a + 1,
                               FRONT_NODES[node].name);
        else ui_toast(L"Начата операция: %ls. Армия %d выступила: в пути %d %ls, бой начнётся, когда она дойдёт до города %ls.",
                      REGION_NAMES[r], a + 1, eta, w_turns(eta), FRONT_NODES[node].name);
        map_invalidate();
        return;
    }
    say_order(a, node);
}

static void focus_army(int a)
{
    if (!front_army_alive(&G, a) || !map3d_on()) return;
    const FrontNode *n = &FRONT_NODES[G.armies[a].node];
    map_cam_fly_to(n->lon, n->lat, 2.2, 48, 900, NULL);
}

/* объявить войну (кнопки вкладки) и сразу отправить армию */
static void declare_and_send(int kind, int target)
{
    const wchar_t *why = war_cannot_declare(&G, kind, target);
    if (why) { ui_toast(L"%ls", why); return; }
    war_declare(&G, kind, target);
    const wchar_t *name = kind == WAR_CONQUEST ? PROVINCES[target].name : REGION_NAMES[target];
    int node = front_target_node(&G), s = sel_army(), a = -1;
    if (s >= 0 && !front_cannot_send(&G, s, node)) a = s;
    else a = front_army_for(&G, node);
    if (a >= 0 && G.armies[a].node == node) {
        ui_toast(L"%ls «%ls»: армия %d уже на месте — бой в конце хода.", kind == WAR_CONQUEST ? L"Начат поход на землю" : L"Начата операция:",
                 name, a + 1);
    } else if (a >= 0 && front_send(&G, a, node)) {
        int eta = front_eta(&G, G.armies[a].node, node);
        U.selArmy = a;
        if (kind == WAR_CONQUEST)
            ui_toast(L"Начат поход на землю «%ls». Армия %d выступила: в пути %d %ls, бой начнётся, когда она дойдёт.", name, a + 1, eta, w_turns(eta));
        else ui_toast(L"Начата операция: %ls. Армия %d выступила: в пути %d %ls.", name, a + 1, eta, w_turns(eta));
    } else ui_toast(L"Война объявлена: %ls. Выберите армию на карте и отправьте её к цели.", name);
    map_invalidate();
}

/* курсор над 3D-картой (панель или карта во весь экран), а не над списками вкладок */
static int cursor_on_map(void)
{
    POINT p;
    if (!GetCursorPos(&p) || !ScreenToClient(g_hwnd, &p)) return 0;
    return map_cam_hit(p.x, p.y);
}

int front_ui_click(int act, int arg)
{
    if (U.screen != SCR_GAME) return 0;
    switch (act) {
    case A_ARMY_SEL:
        if (arg < 0) { U.selArmy = -1; return 1; }
        if ((arg & 15) >= MAX_ARMIES || !front_army_alive(&G, arg & 15)) { U.selArmy = -1; return 1; }
        if (arg >= 16) { U.selArmy = arg & 15; focus_army(U.selArmy); return 1; }
        U.selArmy = U.selArmy == arg ? -1 : arg;
        return 1;
    case A_FRONT_NODE: {
        int s = sel_army();
        if (s >= 0) { order(s, arg); return 1; }
        unsigned m = front_armies_at(&G, arg);
        for (int a = 0; a < MAX_ARMIES; a++) if ((m >> a) & 1u) { U.selArmy = a; return 1; }
        ui_toast(L"%ls: сначала выберите армию — нажмите на её фигурку на карте.", FRONT_NODES[arg].name);
        return 1;
    }
    case A_RAIL_BUILD: {
        const wchar_t *why = front_cannot_build_rail(&G, arg);
        if (why) ui_toast(L"%ls", why);
        else if (front_build_rail(&G, arg))
            ui_toast(L"Железная дорога построена: %ls — %ls. По ней пошли поезда.", FRONT_NODES[FRONT_EDGES[arg].a].name, FRONT_NODES[FRONT_EDGES[arg].b].name);
        return 1;
    }
    case A_ARMY_SPLIT: {
        int a = arg & 15, op = arg / 16;
        if (!front_army_alive(&G, a)) return 1;
        if (op == FA_SPLIT) {
            int b = front_split(&G, a);
            if (b >= 0) { U.selArmy = b; ui_toast(L"Армия %d разделена: половина войск — теперь армия %d.", a + 1, b + 1); }
            else ui_toast(L"%ls", front_army_size(&G, a) < 2 ? L"В армии одно соединение — делить нечего." : L"Армий не может быть больше четырёх.");
        } else if (op == FA_MERGE) {
            int k = front_merge(&G, a);
            if (k) ui_toast(L"Армии объединены: теперь в армии %d — %d %ls.", a + 1, front_army_size(&G, a), w_units(front_army_size(&G, a)));
            else ui_toast(L"Рядом нет других армий.");
        } else if (op == FA_HALT) {
            if (front_halt(&G, a)) ui_toast(L"Армия %d остановилась: %ls.", a + 1, FRONT_NODES[G.armies[a].node].name);
        } else if (op == FA_HOME) {
            int home = front_recruit_node(&G);
            if (G.armies[a].node == home) ui_toast(L"Армия %d уже у столицы.", a + 1);
            else order(a, home);
        } else if (op == FA_TARGET) {
            int tn = front_target_node(&G);
            if (tn >= 0) order(a, tn);
        }
        return 1;
    }
    case A_DECLARE: declare_and_send(WAR_CONQUEST, arg); return 1;
    case A_LIBERATE: declare_and_send(WAR_LIBERATION, arg); return 1;
    case A_CITY: case A_PROVINCE: {   /* армия выбрана: щелчок по городу или земле на карте — приказ идти туда */
        int s = sel_army();
        if (s < 0 || !cursor_on_map()) return 0;
        order(s, act == A_CITY ? front_node_near_city(arg) : front_node_of_prov(arg));
        return 1;
    }
    }
    return 0;
}

int front_ui_rclick(int act, int arg)
{
    if (U.screen != SCR_GAME || (U.modal != M_NONE && U.modal != M_MAP)) return 0;
    int node = -1, s = sel_army();
    switch (act) {
    case A_FRONT_NODE: node = arg; break;
    case A_CITY:       node = front_node_near_city(arg); break;
    case A_PROVINCE:   node = front_node_of_prov(arg); break;
    case A_HOVER_REGION: node = front_node_of_region(arg); break;
    case A_ARMY_SEL:
        if (arg < 0 || arg >= MAX_ARMIES || !front_army_alive(&G, arg)) return 0;
        if (s < 0 || s == arg) { U.selArmy = arg; return 1; }
        node = G.armies[arg].node;          /* к другой армии — чтобы потом объединиться */
        break;
    default: return 0;
    }
    if (node < 0) return 0;
    if (s < 0) {
        unsigned m = front_armies_at(&G, node);
        for (int a = 0; a < MAX_ARMIES; a++) if ((m >> a) & 1u) { U.selArmy = a; return 1; }
        ui_toast(L"Сначала выберите армию: нажмите на её фигурку на карте.");
        return 1;
    }
    order(s, node);
    return 1;
}

void front_ui_hover(int act, int arg, wchar_t *buf, int n)
{
    buf[0] = 0;
    if (U.screen != SCR_GAME) return;
    int s = sel_army();
    if (act == A_ARMY_SEL && arg >= 0 && (arg & 15) < MAX_ARMIES && front_army_alive(&G, arg & 15)) {
        int a = arg & 15, u = front_lead_unit(&G, a), sz = front_army_size(&G, a);
        swprintf(buf, n, L"Армия %d: %d %ls (сильнее всех — «%ls»), %ls · %ls. %ls",
                 a + 1, sz, w_units(sz), u >= 0 ? UNIT_TYPES[u].name : L"—", FRONT_NODES[G.armies[a].node].name,
                 front_army_supplied(&G, a) ? L"снабжение есть" : L"нет снабжения, сила ×0,6",
                 arg >= 16 ? L"Нажмите — камера покажет армию на карте." : L"Нажмите, чтобы выбрать, затем щёлкните место на карте.");
    } else if (act == A_ARMY_SEL && arg < 0) swprintf(buf, n, L"Снять выбор армии");
    else if (act == A_FRONT_NODE && arg >= 0 && arg < FRONT_NODE_COUNT) {
        const FrontNode *N = &FRONT_NODES[arg];
        if (s >= 0) {
            const wchar_t *why = front_cannot_send(&G, s, arg);
            int p = node_available_prov(arg);
            if (!why) {
                int eta = front_eta(&G, G.armies[s].node, arg);
                swprintf(buf, n, L"%ls: армия %d дойдёт за %d %ls%ls. Щёлкните — приказ идти.", N->name, s + 1, eta, w_turns(eta),
                         front_supplied_at(&G, arg) ? L"" : L" · там нет снабжения");
            } else if (p >= 0 && N->kind == FN_LAND)
                swprintf(buf, n, L"%ls (%ls): щёлкните — объявить поход и отправить армию %d.", N->name, PROVINCES[p].owner, s + 1);
            else swprintf(buf, n, L"%ls: %ls", N->name, why);
        } else if (N->kind == FN_REAR)
            swprintf(buf, n, L"Большая земля — тыл фронта 1941–1944 годов: отсюда идут снабжение и пополнение.");
        else swprintf(buf, n, L"%ls%ls. Выберите армию, чтобы отдать приказ.", N->name,
                      front_node_occupied(&G, arg) ? L" — под оккупацией" : front_supplied_at(&G, arg) ? L" — снабжение есть" : L" — снабжения нет");
    } else if (act == A_RAIL_BUILD && arg >= 0 && arg < FRONT_EDGE_COUNT) {
        const FrontEdge *E = &FRONT_EDGES[arg];
        const wchar_t *why = front_cannot_build_rail(&G, arg);
        double rg, ri;
        war_rail_income(&G, &rg, &ri);
        swprintf(buf, n, L"%ls — %ls, %d км: казна %d, производство %d. Марш до трёх перегонов за ход, снабжение армий, доход +%.0f казны и +%.0f производства за ход.%ls%ls",
                 FRONT_NODES[E->a].name, FRONT_NODES[E->b].name, E->km, front_rail_gold(&G, arg), front_rail_ind(&G, arg), rg, ri,
                 why ? L" " : L"", why ? why : L"");
    } else if (act == A_ARMY_SPLIT && front_army_alive(&G, arg & 15)) {
        int a = arg & 15;
        switch (arg / 16) {
        case FA_SPLIT: swprintf(buf, n, L"Разделить армию %d пополам: новая армия встанет рядом и сможет идти отдельно.", a + 1); break;
        case FA_MERGE: swprintf(buf, n, L"Собрать в армию %d все армии, которые стоят в этом же месте.", a + 1); break;
        case FA_HALT:  swprintf(buf, n, L"Отменить приказ: армия %d останется там, где стоит.", a + 1); break;
        case FA_HOME:  swprintf(buf, n, L"Отправить армию %d к столице: там собираются новые войска.", a + 1); break;
        case FA_TARGET: swprintf(buf, n, L"Отправить армию %d к цели войны: бой начнётся, когда она дойдёт.", a + 1); break;
        }
    } else if (act == A_DECLARE && arg >= 0 && arg < PROVINCE_COUNT) {
        int node = front_node_of_prov(arg), a = front_army_for(&G, node);
        if (a >= 0) {
            int eta = front_eta(&G, G.armies[a].node, node);
            swprintf(buf, n, L"Объявить поход на землю «%ls» (%ls): армия %d дойдёт за %d %ls, бой — когда она будет на месте.",
                     PROVINCES[arg].name, PROVINCES[arg].owner, a + 1, eta, w_turns(eta));
        }
    } else if (act == A_LIBERATE && arg >= 0 && arg < REGION_COUNT) {
        int node = front_node_of_region(arg), a = front_army_for(&G, node);
        if (a >= 0) {
            int eta = front_eta(&G, G.armies[a].node, node);
            swprintf(buf, n, L"Освободить: %ls — армия %d дойдёт до города %ls за %d %ls, затем бои.", REGION_NAMES[arg], a + 1,
                     FRONT_NODES[node].name, eta, w_turns(eta));
        }
    } else if (act == A_HOVER_STAT && arg == 21)
        swprintf(buf, n, L"Сила всех армий вместе. В бою участвуют только армии, которые стоят у цели, — соберите войска "
                         L"в одну армию (кнопка «Объединить») или приведите к цели несколько армий.");
    else if (act == A_HOVER_STAT && arg == 22)
        swprintf(buf, n, L"Оборона столицы: армии в городе — полностью, в соседних — на три четверти, дальние — наполовину, плюс крепости. "
                         L"Так же считается оборона любого города от набегов и осад.");
}

/* ---------- поезда ---------- */
#define TR_MAX 8
#define TR_PTS (FRONT_MAX_NODES * ES)

/* вагон или паровоз: прямоугольник вдоль направления (dx, dy) */
static void car(double cx, double cy, double dx, double dy, double len, double wid, COLORREF fill, COLORREF edge)
{
    double nx = -dy, ny = dx;
    POINT p[5] = {
        { (int)(cx + dx * len / 2 + nx * wid / 2), (int)(cy + dy * len / 2 + ny * wid / 2) },
        { (int)(cx + dx * len / 2 - nx * wid / 2), (int)(cy + dy * len / 2 - ny * wid / 2) },
        { (int)(cx - dx * len / 2 - nx * wid / 2), (int)(cy - dy * len / 2 - ny * wid / 2) },
        { (int)(cx - dx * len / 2 + nx * wid / 2), (int)(cy - dy * len / 2 + ny * wid / 2) },
        { (int)(cx + dx * len / 2 + nx * wid / 2), (int)(cy + dy * len / 2 + ny * wid / 2) } };
    d_poly(p, 5, fill, edge, 1);
}

/* точка ломаной на расстоянии s пикселей от начала и направление в ней */
static int poly_at(const POINT *p, const double *cum, int n, double s, double *x, double *y, double *dx, double *dy)
{
    if (n < 2) return 0;
    if (s < 0) s = 0;
    if (s > cum[n - 1]) s = cum[n - 1];
    int i = 0;
    while (i < n - 2 && cum[i + 1] < s) i++;
    double L = cum[i + 1] - cum[i], t = L > 1e-6 ? (s - cum[i]) / L : 0;
    *x = p[i].x + (p[i + 1].x - p[i].x) * t;
    *y = p[i].y + (p[i + 1].y - p[i].y) * t;
    double ddx = p[i + 1].x - p[i].x, ddy = p[i + 1].y - p[i].y, d = sqrt(ddx * ddx + ddy * ddy);
    if (d < 1e-6) { *dx = 1; *dy = 0; } else { *dx = ddx / d; *dy = ddy / d; }
    return 1;
}

static void draw_trains(const MapProj *pj, double t, int large)
{
    static int routes[TR_MAX][FRONT_MAX_NODES], lens[TR_MAX];
    static POINT pt[TR_PTS];
    static double cum[TR_PTS], km[TR_PTS];
    int nr = front_train_routes(&G, routes, lens, TR_MAX);
    int mx, my, mw, mh;
    mapv_rect(pj, &mx, &my, &mw, &mh);
    for (int r = 0; r < nr; r++) {
        if (lens[r] < 2) continue;
        /* ломаная маршрута на экране и километры вдоль неё */
        int n = 0, bad = 0;
        double kmAcc = 0, kAvg = 0;
        for (int i = 0; i + 1 < lens[r] && !bad; i++) {
            POINT ep[ES];
            int ok[ES];
            double kk[ES];
            int a = routes[r][i], b = routes[r][i + 1], e = front_edge(a, b);
            int m = edge_pts(pj, a, b, ep, ok, kk, 0.25, 0, i > 0);
            double ekm = e >= 0 ? FRONT_EDGES[e].km : 50;
            for (int j = 0; j < m && n < TR_PTS; j++) {
                if (!ok[j]) { bad = 1; break; }
                pt[n] = ep[j];
                km[n] = kmAcc + ekm * (double)(j + (i > 0)) / (m - (i > 0 ? 0 : 1));
                kAvg += kk[j];
                n++;
            }
            kmAcc += ekm;
        }
        if (bad || n < 2) continue;
        kAvg /= n;
        cum[0] = 0;
        for (int i = 1; i < n; i++) {
            double dx = pt[i].x - pt[i - 1].x, dy = pt[i].y - pt[i - 1].y;
            cum[i] = cum[i - 1] + sqrt(dx * dx + dy * dy);
        }
        double total = cum[n - 1];
        if (total < SC(30)) continue;
        /* туда и обратно с постоянной скоростью в километрах */
        double period = kmAcc / 70.0 + 1.5, ph = fmod(t + r * 2.3, 2 * period);
        double f = ph < period ? ph / period : 2 - ph / period;
        int back = ph >= period;
        /* километры → пиксели */
        double skm = f * kmAcc, spx = 0;
        for (int i = 1; i < n; i++)
            if (km[i] >= skm) { double u = km[i] - km[i - 1] > 1e-6 ? (skm - km[i - 1]) / (km[i] - km[i - 1]) : 0; spx = cum[i - 1] + (cum[i] - cum[i - 1]) * u; break; }
        double sc = mapv_scale(pj, kAvg);
        if (sc < 0.7) sc = 0.7;
        if (sc > 1.6) sc = 1.6;
        double L = SC(large ? 21 : 15) * sc, W = SC(large ? 9 : 7) * sc, gap = SC(2) * sc;
        int occ = 0;
        for (int i = 0; i < lens[r]; i++) occ |= front_node_occupied(&G, routes[r][i]);
        COLORREF loco = occ ? RGB(78, 78, 80) : RGB(34, 36, 44), trim = occ ? RGB(120, 120, 120) : RGB(196, 48, 40);
        COLORREF wag[3] = { occ ? RGB(96, 96, 92) : RGB(132, 78, 44), occ ? RGB(84, 88, 84) : RGB(62, 112, 74), occ ? RGB(96, 96, 92) : RGB(150, 120, 60) };
        double hx, hy, dx, dy;
        if (!poly_at(pt, cum, n, spx, &hx, &hy, &dx, &dy)) continue;
        if (back) { dx = -dx; dy = -dy; }
        if (hx < mx - SC(40) || hx > mx + mw + SC(40) || hy < my - SC(40) || hy > my + mh + SC(40)) continue;
        /* вагоны: позади паровоза вдоль пути */
        for (int w = 3; w >= 1; w--) {
            double off = L * 0.5 + gap + (L * 0.8 + gap) * (w - 0.5);
            double wx, wy, wdx, wdy;
            poly_at(pt, cum, n, back ? spx + off : spx - off, &wx, &wy, &wdx, &wdy);
            if (back) { wdx = -wdx; wdy = -wdy; }
            car(wx, wy, wdx, wdy, L * 0.8, W * 0.9, wag[w - 1], RGB(30, 24, 20));
        }
        car(hx, hy, dx, dy, L, W, loco, RGB(16, 16, 20));
        car(hx + dx * L * 0.38, hy + dy * L * 0.38, dx, dy, L * 0.22, W * 1.05, trim, RGB(16, 16, 20));   /* передок */
        car(hx - dx * L * 0.3, hy - dy * L * 0.3, dx, dy, L * 0.3, W * 0.75, mix(loco, RGB(255, 255, 255), 40), RGB(16, 16, 20));  /* будка */
        /* труба и дым: клубы поднимаются и тают */
        double cx = hx + dx * L * 0.18, cy = hy + dy * L * 0.18;
        d_circle((int)cx, (int)cy, (int)(W * 0.28) + 1, RGB(20, 20, 20), CLR_INVALID, 1);
        for (int k = 0; k < 4; k++) {
            double age = fmod(t * 1.6 + k * 0.25, 1.0);
            int rr = (int)(W * (0.35 + age * 0.75));
            int sx = (int)(cx - dx * L * age * 1.4), sy = (int)(cy - dy * L * age * 1.4 - W * (0.8 + age * 2.2));
            COLORREF sm = mix(RGB(236, 236, 232), P.surface, (int)(age * 150));
            if (occ) sm = mix(RGB(150, 150, 150), P.surface, (int)(age * 150));
            d_circle(sx, sy, rr, sm, CLR_INVALID, 1);
        }
        /* 1941–1944: партизаны пускают эшелоны оккупантов под откос («рельсовая война») */
        if (occ) {
            double ex = fmod(t + r * 1.7, 7.0);
            if (ex < 0.8) {
                double bx, by, bdx, bdy;
                poly_at(pt, cum, n, total * (0.3 + 0.4 * fmod(r * 0.37, 1.0)), &bx, &by, &bdx, &bdy);
                int R = (int)(SC(9) * sc * (0.6 + ex));
                POINT star[17];
                for (int i = 0; i < 16; i++) {
                    double a = i * PI / 8 + ex, rr = i % 2 ? R * 0.45 : R;
                    star[i].x = (int)bx + (int)(cos(a) * rr);
                    star[i].y = (int)by + (int)(sin(a) * rr);
                }
                star[16] = star[0];
                d_poly(star, 17, mix(RGB(255, 200, 60), RGB(230, 70, 30), (int)(ex * 300) > 255 ? 255 : (int)(ex * 300)), RGB(90, 30, 10), 1);
            }
        }
    }
}

/* ---------- рельсы со шпалами ---------- */
static void draw_rail(const MapProj *pj, int e, int large)
{
    POINT pt[ES];
    int ok[ES];
    double kk[ES];
    int n = edge_pts(pj, FRONT_EDGES[e].a, FRONT_EDGES[e].b, pt, ok, kk, 0.2, 0, 0);
    double k = kk[n / 2], sc = mapv_scale(pj, k);
    if (sc < 0.7) sc = 0.7;
    if (sc > 2.2) sc = 2.2;
    int w = (int)(SC(large ? 4 : 3) * sc);
    if (w < 3) w = 3;
    int enemy = front_node_occupied(&G, FRONT_EDGES[e].a) || front_node_occupied(&G, FRONT_EDGES[e].b);
    COLORREF dark = enemy ? RGB(70, 34, 30) : RGB(44, 34, 28), light = enemy ? RGB(170, 130, 120) : RGB(240, 230, 210);
    /* шпалы — поперечные чёрточки (на крупном плане) */
    if (w >= 4) {
        double step = w * 2.2, acc = 0;
        for (int i = 0; i + 1 < n; i++) {
            if (!ok[i] || !ok[i + 1]) continue;
            double dx = pt[i + 1].x - pt[i].x, dy = pt[i + 1].y - pt[i].y, L = sqrt(dx * dx + dy * dy);
            if (L < 0.5) continue;
            double ux = dx / L, uy = dy / L;
            for (double s = fmod(step - fmod(acc, step), step); s < L; s += step) {
                int cx = pt[i].x + (int)(ux * s), cy = pt[i].y + (int)(uy * s);
                d_line(cx - (int)(uy * w * 1.15), cy + (int)(ux * w * 1.15), cx + (int)(uy * w * 1.15), cy - (int)(ux * w * 1.15), dark, w >= 6 ? 2 : 1);
            }
            acc += L;
        }
    }
    poly_line(pt, ok, n, dark, w);
    dash_line(pt, ok, n, light, w - 2 > 1 ? w - 2 : 1, SC(5) * sc, SC(5) * sc, 0);
}

/* перегон, который можно проложить сейчас (или когда хватит казны) */
static int rail_plannable(int e)
{
    const wchar_t *why = front_cannot_build_rail(&G, e);
    return !why || !wcscmp(why, L"Не хватает казны или производства.");
}

/* ---------- будущие железные дороги: пунктир и кнопка «+» посередине перегона (вкладка «Армия») ---------- */
static void draw_rail_plans(const MapProj *pj, int interactive, int large)
{
    int mx, my, mw, mh;
    mapv_rect(pj, &mx, &my, &mw, &mh);
    for (int e = 0; e < FRONT_EDGE_COUNT; e++) {
        int hv = hover(A_RAIL_BUILD, e);
        if (!rail_plannable(e) || (!large && !hv)) continue;      /* на маленькой карте — только перегон под мышью во вкладке */
        POINT pt[ES];
        int ok[ES];
        double kk[ES];
        int n = edge_pts(pj, FRONT_EDGES[e].a, FRONT_EDGES[e].b, pt, ok, kk, 0.2, 0, 0);
        int can = !front_cannot_build_rail(&G, e);
        COLORREF c = hv ? P.gold : mix(P.gold, P.surface, can ? 70 : 140);
        dash_line(pt, ok, n, c, hv ? SC(4) : SC(2), SC(6), SC(4), 0);
        int m = n / 2;
        if (!large || !ok[m] || pt[m].x < mx || pt[m].x > mx + mw || pt[m].y < my || pt[m].y > my + mh) continue;
        int r = large ? SC(9) : SC(7);
        d_circle(pt[m].x, pt[m].y, r, hv ? P.gold : P.surface, can ? P.gold : P.muted, 2);
        COLORREF pc = hv ? P.onAccent : can ? P.gold : P.muted;
        d_line(pt[m].x - r / 2, pt[m].y, pt[m].x + r / 2, pt[m].y, pc, 2);
        d_line(pt[m].x, pt[m].y - r / 2, pt[m].x, pt[m].y + r / 2, pc, 2);
        if (interactive) hit(pt[m].x - r - SC(2), pt[m].y - r - SC(2), 2 * r + SC(4), 2 * r + SC(4), A_RAIL_BUILD, e);
    }
}

/* ---------- марш: стрелка по дорогам от армии к цели ---------- */
static void draw_march(const MapProj *pj, int from, int to, COLORREF c, int strong, double t, int large, int preview)
{
    int path[FRONT_MAX_NODES];
    int n = front_path(&G, from, to, path, FRONT_MAX_NODES);
    if (n < 2) return;
    static POINT pts[FRONT_MAX_NODES * ES];
    static int ok[FRONT_MAX_NODES * ES];
    int m = 0;
    for (int i = 0; i + 1 < n && m < FRONT_MAX_NODES * (ES - 1); i++)
        m += edge_pts(pj, path[i], path[i + 1], pts + m, ok + m, 0, 1.0, 0.06, i > 0);
    int w = strong ? SC(large ? 5 : 4) : SC(3);
    COLORREF dark = RGB(40, 20, 16);
    if (!preview) poly_line(pts, ok, m, dark, w + SC(2));
    dash_line(pts, ok, m, preview ? mix(c, P.surface, 90) : c, w, SC(9), SC(6), preview ? 0 : -t * SC(24));
    arrow_head(pts, ok, m, c, dark, w * 3);
    /* сколько ходов идти */
    int eta = front_eta(&G, from, to);
    if (eta > 0 && ok[m - 1]) {
        wchar_t s[32];
        swprintf(s, 32, L"%d %ls", eta, w_turns(eta));
        tag(pts[m - 1].x, pts[m - 1].y + SC(8), s, F_TINY, P.ink, c);
    }
}

/* ---------- армия на карте ---------- */
static void army_badge(int a, int x, int y, int selected, int interactive, int *outH)
{
    wchar_t num[16], an[4];
    int size = front_army_size(&G, a), sup = front_army_supplied(&G, a);
    swprintf(num, 16, L"%d", size);
    swprintf(an, 4, L"%d", a + 1);
    int f = F_SMALL_B, th = d_font_h(f), h = th + SC(4), dr = h / 2 - SC(1);
    int tw = d_text_w(num, f), iw = SC(12);
    int w = dr * 2 + SC(6) + tw + SC(6) + iw + SC(6);
    int bx = x - w / 2;
    d_rrect(bx, y, w, h, h / 2, P.surface, selected ? army_col(a) : mix(P.line, P.ink, 60));
    d_circle(bx + SC(2) + dr, y + h / 2, dr, army_col(a), CLR_INVALID, 1);
    d_text_c(bx + SC(2), y + (h - d_font_h(F_TINY)) / 2, dr * 2 + 1, an, F_TINY, P.onAccent);
    d_text(bx + dr * 2 + SC(8), y + SC(2), num, f, P.ink);
    int ix = bx + w - iw - SC(5), iy = y + (h - iw) / 2;
    if (sup) {   /* мешок с припасами — снабжение есть */
        d_circle(ix + iw / 2, iy + iw / 2 + SC(1), iw / 2, mix(P.green, P.surface, 60), P.green, 1);
        d_line(ix + iw / 2 - SC(2), iy + SC(1), ix + iw / 2 + SC(2), iy + SC(1), P.green, 2);
    } else {     /* красный треугольник — снабжения нет */
        POINT tri[4] = { { ix + iw / 2, iy }, { ix + iw, iy + iw }, { ix, iy + iw }, { ix + iw / 2, iy } };
        d_poly(tri, 4, P.red, CLR_INVALID, 1);
        d_text_c(ix, iy + iw - d_font_h(F_TINY) + SC(1), iw, L"!", F_TINY, P.onAccent);
    }
    if (interactive) hit(bx, y, w, h, A_ARMY_SEL, a);
    *outH = h;
}

static void army_token(int a, int x, int y, int sz, int selected, int interactive, double t)
{
    COLORREF c = army_col(a);
    /* тень и кольцо выбора на земле */
    ellipse_ring(x, y, sz * 30 / 100, sz * 12 / 100, mix(RGB(0, 0, 0), P.surface, 120), SC(3));
    if (selected) {
        int pulse = (int)(SC(3) * (1 + sin(t * 5)));
        ellipse_ring(x, y, sz * 42 / 100 + pulse, sz * 17 / 100 + pulse / 2, P.surface, SC(5));
        ellipse_ring(x, y, sz * 42 / 100 + pulse, sz * 17 / 100 + pulse / 2, c, SC(3));
    }
    int u = front_lead_unit(&G, a), idx = u >= 0 ? art_unit_index(UNIT_TYPES[u].id) : -1;
    if (idx >= 0) art_unit_draw(idx, x - sz / 2, y - sz * 9 / 10, sz, sz);
    else {
        d_circle(x, y - sz / 3, sz / 4, P.surface, c, 2);
        d_icon(IC_WAR, x - sz / 6, y - sz / 3 - sz / 6, sz / 3, c);
    }
    int bh;
    army_badge(a, x, y + SC(3), selected, interactive, &bh);
    if (interactive) hit(x - sz / 3, y - sz * 8 / 10, sz * 2 / 3, sz * 8 / 10 + SC(3), A_ARMY_SEL, a);
}

/* значок цели войны: пульсирующее кольцо и скрещённые мечи */
static void target_mark(int x, int y, double sc, double t)
{
    int r = (int)(SC(15) * sc), pulse = (int)(fmod(t, 1.4) / 1.4 * SC(14) * sc);
    d_circle(x, y, r + pulse, CLR_INVALID, mix(P.red, P.surface, pulse * 255 / (SC(14) * sc + 1) > 230 ? 230 : (int)(pulse * 255 / (SC(14) * sc + 1))), 2);
    d_circle(x, y, r, CLR_INVALID, P.red, 3);
}

/* красная звезда Большой земли */
static void rear_mark(int x, int y, double sc, int large)
{
    int R = (int)(SC(large ? 11 : 9) * sc);
    POINT star[11];
    for (int i = 0; i < 10; i++) {
        double a = -PI / 2 + i * PI / 5, rr = i % 2 ? R * 0.42 : R;
        star[i].x = x + (int)(cos(a) * rr);
        star[i].y = y + (int)(sin(a) * rr);
    }
    star[10] = star[0];
    d_poly(star, 11, RGB(206, 40, 34), RGB(90, 16, 12), 1);
    /* подпись слева от звезды (справа встают армии); на маленькой карте — только звезда */
    if (!large) return;
    int tw = d_text_w(L"Большая земля", F_TINY);
    tag(x - R - SC(10) - tw / 2, y - (d_font_h(F_TINY) + SC(4)) / 2, L"Большая земля", F_TINY, P.ink, RGB(206, 40, 34));
}

/* окошко выбранной армии в углу карты */
static void sel_box(const MapProj *pj, int a, int large)
{
    int mx, my, mw, mh;
    mapv_rect(pj, &mx, &my, &mw, &mh);
    wchar_t l1[160], l2[200];
    const wchar_t *hint = large ? L"Щёлкните город или землю — армия пойдёт туда (можно и правой кнопкой). Щелчок по армии снимает выбор."
                                : L"Щёлкните место на карте — приказ идти";
    int sz = front_army_size(&G, a), d = G.armies[a].dest;
    swprintf(l1, 160, L"Армия %d · %d %ls · %ls", a + 1, sz, w_units(sz), FRONT_NODES[G.armies[a].node].name);
    if (d >= 0) {
        int eta = front_eta(&G, G.armies[a].node, d);
        swprintf(l2, 200, L"Идёт к цели «%ls»: %d %ls%ls", FRONT_NODES[d].name, eta, w_turns(eta),
                 front_army_supplied(&G, a) ? L"" : L" · нет снабжения (×0,6)");
    } else if (G.armies[a].node == front_target_node(&G))
        swprintf(l2, 200, L"У цели: бой в конце хода%ls", front_army_supplied(&G, a) ? L" · снабжение есть" : L" · нет снабжения: сила ×0,6");
    else swprintf(l2, 200, L"%ls", front_army_supplied(&G, a) ? L"Стоит на месте · снабжение есть" : L"Стоит на месте · нет снабжения: сила ×0,6");
    int f1 = large ? F_SMALL_B : F_TINY, f2 = F_TINY;
    int w1 = d_text_w(l1, f1), w2 = d_text_w(l2, f2), w3 = d_text_w(hint, F_TINY);
    int w = (w1 > w2 ? w1 : w2) + SC(4);
    if (w3 + SC(4) > w) w = w3 + SC(4);
    int maxw = mw - SC(60);
    if (w > maxw) w = maxw;
    int pad = SC(8), h = pad + d_font_h(f1) + SC(2) + d_font_h(f2) + SC(2) + d_font_h(F_TINY) + pad;
    int x = mx + SC(8), y = my + SC(8);
    d_rrect(x, y, w + pad * 2 + SC(22), h, SC(6), P.surface, army_col(a));
    d_ellipsis(x + pad, y + pad, w, l1, f1, P.ink);
    d_ellipsis(x + pad, y + pad + d_font_h(f1) + SC(2), w, l2, f2, front_army_supplied(&G, a) ? P.muted : P.red);
    d_ellipsis(x + pad, y + pad + d_font_h(f1) + d_font_h(f2) + SC(4), w, hint, F_TINY, P.muted);
    /* × — снять выбор */
    int bx = x + w + pad * 2 + SC(2), by = y + SC(4), bs = SC(16);
    if (hover(A_ARMY_SEL, -1)) d_circle(bx + bs / 2, by + bs / 2, bs / 2, P.surface2, CLR_INVALID, 1);
    d_line(bx + SC(4), by + SC(4), bx + bs - SC(4), by + bs - SC(4), P.muted, 2);
    d_line(bx + bs - SC(4), by + SC(4), bx + SC(4), by + bs - SC(4), P.muted, 2);
    hit(bx - SC(2), by - SC(2), bs + SC(4), bs + SC(4), A_ARMY_SEL, -1);
    hit(x, y, w + pad * 2 + SC(22), h, A_ARMY_SEL, a + 16);     /* показать армию: камера летит к ней */
}

/* пушки по обе стороны от звезды боя этого хода (набег, осада или бой, у которого армии уже нет) */
static void battle_cannons(const MapProj *pj, int large, double t)
{
    if (G.battleTurn != G.turn || G.over) return;
    int tn = front_target_node(&G);
    if (G.battleKind != WAR_NONE && G.warKind != WAR_NONE && tn >= 0 && front_armies_at(&G, tn)) return;   /* пушки у армии */
    int sx, sy, mx, my, mw, mh;
    double k;
    if (!mapv_ll(pj, G.battleLon, G.battleLat, 1.0, &sx, &sy, &k)) return;
    mapv_rect(pj, &mx, &my, &mw, &mh);
    if (sx < mx || sx > mx + mw || sy < my || sy > my + mh) return;
    int r = large ? SC(20) : SC(14), sz = large ? SC(30) : SC(22);
    fx_cannon(sx - r - sz, sy + SC(2), sz, +1, t);
    fx_cannon(sx + r + sz, sy + r + SC(4), sz, -1, t + 0.7);     /* враг — справа, под подписью боя */
}

void front_overlay(const MapProj *pj, int interactive, int large)
{
    if (U.screen != SCR_GAME) return;
    if (!G.frontReady) front_sync(&G);       /* старое сохранение: армии встают на карту сразу */
    int mx, my, mw, mh;
    mapv_rect(pj, &mx, &my, &mw, &mh);
    double t = (GetTickCount() % 3600000u) / 1000.0;
    int sel = sel_army();
    int top = interactive && ((large && U.modal == M_MAP) || (!large && U.modal == M_NONE));
    int showNodes = top && (sel >= 0 || U.tab == TAB_ARMY);
    int anim = 0;
    static int nx[FRONT_MAX_NODES], ny[FRONT_MAX_NODES], nv[FRONT_MAX_NODES];
    static double nk[FRONT_MAX_NODES];
    for (int n = 0; n < FRONT_NODE_COUNT; n++) {
        nv[n] = front_node_visible(&G, n) && node_xy(pj, n, 0.5, &nx[n], &ny[n], &nk[n]);
        if (nv[n] && (nx[n] < mx - SC(30) || nx[n] > mx + mw + SC(30) || ny[n] < my - SC(30) || ny[n] > my + mh + SC(30))) nv[n] = 0;
    }
    /* дороги между узлами */
    if (showNodes)
        for (int e = 0; e < FRONT_EDGE_COUNT; e++) {
            const FrontEdge *E = &FRONT_EDGES[e];
            if (E->a < 0 || !front_node_visible(&G, E->a) || !front_node_visible(&G, E->b)) continue;
            if (!nv[E->a] && !nv[E->b]) continue;
            POINT pt[ES];
            int ok[ES];
            int n = edge_pts(pj, E->a, E->b, pt, ok, 0, 0.3, 0, 0);
            int open = front_node_open(&G, E->a) && front_node_open(&G, E->b);
            dash_line(pt, ok, n, open ? mix(P.ink, P.surface, 110) : mix(P.red, P.surface, 110), SC(2), SC(4), SC(4), 0);
        }
    /* железные дороги и поезда; на вкладке «Армия» — ещё и перегоны, которые можно проложить */
    int rails = 0;
    for (int e = 0; e < FRONT_EDGE_COUNT; e++) if (front_rail(&G, e)) { draw_rail(pj, e, large); rails++; }
    if (interactive && U.tab == TAB_ARMY && game_has_tech(&G, "railway") && !G.over && (top || (!large && U.modal == M_NONE)))
        draw_rail_plans(pj, top, large);
    if (rails) { draw_trains(pj, t, large); anim = 1; }
    /* марши армий */
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(&G, a) || G.armies[a].dest < 0) continue;
        draw_march(pj, G.armies[a].node, G.armies[a].dest, army_col(a), a == sel, t, large, 0);
        anim = 1;
    }
    /* предпросмотр пути к узлу под мышью */
    if (sel >= 0 && top)
        for (int n = 0; n < FRONT_NODE_COUNT; n++)
            if (hover(A_FRONT_NODE, n) && n != G.armies[sel].dest && !front_cannot_send(&G, sel, n))
                draw_march(pj, G.armies[sel].node, n, army_col(sel), 1, t, large, 1);
    /* узлы фронта */
    int tn = front_target_node(&G);
    for (int n = 0; n < FRONT_NODE_COUNT; n++) {
        if (!nv[n]) continue;
        const FrontNode *N = &FRONT_NODES[n];
        double sc = mapv_scale(pj, nk[n]);
        if (sc < 0.8) sc = 0.8;
        if (sc > 1.8) sc = 1.8;
        if (N->kind == FN_REAR) { rear_mark(nx[n], ny[n], sc, large); if (interactive) hit(nx[n] - SC(12), ny[n] - SC(12), SC(24), SC(24), A_FRONT_NODE, n); continue; }
        if (!showNodes) continue;
        int r = (int)(SC(5) * sc), hv = interactive && hover(A_FRONT_NODE, n);
        int open = front_node_open(&G, n), can = sel >= 0 && !front_cannot_send(&G, sel, n);
        int foreign = !open && node_available_prov(n) >= 0;
        COLORREF fill = front_node_occupied(&G, n) ? RGB(122, 34, 28) : P.surface;
        COLORREF ring = foreign ? P.red : can ? army_col(sel) : mix(P.ink, P.surface, 70);
        if (hv) d_circle(nx[n], ny[n], r + SC(5), CLR_INVALID, ring, 2);
        d_circle(nx[n], ny[n], r, fill, ring, can || foreign ? 3 : 2);
        if (N->city < 0 && N->kind == FN_CITY && (large || hv)) {   /* узловые станции: своих подписей на карте нет */
            int f = F_TINY, tw = d_text_w(N->name, f), lx = nx[n] - r - SC(6) - tw;     /* слева: справа — армии */
            d_blend(lx - SC(3), ny[n] - d_font_h(f) / 2, tw + SC(6), d_font_h(f), P.surface, 170);
            d_text(lx, ny[n] - d_font_h(f) / 2, N->name, f, P.muted);
        }
        if (interactive) hit(nx[n] - r - SC(5), ny[n] - r - SC(5), 2 * r + SC(10), 2 * r + SC(10), A_FRONT_NODE, n);
    }
    /* цель войны */
    if (tn >= 0 && nv[tn]) {
        double sc = mapv_scale(pj, nk[tn]);
        if (sc < 0.8) sc = 0.8;
        if (sc > 1.8) sc = 1.8;
        target_mark(nx[tn], ny[tn], sc, t);
        anim = 1;
    }
    /* армии: дальние — первыми; несколько армий в одном узле — рядом */
    int order_[MAX_ARMIES], na = 0;
    for (int a = 0; a < MAX_ARMIES; a++) if (front_army_alive(&G, a) && nv[G.armies[a].node]) order_[na++] = a;
    for (int i = 1; i < na; i++) {
        int v = order_[i], j = i - 1;
        while (j >= 0 && ny[G.armies[order_[j]].node] > ny[G.armies[v].node]) { order_[j + 1] = order_[j]; j--; }
        order_[j + 1] = v;
    }
    for (int i = 0; i < na; i++) {
        int a = order_[i], n = G.armies[a].node;
        double sc = mapv_view(pj) ? pow(nk[n] / (mapv_px_per_km(pj) > 0 ? mapv_px_per_km(pj) : 1), 0.6) : 1.0;
        int sz = (int)((large ? SC(92) : SC(62)) * sc);
        if (sz < SC(40)) sz = SC(40);
        if (sz > SC(150)) sz = SC(150);
        unsigned here = front_armies_at(&G, n);
        int cnt = 0, idx = 0;
        for (int b = 0; b < MAX_ARMIES; b++) if ((here >> b) & 1u) { if (b < a) idx++; cnt++; }
        /* армия стоит справа внизу от узла: подпись города над ним остаётся видна */
        int x = nx[n] + sz / 2 + idx * sz * 62 / 100, y = ny[n] + sz / 5;
        int battle = n == tn && !G.over;
        if (battle) x = nx[n] - sz * 55 / 100 - idx * sz * 62 / 100;   /* у цели армия стоит перед врагом, слева */
        /* марш: в начале хода фигурка доезжает от прежнего узла (около секунды) */
        static DWORD seenAt;
        static int seenTurn = -1;
        if (G.armyMoveTurn == G.turn && G.armyPrev[a] >= 0 && G.armyPrev[a] < FRONT_NODE_COUNT) {
            if (seenTurn != G.turn) { seenTurn = G.turn; seenAt = GetTickCount(); }
            double p = (GetTickCount() - seenAt) / 1100.0;
            int px, py;
            double pk;
            if (p < 1 && node_xy(pj, G.armies[a].node == n ? G.armyPrev[a] : n, 0.5, &px, &py, &pk)) {
                double e = p * p * (3 - 2 * p);
                x = px + (int)((x - px) * e);
                y = py + (int)((y - py) * e);
                anim = 1;
            }
        }
        if (x < mx - sz || x > mx + mw + sz || y < my - sz || y > my + mh + sz) continue;
        if (battle) {   /* бой в конце хода: пушки бьют по врагу */
            fx_cannon(x + sz * 32 / 100, y + SC(2), sz * 40 / 100, +1, t);
            anim = 1;
        }
        army_token(a, x, y, sz, a == sel, interactive, t);
    }
    if (G.battleTurn == G.turn && !G.over) { battle_cannons(pj, large, t); anim = 1; }
    if (sel >= 0) anim = 1;
    if (sel >= 0 && top) sel_box(pj, sel, large);
    else if (top && U.tab == TAB_ARMY && front_army_count(&G)) {
        const wchar_t *h = L"Нажмите на армию, чтобы отдать приказ";
        int tw = d_text_w(h, F_TINY);
        d_blend(mx + SC(8), my + SC(8), tw + SC(16), d_font_h(F_TINY) + SC(8), P.surface, 200);
        d_text(mx + SC(16), my + SC(12), h, F_TINY, P.muted);
    }
    if (anim) ui_anim_request();
}

/* ---------- вкладка «Армия»: армии на карте и железные дороги ---------- */
typedef struct { const wchar_t *l; int op, on, wd; } ArmyBtn;     /* op < 0 — «На карте» */
static ArmyBtn army_btn(const wchar_t *l, int op, int on, int wd) { ArmyBtn b = { l, op, on, wd }; return b; }

static int army_card(int a, int x, int y, int w, int draw)
{
    int h = SC(118);
    if (!draw) return h;
    COLORREF c = army_col(a);
    int sel = sel_army() == a, sz = front_army_size(&G, a), node = G.armies[a].node, d = G.armies[a].dest, tn = front_target_node(&G);
    int sup = front_army_supplied(&G, a);
    d_rrect(x, y, w, h, SC(6), sel ? mix(tab_card_fill(), c, 40) : tab_card_fill(), sel ? c : P.line);
    d_fill(x + SC(1), y + SC(6), SC(4), h - SC(12), c);
    int bs = SC(76);
    unit_badge(front_lead_unit(&G, a), x + SC(12), y + SC(10), bs, bs, c);
    int tx = x + bs + SC(24), tw = w - bs - SC(36);
    wchar_t buf[300], title[64];
    swprintf(title, 64, L"Армия %d", a + 1);
    d_text(tx, y + SC(8), title, F_H3, P.ink);
    const wchar_t *st = node == tn ? L"у цели: бой в конце хода" : d >= 0 ? L"в пути" : L"стоит";
    int pw = w_pill(tx + d_text_w(title, F_H3) + SC(10), y + SC(11), st, node == tn || d >= 0 ? P.onAccent : P.muted,
                    node == tn ? P.red : d >= 0 ? c : P.surface2);
    (void)pw;
    /* состав */
    int len = swprintf(buf, 300, L"%ls · %d %ls:", FRONT_NODES[node].name, sz, w_units(sz));
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS && len < 280; u++)
        if (G.armies[a].units[u] > 0) len += swprintf(buf + len, 300 - len, L" %ls ×%d,", UNIT_TYPES[u].name, G.armies[a].units[u]);
    if (len > 0 && buf[len - 1] == L',') buf[len - 1] = 0;
    d_ellipsis(tx, y + SC(34), tw, buf, F_SMALL, P.ink);
    double atk = front_army_attack(&G, a, WAR_CONQUEST, -1), def = front_army_defense(&G, a);
    if (d >= 0) {
        int eta = front_eta(&G, node, d);
        swprintf(buf, 300, L"Атака %ld · оборона %ld · идёт: %ls → %ls, ещё %d %ls", lround(atk), lround(def), FRONT_NODES[node].name,
                 FRONT_NODES[d].name, eta, w_turns(eta));
    } else swprintf(buf, 300, L"Атака %ld · оборона %ld · %ls", lround(atk), lround(def),
                    sup ? L"снабжение есть" : front_attrition_at(&G, node) ? L"нет снабжения: сила ×0,6 и потери каждый второй ход"
                                                                        : L"нет снабжения: сила ×0,6");
    d_ellipsis(tx, y + SC(54), tw, buf, F_SMALL, sup ? P.muted : P.red);
    /* кнопки */
    int by = y + h - SC(40), bx = tx;
    ArmyBtn B[6];
    int nb = 0;
    unsigned here = front_armies_at(&G, node) & ~(1u << a);
    B[nb++] = army_btn(L"На карте", -1, 1, SC(104));
    if (tn >= 0 && node != tn && d != tn) B[nb++] = army_btn(L"К цели", FA_TARGET, !front_cannot_send(&G, a, tn), SC(84));
    B[nb++] = army_btn(L"Разделить", FA_SPLIT, sz >= 2 && front_army_count(&G) < MAX_ARMIES, SC(110));
    if (here) B[nb++] = army_btn(L"Объединить", FA_MERGE, 1, SC(122));
    if (d >= 0) B[nb++] = army_btn(L"Стоп", FA_HALT, 1, SC(66));
    else if (node != front_recruit_node(&G)) B[nb++] = army_btn(L"К столице", FA_HOME, !front_cannot_send(&G, a, front_recruit_node(&G)), SC(110));
    for (int i = 0; i < nb; i++) {
        if (bx + B[i].wd > x + w - SC(10)) break;
        if (B[i].op < 0) w_button(bx, by, B[i].wd, SC(30), B[i].l, sel ? B_SECONDARY : B_PRIMARY, !G.over, A_ARMY_SEL, a + 16);
        else w_button(bx, by, B[i].wd, SC(30), B[i].l, B_SECONDARY, B[i].on && !G.over, A_ARMY_SPLIT, FRONT_ARG(a, B[i].op));
        bx += B[i].wd + SC(6);
    }
    return h;
}

int front_tab_armies(int x, int y, int w)
{
    int y0 = y;
    front_sync(&G);         /* после загрузки старого сохранения армии появляются сразу (идемпотентно) */
    tab_section(x, &y, L"АРМИИ НА КАРТЕ");
    y += d_wrap(x, y, w, L"Воюйте сами, как в стратегиях: нажмите на фигурку армии на карте (или «На карте» ниже), затем щёлкните город "
                         L"или землю — армия пойдёт туда; правая кнопка мыши тоже отдаёт приказ. За ход армия проходит одну дорогу "
                         L"(в долгие ходы средних веков — две-три), а по своей железной дороге — до трёх перегонов. Бой похода идёт, "
                         L"только когда армия стоит на земле-цели. Новые войска собираются у столицы. Армии нужно снабжение: "
                         L"не дальше двух дорог от столицы, крупного города, своей земли или станции; без него сила ×0,6.",
                  F_SMALL, P.muted, 1) + SC(10);
    int any = 0;
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(&G, a)) continue;
        int h = army_card(a, x, y, w, 0);
        if (y + h > U.panelY - SC(40) && y < U.panelY + U.panelH + SC(40)) army_card(a, x, y, w, 1);
        y += h + SC(8);
        any = 1;
    }
    if (!any) {
        y += d_wrap(x, y, w, L"Армий пока нет: наберите войска ниже — они соберутся в армию у столицы и появятся на карте.",
                    F_SMALL, P.ink, 1) + SC(10);
    }
    return y - y0 + SC(6);
}

int front_tab_rails(int x, int y, int w)
{
    int y0 = y, has = game_has_tech(&G, "railway"), built = front_rail_count(&G);
    if (!has && !built && G.era < 4) return 0;
    wchar_t buf[400];
    double rg, ri;
    war_rail_income(&G, &rg, &ri);
    tab_section(x, &y, L"ЖЕЛЕЗНЫЕ ДОРОГИ");
    swprintf(buf, 400, L"По своей железной дороге армия проходит до трёх перегонов за ход, станции снабжают армии, а каждый перегон "
                       L"приносит +%.0f казны и +%.0f производства за ход. Дорогу строят от столицы, продолжая сеть: кнопкой ниже "
                       L"или знаком «+» на большой карте. Стройки "
                       L"«Московско-Брестская дорога» и «Либаво-Роменская дорога» прокладывают целые магистрали.", rg, ri);
    y += d_wrap(x, y, w, buf, F_SMALL, P.muted, 1) + SC(10);
    if (!has) {
        y += d_wrap(x, y, w, L"Нужна технология «Железные дороги» (1871 год, вкладка «Технологии»).", F_SMALL_B, P.ink, 1) + SC(12);
        return y - y0;
    }
    /* что можно построить: сначала исторические перегоны */
    int list[FRONT_MAX_EDGES], n = 0;
    for (int e = 0; e < FRONT_EDGE_COUNT; e++) if (rail_plannable(e)) list[n++] = e;
    for (int i = 1; i < n; i++) {
        int v = list[i], j = i - 1;
        int kv = FRONT_EDGES[v].year ? FRONT_EDGES[v].year : 3000;
        while (j >= 0 && (FRONT_EDGES[list[j]].year ? FRONT_EDGES[list[j]].year : 3000) > kv) { list[j + 1] = list[j]; j--; }
        list[j + 1] = v;
    }
    int show = n < 12 ? n : 12, rh = SC(44);
    for (int i = 0; i < show; i++) {
        int e = list[i];
        const FrontEdge *E = &FRONT_EDGES[e];
        int ry = y + i * (rh + SC(4));
        int hv = hover(A_RAIL_BUILD, e);
        d_rrect(x, ry, w, rh, SC(6), hv ? mix(tab_card_fill(), P.ink, 14) : tab_card_fill(), P.line);
        swprintf(buf, 400, L"%ls — %ls", FRONT_NODES[E->a].name, FRONT_NODES[E->b].name);
        d_ellipsis(x + SC(12), ry + SC(5), w - SC(300), buf, F_SMALL_B, P.ink);
        if (E->line) swprintf(buf, 400, L"%d км · %ls (%d)", E->km, E->line, E->year);
        else swprintf(buf, 400, L"%d км", E->km);
        d_ellipsis(x + SC(12), ry + SC(24), w - SC(300), buf, F_TINY, E->line ? P.blue : P.muted);
        int cg = front_rail_gold(&G, e), ci = front_rail_ind(&G, e);
        wchar_t a1[32], a2[32];
        fmt_num(cg, a1, 32);
        fmt_num(ci, a2, 32);
        int bx = x + w - SC(10) - SC(110);
        int cx = bx - SC(150);
        d_icon(IC_GOLD, cx, ry + SC(13), SC(16), P.gold);
        d_text(cx + SC(20), ry + SC(12), a1, F_SMALL_B, G.gold >= cg ? P.ink : P.red);
        int ox = cx + SC(26) + d_text_w(a1, F_SMALL_B);
        d_icon(IC_IND, ox, ry + SC(13), SC(16), mix(P.gold, P.red, 90));
        d_text(ox + SC(20), ry + SC(12), a2, F_SMALL_B, G.ind >= ci ? P.ink : P.red);
        w_button(bx, ry + SC(7), SC(110), SC(30), L"Построить", B_PRIMARY, !front_cannot_build_rail(&G, e), A_RAIL_BUILD, e);
        hit(x, ry, bx - x - SC(4), rh, A_RAIL_BUILD, e);
    }
    y += show * (rh + SC(4)) + SC(6);
    if (n > show) {
        swprintf(buf, 400, L"И ещё перегонов, которые можно проложить: %d — все они отмечены знаком «+» на большой карте.", n - show);
        y += d_wrap(x, y, w, buf, F_TINY, P.muted, 1) + SC(6);
    } else if (!n) y += d_wrap(x, y, w, L"Сейчас продолжить сеть негде: все соседние перегоны уже построены или под оккупацией.", F_SMALL, P.muted, 1) + SC(6);
    if (built) {
        int len = swprintf(buf, 400, L"Построено перегонов: %d —", built);
        for (int e = 0; e < FRONT_EDGE_COUNT && len < 360; e++)
            if (front_rail(&G, e)) len += swprintf(buf + len, 400 - len, L" %ls — %ls,", FRONT_NODES[FRONT_EDGES[e].a].name, FRONT_NODES[FRONT_EDGES[e].b].name);
        if (len > 0 && buf[len - 1] == L',') buf[len - 1] = L'.';
        y += d_wrap(x, y, w, buf, F_SMALL, P.ink, 1) + SC(6);
    }
    return y - y0 + SC(10);
}

/* ---------- сцены разработчика 40–49 ---------- */
static int unit_id(const char *id)
{
    for (int u = 0; u < UNIT_COUNT; u++) if (!strcmp(UNIT_TYPES[u].id, id)) return u;
    return -1;
}

static int prov_id(const char *id)
{
    for (int p = 0; p < PROVINCE_COUNT; p++) if (!strcmp(PROVINCES[p].id, id)) return p;
    return -1;
}

static void own(const char *id)
{
    int p = prov_id(id);
    if (p >= 0 && p < MAX_PROVINCES) G.provOwned[p] = 1;
}

static int node_named(const wchar_t *name)
{
    for (int n = 0; n < FRONT_NODE_COUNT; n++) if (!wcscmp(FRONT_NODES[n].name, name)) return n;
    return -1;
}

/* набрать n соединений вида id (без оглядки на казну и предел войска) */
static void recruit(const char *id, int n)
{
    int u = unit_id(id);
    for (int i = 0; i < n && u >= 0; i++) {
        G.gold += 5000; G.ind += 5000;
        if (war_army_size(&G) >= war_army_cap(&G)) G.pop += 400;
        if (!war_recruit(&G, u)) { G.units[u]++; front_sync(&G); }
    }
}

/* все перегоны исторической магистрали по Беларуси */
static void rail_line(const wchar_t *line)
{
    for (int e = 0; e < FRONT_EDGE_COUNT; e++) {
        const FrontEdge *E = &FRONT_EDGES[e];
        if (E->a >= 0 && E->line && !wcscmp(E->line, line) && FRONT_NODES[E->a].kind == FN_CITY && FRONT_NODES[E->b].kind == FN_CITY)
            G.rails[e >> 6] |= 1ull << (e & 63);
    }
}

static void cam(double lon, double lat, double zoom, double pitch, double yaw)
{
    t3d_cam_reset(&U.cam);
    U.camInit = 1;
    t3d_lonlat(lon, lat, &U.cam.tx, &U.cam.ty);
    U.cam.zoom = zoom; U.cam.pitch = pitch; U.cam.yaw = yaw;
}

/* 1340: две армии ВКЛ — у Бреста и в Новогрудке, первая выбрана и идёт в поход на Волынь */
static void scene_1340(int battle)
{
    dev_play_until(1340); dev_resolve();
    memset(G.units, 0, sizeof G.units);
    front_sync(&G);
    recruit("khorugv", 4); recruit("crossbow", 3); recruit("druzhina", 3);
    int b = front_split(&G, 0);
    int vol = prov_id("volhynia"), node = front_node_of_prov(vol);
    G.armies[0].node = front_node_of_city(C_BREST);
    if (b >= 0) G.armies[b].node = front_node_of_city(C_NOVOGRUDOK);
    own("smolensk");
    war_declare(&G, WAR_CONQUEST, vol);
    if (battle) {                                       /* армия уже у цели: бой в конце хода */
        G.armies[0].node = node;
        G.warProgress = 45; G.warTurns = 1;
        G.battleLon = (float)PROVINCES[vol].lon; G.battleLat = (float)PROVINCES[vol].lat;
        G.battleTurn = G.turn; G.battleWon = 1; G.battleKind = WAR_CONQUEST;
        swprintf(G.warReport, 200, L"Поход на землю «Волынь»: продвижение 45%% (за ход +45). Армия цела.");
    } else front_send(&G, 0, node);
    U.selArmy = 0;
    U.tab = TAB_ARMY;
}

/* 1890: магистрали 1871–1886 годов, по ним ходят поезда; армии в Минске и Бресте */
static void scene_1890(void)
{
    dev_play_until(1890); dev_resolve();
    memset(G.units, 0, sizeof G.units);
    front_sync(&G);
    recruit("line", 6); recruit("artillery", 3);
    int b = front_split(&G, 0);
    G.tech[tech_index("railway")] = 1;
    G.rails[0] = G.rails[1] = 0;
    rail_line(L"Московско-Брестская");
    rail_line(L"Либаво-Роменская");
    rail_line(L"Полесские дороги");
    if (b >= 0) { G.armies[b].node = front_node_of_city(C_BREST); front_send(&G, b, front_node_of_city(C_GOMEL)); }
    U.selArmy = b;
    U.tab = TAB_ARMY;
}

/* 1943: оккупация; Гомельщина освобождена, армии у Гомеля и на Большой земле, операция на Могилёвщине */
static void scene_1943(void)
{
    dev_play_until(1943); dev_resolve();
    memset(G.units, 0, sizeof G.units);
    front_sync(&G);
    G.occupied &= ~(1u << 5); G.liberated |= 1u << 5;
    recruit("rifle", 6); recruit("air", 2); recruit("partisans", 3);
    int b = front_split(&G, 0);
    G.armies[0].node = front_node_of_city(C_GOMEL);
    if (b >= 0) G.armies[b].node = front_rear_node();
    war_declare(&G, WAR_LIBERATION, 4);
    front_send(&G, 0, front_node_of_city(C_MOGILEV));
    if (b >= 0) front_send(&G, b, front_node_of_city(C_VITEBSK));
    G.warProgress = 20; G.warTurns = 1;
    U.selArmy = 0;
    U.tab = TAB_ARMY;
}

void front_dev_scene(int n)
{
    switch (n) {
    case 40:                                            /* 1340: выбранная армия, путь на Волынь */
        scene_1340(0);
        U.modal = M_MAP;
        cam(25.0, 52.1, 1.35, 52, 0);
        break;
    case 41:                                            /* бой на Волыни: пушки у армии */
        scene_1340(1);
        U.modal = M_MAP;
        cam(25.3, 51.25, 2.6, 44, 0);
        break;
    case 42:                                            /* 1890: железные дороги и поезда */
        scene_1890();
        U.modal = M_MAP;
        cam(27.4, 53.4, 1.45, 50, 0);
        break;
    case 43:                                            /* 1943: освобождение, армии у центров областей */
        scene_1943();
        U.modal = M_MAP;
        cam(30.4, 54.0, 1.55, 48, 0);
        break;
    case 44:                                            /* вкладка «Армия»: армии на карте (1340) */
        scene_1340(0);
        U.scroll[TAB_ARMY] = 0;
        break;
    case 45:                                            /* вкладка «Армия»: железные дороги (1890) */
        scene_1890();
        G.gold += 2000; G.ind += 1000;
        break;
    case 46:                                            /* тёмная тема, карта на панели */
        scene_1340(0);
        ui_toggle_theme();
        break;
    case 47: {                                          /* 1942: армия без снабжения в тылу врага */
        scene_1943();
        G.occupied = ((1u << REGION_COUNT) - 1) & ~(1u << 5);
        G.warKind = 0; G.warTarget = -1; G.warProgress = 0;
        G.armies[0].node = front_node_of_city(C_MINSK); G.armies[0].dest = -1;
        G.armyLow[0] = 1;
        U.modal = M_MAP;
        cam(28.6, 54.0, 1.8, 46, 0);
        break;
    }
    case 48: {                                          /* 1812: присоединённые земли остались своими после 1795 года */
        dev_play_until(1812); dev_resolve();
        own("smolensk"); own("volhynia"); own("riga");
        recruit("line", 5);
        U.selArmy = -1;
        U.tab = TAB_ARMY;
        U.modal = M_MAP;
        cam(27.8, 53.9, 0.85, 52, 0);
        map_invalidate();
        break;
    }
    case 49:                                            /* вкладка «Армия» в 1943 году */
        scene_1943();
        U.scroll[TAB_ARMY] = 0;
        break;
    default: break;
    }
    (void)node_named;
}

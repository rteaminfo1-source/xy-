/* Админ-версия (BelarusTech_Admin.exe, сборка с -DADMIN_BUILD): панель администратора для учителя и защиты
 * проекта — выдать казну и другие ресурсы, перенестись в любой ключевой год, сразу показать войну, взлёт
 * ракеты и метро, открыть встроенные в exe презентацию и текст выступления. В обычной сборке панели нет. */
#include "ui.h"
#include "mapview.h"
#include "war.h"
#include "front.h"
#include "citynet.h"
#include <stdio.h>
#include <string.h>

#ifdef ADMIN_BUILD

enum { AD_GOLD1, AD_GOLD10, AD_SCI, AD_IND, AD_CULT, AD_POP, AD_STAB, AD_ECO,
       AD_TECH, AD_BUILD, AD_PEOPLE, AD_ARMY, AD_WIN, AD_FREE, AD_LAUNCH, AD_METRO, AD_MAP, AD_CHAES,
       AD_PPTX, AD_DOCX, AD_YEAR = 100 };

static const int YEARS[] = { 1100, 1250, 1340, 1410, 1517, 1569, 1655, 1772, 1795, 1812, 1863, 1905,
                             1921, 1941, 1942, 1943, 1944, 1945, 1961, 1975, 1984, 1986, 1991, 2024 };
#define NYEARS ((int)(sizeof YEARS / sizeof YEARS[0]))

/* ---------- встроенные файлы: презентация и текст выступления ---------- */
static void open_embedded(int id, const wchar_t *file, const wchar_t *name)
{
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    HGLOBAL h = r ? LoadResource(NULL, r) : NULL;
    const void *data = h ? LockResource(h) : NULL;
    DWORD size = r ? SizeofResource(NULL, r) : 0;
    if (!data || !size) { ui_toast(L"Файл не встроен в эту сборку"); return; }
    wchar_t dir[MAX_PATH], path[MAX_PATH + 64];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    if (!n || n >= MAX_PATH) { ui_toast(L"Нет папки для временных файлов"); return; }
    swprintf(path, MAX_PATH + 64, L"%lsBelarusTechFate", dir);
    CreateDirectoryW(path, NULL);
    swprintf(path, MAX_PATH + 64, L"%lsBelarusTechFate\\%ls", dir, file);
    HANDLE fh = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD done = 0;
    BOOL ok = fh != INVALID_HANDLE_VALUE && WriteFile(fh, data, size, &done, NULL) && done == size;
    if (fh != INVALID_HANDLE_VALUE) CloseHandle(fh);
    if (!ok) { ui_toast(L"Не удалось сохранить «%ls» — возможно, файл уже открыт", name); return; }
    /* открыть программой по умолчанию (PowerPoint, Word…) */
    HMODULE sh = LoadLibraryW(L"shell32.dll");
    HINSTANCE (WINAPI *exec)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT) = sh ? (void *)GetProcAddress(sh, "ShellExecuteW") : NULL;
    if (!exec || (INT_PTR)exec(g_hwnd, L"open", path, NULL, NULL, SW_SHOWNORMAL) <= 32)
        ui_toast(L"Не удалось открыть «%ls»: нужна программа для таких файлов", name);
    else ui_toast(L"Открываю «%ls»", name);
}

/* ---------- перенос во времени: партия пересчитывается до года ---------- */
static void jump_to(int year)
{
    dev_play_until(year);
    G.cutscenes = 0;                 /* взлёты ракет и приближение к Минску покажутся, если их год рядом */
    U.screen = SCR_GAME;
    U.modal = M_NONE;
    U.camInit = 0;
    U.selArmy = -1;
    U.city = -1;
    U.eraFilter = G.era;
    memset(U.scroll, 0, sizeof U.scroll);
    map_invalidate();
    ui_toast(L"Перенеслись в %d год", G.year);
    ui_after_change();
}

static void add_army(void)
{
    int best = -1;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) if (war_unit_state(&G, u) == US_AVAILABLE) best = u;
    if (best < 0) best = 0;
    G.units[best] += 5;
    front_sync(&G);
    ui_toast(L"В строю: +5 «%ls»", UNIT_TYPES[best].name);
    map_invalidate();
}

static void win_war(void)
{
    int tn = front_target_node(&G);
    if (G.warKind == WAR_NONE || tn < 0) { ui_toast(L"Сейчас нет войны"); return; }
    if (!front_armies_at(&G, tn)) {           /* армия мгновенно у цели */
        int a = front_army_for(&G, tn);
        if (a < 0) { add_army(); a = front_army_for(&G, tn); }
        if (a >= 0) { G.armies[a].node = tn; G.armies[a].dest = -1; }
    }
    G.warProgress = 160;
    if (war_front_battle(&G)) ui_toast(L"%ls", G.warReport);
    map_invalidate();
}

static int person_named(const char *id)
{
    for (int i = 0; i < PEOPLE_COUNT; i++) if (!strcmp(PEOPLE[i].id, id)) return i;
    return -1;
}

int admin_click(int arg)
{
    int game = U.screen == SCR_GAME && !G.over;
    if (arg >= AD_YEAR && arg < AD_YEAR + NYEARS) { jump_to(YEARS[arg - AD_YEAR]); return 1; }
    switch (arg) {
    case AD_PPTX: open_embedded(801, L"BelarusTech-Presentation.pptx", L"Презентация"); return 1;
    case AD_DOCX: open_embedded(802, L"BelarusTech-Speech.docx", L"Текст выступления"); return 1;
    }
    if (!game) { ui_toast(L"Сначала начните игру или перенеситесь в год"); return 1; }
    switch (arg) {
    case AD_GOLD1:  G.gold += 1000;  ui_toast(L"Казна +1 000"); break;
    case AD_GOLD10: G.gold += 10000; ui_toast(L"Казна +10 000"); break;
    case AD_SCI:    G.sci += 1000;   ui_toast(L"Знания +1 000"); break;
    case AD_IND:    G.ind += 1000;   ui_toast(L"Производство +1 000"); break;
    case AD_CULT:   G.cult += 500;   ui_toast(L"Культура +500"); break;
    case AD_POP:    G.pop += 500;    ui_toast(L"Население +500 тыс."); break;
    case AD_STAB:   G.stab = 100;    ui_toast(L"Стабильность 100"); break;
    case AD_ECO:    G.eco = 100;     ui_toast(L"Экология 100"); break;
    case AD_TECH: {
        int n = 0;
        for (int t = 0; t < TECH_COUNT; t++) if (TECHS[t].era <= G.era && !G.tech[t]) { G.tech[t] = 1; G.techTurn[t] = G.turn; n++; }
        ui_toast(L"Открыто технологий: %d", n);
        break;
    }
    case AD_BUILD: {
        int n = 0;
        G.gold += 100000; G.ind += 100000;
        for (int p = 0; p < PROJECT_COUNT; p++) if (game_proj_state(&G, p) == PS_AVAILABLE && game_build(&G, p)) n++;
        G.gold -= 100000; G.ind -= 100000;
        if (G.gold < 0) G.gold = 0;
        if (G.ind < 0) G.ind = 0;
        ui_toast(L"Построено: %d", n);
        break;
    }
    case AD_PEOPLE: {
        int n = 0;
        G.gold += 100000;
        for (int i = 0; i < PEOPLE_COUNT; i++) if (game_person_state(&G, i) == HS_AVAILABLE && game_hire(&G, i)) n++;
        G.gold -= 100000;
        if (G.gold < 0) G.gold = 0;
        ui_toast(L"Приглашено деятелей: %d", n);
        break;
    }
    case AD_ARMY: add_army(); break;
    case AD_WIN: win_war(); break;
    case AD_FREE:
        if (!G.occupied) { ui_toast(L"Оккупированных областей нет"); break; }
        G.liberated |= G.occupied;
        G.occupied = 0;
        if (G.warKind == WAR_LIBERATION) { G.warKind = WAR_NONE; G.warTarget = -1; G.warProgress = 0; G.warTurns = 0; }
        front_sync(&G);
        ui_toast(L"Все области освобождены");
        break;
    case AD_LAUNCH: {
        int p = person_named("vasilevskaya");
        if (p >= 0) { U.modal = M_NONE; ui_click(A_NEXT, 7100 + p); }   /* взлёт ракеты (ui_fx.c) */
        return 1;
    }
    case AD_METRO: {
        int n = citynet_of_city(C_MINSK);
        if (n >= 0) { U.modal = M_NONE; ui_click(A_NET_OPEN, n); }
        return 1;
    }
    case AD_MAP: U.modal = M_MAP; return 1;
    case AD_CHAES: U.modal = M_NONE; ui_click(A_NEXT, FX_ARG_CHAES); return 1;   /* авария на ЧАЭС (ui_fx.c) */
    }
    map_invalidate();
    ui_invalidate();
    return 1;
}

/* ---------- окно панели ---------- */
static void section(int x, int *y, int w, const wchar_t *s)
{
    *y += SC(6);
    d_text(x, *y, s, F_SMALL_B, P.red);
    *y += d_font_h(F_SMALL_B) + SC(4);
    d_fill(x, *y, w, 1, P.line);
    *y += SC(8);
}

/* ряд кнопок равной ширины; возвращает высоту */
static int buttons(int x, int y, int w, const wchar_t **labels, const int *args, int n, int cols, int enabled)
{
    int gap = SC(8), bh = SC(34), bw = (w - (cols - 1) * gap) / cols, rows = (n + cols - 1) / cols;
    for (int i = 0; i < n; i++)
        w_button(x + (i % cols) * (bw + gap), y + (i / cols) * (bh + gap), bw, bh, labels[i], B_SECONDARY, enabled, A_ADMIN, args[i]);
    return rows * (bh + gap);
}

int admin_body(int x, int y, int w)
{
    int y0 = y, game = U.screen == SCR_GAME && !G.over;
    d_text(x, y, L"АДМИН-ПАНЕЛЬ", F_SMALL_B, P.red);
    y += d_font_h(F_SMALL_B) + SC(2);
    y += d_wrap(x, y, w, L"Для учителя и защиты проекта: ресурсы, перенос во времени, война, анимации и встроенная презентация. F9 — открыть или закрыть.", F_SMALL, P.muted, 1) + SC(4);

    section(x, &y, w, L"Презентация и выступление (встроены в программу)");
    {
        static const wchar_t *L1[] = { L"Открыть презентацию", L"Открыть текст выступления" };
        static const int A1[] = { AD_PPTX, AD_DOCX };
        y += buttons(x, y, w, L1, A1, 2, 2, 1);
    }
    section(x, &y, w, game ? L"Ресурсы" : L"Ресурсы — после начала игры");
    {
        static const wchar_t *L2[] = { L"Казна +1 000", L"Казна +10 000", L"Знания +1 000", L"Производство +1 000",
                                       L"Культура +500", L"Население +500 тыс.", L"Стабильность 100", L"Экология 100" };
        static const int A2[] = { AD_GOLD1, AD_GOLD10, AD_SCI, AD_IND, AD_CULT, AD_POP, AD_STAB, AD_ECO };
        y += buttons(x, y, w, L2, A2, 8, 4, game);
    }
    section(x, &y, w, L"Перенестись в год (партия пересчитается до этого года)");
    {
        wchar_t lab[NYEARS][8];
        const wchar_t *L3[NYEARS];
        int A3[NYEARS];
        for (int i = 0; i < NYEARS; i++) { swprintf(lab[i], 8, L"%d", YEARS[i]); L3[i] = lab[i]; A3[i] = AD_YEAR + i; }
        y += buttons(x, y, w, L3, A3, NYEARS, 8, 1);
    }
    section(x, &y, w, game ? L"Война, открытия и анимации" : L"Война, открытия и анимации — после начала игры");
    {
        static const wchar_t *L4[] = { L"Победа в войне сейчас", L"Освободить все области", L"Армия: +5 соединений", L"3D-карта во весь экран",
                                       L"Открыть технологии эпохи", L"Построить всё доступное", L"Пригласить всех деятелей", L"Взлёт ракеты",
                                       L"Метро Минска", L"Авария на ЧАЭС" };
        static const int A4[] = { AD_WIN, AD_FREE, AD_ARMY, AD_MAP, AD_TECH, AD_BUILD, AD_PEOPLE, AD_LAUNCH, AD_METRO, AD_CHAES };
        y += buttons(x, y, w, L4, A4, 10, 4, game);
    }
    y += SC(6);
    w_button(x + w - SC(140), y, SC(140), SC(40), L"Закрыть", B_PRIMARY, 1, A_CLOSE, 0);
    return y + SC(40) - y0;
}

#else

int admin_click(int arg) { (void)arg; return 0; }
int admin_body(int x, int y, int w) { (void)x; (void)y; (void)w; return 0; }

#endif

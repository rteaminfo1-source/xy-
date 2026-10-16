/* Режим разработчика: BelarusTech.exe --scene=N открывает заданное состояние (для скриншотов и проверки). */
#include "ui.h"
#include "war.h"
#include <string.h>

static void bot_turn(Game *g)
{
    for (int guard = 0; guard < 200; guard++) {
        int best = -1, bc = 1 << 30;
        for (int t = 0; t < TECH_COUNT; t++)
            if (game_tech_state(g, t) == TS_AVAILABLE) {
                int c = game_tech_cost(g, t);
                if (c <= g->sci && c < bc) { bc = c; best = t; }
            }
        if (best < 0) break;
        game_research(g, best);
    }
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (game_proj_state(g, p) == PS_AVAILABLE && g->gold >= game_proj_gold(g, p) + 10 && g->ind >= game_proj_ind(g, p))
            game_build(g, p);
    for (int i = 0; i < PEOPLE_COUNT; i++)
        if (game_person_state(g, i) == HS_AVAILABLE && g->gold >= game_person_cost(g, i) * 1.5) game_hire(g, i);
    int q = game_quiz_pick(g);
    if (q >= 0) {
        int ans = 0;
        for (int i = 0; i < 4; i++) if (g->quizPerm[i] == QUIZ[q].correct) ans = i;
        game_quiz_answer(g, ans);
    }
}

static void resolve(Game *g)
{
    QItem q;
    while (game_pending(g, &q)) {
        if (q.type == Q_HIST || q.type == Q_RAND) {
            const Event *e = game_event_ptr(&q);
            int pick = 0;
            for (int c = e->nch - 1; c >= 0; c--) if (game_choice_ok(g, e, c)) { pick = c; break; }
            Outcome o;
            game_choose(g, pick, &o);
        } else game_ack(g);
    }
}

/* Играет ботом до указанного года; события последнего хода не разрешает. */
static void play_until(int year)
{
    game_new(&G, 1, 20260101u);
    while (!G.over && game_next_year(&G) <= year) {
        resolve(&G);
        bot_turn(&G);
        game_end_turn(&G);
    }
}

void dev_scene(int n)
{
    U.screen = SCR_GAME;
    U.city = -1;
    switch (n) {
    case 1: U.screen = SCR_TITLE; break;
    case 2: U.screen = SCR_TITLE; U.modal = M_NEWGAME; break;
    case 3: game_new(&G, 1, 7u); break;                                   /* старт: окно эпохи */
    case 4: play_until(1510); break;                                      /* событие (Орша/Скорина) */
    case 5: play_until(1960); resolve(&G); U.tab = TAB_TECH; break;
    case 6: play_until(2024); resolve(&G); U.tab = TAB_RESULTS; break;
    case 7: play_until(2030); break;                                      /* итоги */
    case 8: play_until(1905); resolve(&G); U.tab = TAB_POLICY; break;
    case 9: play_until(1772); resolve(&G); U.tab = TAB_QUIZ; G.quizCur = -1; G.quizAnswered = 0; break;
    case 10: play_until(1960); resolve(&G); U.tab = TAB_PROJ; ui_toggle_theme(); break;
    case 11: play_until(1600); resolve(&G); U.tab = TAB_PEOPLE; break;
    case 12: play_until(1600); resolve(&G); U.modal = M_TECH; U.modalArg = tech_index("rocketry"); break;
    case 13: play_until(1986); resolve(&G); U.tab = TAB_LOG; break;
    case 14: play_until(1700); resolve(&G); U.modal = M_CITY; U.modalArg = C_GRODNO; break;
    case 15: U.screen = SCR_TITLE; U.modal = M_CREDITS; break;                  /* об игре и издатели */
    case 16: play_until(1517); resolve(&G); U.minuteMs = 52000; break;          /* до минутного дохода 8 секунд */
    case 17: U.screen = SCR_TITLE; U.hasSave = 1; break;                         /* титул с кнопкой «Продолжить» */
    case 18: play_until(2024); resolve(&G); U.modal = M_MAP; break;                  /* 3D-карта во весь экран */
    case 19: play_until(1490); resolve(&G); U.tab = TAB_QUESTS; break;               /* задания */
    case 20: play_until(1861); resolve(&G); U.tab = TAB_PEOPLE; break;               /* правители и деятели */
    case 21: play_until(1950); resolve(&G); U.modal = M_RULER; U.modalArg = 25; break; /* карточка правителя */
    case 22: play_until(1700); resolve(&G); U.modal = M_MAP; break;                  /* карта XVII века */
    case 23: case 24: {                                                             /* армия и поход, 1340 */
        play_until(1340); resolve(&G);
        G.gold += 400; G.ind += 200;
        int kh = -1;
        for (int u = 0; u < UNIT_COUNT; u++) if (!strcmp(UNIT_TYPES[u].id, "druzhina")) kh = u;
        for (int i = 0; i < 5 && kh >= 0; i++) war_recruit(&G, kh);
        for (int p = 0; p < PROVINCE_COUNT; p++) if (!strcmp(PROVINCES[p].id, "smolensk")) G.provOwned[p] = 1;
        for (int p = 0; p < PROVINCE_COUNT; p++) if (!strcmp(PROVINCES[p].id, "volhynia")) { war_declare(&G, WAR_CONQUEST, p); G.warProgress = 45; }
        swprintf(G.warReport, 200, L"Поход на землю «Волынь»: продвижение 45%% (за ход +45). Войско цело.");
        U.tab = TAB_ARMY;
        if (n == 24) U.modal = M_MAP;
        break;
    }
    case 25: case 26: {                                                             /* оккупация 1942 */
        play_until(1942); resolve(&G);
        G.gold += 3000; G.ind += 2000;
        int best = -1;
        for (int u = 0; u < UNIT_COUNT; u++) if (war_unit_state(&G, u) == US_AVAILABLE) best = u;
        for (int i = 0; i < 12 && best >= 0; i++) war_recruit(&G, best);
        G.occupied &= ~(1u << 5); G.liberated |= 1u << 5;
        war_declare(&G, WAR_LIBERATION, 2);
        U.tab = TAB_ARMY;
        if (n == 26) U.modal = M_MAP;
        break;
    }
    case 27: play_until(1700); resolve(&G); U.modal = M_CITY; U.modalArg = C_SLUTSK; break;  /* профиль города */
    case 28: case 29: case 30: {                                                     /* 3D-карта: поворот, наклон, приближение */
        play_until(n == 30 ? 1986 : 1700); resolve(&G);
        t3d_cam_reset(&U.cam); U.camInit = 1;
        if (n == 28) { U.cam.yaw = 35; U.cam.pitch = 38; }
        if (n == 29) { U.cam.yaw = 330; U.cam.pitch = 30; U.cam.zoom = 3.2; t3d_lonlat(27.56, 53.90, &U.cam.tx, &U.cam.ty); }
        if (n == 30) { U.cam.yaw = 200; U.cam.pitch = 22; U.cam.zoom = 1.25; }
        if (n != 28) U.modal = M_MAP;
        break;
    }
    }
    if (U.screen == SCR_GAME) {
        U.eraFilter = G.era;
        if (U.modal == M_NONE) ui_after_change();
    }
}

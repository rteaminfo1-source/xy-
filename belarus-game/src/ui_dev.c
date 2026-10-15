/* Режим разработчика: BelarusTech.exe --scene=N открывает заданное состояние (для скриншотов и проверки). */
#include "ui.h"
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
    case 6: play_until(2024); resolve(&G); U.tab = TAB_CHARTS; break;
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
    }
    if (U.screen == SCR_GAME) {
        U.eraFilter = G.era;
        if (U.modal == M_NONE) ui_after_change();
    }
}

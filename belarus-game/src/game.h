/*
 * Судьба Республики Беларусь в объективе технологий (1100–2026)
 * Общие типы, данные и API игрового движка.
 * Движок (engine.c, data_*.c) не зависит от Windows и собирается любым C99-компилятором.
 */
#ifndef BY_GAME_H
#define BY_GAME_H

#include <wchar.h>
#include <stdio.h>

/* ---------- Ресурсы и эффекты ---------- */
enum {
    R_GOLD,   /* казна: прибавка за ход                         */
    R_SCI,    /* знания: прибавка за ход                        */
    R_IND,    /* производство: прибавка за ход                  */
    R_DEF,    /* оборона: рейтинг (не копится)                  */
    R_STAB,   /* стабильность: сдвиг целевого уровня            */
    R_CULT,   /* культура и идентичность: прибавка за ход       */
    R_ECO,    /* экология: изменение за ход                     */
    R_GROW,   /* рост населения, сотые доли процента в год      */
    R_COUNT
};

typedef struct { int v[R_COUNT]; } Fx;

#define FX(...) {{ __VA_ARGS__ }}
#ifdef BY_DATA   /* короткие макросы только для файлов данных */
#define G(x) [R_GOLD] = (x)
#define S(x) [R_SCI]  = (x)
#define I(x) [R_IND]  = (x)
#define D(x) [R_DEF]  = (x)
#define T(x) [R_STAB] = (x)
#define C(x) [R_CULT] = (x)
#define E(x) [R_ECO]  = (x)
#define P(x) [R_GROW] = (x)
#endif

/* ---------- Эпохи ---------- */
#define ERA_COUNT 8
typedef struct {
    const wchar_t *name;
    const wchar_t *polity;    /* подпись на карте */
    int from, to;
    const wchar_t *intro;
    int base_grow;            /* базовый рост населения, сотые % в год */
} Era;

/* ---------- Отрасли технологий ---------- */
enum { BR_CRAFT, BR_KNOW, BR_BUILD, BR_AGRO, BR_WAR, BR_TRANS, BR_ENERGY, BR_MED, BR_DIGI, BR_COUNT };

typedef struct {
    const char    *id;
    const wchar_t *name;
    int            era;       /* 1..8 */
    int            branch;
    int            cost;      /* в очках знаний */
    const char    *req[3];
    int            year;      /* историческая дата */
    Fx             fx;
    const wchar_t *desc;
    int            reqi[3];   /* заполняется data_init() */
} Tech;

/* ---------- Города и карта ---------- */
typedef struct { const wchar_t *name; double lon, lat; int major; int lx, ly; } City;
typedef struct { double lon, lat; } GeoPt;
typedef struct { const wchar_t *name; const GeoPt *pts; int n; } River;

/* ---------- Стройки ---------- */
typedef struct {
    const char    *id;
    const wchar_t *name;
    int            city;
    int            era;
    const char    *tech;
    int            gold, ind;
    int            year;      /* историческая дата */
    int            until;     /* 0 — без срока; иначе после этого года стройка упущена */
    Fx             fx;
    const wchar_t *desc;
    int            techi;
} Project;

/* ---------- Деятели ---------- */
typedef struct {
    const char    *id;
    const wchar_t *name;
    const wchar_t *life;
    const wchar_t *role;
    int            from, to;  /* окно, когда деятеля можно пригласить */
    int            cost;      /* в казне */
    Fx             fx;
    const char    *discount;  /* технология, которая дешевеет на 35% */
    const wchar_t *desc;
    int            disci;
} Person;

/* ---------- События ---------- */
enum { K_POLIT, K_WAR, K_DISASTER, K_CULTURE, K_SCIENCE, K_ECON, K_TECH, K_SPACE, K_COUNT };

typedef struct {
    const wchar_t *label;
    int dGold, dSci, dInd, dStab, dCult, dEco;  /* мгновенные изменения */
    int popPct;          /* мгновенное изменение населения, % */
    int war;             /* тяжесть удара по населению, %, смягчается обороной */
    int indLoss;         /* потеря запасов производства, %, смягчается обороной */
    Fx  mod;             /* модификатор за ход */
    int modTurns;        /* 0 — навсегда */
    const wchar_t *modName;
    const char *reqTech; /* нужна технология */
    const char *grant;   /* даёт технологию */
    int setFlag;
    const wchar_t *result;
    int reqi, granti;
} Choice;

#define MAX_CHOICES 3
typedef struct {
    const char    *id;
    int            year;      /* исторические: год; случайные: 0 */
    int            from, to;  /* случайные: диапазон лет */
    int            kind;
    const wchar_t *title;
    const wchar_t *text;
    Choice         ch[MAX_CHOICES];
    int            nch;       /* заполняется data_init() */
} Event;

/* ---------- Викторина и достижения ---------- */
typedef struct { int era; const wchar_t *q; const wchar_t *a[4]; int correct; const wchar_t *expl; } Quiz;
typedef struct { const wchar_t *name; const wchar_t *desc; } Achievement;

/* ---------- Задания ----------
 * Задание открывается, когда игра доходит до эпохи era, и выполняется, если условие
 * соблюдено в ход с годом <= deadline. Если ход перешёл за deadline, а условие не выполнено,
 * задание провалено. Задания «на срок» (тип QT_ECO_AT или at = 1) проверяются один раз —
 * по итогам года deadline (когда игра переходит за него или летопись заканчивается).
 * Награда начисляется сразу при выполнении: казна, знания, культура, стабильность. */
enum {
    QT_TECH,     /* изучить технологию target                          */
    QT_PROJ,     /* построить стройку target                           */
    QT_PERSON,   /* пригласить деятеля target                          */
    QT_POP,      /* население >= num тыс.                              */
    QT_DEF,      /* оборона (game_def) >= num                          */
    QT_TECHN,    /* изучено технологий >= num                          */
    QT_CULT,     /* накопленная культура >= num                        */
    QT_ECO_AT,   /* экология >= num по итогам года deadline            */
    QT_STAB,     /* стабильность >= num                                */
    QT_COUNT
};
enum { QS_HIDDEN, QS_ACTIVE, QS_DONE, QS_FAILED };   /* Game.questState[] */

typedef struct {
    const char    *id;
    int            era;       /* 1..8: эпоха, с которой задание открыто */
    int            type;      /* QT_* */
    const char    *target;    /* id технологии, стройки или деятеля (QT_TECH/QT_PROJ/QT_PERSON), иначе 0 */
    int            num;       /* порог для численных типов */
    int            deadline;  /* год-срок */
    int            at;        /* 1 — проверка только по итогам года-срока (у QT_ECO_AT всегда) */
    int            city;      /* город для значка на карте (C_*), -1 — без значка */
    int            gold, sci, cult, stab;   /* награда */
    const wchar_t *title;
    const wchar_t *desc;      /* историческая справка, 1–2 предложения */
    int            ti;        /* индекс цели в TECHS/PROJECTS/PEOPLE, заполняется data_init(); -1 — нет */
} Quest;

/* ---------- Курс (приоритет) ---------- */
typedef struct { const wchar_t *name; const wchar_t *desc; int goldPct, sciPct, indPct, defPct, stabAdd, ecoAdd, growAdd; } Priority;
typedef struct { const wchar_t *name; const wchar_t *desc; int incomePct; int harmPct; int costPct; } Difficulty;

/* ---------- Таблицы данных ---------- */
extern const Era        ERAS[ERA_COUNT];
extern const int        TURN_YEARS[];
extern const int        TURN_COUNT;
extern const int        END_YEAR;
extern const wchar_t   *BRANCH_NAMES[BR_COUNT];
extern const wchar_t   *KIND_NAMES[K_COUNT];
extern const City       CITIES[];
extern const int        CITY_COUNT;
extern const GeoPt      BORDER[];
extern const int        BORDER_N;
extern const River      RIVERS[];
extern const int        RIVER_COUNT;
extern const Priority   PRIORITIES[];
extern const int        PRIORITY_COUNT;
extern const Difficulty DIFFICULTIES[];
extern const int        DIFFICULTY_COUNT;

extern Tech        TECHS[];
extern const int   TECH_COUNT;
extern Project     PROJECTS[];
extern const int   PROJECT_COUNT;
extern Person      PEOPLE[];
extern const int   PEOPLE_COUNT;
extern Event       HIST_EVENTS[];
extern const int   HIST_COUNT;
extern Event       RAND_EVENTS[];
extern const int   RAND_COUNT;
extern const Quiz  QUIZ[];
extern const int   QUIZ_COUNT;
extern const Achievement ACHIEVEMENTS[];
extern const int   ACH_COUNT;
extern Quest       QUESTS[];
extern const int   QUEST_COUNT;

/* Индексы городов */
enum {
    C_MINSK, C_POLOTSK, C_VITEBSK, C_MOGILEV, C_GOMEL, C_GRODNO, C_BREST, C_TUROV,
    C_NOVOGRUDOK, C_PINSK, C_NESVIZH, C_MIR, C_ORSHA, C_SLUTSK, C_NOVOPOLOTSK,
    C_ZHODINO, C_SOLIGORSK, C_OSTROVETS, C_MOZYR, C_BOBRUISK, C_LIDA, C_KAMENETS,
    C_GORKI, C_BORISOV, C_SMOLEVICHI, C_URECHYE, C_NALIBOKI, C_MSTISLAVL, C_RECHITSA,
    C_KREVO, C_COUNT_
};

/* ---------- Состояние игры ---------- */
#define MAX_TECH    160
#define MAX_PROJ    80
#define MAX_PEOPLE  60
#define MAX_HIST    120
#define MAX_RAND    60
#define MAX_QUIZ    160
#define MAX_ACH     32
#define MAX_QUESTS  48
#define MAX_TURNS   80
#define MAX_LOG     360
#define MAX_MODS    24
#define LOG_LEN     200
#define MOD_NAME    48

enum { Q_HIST, Q_RAND, Q_ERA, Q_FINAL };
typedef struct { int type, idx; } QItem;

enum { LOG_INFO, LOG_TECH, LOG_BUILD, LOG_PERSON, LOG_EVENT, LOG_WAR, LOG_ERA, LOG_QUIZ, LOG_ACH, LOG_QUEST };
typedef struct { int year, kind; wchar_t text[LOG_LEN]; } LogEntry;

typedef struct { Fx fx; int until; wchar_t name[MOD_NAME]; } Mod;

typedef struct {
    int    version;
    int    difficulty;
    int    turn;            /* индекс в TURN_YEARS */
    int    year;
    int    era;             /* 1..8 */
    int    priority;
    double pop;             /* тыс. человек */
    double gold, sci, ind;  /* запасы */
    double stab, eco, cult; /* stab, eco: 0..100; cult копится */
    unsigned char tech[MAX_TECH];
    unsigned char proj[MAX_PROJ];
    unsigned char person[MAX_PEOPLE];
    unsigned char histDone[MAX_HIST];
    unsigned char randSeen[MAX_RAND];
    unsigned char quizDone[MAX_QUIZ];   /* 0 нет, 1 верно, 2 неверно */
    unsigned char ach[MAX_ACH];
    int    techTurn[MAX_TECH];
    unsigned flags;
    Mod    mods[MAX_MODS];
    int    modN;
    /* очередь событий текущего хода */
    QItem  queue[24];
    int    qn, qpos;
    /* викторина */
    int    quizCur;         /* -1 нет вопроса */
    int    quizAnswered;    /* в этом ходу */
    int    quizLastPick;
    int    quizRight, quizWrong;
    int    quizPerm[4];     /* порядок вариантов на экране */
    int    quizLastAns;     /* выбранный вариант (экранный), -1 нет */
    unsigned erasBuilt;     /* биты эпох, в которых что-то построено */
    int    aheadOfTime;
    /* история для графиков */
    int    histN;
    float  hYear[MAX_TURNS], hPop[MAX_TURNS], hGold[MAX_TURNS], hSci[MAX_TURNS],
           hInd[MAX_TURNS], hStab[MAX_TURNS], hEco[MAX_TURNS], hTech[MAX_TURNS], hCult[MAX_TURNS];
    /* летопись */
    LogEntry logs[MAX_LOG];
    int    logN;
    /* статистика */
    int    wars;
    double popLostWar;
    double popPeak;
    double pop1945;
    unsigned rng;
    int    over;
    int    lastRand;
    /* ---- версия 4: задания (поля только дописываются в конец) ---- */
    unsigned char questState[MAX_QUESTS];   /* QS_* */
    int    questsDone;
} Game;

/* Флаги исторических решений */
enum {
    F_TRADE_RIGA   = 1u << 0,
    F_SKARYNA      = 1u << 1,
    F_TOLERANCE    = 1u << 2,
    F_EVACUATION   = 1u << 3,
    F_PARTISANS    = 1u << 4,
    F_BELARUSIZE   = 1u << 5,
    F_OPEN_IT      = 1u << 6,
    F_CHERNOBYL_EV = 1u << 7,
    F_GRADUAL      = 1u << 8,
    F_GREEN        = 1u << 9,
    F_HOUSING      = 1u << 10,
    F_NUCLEAR_OK   = 1u << 11
};

/* Разбивка дохода за ход */
typedef struct {
    double base[R_COUNT], techs[R_COUNT], projs[R_COUNT], people[R_COUNT], mods[R_COUNT], popb[R_COUNT];
    double total[R_COUNT];  /* после множителей */
    double mult[R_COUNT];
    double growYear;        /* итоговый рост, доля в год */
    double stabTarget;
} Income;

typedef struct {
    wchar_t text[600];      /* что произошло, по-человечески */
    double  popBefore, popAfter;
} Outcome;

typedef struct {
    int techs, projs, people, pop, stab, eco, cult, ach, quiz, quests, total;  /* quests: 15 за задание */
    const wchar_t *rank;
    wchar_t epilogue[2400];
} Score;

/* ---------- API движка ---------- */
void data_init(void);
int  tech_index(const char *id);

void game_new(Game *g, int difficulty, unsigned seed);
void game_income(const Game *g, Income *inc);
int  game_def(const Game *g);

enum { TS_FUTURE, TS_LOCKED, TS_AVAILABLE, TS_DONE };
int  game_tech_state(const Game *g, int t);
int  game_tech_cost(const Game *g, int t);
int  game_research(Game *g, int t);
int  game_tech_count(const Game *g);

enum { PS_FUTURE, PS_LOCKED, PS_AVAILABLE, PS_DONE, PS_MISSED };
int  game_proj_state(const Game *g, int p);
int  game_proj_gold(const Game *g, int p);
int  game_proj_ind(const Game *g, int p);
int  game_build(Game *g, int p);
int  game_sell_amount(const Game *g);
int  game_sell_ind(Game *g);              /* продать излишки производства: вернёт прибавку казны */

/* Минутный доход: каждую реальную минуту партии — казна, знания и производство (растут с эпохой) */
typedef struct { int gold, sci, ind; } MinuteBonus;
void game_minute_preview(const Game *g, MinuteBonus *b);
int  game_minute_bonus(Game *g, MinuteBonus *b);   /* 1 — начислено; после финала не начисляется */
int  game_proj_count(const Game *g);

enum { HS_FUTURE, HS_AVAILABLE, HS_ACTIVE, HS_LEGACY, HS_MISSED };
int  game_person_state(const Game *g, int i);
int  game_person_cost(const Game *g, int i);
int  game_hire(Game *g, int i);
int  game_people_count(const Game *g);

const Event *game_event_ptr(const QItem *q);
int  game_pending(const Game *g, QItem *out);   /* 1 если есть событие */
int  game_choice_ok(const Game *g, const Event *e, int ch);
void game_choose(Game *g, int ch, Outcome *o);  /* для текущего события очереди */
void game_ack(Game *g);                         /* закрыть событие без выбора (эра/финал) */
void game_end_turn(Game *g);
int  game_next_year(const Game *g);

int  game_quiz_pick(Game *g);
int  game_quiz_answer(Game *g, int ans);        /* 1 верно */
int  game_quiz_reward(const Game *g);

void game_score(const Game *g, Score *s);
void game_check_achievements(Game *g);
void game_log(Game *g, int kind, const wchar_t *fmt, ...);
void fx_describe(const Fx *fx, wchar_t *buf, int n);
void choice_describe(const Game *g, int isRandom, const Choice *c, wchar_t *buf, int n);
double era_scale(int era);
int  era_of_year(int year);
void fmt_pop(double popK, wchar_t *buf, int n);
int  game_has_tech(const Game *g, const char *id);
int  game_has_proj(const Game *g, const char *id);

/* Задания. game_check_quests открывает задания наступившей эпохи, засчитывает выполненные
 * (награда + запись LOG_QUEST в летописи) и проваливает просроченные. Движок сам вызывает её
 * из game_new и из game_check_achievements (то есть после исследования, стройки, найма,
 * выбора в событии, викторины и в конце хода); UI вызывать её не обязан. */
void game_check_quests(Game *g);
int  game_quest_state(const Game *g, int q);                        /* QS_*; вне диапазона — QS_HIDDEN */
void game_quest_progress(const Game *g, int q, double *cur, double *target);  /* для полосы прогресса; 0/1 из 1 для целей-объектов */
int  game_quest_count(const Game *g, int state);                    /* сколько заданий в состоянии QS_* */
void quest_describe_goal(int q, wchar_t *buf, int n);               /* «Изучить «Скрипторий» до 1175 г.» */
void quest_describe_reward(int q, wchar_t *buf, int n);             /* «казна +22 · знания +8 · культура +8» */

int  game_save(const Game *g, FILE *f);
int  game_load(Game *g, FILE *f);   /* принимает и сохранения версии 3 (до заданий) */

#define GAME_VERSION 4

#endif

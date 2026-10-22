/* Армия и война (в духе стратегий вроде HOI4), провинции, оккупация 1941–1944, специализации городов. */
#ifndef BY_WAR_H
#define BY_WAR_H

#include "game.h"

/* ---------- войска ---------- */
typedef struct {
    const char    *id;
    const wchar_t *name;
    int            era;       /* эпоха появления */
    const char    *tech;      /* нужна технология (или NULL) */
    int            atk, def;  /* сила в наступлении и обороне за одно соединение */
    int            gold, ind; /* стоимость набора */
    int            upkeep;    /* содержание: десятые доли казны за ход */
    const wchar_t *desc;
    int            techi;
} UnitType;

extern UnitType UNIT_TYPES[];
extern const int UNIT_COUNT;

/* ---------- земли для завоевания ---------- */
typedef struct {
    const char    *id;
    const char    *group;     /* одна и та же земля в разные эпохи */
    const wchar_t *name;
    const wchar_t *owner;     /* кто владеет землёй в окне завоевания */
    double         lon, lat, radiusKm;
    int            from, to;  /* годы, когда поход возможен */
    int            strength;  /* сила обороны противника */
    int            popK;      /* население, которое присоединяется (тыс.) */
    Fx             fx;        /* доход за ход (масштабируется вместе с эпохой) */
    int            era;       /* эпоха земли — для масштаба дохода */
    const wchar_t *desc;
} Province;

extern const Province PROVINCES[];
extern const int PROVINCE_COUNT;

/* ---------- оккупация: шесть областей ---------- */
#define REGION_COUNT 6
extern const int REGION_CITY[REGION_COUNT];          /* C_* центров областей */
extern const wchar_t *REGION_NAMES[REGION_COUNT];

/* ---------- города ---------- */
enum { SP_CAPITAL, SP_FORTRESS, SP_TRADE, SP_SCIENCE, SP_CULTURE, SP_INDUSTRY, SP_RESOURCE, SP_ENERGY, SP_FARM, SP_RIVER, SP_FOREST, SP_CRAFT, SP_COUNT };
typedef struct { int spec; const wchar_t *desc; } CityProfile;
extern const CityProfile CITY_PROFILES[];            /* в порядке C_* */
extern const wchar_t *SPEC_NAMES[SP_COUNT];
extern const Fx SPEC_FX[SP_COUNT];                    /* бонус за уровень города */

/* ---------- состояние войны ---------- */
enum { WAR_NONE, WAR_CONQUEST, WAR_LIBERATION };

typedef struct {
    double attack, defense;   /* суммарная сила (с учётом технологий) */
    int    units, upkeep;    /* upkeep — в десятых долях казны */
} Army;

void war_army(const Game *g, Army *a);
double war_tech_mult(const Game *g);                  /* ×1.0 … ×2.0 от военных технологий */
double war_unit_eff(const Game *g, int u);            /* устаревание: 1.0 для войск своей и прошлой эпохи */
int  war_army_size(const Game *g);
int  war_army_cap(const Game *g);                     /* предел численности: растёт с населением */
double war_attack_power(const Game *g, int kind);     /* сила самой сильной армии (партизаны сильнее при освобождении) */
double war_defense_power(const Game *g);              /* всё войско вместе + крепости */
double war_defense_at(const Game *g, int node);       /* оборона узла фронта: армии в нём и рядом + крепости */
int  war_raid_cities(const Game *g, int *out);        /* города, на которые в эту эпоху идут набеги (до 4) */
void war_rail_income(const Game *g, double *gold, double *ind);   /* доход одного перегона железной дороги */
double war_raid_chance(const Game *g);                /* вероятность набега в конце хода */
double war_raid_strength(const Game *g);              /* типичная сила набега */
double war_enemy_power(const Game *g, int kind, int target);
int  war_eta(const Game *g, int kind, int target);    /* ходов до победы с маршем армии к цели; 99 — не победить */
int  war_battles_left(const Game *g, int kind, int target); /* сколько боёв осталось до закрытия окна */
int  capital_of_era(int era);                         /* C_* или -1 */

enum { US_LOCKED, US_AVAILABLE, US_FUTURE };
int  war_unit_state(const Game *g, int u);
int  war_unit_cost_gold(const Game *g, int u);
int  war_unit_cost_ind(const Game *g, int u);
int  war_recruit(Game *g, int u);
int  war_disband(Game *g, int u);

enum { PV_FUTURE, PV_AVAILABLE, PV_OWNED, PV_LEGACY, PV_CLOSED };   /* PV_LEGACY больше не бывает: земли свои навсегда */
int  war_province_state(const Game *g, int p);
int  war_group_owner(const Game *g, int p);             /* своя земля той же группы (та же земля в другую эпоху) или -1 */
int  war_province_strength(const Game *g, int p);     /* с учётом года */
int  war_region_strength(const Game *g);             /* сила оккупантов в текущем году */

/* Причина, по которой нельзя начать войну: NULL — можно. */
const wchar_t *war_cannot_declare(const Game *g, int kind, int target);
int  war_declare(Game *g, int kind, int target);
void war_peace(Game *g);

/* Ход войны: вызывается из game_end_turn до смены года. Сначала марш армий (front_end_turn), затем бой —
 * если армия стоит на узле цели, — или набег, если войны нет. */
void war_end_turn(Game *g);
int  war_front_battle(Game *g);         /* бой в реальном времени (раз в 10 с, пока армия у цели); 1 — был */
/* Начало и конец оккупации — при смене года. */
void war_year_changed(Game *g, int oldYear);

int  war_occupied_count(const Game *g);
double war_occupation_mult(const Game *g);            /* множитель доходов */
double war_occupation_harm(const Game *g);            /* множитель потерь в событиях эпохи VI */

/* Города */
int  city_level(const Game *g, int c);
void city_bonus(const Game *g, int c, double out[R_COUNT]);
void cities_income(const Game *g, double out[R_COUNT]);
void provinces_income(const Game *g, double out[R_COUNT]);
int  war_province_count(const Game *g);

void war_data_init(void);

#endif

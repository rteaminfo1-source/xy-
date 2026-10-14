/* Какая 3D-модель (спрайт из Blender) изображает каждую стройку на карте.
 * Индексы — порядок спрайтов в assets/sprites.json (см. tools/blender/build_sprites.py). */
#include "ui.h"
#include <string.h>

enum {
    LM_FORT, LM_CHURCH, LM_STONE, LM_PIER, LM_WORKSHOP, LM_MILL, LM_TOWER, LM_CASTLE, LM_PRESS, LM_TOWNHALL,
    LM_COLLEGE, LM_MANUFACTORY, LM_CANAL, LM_RAILWAY, LM_POWERPLANT, LM_TRAM, LM_LAB, LM_RADIO, LM_FACTORY,
    LM_DUMPTRUCK, LM_REFINERY, LM_MINE, LM_CAMP, LM_METRO, LM_TECHPARK, LM_LIBRARY, LM_DISH, LM_NPP, LM_SOLAR
};

static const struct { const char *proj; int sprite; } LANDMARKS[] = {
    { "minsk_castle", LM_FORT },          { "polotsk_library", LM_CHURCH },     { "turov_detinets", LM_FORT },
    { "dvina_portage", LM_PIER },         { "spas_church", LM_CHURCH },         { "borisov_stones", LM_STONE },
    { "kolozha", LM_CHURCH },             { "novogrudok_craft", LM_WORKSHOP },  { "kamenets_tower", LM_TOWER },
    { "novogrudok_castle", LM_CASTLE },   { "lida_castle", LM_CASTLE },         { "krevo_castle", LM_CASTLE },
    { "neman_mills", LM_MILL },           { "mir_castle", LM_CASTLE },          { "nesvizh_press", LM_PRESS },
    { "brest_press", LM_PRESS },          { "minsk_ratusha", LM_TOWNHALL },     { "polotsk_college", LM_COLLEGE },
    { "nesvizh_castle", LM_CASTLE },      { "slutsk_fortress", LM_CASTLE },     { "mogilev_ratusha", LM_TOWNHALL },
    { "naliboki_glass", LM_MANUFACTORY }, { "urechye", LM_MANUFACTORY },        { "slutsk_persiyarnya", LM_MANUFACTORY },
    { "grodno_manufactories", LM_MANUFACTORY }, { "grodno_medschool", LM_COLLEGE }, { "oginski_canal", LM_CANAL },
    { "dnieper_bug_canal", LM_CANAL },    { "brest_fortress", LM_CASTLE },      { "gorki_institute", LM_COLLEGE },
    { "moscow_brest_rw", LM_RAILWAY },    { "libava_romny_rw", LM_RAILWAY },    { "neman_glass", LM_MANUFACTORY },
    { "borisov_matches", LM_MANUFACTORY },{ "nadneman_lab", LM_LAB },           { "minsk_power", LM_POWERPLANT },
    { "vitebsk_tram", LM_TRAM },          { "bsu", LM_COLLEGE },                { "minsk_radio", LM_RADIO },
    { "academy_building", LM_COLLEGE },   { "belgres", LM_POWERPLANT },         { "gomselmash", LM_FACTORY },
    { "partisan_base", LM_CAMP },         { "forest_hospital", LM_CAMP },       { "maz", LM_FACTORY },
    { "mtz", LM_FACTORY },                { "horizont", LM_FACTORY },           { "belomo", LM_FACTORY },
    { "computer_plant", LM_FACTORY },     { "belaz", LM_DUMPTRUCK },            { "integral", LM_FACTORY },
    { "naftan", LM_REFINERY },            { "belaruskali", LM_MINE },           { "grodno_azot", LM_REFINERY },
    { "sosny", LM_LAB },                  { "lukoml", LM_POWERPLANT },          { "belshina", LM_FACTORY },
    { "mozyr_refinery", LM_REFINERY },    { "minsk_metro", LM_METRO },          { "htp_minsk", LM_TECHPARK },
    { "national_library", LM_LIBRARY },   { "minsk_gamedev", LM_TECHPARK },     { "great_stone", LM_FACTORY },
    { "space_center", LM_DISH },          { "rechitsa_solar", LM_SOLAR },       { "belgee", LM_FACTORY },
    { "electrobus_lines", LM_TRAM },      { "data_center", LM_TECHPARK },       { "belnpp", LM_NPP },
};

/* Спрайт стройки p или -1, если для неё модели нет. */
int landmark_sprite(int p)
{
    static int cache[MAX_PROJ];
    static int ready;
    if (!ready) {
        for (int i = 0; i < PROJECT_COUNT; i++) {
            cache[i] = -1;
            for (size_t k = 0; k < sizeof LANDMARKS / sizeof LANDMARKS[0]; k++)
                if (strcmp(LANDMARKS[k].proj, PROJECTS[i].id) == 0) { cache[i] = LANDMARKS[k].sprite; break; }
        }
        ready = 1;
    }
    return p >= 0 && p < PROJECT_COUNT ? cache[p] : -1;
}

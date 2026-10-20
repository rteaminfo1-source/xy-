/* Армии на карте и снабжение (заглушка контракта — реализуется отдельно). */
#include "front.h"

void front_init(void) {}

void front_new_game(Game *g)
{
    for (int a = 0; a < MAX_ARMIES; a++) g->armies[a].node = g->armies[a].dest = -1;
    g->frontReady = 0;
}

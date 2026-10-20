/* front.h — армии на карте в духе HOI: игрок сам двигает войска по узлам фронта, строит железные
 * дороги снабжения, по ним ходят поезда. Движок переносимый (без windows.h); интерфейс — ui_front.c.
 * (Заглушка контракта: модуль наполняется отдельно.) */
#ifndef BY_FRONT_H
#define BY_FRONT_H

#include "game.h"

void front_init(void);              /* из data_init(): статические таблицы */
void front_new_game(Game *g);       /* из game_new(): армий на карте ещё нет */

#endif

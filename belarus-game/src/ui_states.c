/* Исторические территории на карте — интерфейс (заглушка контракта). */
#include "mapview.h"
#include "states.h"

unsigned states_prepare_3d(void) { return 0; }
void states_overlay(const MapProj *pj, int interactive, int large) { (void)pj; (void)interactive; (void)large; }
void states_dev_scene(int n) { (void)n; }

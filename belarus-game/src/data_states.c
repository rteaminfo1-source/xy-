/* Исторические территории для 3D-карты (временная заглушка — заполняется отдельно). */
#include "states.h"

const HistArea HIST_AREAS[] = { { L"", HA_STATE, 0, -1, 0, 0, 0, 0, {{0, 0}} } };
const int HIST_AREA_COUNT = 0;

const wchar_t *hist_state_name(int year) { (void)year; return L""; }

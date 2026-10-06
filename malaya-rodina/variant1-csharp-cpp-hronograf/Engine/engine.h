// =====================================================================
//  ХРОНОГРАФ МАЛОЙ РОДИНЫ — нативный движок (C++17)
//  Судьба малой родины в объективе технологий
//
//  Движок делает «тяжёлую» работу:
//   1) процедурно генерирует уникальную карту села по его названию;
//   2) моделирует 160 лет жизни (1900–2060): демографию (когортная
//      модель), диффузию технологий (S-кривые Басса), застройку,
//      поля, дороги, общественные здания;
//   3) в реальном времени обрабатывает кадр «объективом эпохи»:
//      ночное освещение, плёнка, сепия, зерно, виньетка, VHS, AR.
//
//  Интерфейс — чистый C ABI, его вызывает C# через P/Invoke.
// =====================================================================
#pragma once
#include <stdint.h>

#if defined(_WIN32)
#  define HG_API extern "C" __declspec(dllexport)
#else
#  define HG_API extern "C" __attribute__((visibility("default")))
#endif

#define HG_TECH_COUNT 13

// Показатели села за один год. Раскладка совпадает с C#-структурой.
typedef struct HgStats {
    int32_t year;
    int32_t population;
    int32_t houses;      // жилые дома
    int32_t abandoned;   // брошенные дома и руины
    int32_t young;       // 0–29 лет
    int32_t middle;      // 30–59 лет
    int32_t old;         // 60+ лет
    int32_t sites;       // всего участков под дома
    double  quality;     // индекс качества жизни, 0..100
    double  connectivity;// связь, 0..100
    double  economy;     // экономика и рабочие места, 0..100
    double  memory;      // сохранение памяти и наследия, 0..100
    double  ecology;     // экология, 0..100
    double  services;    // школа, ФАП, магазин, клуб, сервисы, 0..100
    double  tech[HG_TECH_COUNT]; // уровень проникновения технологий 0..1
} HgStats;

// Здание на карте (координаты в клетках).
typedef struct HgBuilding {
    int32_t x, y, w, h;
    int32_t type;    // HG_B_*
    int32_t flags;   // HG_F_*
    int32_t id;
    int32_t variant;
} HgBuilding;

// Событие летописи. scenario = -1 — общая история (до «сегодня»).
typedef struct HgEvent {
    int32_t year, code, value, scenario;
} HgEvent;

// Источник света для ночного режима (в пикселях кадра).
typedef struct HgLight {
    float x, y, radius;
    float r, g, b;       // 0..1
    float intensity;
    int32_t blink;       // 0 — постоянный, 1 — мигает
} HgLight;

// Параметры «объектива» для кадра.
typedef struct HgLens {
    float   year;   // год (можно дробный — для плавных переходов)
    float   night;  // 0 — день, 1 — ночь
    float   time;   // секунды с запуска (анимация зерна, мигания)
    int32_t frame;  // номер кадра
} HgLens;

HG_API int32_t hg_version(void);
// what: 0 — ширина карты, 1 — высота, 2 — первый год, 3 — последний,
//       4 — «сегодня», 5 — число технологий
HG_API int32_t hg_info(int32_t what);
HG_API void*   hg_create(const char* nameUtf8, int32_t scenario);
HG_API void    hg_destroy(void* engine);
HG_API int32_t hg_shade(void* engine, uint8_t* out);                       // W*H рельеф
HG_API int32_t hg_terrain(void* engine, int32_t year, uint8_t* out);       // W*H
HG_API int32_t hg_buildings(void* engine, int32_t year, HgBuilding* out, int32_t max);
HG_API int32_t hg_stats(void* engine, int32_t year, HgStats* out);
HG_API int32_t hg_events(void* engine, HgEvent* out, int32_t max);
// Обработка кадра BGRA на месте. Возвращает время обработки в мс.
HG_API double  hg_lens(uint8_t* bgra, int32_t w, int32_t h, int32_t stride,
                       const HgLight* lights, int32_t count, const HgLens* p);

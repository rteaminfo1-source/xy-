/* Проверка сборки без Windows на Linux: игра рисует сцену --scene=N в буфер, кадр сохраняется в PPM.
 *   port/linux/run.sh SCENE OUT.ppm [W H SCALE] — собирает и запускает (нужны gcc и FreeType). */
#include "ui.h"
#include "plat_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *s_res = "res", *s_data = "build/linux-data";
void host_invalidate(void) {}
void host_set_timer(int id, int ms) { (void)id; (void)ms; }
void host_kill_timer(int id) { (void)id; }
const char *host_data_dir(void) { return s_data; }
const char *host_res_dir(void) { return s_res; }
void host_quit(void) {}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "host SCENE OUT.ppm [W H SCALE [TAPX TAPY]]\n"); return 1; }
    int w = argc > 4 ? atoi(argv[3]) : 1366, h = argc > 4 ? atoi(argv[4]) : 768;
    float scale = argc > 5 ? (float)atof(argv[5]) : 1.0f;
    mkdir("build", 0755);
    mkdir(s_data, 0755);
    wchar_t cmd[64];
    swprintf(cmd, 64, L"--scene=%d", atoi(argv[1]));
    ui_ev_start(scale, cmd);
    ui_ev_size(w, h);
    for (int i = 0; i < 3; i++) ui_ev_paint();
    if (argc > 7) {                                  /* касание: нажать и отпустить, перерисовать */
        int x = atoi(argv[6]), y = atoi(argv[7]);
        ui_ev_move(x, y);
        ui_ev_down(x, y, 0);
        ui_ev_up(x, y, 0);
        for (int i = 0; i < 2; i++) ui_ev_paint();
    }
    FILE *f = fopen(argv[2], "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    const unsigned *px = d_bits();
    for (int i = 0; i < w * h; i++) {
        unsigned v = px[i];
        unsigned char rgb[3] = { (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}

/* Modelo em branco: um contador de toques. Edite, clique em Compilar e instale no relogio.
 * Funcoes do relogio (F3Api, via A->): rect, rrect, disc, text, ctext, num (desenho), ticks (ms), vibrate, fopen/fread/fwrite/fclose (so em /user/),
 * time (hora real, pode ser NULL) e battery (bateria, pode ser NULL). Tela: 256 x 402. O X de sair e desenhado pelo relogio no canto superior direito.
 * Sem biblioteca C: nao use printf, memcpy, malloc etc. Use A->malloc e as funcoes de f3game.h (itoa_pad, fill8, clampi...). */
#include "f3game.h"

typedef struct { F3App base; int count; } St;

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)pressed; (void)x; (void)y;
    if (tap) { s->count++; A->vibrate(1); }                     /* tap = 1 no quadro em que o dedo e solto */
    clear(C_BG);
    title(TR("Meu app", "My app"), -1, 0);
    A->num(FB, 128 - 4 * 8 * 6, 160, s->count, 6, RGB(255, 255, 255));
    A->ctext(FB, 128, 300, TR("toque para contar", "tap to count"), 1, C_MUTED);
    return 1;                                                   /* 0 fecha o app */
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; return &s->base;
}

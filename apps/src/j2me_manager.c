/* J2ME Manager: launcher e gerenciador de jogos Java ME para o Galaxy Fit3.
 * App nativo .f3a que roda direto no firmware (sem reflash).
 * Lista jogos em /user/j2me/index.txt, executa via player j2me.f3a,
 * integra teclado virtual para entrada de texto e tema Galaxy Watch.
 */
#include "f3app.h"
#include "f3game.h"

/* ---------- strings locais (sem libc) ---------- */
static int str_len(const char *s) { int n = 0; while (s && s[n]) n++; return n; }
static int str_cmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
static void str_cpy(char *d, const char *s, int max) { int n = 0; while (s && s[n] && n < max - 1) { d[n] = s[n]; n++; } d[n] = 0; }
static int str_to_int(const char *s) { int v = 0, neg = 0; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; } return neg ? -v : v; }
static void str_cat(char *d, const char *s, int max) { int n = str_len(d); while (*s && n < max - 1) { d[n++] = *s++; } d[n] = 0; }

#define MAX_GAMES 16
#define MAX_NAME  32
#define MAX_PATH  64
#define INDEX_FILE "/user/j2me/index.txt"
#define CURRENT_FILE "/user/j2me/current.txt"
#define SAVE_DIR   "/user/j2me/saves"

/* ---------- cores tema Galaxy Watch / firmware Samsung ---------- */
#define C_BG       RGB(0, 0, 0)
#define C_CARD     RGB(16, 18, 21)
#define C_CARD2    RGB(24, 27, 31)
#define C_LINE     RGB(40, 45, 52)
#define C_TEXT     RGB(255, 255, 255)
#define C_MUTED    RGB(142, 160, 168)
#define C_ACCENT   RGB(28, 78, 128)
#define C_ACCENT2  RGB(42, 111, 201)
#define C_GOOD     RGB(52, 199, 89)
#define C_BAD      RGB(255, 69, 58)
#define C_WARN     RGB(255, 210, 48)

#ifndef TR
#define TR(pt, en) (A && A->lang_pt ? (pt) : (en))
#endif

typedef struct {
    char name[MAX_NAME];
    char path[MAX_PATH];
    u32  size;
} Game;

typedef struct {
    F3App base;
    Game games[MAX_GAMES];
    int  ngames;
    int  sel;            /* índice selecionado na lista */
    int  scroll;         /* deslocamento da lista */
    int  page;           /* 0=list, 1=detalhes, 2=teclado, 3=msg */
    int  mode;           /* 0=normal, 1=digitando nome */
    char input[MAX_NAME];
    int  input_len;
    int  msg_ticks;      /* quando a mensagem some */
    char msg[96];
    u32  last_tick;
} St;

static const F3Api *A;

/* ---------- arquivo índice ---------- */
static int load_index(St *s) {
    int fd = A->fopen(INDEX_FILE, "r");
    if (fd < 0) return 0;
    char line[128];
    s->ngames = 0;
    while (s->ngames < MAX_GAMES) {
        int n = 0;
        int ch;
        /* ler linha simples byte a byte (fread de 1 byte) */
        while (n < (int)sizeof(line) - 1) {
            if (A->fread(fd, &ch, 1) != 1) break;
            if (ch == '\n' || ch == '\r') break;
            line[n++] = (char)ch;
        }
        line[n] = 0;
        if (n == 0) break;
        /* formato: nome|caminho|tamanho */
        char *pipe1 = strchr(line, '|');
        char *pipe2 = pipe1 ? strchr(pipe1 + 1, '|') : 0;
        if (!pipe1 || !pipe2) continue;
        *pipe1 = 0; *pipe2 = 0;
        Game *g = &s->games[s->ngames];
        str_cpy(g->name, line, MAX_NAME - 1); g->name[MAX_NAME - 1] = 0;
        str_cpy(g->path, pipe1 + 1, MAX_PATH - 1); g->path[MAX_PATH - 1] = 0;
        g->size = (u32)str_to_int(pipe2 + 1);
        s->ngames++;
    }
    A->fclose(fd);
    if (s->sel >= s->ngames) s->sel = s->ngames > 0 ? s->ngames - 1 : 0;
    if (s->scroll > s->sel) s->scroll = s->sel;
    return s->ngames;
}

static int save_index(St *s) {
    int fd = A->fopen(INDEX_FILE, "w");
    if (fd < 0) return -1;
    for (int i = 0; i < s->ngames; i++) {
        Game *g = &s->games[i];
        char line[128];
        int n = 0;
        char *p = g->name;
        while (*p && n < (int)sizeof(line) - 1) line[n++] = *p++;
        line[n++] = '|';
        p = g->path;
        while (*p && n < (int)sizeof(line) - 1) line[n++] = *p++;
        line[n++] = '|';
        char sz[16]; int m = 0;
        if (g->size == 0) sz[m++] = '0';
        else { u32 v = g->size; char tmp[16]; int t = 0; while (v) { tmp[t++] = '0' + v % 10; v /= 10; } while (t) sz[m++] = tmp[--t]; } sz[m] = 0;
        p = sz;
        while (*p && n < (int)sizeof(line) - 1) line[n++] = *p++;
        line[n++] = '\n';
        A->fwrite(fd, line, n);
    }
    A->fclose(fd);
    return 0;
}

static int add_game(St *s, const char *name, const char *path, u32 size) {
    if (s->ngames >= MAX_GAMES) return -1;
    Game *g = &s->games[s->ngames];
    str_cpy(g->name, name, MAX_NAME - 1); g->name[MAX_NAME - 1] = 0;
    str_cpy(g->path, path, MAX_PATH - 1); g->path[MAX_PATH - 1] = 0;
    g->size = size;
    s->ngames++;
    s->sel = s->ngames - 1;
    s->scroll = s->sel;
    return save_index(s);
}

/* ---------- utilitários de desenho ---------- */
static void clear_bg(void) {
    A->rect(A->fb, 0, 0, F3_W, F3_H, C_BG);
}

static void card(int x, int y, int w, int h, u16 color) {
    A->rrect(A->fb, x, y, w, h, 10, color);
}

static void title_bar(const char *t) {
    A->rect(A->fb, 0, 0, F3_W, 44, C_ACCENT);
    A->ctext(A->fb, F3_W / 2, 14, t, 2, C_TEXT);
    /* X fechar */
    A->text(A->fb, F3_W - 30, 12, "X", 2, C_TEXT);
}

static void show_msg(St *s, const char *msg, int ms) {
    str_cpy(s->msg, msg, sizeof(s->msg) - 1); s->msg[sizeof(s->msg) - 1] = 0;
    s->msg_ticks = (int)A->ticks() + ms;
    s->page = 3;
}

static void draw_list(St *s) {
    clear_bg();
    title_bar(TR("J2ME Manager", "J2ME Manager"));
    int y = 60;
    int visible = 6;
    int start = s->scroll;
    if (start < 0) start = 0;
    if (start + visible > s->ngames && s->ngames > visible) start = s->ngames - visible;
    if (start < 0) start = 0;
    s->scroll = start;

    for (int i = start; i < s->ngames && i < start + visible; i++) {
        Game *g = &s->games[i];
        int sel = (i == s->sel);
        int oy = y + (i - start) * 56;
        card(12, oy - 6, F3_W - 24, 48, sel ? C_ACCENT : C_CARD);
        A->ctext(A->fb, F3_W / 2, oy + 4, g->name, sel ? 2 : 1, sel ? C_TEXT : C_MUTED);
        char sz[32]; int n = 0;
        if (g->size >= 1024) { n = 0; u32 v = g->size / 1024; char tmp[16]; int t = 0; while (v) { tmp[t++] = '0' + v % 10; v /= 10; } while (t) sz[n++] = tmp[--t]; if (n == 0) sz[n++] = '0'; sz[n++] = ' '; sz[n++] = 'K'; sz[n++] = 'B'; } else { sz[n++] = '0'; sz[n++] = ' '; sz[n++] = 'B'; } sz[n] = 0;
        A->text(A->fb, 24, oy + 30, sz, 1, C_MUTED);
    }
    /* paginação / dicas */
    if (s->ngames == 0) {
        A->ctext(A->fb, F3_W / 2, F3_H / 2, TR("Nenhum jogo J2ME", "No J2ME games"), 1, C_MUTED);
        A->ctext(A->fb, F3_W / 2, F3_H / 2 + 20, TR("adicione via .jar", "add .jar first"), 1, C_MUTED);
    }
    A->ctext(A->fb, F3_W / 2, F3_H - 30, TR("toque: jogar  |  segurar: opções", "tap: play | long-press: options"), 1, C_MUTED);
}

static void draw_details(St *s) {
    clear_bg();
    title_bar(TR("Detalhes", "Details"));
    if (s->ngames == 0) { A->ctext(A->fb, F3_W / 2, F3_H / 2, TR("sem jogos", "no games"), 1, C_MUTED); return; }
    Game *g = &s->games[s->sel];
    card(12, 70, F3_W - 24, 120, C_CARD);
    A->ctext(A->fb, F3_W / 2, 90, g->name, 2, C_TEXT);
    A->ctext(A->fb, F3_W / 2, 120, g->path, 1, C_MUTED);
    char sz[48]; int n = 0;
    sz[n++] = 'T'; sz[n++] = 'a'; sz[n++] = 'm'; sz[n++] = 'a'; sz[n++] = 'n'; sz[n++] = 'o'; sz[n++] = ':';
    sz[n++] = ' ';
    if (g->size >= 1024) {
        u32 kb = g->size / 1024; char tmp[16]; int t = 0;
        while (kb) { tmp[t++] = '0' + kb % 10; kb /= 10; }
        while (t) sz[n++] = tmp[--t];
        if (n < (int)sizeof(sz) - 2) { sz[n++] = ' '; sz[n++] = 'K'; sz[n++] = 'B'; }
    } else {
        char tmp[16]; int t = 0; u32 b = g->size;
        while (b) { tmp[t++] = '0' + b % 10; b /= 10; }
        while (t) sz[n++] = tmp[--t];
        if (n < (int)sizeof(sz) - 2) { sz[n++] = ' '; sz[n++] = 'B'; }
    }
    sz[n] = 0;
    A->ctext(A->fb, F3_W / 2, 150, sz, 1, C_MUTED);
    /* botoes */
    card(20, 210, 100, 44, C_ACCENT); A->ctext(A->fb, 70, 216, TR("jogar", "play"), 1, C_TEXT);
    card(136, 210, 100, 44, C_BAD);   A->ctext(A->fb, 186, 216, TR("deletar", "delete"), 1, C_TEXT);
    card(20, 270, 216, 44, C_CARD2);  A->ctext(A->fb, 128, 276, TR("voltar", "back"), 1, C_TEXT);
}

static void draw_keyboard(St *s) {
    clear_bg();
    title_bar(TR("digite o nome", "enter name"));
    /* linha do texto digitado */
    card(12, 60, F3_W - 24, 34, C_CARD);
    A->ctext(A->fb, F3_W / 2, 70, s->input_len ? s->input : TR("...", "..."), 1, C_TEXT);
    /* teclas QWERTY simplificadas */
    const char *rows[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    int ox[3] = { 0, 26, 52 };
    for (int r = 0; r < 3; r++) {
        for (int i = 0; rows[r][i]; i++) {
            int x = 12 + ox[r] + i * 22;
            int y = 110 + r * 36;
            card(x, y, 18, 28, C_CARD2);
            char c[2] = { rows[r][i], 0 };
            A->ctext(A->fb, x + 9, y + 8, c, 1, C_TEXT);
        }
    }
    /* backspace e espaço e ok */
    card(12, 220, 70, 40, C_BAD);    A->ctext(A->fb, 47, 228, "<-", 2, C_TEXT);
    card(88, 220, 80, 40, C_ACCENT); A->ctext(A->fb, 128, 228, "espaço", 1, C_TEXT);
    card(174, 220, 70, 40, C_GOOD);  A->ctext(A->fb, 209, 228, "ok", 2, C_TEXT);
}

static void save_current(St *s) {
    if (s->ngames == 0) return;
    Game *g = &s->games[s->sel];
    int fd = A->fopen(CURRENT_FILE, "w");
    if (fd < 0) { show_msg(s, TR("erro: nao conseguiu gravar", "error: cannot write"), 1500); return; }
    A->fwrite(fd, g->path, (u32)str_len(g->path));
    A->fclose(fd);
    show_msg(s, TR("selecionado: ", "selected: "), 1000);
    /* anexa o nome */
    char tmp[128]; int n = 0;
    const char *p = g->name;
    while (*p && n < (int)sizeof(tmp) - 1) tmp[n++] = *p++;
    tmp[n] = 0;
    /* mensagem já mostrada; o player le CURRENT_FILE */
    s->page = 0;
}

/* ---------- tick ---------- */
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self;
    u32 now = A->ticks();
    if (s->msg_ticks && now > (u32)s->msg_ticks) { s->msg_ticks = 0; if (s->page == 3) s->page = 0; }

    if (s->page == 0) {
        draw_list(s);
        if (tap) {
            if (s->ngames == 0) return 1;
            /* toque na lista */
            int start = s->scroll;
            int visible = 6;
            int idx = -1;
            for (int i = start; i < s->ngames && i < start + visible; i++) {
                int oy = 60 + (i - start) * 56;
                if (y >= oy - 6 && y < oy + 42 && x >= 12 && x < F3_W - 12) { idx = i; break; }
            }
            if (idx >= 0) {
                s->sel = idx;
                save_current(s);
            }
        }
        /* setas de página simuladas por toque nas bordas */
        if (x < 30 && y > 100 && y < 300 && tap) {
            if (s->sel > 0) { s->sel--; if (s->sel < s->scroll) s->scroll = s->sel; }
        }
        if (x > F3_W - 30 && y > 100 && y < 300 && tap) {
            if (s->sel < s->ngames - 1) { s->sel++; if (s->sel >= s->scroll + 6) s->scroll++; }
        }
        /* botao inferior esquerdo: adicionar */
        if (y > 350 && x < 100 && tap) { s->mode = 1; s->input_len = 0; s->input[0] = 0; s->page = 2; }
        /* botao inferior direito: detalhes */
        if (y > 350 && x > F3_W - 100 && tap) { s->page = 1; }
        return 1;
    }
    if (s->page == 1) {
        draw_details(s);
        if (tap) {
            if (y > 200 && y < 260) {
                if (x < 120) { save_current(s); }
                else if (x > 130) {
                    /* deletar: remove do index */
                    if (s->ngames > 0) {
                        for (int i = s->sel; i < s->ngames - 1; i++) s->games[i] = s->games[i + 1];
                        s->ngames--;
                        save_index(s);
                        if (s->sel >= s->ngames) s->sel = s->ngames > 0 ? s->ngames - 1 : 0;
                    }
                    s->page = 0;
                }
            }
            if (y > 260 && y < 320 && x > 20 && x < F3_W - 20) s->page = 0;
        }
        return 1;
    }
    if (s->page == 2) {
        draw_keyboard(s);
        if (tap) {
            /* mapeamento simplificado de toque para tecla */
            int ch = 0;
            const char *rows[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
            int ox[3] = { 0, 26, 52 };
            for (int r = 0; r < 3 && !ch; r++) {
                for (int i = 0; rows[r][i] && !ch; i++) {
                    int kx = 12 + ox[r] + i * 22;
                    int ky = 110 + r * 36;
                    if (x >= kx && x < kx + 18 && y >= ky && y < ky + 28) ch = rows[r][i];
                }
            }
            if (!ch && x >= 12 && x < 82 && y >= 220 && y < 260) ch = 8;   /* backspace */
            if (!ch && x >= 88 && x < 168 && y >= 220 && y < 260) ch = 32; /* espaço */
            if (!ch && x >= 174 && x < 244 && y >= 220 && y < 260) ch = 13;/* enter */
            if (ch == 8) { if (s->input_len > 0) s->input[--s->input_len] = 0; }
            else if (ch == 13) {
                if (s->input_len > 0) {
                    s->input[s->input_len] = 0;
                    /* adicionar jogo fictício (o .jar deve estar no caminho) */
                    char path[MAX_PATH]; int n = 0;
                    const char *p = s->input;
                    while (*p && n < (int)sizeof(path) - 1) path[n++] = *p++;
                    path[n] = 0;
                    if (add_game(s, s->input, path, 0) >= 0) {
                        s->page = 0;
                        show_msg(s, TR("jogo adicionado", "game added"), 1000);
                    } else {
                        show_msg(s, TR("lista cheia", "list full"), 1000);
                    }
                }
                s->mode = 0; s->input_len = 0;
            } else if (ch == 32) {
                if (s->input_len < MAX_NAME - 1) s->input[s->input_len++] = ' ';
            } else if (ch) {
                if (s->input_len < MAX_NAME - 1) s->input[s->input_len++] = (char)ch;
            }
        }
        return 1;
    }
    if (s->page == 3) {
        /* tela de mensagem: so aguarda sumir ou toque */
        if (tap) s->page = 0;
        return 1;
    }
    return 1;
}

static void close_(F3App *self) {
    St *s = (St *)self;
    save_index(s);
    A->free(self);
}

F3App *f3app_main(const F3Api *api) {
    A = api;
    St *s = (St *)api->malloc(sizeof(St));
    if (!s) return 0;
    fill8(s, 0, sizeof(St));
    s->base.tick = tick;
    s->base.close = close_;
    s->sel = 0; s->scroll = 0; s->page = 0;
    load_index(s);
    return &s->base;
}

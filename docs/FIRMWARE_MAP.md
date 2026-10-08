# Mapa do firmware AZA3 (Galaxy Fit3, SM-R390 / R390XXU0AZA3)

Descobertas por engenharia reversa **estática** da imagem principal (`best1502x_b319_user.bin`, SoC Bestechnic BES1502x, Cortex-M4F, Thumb-2).
Endereços de execução (XIP): arquivo `+0` = `0x2c080000`. Tudo aqui é para interoperabilidade e pesquisa; **não há firmware neste repositório**.

## Pacote FWD (o que o flasher envia)
`0x7e5a5a60` | origem `R390XXU0AZA3` | updater | pacote `0x7e5a5a5a` com 73 componentes ZIP (cada um com CRC32).
Imagem principal: cabeçalho em `0x1000` (`0x11223344 0xdeaddead`, CRC32 em `0x1008`, tamanho em `0x100c`). Sem assinatura criptográfica observada: o CRC e o validador do flasher bastam.
Limite do validador do flasher para a imagem principal: `0x380000` (3,5 MB). Flash total: `0x400000` (4 MB).

## Área livre usada para código injetado
`0x2c3a0522`..`+0xf3fe` (arquivo `0x320522`, ~62 KB de zeros). O código injetado vive aí (flash é somente leitura: sem variáveis globais).

## Funções (todas Thumb; chame com o bit 0 setado)
| Endereço | Função |
|---|---|
| `0x2c2974f8` | log(nível, tag, fmt, ...) |
| `0x2c0eb00c` / `0x2c0eb018` | malloc / free (pool 2) |
| `0x2c1423f8` | motor_once_ctrl(id, 0) (vibração; tabela de padrões em `0x2c2e8b74`) |
| `0x2c112f58` | send_reply(seq_id, texto, len) (resposta rápida -> celular) |
| `0x2c0a76d8` | tp_sample_get(amostra): estado `+1` (1/2 tocando), x `+4`, y `+6` (256x402) |
| `0x2c288ef4` | lv_tick_get |
| `0x2c28a3f0` / `0x2c28a448` | lv_timer_create(cb, período, user_data) / lv_timer_del (user_data em `timer+0xC`) |
| `0x2c2905ac` / `0x2c29064c` | lv_img_create(pai) / lv_img_set_src(img, src) |
| `0x2c27908c` | lv_obj_align_to(obj, base, align, x, y) com tratamento RTL |
| `0x2c277b2c` | lv_obj_set_size |
| `0x2c27cbb8` / `0x2c27cc5c` | lv_obj_add_flag / clear_flag |
| `0x2c27d9cc` | lv_obj_add_event_cb(obj, cb, filtro, user_data) |
| `0x2c27d980/84/88/90/94` | lv_event_get_target / current_target / code / param / user_data |
| `0x2c2782a8` | lv_obj_invalidate |
| `0x200f73b8` (RAM) | tabela VFS: `[0]` open(path,"r"/"w") `[1]` read `[2]` write `[3]` close |

LVGL v8, cores RGB565 de 16 bits. `lv_img_dsc_t.header = 4 | (w<<10) | (h<<21)`.

## Pontos de gancho (substituem uma instrução `bl`)
* `0x2c1d0802` — `bl send_reply` no tratador de clique da resposta rápida (teclado de resposta e jogos).
* `0x2c1b76a0` — `bl log` em `app_settings_tutorials_goto_sub_page` (menu "Apps extras").
* `0x2c1b3eb6` / `0x2c1b4b26` — páginas de vibração (usados nas sondas).

## Registro da notificação
Preenchido por `0x2c0ed1f8` no tratador de clique (em `sp+0xc0`). `seq_id` em `+0x124`; estrutura interna em `+0x120` (0x448 bytes): `app_id` em `+8`, **título** em `+0x119`, **texto** em `+0x35c`.

## Protocolo serial Bluetooth (o do flasher)
* Escrever arquivo: `300` -> `33bin,<caminho>,<tamanho>` -> blocos de 39600 B + CRC32 LE (cada um confirmado por `310`) -> `32` -> `34`.
* Ler arquivo: `061<caminho>\0` e depois `062` por bloco.
* Comandos AT de fábrica: `00AT^...` (por exemplo `AT^NOTICE`, `AT^FAC_*_DETECT`). `AT^MEMORY_READ/WRITE` e `AT^JUMP` são stubs desativados no firmware de produção.

## Tela e toque
LCD ICNA3311 (QSPI) 256x402; LCDC em `0x40200000`. Toque por `tp_sample_get` (drivers hyn/chsc/stx/ztw por baixo).

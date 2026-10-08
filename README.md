# fit3-mods: apps injetados no Galaxy Fit3 (SM-R390, firmware R390XXU0AZA3)

Projeto **não oficial** e **sem vínculo com a Samsung**. Reúne as ferramentas e o código para injetar apps pequenos na imagem principal do
firmware AZA3 e reempacotá-la para o instalador web fit3-flasher (o mesmo que você já usa).

> **Aviso.** Gravar firmware modificado pode **inutilizar o relógio** e anula a garantia. Faça por sua conta e risco, com bateria carregada
> e tendo o `stock-aza3.bin` original à mão. **Este repositório não contém firmware da Samsung** e você não deve publicar os pacotes gerados
> (eles contêm código da Samsung): gere os seus a partir do **seu** `stock-aza3.bin`.

## O que tem
* **Teclado de resposta em tela cheia** (acentos, `ç`, UTF-8): responda uma notificação com a resposta rápida `...` e a mensagem aparece junto da caixa de texto.
* **Menu "Apps extras"** em Configurações (a entrada "Dicas e tutoriais" é renomeada nos pacotes de idioma).
* **Jogos originais:** Snake, Flappy, Tetris, 2048.
* **Navegador remoto:** um Chrome sem janela roda no PC e o relógio mostra quadros de 16 cores; toque, rolagem, teclado do relógio **e teclado/mouse do PC**.
  Mais um leitor de texto leve. Tudo pela serial Bluetooth do flasher, com uma ponte local (`webbridge/`).
* Núcleo de uma JVM minimalista (`src/vm.c`) e um rasterizador 3D por software RGB565 (`src/m3d.c`).

Os jogos 3D (um port do Minecraft rd-132211 e um FPS) **não estão** neste repositório porque derivam de material de terceiros; compile com `--no3d`.

## Como funciona
1. `tools/fwd.py` extrai o pacote FWD do `stock-aza3.bin`; a imagem principal tem CRC32 próprio e não é assinada.
2. O código (C freestanding, Thumb, sem libc e **sem variáveis globais**, porque roda em flash) é compilado para a área livre de ~62 KB da imagem.
3. `inject/inject.py` troca instruções `bl` existentes por desvios para o nosso código ("ganchos"). Veja `docs/FIRMWARE_MAP.md`.
4. `tools/repack.py` refaz todos os CRCs do pacote e `tools/val.mjs` roda o **validador real do flasher** sobre o resultado.

Limites sempre respeitados: flash de 4 MB, teto de 3,5 MB do validador do flasher; o build recusa dados graváveis e símbolos externos.

## Requisitos (Windows)
Python 3.10+ (com `pip install capstone`), Node 18+, [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (`arm-none-eabi-*`),
o seu `stock-aza3.bin` e uma cópia do projeto do flasher (para `validation-worker.js`).

```
set ARM_GNU_BIN=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin
set FIT3_STOCK=C:\caminho\stock-aza3.bin
set FLASHER_DIR=C:\caminho\fit3-flasher
python inject/build2.py apps --no3d
```
Gera `dist/fit3-apps.bin` (o validador do flasher é executado no fim). O primeiro build também gera a fonte (`kbd/make_font.py`, usa uma fonte do seu Windows).

## Ponte do navegador
```
cd webbridge
node server.js        (ou start-bridge.bat)
```
Abra `http://127.0.0.1:8787` no Chrome/Edge, pareie o relógio, **Escolher o relógio**, **Testar arquivos** e **Ligar modo navegador**. No relógio: Apps extras, Web.
Requer o Chrome ou o Edge instalado no PC.

## Testes no PC
`inject/host_*_test.c` compilam o mesmo código dos apps com simulações do LVGL e do sistema de arquivos (`gcc -DNO_3D -DHOST_TEST -I kbd -I src ...`).
`webbridge/test_pc_control.js` testa de ponta a ponta o painel de teclado e mouse.

## Estado
Validado **no relógio**: injeção de código, vibração, leitura de toque, imagem LVGL, tela cheia e o teclado de resposta. Os jogos, o menu e o navegador foram
testados no PC com simulação; o acesso a arquivos do firmware (`/user/web_*`) ainda precisa de confirmação no aparelho.

# Fit3 Toolkit — Galaxy Fit3 (SM-R390)

Ferramentas, apps e firmware para o Galaxy Fit3 (chip best1502x, FreeRTOS).
Inclui: launcher J2ME nativo (.f3a), tema Galaxy Watch nas interfaces web, teclado virtual,
empacotador de firmware "tudo em um" e correções no canal de arquivo do watch.

## Estrutura

```
j2me-fit3/
├── apps/src/j2me_manager.c   # App nativo .f3a: gerencia e roda jogos J2ME no watch
├── build_j2me_manager.py     # Compila o app J2ME Manager em .f3a
├── pack_j2me_complete.py     # Monta firmware_fit3_tudo_em_um.zip (J2ME + GB + teclado)
├── build_firmware_completo.py # Empacotador de componentes (player, runtime, jogos)
├── webflasher/style.css      # Tema Galaxy Watch (preto, azul Samsung, translúcido)
├── webbridge/bridge.html     # Ponte Web + teclado virtual + validação de arquivo /user (16 MiB)
├── inject/menu.inc.c         # Menu Apps extras: tema escuro Samsung + opção --nodrag
├── inject/build2.py          # Flag --nodrag para travar arrasto no menu Apps extras
└── .gitignore
```

## Pré-requisitos

- Python 3
- ARM GNU Toolchain (`arm-none-eabi-gcc`) — defina `ARM_GNU_BIN` ou instale em
  `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin`
- JDK 8 (Adoptium) para regenerar `rt.pak` com `mkrt.py` (opcional, usa o já empacotado por padrão)

## Compilar o app J2ME Manager (.f3a)

```bash
cd C:\Users\Usuario\j2me-fit3
py build_j2me_manager.py --out apps/dist/j2me_manager.f3a
```

Gera `apps/dist/j2me_manager.f3a` (~40 KB), instalável pelo Fit3 Manager
(aba "Apps e jogos" → instalar `.f3a`) sem reflash do firmware.

## Montar o firmware "tudo em um"

```bash
py build_firmware_completo.py --out firmware_fit3_tudo_em_um.zip
```

Conteúdo do zip:
- `j2me.f3a` — player J2ME
- `rt.pak` — runtime Java
- `jogos/*.jar` — jogos J2ME (até 8, até ~1,5 MB cada)
- `firmware_bin/*.bin` — firmwares com emulador Game Boy (max-cs, max-mc, net-gb, stock-aza3)
- `teclado/*.bin` — teclado para resposta rápida
- `tema_galaxy_watch/` — CSS do Web Flasher e HTML da Ponte Web com tema Samsung

## Instalar / flashear

1. **Web Flasher:** abra `webflasher/index.html` no Chrome/Edge (HTTPS ou arquivo local),
   pareie o Galaxy Fit3 via Bluetooth do Windows, feche o Galaxy Wearable, escolha o `.bin`
   e transfira (Web Serial).
2. **Fit3 Manager (Android):** instale apps `.f3a` e jogos `.jar` em `/user/apps/` e `/user/j2me/`.
3. **Ponte Web:** `webbridge/bridge.html` envia ROMs de Game Boy para `/user/gb.gb` e controla o watch pelo PC.

## Limites do Galaxy Fit3 (respeitados pelo toolkit)

- Gravação de arquivos: apenas em `/user`, limite de 16 MiB por arquivo
- Firmware `.bin` (~18 MB) **não** vai pelo gerenciador de arquivos; use o Web Flasher ou OTA
- J2ME: até 8 jogos, JAR de até ~1,5 MB
- Game Boy: sem som, ROM própria em `/user/gb.gb`, save em `/user/gb.sav`

## Subir para o GitHub

```bash
cd C:\Users\Usuario\j2me-fit3
git init
git add .
git commit -m "feat: Fit3 toolkit — J2ME manager, Galaxy Watch theme, firmware pack"
git remote add origin https://github.com/SEU_USUARIO/fit3-toolkit.git
git branch -M main
git push -u origin main
```

Crie primeiro o repositório vazio em https://github.com/new e substitua `SEU_USUARIO` pelo seu nome.

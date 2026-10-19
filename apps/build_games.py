"""build_games.py: build every game app in apps/src into apps/dist/*.f3a (+ .meta) with appcc.py. usage: python build_games.py [id ...]"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import appcc
import make_icons
# id, Portuguese name, English name, background RRGGBB, foreground RRGGBB (the launcher tile uses bg for the tile and fg for the letter/text)
GAMES = [
    ("snake", "Snake", "Snake", "BFEFC4", "1E7A3A"),
    ("flappy", "Flappy", "Flappy", "BFE3F8", "1F6FA8"),
    ("tetris", "Tetris", "Tetris", "E0CCF6", "6A38B0"),
    ("g2048", "2048", "2048", "F8DDB8", "B0661C"),
    ("pong", "Pong", "Pong", "C8EAD8", "1F7A52"),
    ("velha", "Jogo da velha", "Tic-tac-toe", "F8C8D2", "B03856"),
    ("memoria", "Memória", "Memory", "FBE6A8", "9A7410"),
    ("sudoku", "Sudoku", "Sudoku", "C9D6F8", "34509E"),
    ("campomin", "Campo minado", "Minesweeper", "D8DCE6", "4A5368"),
    ("breakout", "Breakout", "Breakout", "F8CFC4", "B04A2E"),
    ("respirar", "Respirar", "Breathe", "BDEBEE", "1F7C86"),
    ("timer", "Timer", "Timer", "D6DAF8", "3A44A8"),
    ("contador", "Contador", "Counter", "E6E0C8", "7A6A28"),
    ("sorteio", "Sorteio", "Draw", "F4E2B0", "A87818"),
    ("reflexo", "Reflexo", "Reaction", "C6EFCB", "2A8A3E"),
    ("simon", "Simon", "Simon", "F2C8EE", "9A3A8E"),
    ("lanterna", "Lanterna", "Flashlight", "F6F2C4", "8A8420"),
    ("hello", "Olá", "Hello", "F8D8B0", "A85A20"),
    ("paint", "Desenho", "Paint", "D0F0E0", "208050"),
    ("dice", "Dados", "Dice", "C8F0D8", "107048"),
    ("relogio", "Relógio", "Clock", "CFE0FB", "2A55A8"),
    ("alarme", "Alarme", "Alarm", "FBD0D0", "A83030"),
    ("xadrez", "Xadrez", "Chess", "E0CCF6", "6A38B0"),
]

if __name__ == "__main__":
    want = sys.argv[1:]
    for gid, pt, en, bg, fg in GAMES:
        if want and gid not in want: continue
        out = os.path.join(HERE, "dist", gid + ".f3a")
        n = appcc.build(os.path.join(HERE, "src", gid + ".c"), gid, pt, en, out)
        open(os.path.join(HERE, "dist", gid + ".meta"), "w", encoding="utf-8").write("%s;%s;%s;%s;%s;%s\n" % (gid, bg, fg, pt, en, make_icons.ICONS.get(gid, "")))
        print("%-8s image %5d B, bss %5d B, relocs %3d, file %5d B" % (gid, n[0], n[1], n[2], n[3]))

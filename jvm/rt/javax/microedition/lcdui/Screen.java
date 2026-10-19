package javax.microedition.lcdui;
public abstract class Screen extends Displayable {
    static final int TB = 18, CB = 24;
    int scroll, selected; boolean menuOpen; int menuSel; int dragY, dragStart; boolean dragging; int pressX, pressY; Ticker ticker;
    Screen() {}
    public void setTicker(Ticker t) { ticker = t; }
    public Ticker getTicker() { return ticker; }
    abstract void paintBody(Graphics g, int w, int h);
    abstract int bodyHeight(int w);
    abstract void activate();
    abstract void move(int dir);
    abstract void tapAt(int x, int y);
    boolean isBack(Command c) { int t = c.type; return t == Command.BACK || t == Command.EXIT || t == Command.CANCEL || t == Command.STOP; }
    Command left() { for (int i = 0; i < commands.size(); i++) { Command c = (Command) commands.elementAt(i); if (!isBack(c)) return c; } return commands.size() > 0 && commands.size() == 1 ? null : (commands.size() > 0 ? (Command) commands.elementAt(0) : null); }
    Command right() { Command l = left(); for (int i = 0; i < commands.size(); i++) { Command c = (Command) commands.elementAt(i); if (c != l && isBack(c)) return c; } if (commands.size() == 1 && l == null) return (Command) commands.elementAt(0); return null; }
    int more() { int n = 0; Command l = left(), r = right(); for (int i = 0; i < commands.size(); i++) { Command c = (Command) commands.elementAt(i); if (c != l && c != r) n++; } return n; }
    Command moreAt(int k) { Command l = left(), r = right(); for (int i = 0; i < commands.size(); i++) { Command c = (Command) commands.elementAt(i); if (c != l && c != r) { if (k-- == 0) return c; } } return null; }
    public void _shown() { dirty = true; }
    public void _tick() { if (dirty) { dirty = false; Graphics g = Graphics.screen(); paintAll(g); jvmhost.Host.present(null); } }
    void clampScroll() { int bh = bodyHeight(getWidth()), vh = getHeight() - TB - CB; int mx = bh - vh; if (mx < 0) mx = 0; if (scroll > mx) scroll = mx; if (scroll < 0) scroll = 0; }
    void paintAll(Graphics g) {
        int w = getWidth(), h = getHeight(), vh = h - TB - CB; clampScroll();
        g.setClip(0, 0, w, h); g.setColor(0xffffff); g.fillRect(0, 0, w, h);
        g.setClip(0, TB, w, vh); g.translate(0, TB - scroll); paintBody(g, w, vh); g.translate(0, scroll - TB);
        g.setClip(0, 0, w, h); g.setColor(0x2a5db0); g.fillRect(0, 0, w, TB); g.setColor(0xffffff); g.drawString(title == null ? "" : title, 4, 3, Graphics.TOP | Graphics.LEFT);
        g.setColor(0xdde3ee); g.fillRect(0, h - CB, w, CB); g.setColor(0x202020);
        Command l = left(), r = right(); if (more() > 0) { if (r == null) r = new Command("Menu", Command.SCREEN, 99); else l = l == null ? r : l; }
        if (l != null) g.drawString(l.label, 4, h - CB + 6, Graphics.TOP | Graphics.LEFT);
        if (more() > 0) g.drawString("Menu", w - 4, h - CB + 6, Graphics.TOP | Graphics.RIGHT); else if (r != null) g.drawString(r.label, w - 4, h - CB + 6, Graphics.TOP | Graphics.RIGHT);
        if (menuOpen) {
            int n = more() + 1, mh = n * 20 + 4, my = h - CB - mh; g.setColor(0xffffff); g.fillRect(w / 4, my, w * 3 / 4, mh); g.setColor(0x2a5db0); g.drawRect(w / 4, my, w * 3 / 4 - 1, mh - 1);
            for (int i = 0; i < n; i++) { if (i == menuSel) { g.setColor(0x2a5db0); g.fillRect(w / 4 + 2, my + 2 + i * 20, w * 3 / 4 - 4, 20); g.setColor(0xffffff); } else g.setColor(0); Command c = i == 0 ? r0() : moreAt(i - 1); g.drawString(c == null ? "" : c.label, w / 4 + 6, my + 6 + i * 20, Graphics.TOP | Graphics.LEFT); }
        }
    }
    Command r0() { Command r = right(); return r == null ? left() : r; }
    void runMenu() { Command c = menuSel == 0 ? r0() : moreAt(menuSel - 1); menuOpen = false; dirty = true; if (c != null) _fire(c); }
    public void _key(int code, boolean down) {
        if (!down) return; dirty = true;
        if (menuOpen) { int n = more() + 1; if (code == -1 || code == 50) menuSel = (menuSel + n - 1) % n; else if (code == -2 || code == 56) menuSel = (menuSel + 1) % n; else if (code == -5 || code == 53) runMenu(); else if (code == -7 || code == -6) menuOpen = false; return; }
        if (code == -6) { Command l = left(); if (l == null && more() > 0) { menuOpen = true; menuSel = 0; } else if (l != null) _fire(l); else { Command r = right(); if (r != null) _fire(r); } }
        else if (code == -7) { if (more() > 0) { menuOpen = true; menuSel = 0; } else { Command r = right(); if (r != null) _fire(r); } }
        else if (code == -1 || code == 50) move(-1); else if (code == -2 || code == 56) move(1); else if (code == -3 || code == 52) move(-2); else if (code == -4 || code == 54) move(2);
        else if (code == -5 || code == 53) activate();
    }
    public void _pointer(int type, int x, int y) {
        int h = getHeight(), w = getWidth(); dirty = true;
        if (type == 3) { pressX = x; pressY = y; dragY = y; dragStart = scroll; dragging = false; }
        else if (type == 4) { if (Math.abs(y - pressY) > 6) dragging = true; if (dragging && !menuOpen) { scroll = dragStart + (pressY - y); } }
        else if (type == 5) {
            if (dragging) { dragging = false; return; }
            if (y >= h - CB) { int k = x < w / 2 ? -6 : -7; _key(k, true); return; }
            if (menuOpen) { int n = more() + 1, mh = n * 20 + 4, my = h - CB - mh; if (y >= my && y < h - CB && x >= w / 4) { menuSel = (y - my - 2) / 20; if (menuSel >= n) menuSel = n - 1; runMenu(); } else menuOpen = false; return; }
            if (y >= TB) tapAt(x, y - TB + scroll);
        }
    }
}

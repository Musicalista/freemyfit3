package javax.microedition.lcdui;
import java.util.Vector;
public class Form extends Screen {
    Vector items = new Vector(); ItemStateListener isl; int focus = -1;
    public Form(String title) { this.title = title; }
    public Form(String title, Item[] it) { this(title); if (it != null) for (int i = 0; i < it.length; i++) append(it[i]); }
    public int append(Item i) { items.addElement(i); i.form = this; dirty = true; if (focus < 0 && i.focusable()) focus = items.size() - 1; return items.size() - 1; }
    public int append(String s) { return append(new StringItem(null, s)); }
    public int append(Image im) { return append(new ImageItem(null, im, 0, null)); }
    public void insert(int n, Item i) { items.insertElementAt(i, n); i.form = this; dirty = true; }
    public void delete(int n) { ((Item) items.elementAt(n)).form = null; items.removeElementAt(n); if (focus >= items.size()) focus = items.size() - 1; dirty = true; }
    public void deleteAll() { items.removeAllElements(); focus = -1; dirty = true; }
    public void set(int n, Item i) { items.setElementAt(i, n); i.form = this; dirty = true; }
    public Item get(int n) { return (Item) items.elementAt(n); }
    public int size() { return items.size(); }
    public void setItemStateListener(ItemStateListener l) { isl = l; }
    void changed(Item i) { dirty = true; if (isl != null) isl.itemStateChanged(i); }
    static Vector wrap(String s, int pix) {
        Vector out = new Vector(); if (s == null) s = ""; int cw = pix / 8; if (cw < 4) cw = 4; int i = 0, n = s.length();
        while (i < n || out.size() == 0) {
            int nl = s.indexOf('\n', i); int end = nl < 0 ? n : nl; if (end - i <= cw) { out.addElement(s.substring(i, end)); i = end + 1; if (i > n) break; continue; }
            int cut = i + cw; int sp = s.lastIndexOf(' ', cut); if (sp <= i) sp = cut; out.addElement(s.substring(i, sp)); i = sp < n && s.charAt(sp) == ' ' ? sp + 1 : sp;
        }
        return out;
    }
    int bodyHeight(int w) { int h = 4; for (int i = 0; i < items.size(); i++) h += ((Item) items.elementAt(i)).height(w) + 2; return h; }
    void paintBody(Graphics g, int w, int vh) {
        int y = 2;
        for (int i = 0; i < items.size(); i++) {
            Item it = (Item) items.elementAt(i); int h = it.height(w); boolean f = i == focus;
            if (f) { if (y < scroll) scroll = y - 2; else if (y + h > scroll + vh) scroll = y + h - vh + 2; }
            g.translate(0, y); g.clipRect(0, 0, w, h); it.paint(g, w, f); g.translate(0, -y); g.setClip(0, scroll, w, vh); y += h + 2;
        }
    }
    void activate() { if (focus >= 0) ((Item) items.elementAt(focus)).activate(); }
    void move(int d) {
        if (focus >= 0 && ((Item) items.elementAt(focus)).move(d)) return;
        if (d == -2 || d == 2) { return; }
        int dir = d < 0 ? -1 : 1, i = focus + dir;
        while (i >= 0 && i < items.size() && !((Item) items.elementAt(i)).focusable()) i += dir;
        if (i >= 0 && i < items.size()) focus = i; else scroll += dir * 40;
    }
    void tapAt(int x, int yy) {
        int w = getWidth(), y = 2;
        for (int i = 0; i < items.size(); i++) { Item it = (Item) items.elementAt(i); int h = it.height(w); if (yy >= y && yy < y + h) { if (it.focusable()) { focus = i; it.tap(x, yy - y); } return; } y += h + 2; }
    }
}

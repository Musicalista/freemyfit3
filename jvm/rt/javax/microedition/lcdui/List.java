package javax.microedition.lcdui;
import java.util.Vector;
public class List extends Screen implements Choice {
    public static final Command SELECT_COMMAND = new Command("Select", Command.SCREEN, 0);
    private int type; private Vector strs = new Vector(), imgs = new Vector(); private boolean[] sel = new boolean[8]; private Command selCmd = SELECT_COMMAND; private int fit;
    public List(String title, int type) { this.title = title; this.type = type; }
    public List(String title, int type, String[] s, Image[] im) { this(title, type); for (int i = 0; i < s.length; i++) append(s[i], im == null ? null : im[i]); }
    public int size() { return strs.size(); }
    public String getString(int i) { return (String) strs.elementAt(i); }
    public Image getImage(int i) { return (Image) imgs.elementAt(i); }
    public int append(String s, Image im) { insert(strs.size(), s, im); return strs.size() - 1; }
    public void insert(int i, String s, Image im) {
        strs.insertElementAt(s, i); imgs.insertElementAt(im, i); if (strs.size() > sel.length) { boolean[] n = new boolean[sel.length * 2]; System.arraycopy(sel, 0, n, 0, sel.length); sel = n; }
        for (int k = strs.size() - 1; k > i; k--) sel[k] = sel[k - 1]; sel[i] = false; if (strs.size() == 1 && type != MULTIPLE) sel[0] = true; dirty = true;
    }
    public void delete(int i) { strs.removeElementAt(i); imgs.removeElementAt(i); for (int k = i; k < strs.size(); k++) sel[k] = sel[k + 1]; if (selected >= strs.size()) selected = strs.size() - 1; if (selected < 0) selected = 0; dirty = true; }
    public void deleteAll() { strs.removeAllElements(); imgs.removeAllElements(); selected = 0; for (int k = 0; k < sel.length; k++) sel[k] = false; dirty = true; }
    public void set(int i, String s, Image im) { strs.setElementAt(s, i); imgs.setElementAt(im, i); dirty = true; }
    public boolean isSelected(int i) { return type == IMPLICIT ? i == selected : sel[i]; }
    public int getSelectedIndex() { if (type == MULTIPLE) return -1; return strs.size() == 0 ? -1 : (type == IMPLICIT ? selected : firstSel()); }
    private int firstSel() { for (int i = 0; i < strs.size(); i++) if (sel[i]) return i; return -1; }
    public void setSelectedIndex(int i, boolean s) { if (type == MULTIPLE) sel[i] = s; else { for (int k = 0; k < sel.length; k++) sel[k] = false; sel[i] = true; selected = i; } dirty = true; }
    public int getSelectedFlags(boolean[] a) { int n = 0; for (int i = 0; i < strs.size(); i++) { a[i] = isSelected(i); if (a[i]) n++; } return n; }
    public void setSelectedFlags(boolean[] a) { for (int i = 0; i < strs.size(); i++) sel[i] = a[i]; dirty = true; }
    public void setFitPolicy(int p) { fit = p; } public int getFitPolicy() { return fit; } public void setFont(int i, Font f) {} public Font getFont(int i) { return Font.getDefaultFont(); }
    public void setSelectCommand(Command c) { selCmd = c; }
    int rowH(int i) { Image im = (Image) imgs.elementAt(i); int h = 22; if (im != null && im.getHeight() + 4 > h) h = im.getHeight() + 4; return h; }
    int bodyHeight(int w) { int h = 0; for (int i = 0; i < strs.size(); i++) h += rowH(i); return h; }
    void paintBody(Graphics g, int w, int vh) {
        int y = 0;
        for (int i = 0; i < strs.size(); i++) {
            int rh = rowH(i); Image im = (Image) imgs.elementAt(i); boolean cur = i == selected;
            if (cur) { g.setColor(0xc8d8f4); g.fillRect(0, y, w, rh); }
            int x = 4;
            if (type == EXCLUSIVE || type == MULTIPLE) { g.setColor(0x404040); if (type == EXCLUSIVE) { g.fillArc(x, y + rh / 2 - 6, 12, 12, 0, 360); g.setColor(0xffffff); g.fillArc(x + 2, y + rh / 2 - 4, 8, 8, 0, 360); if (sel[i]) { g.setColor(0x2a5db0); g.fillArc(x + 3, y + rh / 2 - 3, 6, 6, 0, 360); } } else { g.fillRect(x, y + rh / 2 - 6, 12, 12); g.setColor(0xffffff); g.fillRect(x + 1, y + rh / 2 - 5, 10, 10); if (sel[i]) { g.setColor(0x2a5db0); g.fillRect(x + 3, y + rh / 2 - 3, 6, 6); } } x += 18; }
            if (im != null) { g.drawImage(im, x, y + (rh - im.getHeight()) / 2, Graphics.TOP | Graphics.LEFT); x += im.getWidth() + 4; }
            g.setColor(0x101010); g.drawString((String) strs.elementAt(i), x, y + (rh - 12) / 2, Graphics.TOP | Graphics.LEFT); y += rh;
        }
        int sy = 0; for (int i = 0; i < selected && i < strs.size(); i++) sy += rowH(i); int ey = sy + (strs.size() > 0 ? rowH(selected) : 0);
        if (sy < scroll) scroll = sy; else if (ey > scroll + vh) scroll = ey - vh;
    }
    void activate() {
        if (strs.size() == 0) return;
        if (type == MULTIPLE) sel[selected] = !sel[selected]; else if (type == EXCLUSIVE || type == POPUP) { for (int k = 0; k < sel.length; k++) sel[k] = false; sel[selected] = true; }
        else if (selCmd != null) _fire(selCmd);
    }
    void move(int d) { if (d == -1 && selected > 0) selected--; else if (d == 1 && selected < strs.size() - 1) selected++; }
    void tapAt(int x, int yy) { int y = 0; for (int i = 0; i < strs.size(); i++) { int rh = rowH(i); if (yy >= y && yy < y + rh) { boolean same = i == selected; selected = i; if (type == IMPLICIT || same || type == MULTIPLE || type == EXCLUSIVE) activate(); return; } y += rh; } }
}

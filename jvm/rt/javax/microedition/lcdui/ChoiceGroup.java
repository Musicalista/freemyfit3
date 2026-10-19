package javax.microedition.lcdui;
import java.util.Vector;
public class ChoiceGroup extends Item implements Choice {
    private int type; private Vector strs = new Vector(), imgs = new Vector(); private boolean[] sel = new boolean[8]; int cur; private int fit;
    public ChoiceGroup(String label, int type) { this.label = label; this.type = type; }
    public ChoiceGroup(String label, int type, String[] s, Image[] im) { this(label, type); for (int i = 0; i < s.length; i++) append(s[i], im == null ? null : im[i]); }
    public int size() { return strs.size(); } public String getString(int i) { return (String) strs.elementAt(i); } public Image getImage(int i) { return (Image) imgs.elementAt(i); }
    public int append(String s, Image im) { insert(strs.size(), s, im); return strs.size() - 1; }
    public void insert(int i, String s, Image im) { strs.insertElementAt(s, i); imgs.insertElementAt(im, i); if (strs.size() > sel.length) { boolean[] n = new boolean[sel.length * 2]; System.arraycopy(sel, 0, n, 0, sel.length); sel = n; } for (int k = strs.size() - 1; k > i; k--) sel[k] = sel[k - 1]; sel[i] = false; if (strs.size() == 1 && type != MULTIPLE) sel[0] = true; touch(); }
    public void delete(int i) { strs.removeElementAt(i); imgs.removeElementAt(i); for (int k = i; k < strs.size(); k++) sel[k] = sel[k + 1]; touch(); }
    public void deleteAll() { strs.removeAllElements(); imgs.removeAllElements(); for (int k = 0; k < sel.length; k++) sel[k] = false; touch(); }
    public void set(int i, String s, Image im) { strs.setElementAt(s, i); imgs.setElementAt(im, i); touch(); }
    public boolean isSelected(int i) { return sel[i]; }
    public int getSelectedIndex() { if (type == MULTIPLE) return -1; for (int i = 0; i < strs.size(); i++) if (sel[i]) return i; return -1; }
    public void setSelectedIndex(int i, boolean s) { if (type == MULTIPLE) sel[i] = s; else { for (int k = 0; k < sel.length; k++) sel[k] = false; sel[i] = true; } touch(); }
    public int getSelectedFlags(boolean[] a) { int n = 0; for (int i = 0; i < strs.size(); i++) { a[i] = sel[i]; if (a[i]) n++; } return n; } public void setSelectedFlags(boolean[] a) { for (int i = 0; i < strs.size(); i++) sel[i] = a[i]; touch(); }
    public void setFitPolicy(int p) { fit = p; } public int getFitPolicy() { return fit; } public void setFont(int i, Font f) {} public Font getFont(int i) { return Font.getDefaultFont(); }
    boolean focusable() { return strs.size() > 0; }
    boolean move(int dir) { if (type == POPUP) return false; if (dir == -1 && cur > 0) { cur--; return true; } if (dir == 1 && cur < strs.size() - 1) { cur++; return true; } return false; }
    boolean activate() { if (type == MULTIPLE) sel[cur] = !sel[cur]; else if (type == POPUP) { int n = getSelectedIndex(); setSelectedIndex((n + 1) % strs.size(), true); } else setSelectedIndex(cur, true); notifyStateChanged(); touch(); return true; }
    void tap(int x, int y) { if (type != POPUP) { int r = (y - labelH()) / 18; if (r >= 0 && r < strs.size()) cur = r; } activate(); }
    int height(int w) { return labelH() + (type == POPUP ? 20 : 18 * strs.size()); }
    void paint(Graphics g, int w, boolean focus) {
        paintLabel(g); int y = labelH();
        if (type == POPUP) { g.setColor(focus ? 0x2a5db0 : 0x808080); g.fillRect(3, y, w - 6, 18); g.setColor(0xffffff); g.fillRect(4, y + 1, w - 8, 16); g.setColor(0x101010); int s = getSelectedIndex(); g.drawString(s >= 0 ? (String) strs.elementAt(s) : "", 6, y + 3, Graphics.TOP | Graphics.LEFT); return; }
        for (int i = 0; i < strs.size(); i++) {
            int yy = y + 18 * i; if (focus && i == cur) { g.setColor(0xc8d8f4); g.fillRect(0, yy, w, 18); }
            g.setColor(0x404040); if (type == MULTIPLE) g.fillRect(4, yy + 3, 12, 12); else g.fillArc(4, yy + 3, 12, 12, 0, 360); g.setColor(0xffffff); if (type == MULTIPLE) g.fillRect(5, yy + 4, 10, 10); else g.fillArc(6, yy + 5, 8, 8, 0, 360);
            if (sel[i]) { g.setColor(0x2a5db0); if (type == MULTIPLE) g.fillRect(7, yy + 6, 6, 6); else g.fillArc(7, yy + 6, 6, 6, 0, 360); }
            g.setColor(0x101010); g.drawString((String) strs.elementAt(i), 22, yy + 3, Graphics.TOP | Graphics.LEFT);
        }
    }
}

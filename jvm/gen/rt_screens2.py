# javax.microedition.lcdui high level UI: Screen, List, Form, Items, TextBox, on-screen text editor
import os
F = {}
P = 'javax/microedition/lcdui/'
F[P + 'Ticker.java'] = 'package javax.microedition.lcdui;\npublic class Ticker { String s; public Ticker(String s) { this.s = s; } public String getString() { return s; } public void setString(String s) { this.s = s; } }\n'
F[P + 'Choice.java'] = r'''package javax.microedition.lcdui;
public interface Choice {
    int EXCLUSIVE = 1, MULTIPLE = 2, IMPLICIT = 3, POPUP = 4, TEXT_WRAP_DEFAULT = 0, TEXT_WRAP_ON = 1, TEXT_WRAP_OFF = 2;
    int size(); String getString(int i); Image getImage(int i); int append(String s, Image im); void insert(int i, String s, Image im); void delete(int i); void deleteAll(); void set(int i, String s, Image im);
    boolean isSelected(int i); int getSelectedIndex(); void setSelectedIndex(int i, boolean sel); int getSelectedFlags(boolean[] a); void setSelectedFlags(boolean[] a);
    void setFitPolicy(int p); int getFitPolicy(); void setFont(int i, Font f); Font getFont(int i);
}
'''
F[P + 'Screen.java'] = r'''package javax.microedition.lcdui;
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
'''
F[P + 'List.java'] = r'''package javax.microedition.lcdui;
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
'''
F[P + 'Item.java'] = r'''package javax.microedition.lcdui;
public abstract class Item {
    public static final int LAYOUT_DEFAULT = 0, LAYOUT_LEFT = 1, LAYOUT_RIGHT = 2, LAYOUT_CENTER = 3, LAYOUT_NEWLINE_BEFORE = 0x100, LAYOUT_NEWLINE_AFTER = 0x200, LAYOUT_2 = 0x4000;
    public static final int PLAIN = 0, HYPERLINK = 1, BUTTON = 2;
    String label; Form form; int layout; Command defCmd; ItemCommandListener icl;
    public String getLabel() { return label; }
    public void setLabel(String l) { label = l; touch(); }
    public int getLayout() { return layout; } public void setLayout(int l) { layout = l; }
    public void addCommand(Command c) { if (defCmd == null) defCmd = c; } public void removeCommand(Command c) { if (defCmd == c) defCmd = null; }
    public void setItemCommandListener(ItemCommandListener l) { icl = l; } public void setDefaultCommand(Command c) { defCmd = c; }
    public void setPreferredSize(int w, int h) {} public int getPreferredWidth() { return 0; } public int getPreferredHeight() { return 0; } public int getMinimumWidth() { return 0; } public int getMinimumHeight() { return 0; }
    public void notifyStateChanged() { if (form != null) form.changed(this); }
    void touch() { if (form != null) form.dirty = true; }
    abstract int height(int w); abstract void paint(Graphics g, int w, boolean focus);
    boolean focusable() { return false; } boolean activate() { return false; } boolean move(int dir) { return false; } void tap(int x, int y) { activate(); }
    int labelH() { return label == null || label.length() == 0 ? 0 : 14; }
    void paintLabel(Graphics g) { if (labelH() > 0) { g.setColor(0x2a5db0); g.drawString(label, 4, 1, Graphics.TOP | Graphics.LEFT); } }
}
'''
F[P + 'Spacer.java'] = 'package javax.microedition.lcdui;\npublic class Spacer extends Item { int mw, mh; public Spacer(int w, int h) { mw = w; mh = h; } public void setMinimumSize(int w, int h) { mw = w; mh = h; } int height(int w) { return mh; } void paint(Graphics g, int w, boolean f) {} }\n'
F[P + 'StringItem.java'] = r'''package javax.microedition.lcdui;
public class StringItem extends Item {
    String text; int appearance;
    public StringItem(String label, String text) { this.label = label; this.text = text; }
    public StringItem(String label, String text, int app) { this.label = label; this.text = text; appearance = app; }
    public String getText() { return text; } public void setText(String t) { text = t; touch(); } public int getAppearanceMode() { return appearance; } public void setFont(Font f) {} public Font getFont() { return Font.getDefaultFont(); }
    boolean focusable() { return appearance != PLAIN || defCmd != null; }
    boolean activate() { if (icl != null && defCmd != null) { icl.commandAction(defCmd, this); return true; } return false; }
    int height(int w) { return labelH() + (appearance == BUTTON ? 30 : 14 * Form.wrap(text, w - 8).size() + 2); }
    void paint(Graphics g, int w, boolean focus) {
        paintLabel(g); int y = labelH();
        if (appearance == BUTTON) { g.setColor(focus ? 0x2a5db0 : 0xc8d8f4); g.fillRoundRect(4, y + 2, w - 8, 26, 10, 10); g.setColor(focus ? 0xffffff : 0x101010); g.drawString(text == null ? "" : text, w / 2, y + 8, Graphics.TOP | Graphics.HCENTER); return; }
        java.util.Vector ls = Form.wrap(text, w - 8); g.setColor(focus ? 0x2a5db0 : 0x101010);
        for (int i = 0; i < ls.size(); i++) g.drawString((String) ls.elementAt(i), 4, y + 14 * i + 1, Graphics.TOP | Graphics.LEFT);
    }
}
'''
F[P + 'ImageItem.java'] = r'''package javax.microedition.lcdui;
public class ImageItem extends Item {
    Image img; String alt;
    public ImageItem(String label, Image img, int layout, String alt) { this.label = label; this.img = img; this.layout = layout; this.alt = alt; }
    public ImageItem(String label, Image img, int layout, String alt, int app) { this(label, img, layout, alt); }
    public Image getImage() { return img; } public void setImage(Image i) { img = i; touch(); } public String getAltText() { return alt; } public void setAltText(String s) { alt = s; }
    int height(int w) { return labelH() + (img == null ? 0 : img.getHeight() + 2); }
    void paint(Graphics g, int w, boolean f) { paintLabel(g); if (img != null) g.drawImage(img, (layout & 3) == 3 ? (w - img.getWidth()) / 2 : 4, labelH() + 1, Graphics.TOP | Graphics.LEFT); }
}
'''
F[P + 'DateField.java'] = r'''package javax.microedition.lcdui;
public class DateField extends Item {
    public static final int DATE = 1, TIME = 2, DATE_TIME = 3; java.util.Date d; int mode;
    public DateField(String label, int mode) { this.label = label; this.mode = mode; }
    public java.util.Date getDate() { return d; } public void setDate(java.util.Date x) { d = x; touch(); } public int getInputMode() { return mode; } public void setInputMode(int m) { mode = m; }
    int height(int w) { return labelH() + 16; } void paint(Graphics g, int w, boolean f) { paintLabel(g); g.setColor(0x101010); g.drawString(d == null ? "--" : d.toString(), 4, labelH() + 1, Graphics.TOP | Graphics.LEFT); }
}
'''
F[P + 'Gauge.java'] = r'''package javax.microedition.lcdui;
public class Gauge extends Item {
    public static final int INDEFINITE = -1, CONTINUOUS_IDLE = 0, INCREMENTAL_IDLE = 1, CONTINUOUS_RUNNING = 2, INCREMENTAL_UPDATING = 3;
    int max, val; boolean inter;
    public Gauge(String label, boolean interactive, int maxValue, int initialValue) { this.label = label; inter = interactive; max = maxValue; val = initialValue; }
    public int getValue() { return val; } public void setValue(int v) { val = v < 0 ? 0 : (max >= 0 && v > max ? max : v); touch(); } public int getMaxValue() { return max; } public void setMaxValue(int m) { max = m; touch(); } public boolean isInteractive() { return inter; }
    boolean focusable() { return inter; }
    boolean move(int dir) { if (!inter) return false; if (dir == -2 && val > 0) { val--; notifyStateChanged(); return true; } if (dir == 2 && val < max) { val++; notifyStateChanged(); return true; } return dir == -2 || dir == 2; }
    void tap(int x, int y) { if (inter && max > 0) { val = (x - 4) * max / 170; if (val < 0) val = 0; if (val > max) val = max; notifyStateChanged(); } }
    int height(int w) { return labelH() + 18; }
    void paint(Graphics g, int w, boolean focus) { paintLabel(g); int y = labelH() + 3; g.setColor(0xc0c0c0); g.fillRect(4, y, w - 8, 10); g.setColor(focus ? 0x2a5db0 : 0x4080d0); if (max > 0) g.fillRect(4, y, (w - 8) * val / max, 10); g.setColor(0x404040); g.drawRect(4, y, w - 9, 10); }
}
'''
F[P + 'TextField.java'] = r'''package javax.microedition.lcdui;
public class TextField extends Item implements TextEditor.Target {
    public static final int ANY = 0, EMAILADDR = 1, NUMERIC = 2, PHONENUMBER = 3, URL = 4, DECIMAL = 5, PASSWORD = 0x10000, UNEDITABLE = 0x20000, SENSITIVE = 0x40000, NON_PREDICTIVE = 0x80000, INITIAL_CAPS_WORD = 0x100000, INITIAL_CAPS_SENTENCE = 0x200000, CONSTRAINT_MASK = 0xffff;
    String text; int max, cons;
    public TextField(String label, String text, int max, int cons) { this.label = label; this.text = text == null ? "" : text; this.max = max; this.cons = cons; }
    public String getString() { return text; } public void setString(String s) { text = s == null ? "" : s; touch(); }
    public int size() { return text.length(); } public int getMaxSize() { return max; } public int setMaxSize(int m) { max = m; return m; }
    public void insert(String s, int pos) { text = text.substring(0, pos) + s + text.substring(pos); touch(); } public void delete(int off, int len) { text = text.substring(0, off) + text.substring(off + len); touch(); }
    public int getChars(char[] d) { text.getChars(0, text.length(), d, 0); return text.length(); } public void setChars(char[] d, int o, int l) { text = new String(d, o, l); touch(); }
    public int getCaretPosition() { return text.length(); } public void setConstraints(int c) { cons = c; } public int getConstraints() { return cons; } public void setInitialInputMode(String m) {}
    public String _get() { return text; } public void _set(String s) { text = s; notifyStateChanged(); touch(); } public int _max() { return max; } public int _cons() { return cons; } public boolean _multi() { return false; }
    boolean focusable() { return (cons & UNEDITABLE) == 0; }
    boolean activate() { if (form != null) { TextEditor.open(this, label, form); return true; } return false; }
    int height(int w) { return labelH() + 22; }
    void paint(Graphics g, int w, boolean focus) {
        paintLabel(g); int y = labelH(); g.setColor(focus ? 0x2a5db0 : 0x808080); g.fillRect(3, y + 1, w - 6, 20); g.setColor(0xffffff); g.fillRect(4, y + 2, w - 8, 18); g.setColor(0x101010);
        String s = text; if ((cons & PASSWORD) != 0) { StringBuffer sb = new StringBuffer(); for (int i = 0; i < s.length(); i++) sb.append('*'); s = sb.toString(); }
        int cw = (w - 16) / 8; if (s.length() > cw) s = s.substring(s.length() - cw); g.drawString(s, 6, y + 5, Graphics.TOP | Graphics.LEFT);
    }
}
'''
F[P + 'ChoiceGroup.java'] = r'''package javax.microedition.lcdui;
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
'''
F[P + 'Form.java'] = r'''package javax.microedition.lcdui;
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
'''
F[P + 'TextBox.java'] = r'''package javax.microedition.lcdui;
public class TextBox extends Screen implements TextEditor.Target {
    String text; int max, cons;
    public TextBox(String title, String text, int max, int cons) { this.title = title; this.text = text == null ? "" : text; this.max = max; this.cons = cons; }
    public String getString() { return text; } public void setString(String s) { text = s == null ? "" : s; dirty = true; } public int size() { return text.length(); }
    public int getMaxSize() { return max; } public int setMaxSize(int m) { max = m; return m; } public void insert(String s, int pos) { text = text.substring(0, pos) + s + text.substring(pos); dirty = true; }
    public void delete(int o, int l) { text = text.substring(0, o) + text.substring(o + l); dirty = true; } public int getChars(char[] d) { text.getChars(0, text.length(), d, 0); return text.length(); }
    public void setChars(char[] d, int o, int l) { text = new String(d, o, l); dirty = true; } public int getCaretPosition() { return text.length(); } public void setConstraints(int c) { cons = c; } public int getConstraints() { return cons; } public void setInitialInputMode(String m) {}
    public String _get() { return text; } public void _set(String s) { text = s; dirty = true; } public int _max() { return max; } public int _cons() { return cons; } public boolean _multi() { return true; }
    int bodyHeight(int w) { return 14 * Form.wrap(text, w - 8).size() + 8; }
    void paintBody(Graphics g, int w, int vh) { java.util.Vector ls = Form.wrap(text, w - 8); g.setColor(0x101010); for (int i = 0; i < ls.size(); i++) g.drawString((String) ls.elementAt(i), 4, 4 + 14 * i, Graphics.TOP | Graphics.LEFT); }
    void activate() { TextEditor.open(this, title, this); }
    void move(int d) { if (d == -1) scroll -= 28; else if (d == 1) scroll += 28; }
    void tapAt(int x, int y) { activate(); }
}
'''
F[P + 'TextEditor.java'] = r'''package javax.microedition.lcdui;
final class TextEditor extends Displayable {
    interface Target { String _get(); void _set(String s); int _max(); int _cons(); boolean _multi(); }
    private Target t; private Displayable back; private StringBuffer sb; private int layer; private String cap;
    static final String[] ROWS = { "qwertyuiop", "asdfghjkl", "zxcvbnm", "1234567890", "-/:;()$&@\"", ".,?!'#%+=*" };
    static void open(Target t, String label, Displayable back) {
        TextEditor e = new TextEditor(); e.t = t; e.back = back; e.cap = label == null ? "" : label; e.sb = new StringBuffer(t._get()); e.layer = (t._cons() & 0xffff) == 2 || (t._cons() & 0xffff) == 3 ? 1 : 0;
        Display.inst.setCurrent(e);
    }
    private TextEditor() {}
    private int kx(int i, int n, int w) { return (w - n * 24) / 2 + i * 24; }
    public void _tick() {
        if (!dirty) return; dirty = false; Graphics g = Graphics.screen(); int w = getWidth(), h = getHeight();
        g.setColor(0xf0f0f4); g.fillRect(0, 0, w, h); g.setColor(0x2a5db0); g.fillRect(0, 0, w, 18); g.setColor(0xffffff); g.drawString(cap, 4, 3, Graphics.TOP | Graphics.LEFT);
        g.setColor(0xffffff); g.fillRect(4, 24, w - 8, 120); g.setColor(0x808080); g.drawRect(4, 24, w - 9, 119); g.setColor(0x101010);
        String s = sb.toString(); if ((t._cons() & TextField.PASSWORD) != 0) { StringBuffer p = new StringBuffer(); for (int i = 0; i < s.length(); i++) p.append('*'); s = p.toString(); }
        java.util.Vector ls = Form.wrap(s + "_", w - 20); int first = ls.size() > 8 ? ls.size() - 8 : 0; for (int i = first; i < ls.size(); i++) g.drawString((String) ls.elementAt(i), 8, 28 + 14 * (i - first), Graphics.TOP | Graphics.LEFT);
        g.setColor(0x606060); g.drawString(sb.length() + "/" + t._max(), w - 6, 148, Graphics.TOP | Graphics.RIGHT);
        int top = 168; String[] rows = layer == 1 ? new String[] { ROWS[3], ROWS[4], ROWS[5] } : new String[] { ROWS[0], ROWS[1], ROWS[2] };
        for (int r = 0; r < 3; r++) for (int i = 0; i < rows[r].length(); i++) {
            char c = rows[r].charAt(i); if (layer == 2) c = Character.toUpperCase(c); int x = kx(i, rows[r].length(), w) + (r == 2 && layer != 1 ? 0 : 0);
            g.setColor(0xffffff); g.fillRoundRect(x + 1, top + r * 30, 22, 27, 6, 6); g.setColor(0x101010); g.drawChar(c, x + 12, top + r * 30 + 8, Graphics.TOP | Graphics.HCENTER);
        }
        g.setColor(0xc8d8f4); g.fillRoundRect(2, top + 90, 44, 27, 6, 6); g.fillRoundRect(w - 46, top + 90, 44, 27, 6, 6); g.fillRoundRect(2, top + 60, 34, 27, 6, 6); g.fillRoundRect(w - 36, top + 60, 34, 27, 6, 6);
        g.setColor(0xffffff); g.fillRoundRect(50, top + 90, w - 100, 27, 6, 6); g.setColor(0x101010);
        g.drawString(layer == 1 ? "abc" : "123", 24, top + 98, Graphics.TOP | Graphics.HCENTER); g.drawString("OK", w - 24, top + 98, Graphics.TOP | Graphics.HCENTER);
        g.drawString(layer == 2 ? "abc" : "ABC", 19, top + 68, Graphics.TOP | Graphics.HCENTER); g.drawString("<-", w - 19, top + 68, Graphics.TOP | Graphics.HCENTER);
        g.drawString("space", w / 2, top + 98, Graphics.TOP | Graphics.HCENTER);
        jvmhost.Host.present(null);
    }
    private void done(boolean ok) { if (ok) t._set(sb.toString()); Displayable b = back; if (b instanceof Form) ((Form) b).dirty = true; Display.inst.setCurrent(b); }
    private void add(char c) { if (t._max() > 0 && sb.length() >= t._max()) return; sb.append(c); if (layer == 2) layer = 0; dirty = true; }
    public void _key(int code, boolean down) {
        if (!down) return;
        if (code == -6) done(true); else if (code == -7) done(false);
        else if (code >= 32 && code < 127) add((char) code); else if (code == -8) { if (sb.length() > 0) sb.setLength(sb.length() - 1); dirty = true; } else if (code == -5) done(true);
    }
    public void _pointer(int type, int x, int y) {
        if (type != 5) return; int w = getWidth(), top = 168; dirty = true;
        if (y >= top + 90 && y < top + 117) { if (x < 48) layer = layer == 1 ? 0 : 1; else if (x > w - 48) done(true); else if (t._multi()) add('\n'); else add(' '); if (x >= 48 && x <= w - 48) { if (sb.length() > 0 && sb.charAt(sb.length() - 1) == ' ') { } } return; }
        if (y >= top + 60 && y < top + 87 && layer != 1 && x < 36) { layer = layer == 2 ? 0 : 2; return; }
        if (y >= top + 60 && y < top + 87 && x > w - 38) { if (sb.length() > 0) sb.setLength(sb.length() - 1); return; }
        int r = (y - top) / 30; if (r < 0 || r > 2) return; String[] rows = layer == 1 ? new String[] { ROWS[3], ROWS[4], ROWS[5] } : new String[] { ROWS[0], ROWS[1], ROWS[2] };
        int i = (x - kx(0, rows[r].length(), w)) / 24; if (i >= 0 && i < rows[r].length()) { char c = rows[r].charAt(i); add(layer == 2 ? Character.toUpperCase(c) : c); }
    }
}
'''
if __name__ == '__main__':
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rt')
    for p, t in F.items():
        fp = os.path.join(root, p); os.makedirs(os.path.dirname(fp), exist_ok=True); open(fp, 'w', newline='\n', encoding='utf-8').write(t)
    print(len(F), 'files')

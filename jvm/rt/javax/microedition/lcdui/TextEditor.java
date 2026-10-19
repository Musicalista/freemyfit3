package javax.microedition.lcdui;
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

package javax.microedition.lcdui;
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

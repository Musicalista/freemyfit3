package javax.microedition.lcdui;
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

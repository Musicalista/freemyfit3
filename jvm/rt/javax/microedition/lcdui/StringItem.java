package javax.microedition.lcdui;
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

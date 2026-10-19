package javax.microedition.lcdui;
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

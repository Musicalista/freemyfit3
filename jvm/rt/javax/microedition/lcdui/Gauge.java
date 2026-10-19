package javax.microedition.lcdui;
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

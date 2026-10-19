package javax.microedition.lcdui;
public class DateField extends Item {
    public static final int DATE = 1, TIME = 2, DATE_TIME = 3; java.util.Date d; int mode;
    public DateField(String label, int mode) { this.label = label; this.mode = mode; }
    public java.util.Date getDate() { return d; } public void setDate(java.util.Date x) { d = x; touch(); } public int getInputMode() { return mode; } public void setInputMode(int m) { mode = m; }
    int height(int w) { return labelH() + 16; } void paint(Graphics g, int w, boolean f) { paintLabel(g); g.setColor(0x101010); g.drawString(d == null ? "--" : d.toString(), 4, labelH() + 1, Graphics.TOP | Graphics.LEFT); }
}

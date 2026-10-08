package javax.microedition.lcdui;
public class Graphics {
    public static final int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64;
    public void setColor(int rgb) { jvmhost.Host.setColor(rgb & 0xffffff); }
    public void setColor(int r, int g, int b) { jvmhost.Host.setColor((r << 16) | (g << 8) | b); }
    public void fillRect(int x, int y, int w, int h) { jvmhost.Host.fillRect(x, y, w, h); }
    public void drawRect(int x, int y, int w, int h) { drawLine(x, y, x + w, y); drawLine(x + w, y, x + w, y + h); drawLine(x + w, y + h, x, y + h); drawLine(x, y + h, x, y); }
    public void drawLine(int x0, int y0, int x1, int y1) { jvmhost.Host.drawLine(x0, y0, x1, y1); }
    public void drawString(String s, int x, int y, int anchor) { jvmhost.Host.drawString(s, x, y, anchor); }
}

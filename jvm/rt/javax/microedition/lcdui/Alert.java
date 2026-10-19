package javax.microedition.lcdui;
public class Alert extends Displayable {
    public static final int FOREVER = -2;
    String text; Displayable next; int timeout = 3000; long start;
    public Alert(String title) { this.title = title; }
    public Alert(String title, String text, Image img, AlertType type) { this.title = title; this.text = text; }
    public String getString() { return text; }
    public void setString(String s) { text = s; dirty = true; }
    public void setTimeout(int t) { timeout = t; }
    public int getTimeout() { return timeout; }
    public void setType(AlertType t) {}
    public void setImage(Image i) {}
    public void _shown() { dirty = true; start = System.currentTimeMillis(); }
    public void _tick() {
        if (dirty) { dirty = false; Graphics g = Graphics.screen(); g.setColor(0xffffff); g.fillRect(0, 0, getWidth(), getHeight()); g.setColor(0); g.drawString(title == null ? "" : title, 4, 4, Graphics.TOP | Graphics.LEFT); g.drawString(text == null ? "" : text, 4, 30, Graphics.TOP | Graphics.LEFT); jvmhost.Host.present(null); }
        if (timeout != FOREVER && System.currentTimeMillis() - start > timeout) done();
    }
    public void _key(int c, boolean down) { if (down) done(); }
    public void _pointer(int t, int x, int y) { if (t == 3) done(); }
    private void done() { Displayable n = next; if (n == null) return; next = null; Display.inst.setCurrent(n); }
}

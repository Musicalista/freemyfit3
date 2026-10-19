# javax.microedition.lcdui high-level screens (first version: Alert; List/Form/TextBox/Item are added in rt_screens2.py)
import os
F = {}
P = 'javax/microedition/lcdui/'
F[P + 'Item.java'] = r'''package javax.microedition.lcdui;
public abstract class Item {
    String label;
    public String getLabel() { return label; }
    public void setLabel(String l) { label = l; }
    abstract int height(int w);
    abstract void paint(Graphics g, int w, boolean focus);
}
'''
F[P + 'Alert.java'] = r'''package javax.microedition.lcdui;
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
'''
F[P + 'AlertType.java'] = r'''package javax.microedition.lcdui;
public class AlertType {
    public static final AlertType INFO = new AlertType(), WARNING = new AlertType(), ERROR = new AlertType(), ALARM = new AlertType(), CONFIRMATION = new AlertType();
    private AlertType() {}
    public boolean playSound(Display d) { return true; }
}
'''
if __name__ == '__main__':
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rt')
    for p, t in F.items():
        fp = os.path.join(root, p); os.makedirs(os.path.dirname(fp), exist_ok=True); open(fp, 'w', newline='\n', encoding='utf-8').write(t)
    print(len(F), 'files')

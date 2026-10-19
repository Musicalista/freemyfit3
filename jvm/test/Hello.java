import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;
public class Hello extends MIDlet {
    static class Cv extends Canvas implements Runnable {
        int x = 10, dx = 3, frames; boolean running = true; String info = "hi";
        Cv() { new Thread(this).start(); }
        public void run() { while (running) { x += dx; if (x > getWidth() - 30 || x < 0) dx = -dx; frames++; repaint(); serviceRepaints(); try { Thread.sleep(20); } catch (InterruptedException e) {} } }
        protected void paint(Graphics g) {
            g.setColor(0x102030); g.fillRect(0, 0, getWidth(), getHeight());
            g.setColor(0xff8000); g.fillRect(x, 100, 30, 30);
            g.setColor(0xffffff); g.drawString("Hello J2ME " + frames + " " + info, 4, 4, Graphics.TOP | Graphics.LEFT);
            g.setColor(0x00ff00); g.drawLine(0, 0, getWidth() - 1, getHeight() - 1);
            g.fillTriangle(20, 200, 80, 200, 50, 160); g.setColor(0xff0000); g.fillArc(120, 150, 60, 60, 0, 360);
            StringBuffer sb = new StringBuffer(); for (int i = 0; i < 3; i++) sb.append(i * 1.5); g.setColor(0xffff00); g.drawString(sb.toString() + " " + Math.sqrt(2.0) + " " + (7 / 2), 4, 20, Graphics.TOP | Graphics.LEFT);
        }
        protected void keyPressed(int k) { info = "key " + k; }
        protected void pointerPressed(int px, int py) { info = "ptr " + px + "," + py; }
    }
    protected void startApp() { Display.getDisplay(this).setCurrent(new Cv()); }
    protected void pauseApp() {}
    protected void destroyApp(boolean u) {}
}

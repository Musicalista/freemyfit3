package javax.microedition.lcdui.game;
import javax.microedition.lcdui.*;
public abstract class GameCanvas extends Canvas {
    public static final int UP_PRESSED = 2, DOWN_PRESSED = 64, LEFT_PRESSED = 4, RIGHT_PRESSED = 32, FIRE_PRESSED = 256, GAME_A_PRESSED = 512, GAME_B_PRESSED = 1024, GAME_C_PRESSED = 2048, GAME_D_PRESSED = 4096;
    private Image buffer; private Graphics g; private boolean suppress;
    protected GameCanvas(boolean suppressKeyEvents) { suppress = suppressKeyEvents; buffer = Image.createImage(getWidth(), getHeight()); g = buffer.getGraphics(); g.setColor(0xffffff); g.fillRect(0, 0, getWidth(), getHeight()); }
    protected Graphics getGraphics() { return g; }
    public void flushGraphics() { jvmhost.Host.present(buffer); }
    public void flushGraphics(int x, int y, int w, int h) { jvmhost.Host.present(buffer); }
    public int getKeyStates() { return Display.keyStates; }
    public void paint(Graphics gr) { gr.drawImage(buffer, 0, 0, Graphics.TOP | Graphics.LEFT); }
    public void _key(int code, boolean down) { if (suppress && code != -6 && code != -7) return; super._key(code, down); }
    public void _paintNow() { dirty = false; Graphics sg = Graphics.screen(); paint(sg); jvmhost.Host.present(null); }
}

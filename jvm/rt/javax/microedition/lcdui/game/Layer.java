package javax.microedition.lcdui.game;
import javax.microedition.lcdui.Graphics;
public abstract class Layer {
    int x, y, width, height; boolean visible = true;
    Layer(int w, int h) { width = w; height = h; }
    public void setPosition(int x, int y) { this.x = x; this.y = y; }
    public void move(int dx, int dy) { x += dx; y += dy; }
    public final int getX() { return x; }
    public final int getY() { return y; }
    public final int getWidth() { return width; }
    public final int getHeight() { return height; }
    public void setVisible(boolean v) { visible = v; }
    public final boolean isVisible() { return visible; }
    public abstract void paint(Graphics g);
}

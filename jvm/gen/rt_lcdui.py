# javax.microedition.lcdui (+ game) runtime sources
import os
F = {}
P = 'javax/microedition/lcdui/'
F[P + 'CommandListener.java'] = 'package javax.microedition.lcdui;\npublic interface CommandListener { void commandAction(Command c, Displayable d); }\n'
F[P + 'ItemCommandListener.java'] = 'package javax.microedition.lcdui;\npublic interface ItemCommandListener { void commandAction(Command c, Item i); }\n'
F[P + 'ItemStateListener.java'] = 'package javax.microedition.lcdui;\npublic interface ItemStateListener { void itemStateChanged(Item i); }\n'
F[P + 'Command.java'] = r'''package javax.microedition.lcdui;
public class Command {
    public static final int SCREEN = 1, BACK = 2, CANCEL = 3, OK = 4, HELP = 5, STOP = 6, EXIT = 7, ITEM = 8;
    String label, longLabel; int type, priority;
    public Command(String l, int t, int p) { label = l; type = t; priority = p; }
    public Command(String l, String ll, int t, int p) { label = l; longLabel = ll; type = t; priority = p; }
    public String getLabel() { return label; }
    public String getLongLabel() { return longLabel; }
    public int getCommandType() { return type; }
    public int getPriority() { return priority; }
}
'''
F[P + 'Displayable.java'] = r'''package javax.microedition.lcdui;
import java.util.Vector;
public abstract class Displayable {
    public Vector commands = new Vector(); CommandListener listener; String title; public boolean dirty = true;
    public void addCommand(Command c) { if (!commands.contains(c)) { int i = 0; while (i < commands.size() && ((Command) commands.elementAt(i)).priority <= c.priority) i++; commands.insertElementAt(c, i); } dirty = true; }
    public void removeCommand(Command c) { commands.removeElement(c); dirty = true; }
    public void setCommandListener(CommandListener l) { listener = l; }
    public void setTicker(Ticker t) {}
    public Ticker getTicker() { return null; }
    public String getTitle() { return title; }
    public void setTitle(String t) { title = t; dirty = true; }
    public boolean isShown() { return Display.cur == this; }
    public int getWidth() { return jvmhost.Host.width(); }
    public int getHeight() { return jvmhost.Host.height(); }
    public void _fire(Command c) { if (listener != null) listener.commandAction(c, this); }
    public void _shown() { dirty = true; }
    public void _hidden() {}
    public void _key(int code, boolean down) {}
    public void _pointer(int type, int x, int y) {}
    public void _tick() {}
}
'''
F[P + 'Display.java'] = r'''package javax.microedition.lcdui;
import java.util.Vector;
import javax.microedition.midlet.MIDlet;
public class Display {
    static Display inst = new Display(); static Displayable cur; public static int keyStates; static Vector serial = new Vector(); static boolean uiStarted;
    private Display() {}
    public static Display getDisplay(MIDlet m) { if (!uiStarted) { uiStarted = true; new Thread(new UiLoop()).start(); } return inst; }
    public void setCurrent(Displayable d) {
        if (d == null) return;
        Displayable o = cur; if (o != null && o != d) o._hidden();
        cur = d; d._shown();
    }
    public void setCurrent(Alert a, Displayable next) { a.next = next; setCurrent(a); }
    public Displayable getCurrent() { return cur; }
    public void callSerially(Runnable r) { synchronized (serial) { serial.addElement(r); } }
    public boolean isColor() { return true; }
    public int numColors() { return 65536; }
    public int numAlphaLevels() { return 2; }
    public int getColor(int c) { return 0; }
    public int getBorderStyle(boolean h) { return 0; }
    public int getBestImageWidth(int t) { return 32; }
    public int getBestImageHeight(int t) { return 32; }
    public boolean vibrate(int ms) { jvmhost.Host.vibrate(ms); return true; }
    public boolean flashBacklight(int ms) { return true; }
    static void dispatch() {
        int t;
        while ((t = jvmhost.Host.pollEvent()) != 0) {
            int a = jvmhost.Host.pollArg(); Displayable d = cur; if (d == null) continue;
            if (t == 1 || t == 2) { updateKeyState(a, t == 1); d._key(a, t == 1); }
            else d._pointer(t, a & 0xffff, a >>> 16);
        }
    }
    static void updateKeyState(int code, boolean down) {
        int bit = 0;
        switch (code) {
            case -1: case 50: bit = 2; break; case -2: case 56: bit = 64; break; case -3: case 52: bit = 4; break; case -4: case 54: bit = 32; break;
            case -5: case 53: bit = 256; break; case 49: bit = 512; break; case 51: bit = 1024; break; case 55: bit = 2048; break; case 57: bit = 4096; break;
        }
        if (down) keyStates |= bit; else keyStates &= ~bit;
    }
}
'''
F[P + 'UiLoop.java'] = r'''package javax.microedition.lcdui;
final class UiLoop implements Runnable {
    public void run() {
        while (true) {
            Display.dispatch();
            Runnable r = null;
            synchronized (Display.serial) { if (Display.serial.size() > 0) { r = (Runnable) Display.serial.elementAt(0); Display.serial.removeElementAt(0); } }
            if (r != null) { try { r.run(); } catch (Throwable t) { jvmhost.Host.log("serial: " + t); } }
            Displayable d = Display.cur; if (d != null) d._tick();
            try { Thread.sleep(8); } catch (InterruptedException e) {}
        }
    }
}
'''
F[P + 'Canvas.java'] = r'''package javax.microedition.lcdui;
public abstract class Canvas extends Displayable {
    public static final int UP = 1, LEFT = 2, RIGHT = 5, DOWN = 6, FIRE = 8, GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    public static final int KEY_NUM0 = 48, KEY_NUM1 = 49, KEY_NUM2 = 50, KEY_NUM3 = 51, KEY_NUM4 = 52, KEY_NUM5 = 53, KEY_NUM6 = 54, KEY_NUM7 = 55, KEY_NUM8 = 56, KEY_NUM9 = 57, KEY_STAR = 42, KEY_POUND = 35;
    protected Canvas() {}
    public int getWidth() { return jvmhost.Host.width(); }
    public int getHeight() { return jvmhost.Host.height(); }
    public boolean isDoubleBuffered() { return true; }
    public boolean hasPointerEvents() { return true; }
    public boolean hasPointerMotionEvents() { return true; }
    public boolean hasRepeatEvents() { return false; }
    public void setFullScreenMode(boolean b) {}
    public final void repaint() { dirty = true; }
    public final void repaint(int x, int y, int w, int h) { dirty = true; }
    public final void serviceRepaints() { if (dirty) _paintNow(); }
    public void _paintNow() { dirty = false; Graphics g = Graphics.screen(); paint(g); jvmhost.Host.present(null); }
    protected abstract void paint(Graphics g);
    protected void keyPressed(int keyCode) {}
    protected void keyReleased(int keyCode) {}
    protected void keyRepeated(int keyCode) {}
    protected void pointerPressed(int x, int y) {}
    protected void pointerReleased(int x, int y) {}
    protected void pointerDragged(int x, int y) {}
    protected void showNotify() {}
    protected void hideNotify() {}
    protected void sizeChanged(int w, int h) {}
    public int getGameAction(int k) {
        switch (k) {
            case -1: case 50: return UP;
            case -2: case 56: return DOWN;
            case -3: case 52: return LEFT;
            case -4: case 54: return RIGHT;
            case -5: case 53: return FIRE;
            case 49: return GAME_A; case 51: return GAME_B; case 55: return GAME_C; case 57: return GAME_D;
            default: return 0;
        }
    }
    public int getKeyCode(int a) {
        switch (a) { case UP: return -1; case DOWN: return -2; case LEFT: return -3; case RIGHT: return -4; case FIRE: return -5;
            case GAME_A: return 49; case GAME_B: return 51; case GAME_C: return 55; case GAME_D: return 57; default: return 0; }
    }
    public String getKeyName(int k) {
        if (k >= 32 && k < 127) return String.valueOf((char) k);
        switch (k) { case -1: return "UP"; case -2: return "DOWN"; case -3: return "LEFT"; case -4: return "RIGHT"; case -5: return "SELECT"; case -6: return "SOFT1"; case -7: return "SOFT2"; default: return "KEY" + k; }
    }
    public void _shown() { dirty = true; showNotify(); }
    public void _hidden() { hideNotify(); }
    public void _tick() { if (dirty) _paintNow(); }
    public void _key(int code, boolean down) {
        if ((code == -6 || code == -7) && commands.size() > 0) {
            if (down) { Command c = (Command) commands.elementAt(code == -6 ? 0 : commands.size() > 1 ? 1 : 0); _fire(c); }
            return;
        }
        if (down) keyPressed(code); else keyReleased(code);
    }
    public void _pointer(int type, int x, int y) {
        if (type == 3) pointerPressed(x, y); else if (type == 4) pointerDragged(x, y); else pointerReleased(x, y);
    }
}
'''
F[P + 'game/GameCanvas.java'] = r'''package javax.microedition.lcdui.game;
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
'''
F[P + 'Font.java'] = r'''package javax.microedition.lcdui;
public final class Font {
    public static final int FACE_SYSTEM = 0, FACE_MONOSPACE = 32, FACE_PROPORTIONAL = 64;
    public static final int STYLE_PLAIN = 0, STYLE_BOLD = 1, STYLE_ITALIC = 2, STYLE_UNDERLINED = 4;
    public static final int SIZE_SMALL = 8, SIZE_MEDIUM = 0, SIZE_LARGE = 16;
    public static final int FONT_STATIC_TEXT = 0, FONT_INPUT_TEXT = 1;
    int face, style, size; private static Font def = new Font(0, 0, 0);
    private Font(int f, int st, int sz) { face = f; style = st; size = sz; }
    public static Font getDefaultFont() { return def; }
    public static Font getFont(int face, int style, int size) { return new Font(face, style, size); }
    public static Font getFont(int spec) { return def; }
    public int getStyle() { return style; }
    public int getSize() { return size; }
    public int getFace() { return face; }
    public boolean isPlain() { return style == 0; }
    public boolean isBold() { return (style & 1) != 0; }
    public boolean isItalic() { return (style & 2) != 0; }
    public boolean isUnderlined() { return (style & 4) != 0; }
    public int getHeight() { return size == SIZE_LARGE ? 14 : 12; }
    public int getBaselinePosition() { return size == SIZE_LARGE ? 11 : 10; }
    public int charWidth(char c) { return size == SIZE_LARGE ? 9 : 8; }
    public int charsWidth(char[] ch, int off, int len) { return len * charWidth('x'); }
    public int stringWidth(String s) { return s.length() * charWidth('x'); }
    public int substringWidth(String s, int off, int len) { return len * charWidth('x'); }
}
'''
F[P + 'Graphics.java'] = r'''package javax.microedition.lcdui;
public class Graphics {
    public static final int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64;
    public static final int SOLID = 0, DOTTED = 1;
    Image img; int color; int tx, ty, cx, cy, cw, ch; Font font; int stroke;
    Graphics(Image i) { img = i; reset(); }
    public static Graphics screen() { Graphics g = new Graphics(null); return g; }
    void reset() { color = 0; tx = ty = 0; cx = cy = 0; cw = img == null ? jvmhost.Host.width() : img.w; ch = img == null ? jvmhost.Host.height() : img.h; font = Font.getDefaultFont(); }
    public void setColor(int rgb) { color = rgb & 0xffffff; }
    public void setColor(int r, int g, int b) { color = ((r & 255) << 16) | ((g & 255) << 8) | (b & 255); }
    public void setGrayScale(int v) { color = (v << 16) | (v << 8) | v; }
    public int getColor() { return color; }
    public int getRedComponent() { return (color >> 16) & 255; }
    public int getGreenComponent() { return (color >> 8) & 255; }
    public int getBlueComponent() { return color & 255; }
    public int getGrayScale() { return (getRedComponent() + getGreenComponent() + getBlueComponent()) / 3; }
    public int getDisplayColor(int c) { return c; }
    public void setFont(Font f) { font = f == null ? Font.getDefaultFont() : f; }
    public Font getFont() { return font; }
    public void setStrokeStyle(int s) { stroke = s; }
    public int getStrokeStyle() { return stroke; }
    public void translate(int x, int y) { tx += x; ty += y; }
    public int getTranslateX() { return tx; }
    public int getTranslateY() { return ty; }
    public void setClip(int x, int y, int w, int h) { cx = x; cy = y; cw = w < 0 ? 0 : w; ch = h < 0 ? 0 : h; }
    public void clipRect(int x, int y, int w, int h) {
        int x2 = cx + cw, y2 = cy + ch; if (x > cx) cx = x; if (y > cy) cy = y; if (x + w < x2) x2 = x + w; if (y + h < y2) y2 = y + h;
        cw = x2 - cx; ch = y2 - cy; if (cw < 0) cw = 0; if (ch < 0) ch = 0;
    }
    public int getClipX() { return cx; }
    public int getClipY() { return cy; }
    public int getClipWidth() { return cw; }
    public int getClipHeight() { return ch; }
    public native void fillRect(int x, int y, int w, int h);
    public void drawRect(int x, int y, int w, int h) { if (w < 0 || h < 0) return; fillRect(x, y, w + 1, 1); if (h > 0) fillRect(x, y + h, w + 1, 1); if (h > 1) { fillRect(x, y + 1, 1, h - 1); if (w > 0) fillRect(x + w, y + 1, 1, h - 1); } }
    public native void drawLine(int x0, int y0, int x1, int y1);
    public native void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);
    public native void fillArc(int x, int y, int w, int h, int sa, int aa);
    public native void drawArc(int x, int y, int w, int h, int sa, int aa);
    public native void fillRoundRect(int x, int y, int w, int h, int aw, int ah);
    public void drawRoundRect(int x, int y, int w, int h, int aw, int ah) { drawRect(x, y, w, h); }
    public native void drawString(String s, int x, int y, int anchor);
    public void drawSubstring(String s, int off, int len, int x, int y, int anchor) { drawString(s.substring(off, off + len), x, y, anchor); }
    public void drawChar(char c, int x, int y, int anchor) { drawString(String.valueOf(c), x, y, anchor); }
    public void drawChars(char[] d, int off, int len, int x, int y, int anchor) { drawString(new String(d, off, len), x, y, anchor); }
    public native void drawImage0(Image im, int x, int y, int sx, int sy, int w, int h, int transform);
    public void drawImage(Image im, int x, int y, int anchor) {
        if (im == null) throw new NullPointerException();
        drawRegion(im, 0, 0, im.w, im.h, 0, x, y, anchor);
    }
    public void drawRegion(Image im, int sx, int sy, int w, int h, int transform, int x, int y, int anchor) {
        if (im == null) throw new NullPointerException();
        int dw = (transform & 4) != 0 ? h : w, dh = (transform & 4) != 0 ? w : h;
        if ((anchor & HCENTER) != 0) x -= dw / 2; else if ((anchor & RIGHT) != 0) x -= dw;
        if ((anchor & VCENTER) != 0) y -= dh / 2; else if ((anchor & BOTTOM) != 0) y -= dh;
        drawImage0(im, x, y, sx, sy, w, h, transform);
    }
    public native void drawRGB(int[] rgb, int off, int scan, int x, int y, int w, int h, boolean alpha);
    public void copyArea(int xs, int ys, int w, int h, int xd, int yd, int anchor) {}
}
'''
F[P + 'Image.java'] = r'''package javax.microedition.lcdui;
import java.io.*;
public class Image {
    int w, h; short[] px; byte[] alpha; boolean mutable; Graphics gfx;
    private Image() {}
    public static Image createImage(int w, int h) {
        if (w <= 0 || h <= 0) throw new IllegalArgumentException();
        Image i = new Image(); i.w = w; i.h = h; i.px = new short[w * h]; i.mutable = true;
        for (int k = 0; k < i.px.length; k++) i.px[k] = (short) 0xffff; return i;
    }
    public static Image createImage(String name) throws IOException {
        InputStream in = Image.class.getResourceAsStream(name); if (in == null) throw new IOException(name);
        return createImage(in);
    }
    public static Image createImage(InputStream in) throws IOException {
        ByteArrayOutputStream bo = new ByteArrayOutputStream(); byte[] b = new byte[512]; int n;
        while ((n = in.read(b, 0, 512)) > 0) bo.write(b, 0, n);
        byte[] d = bo.toByteArray(); return createImage(d, 0, d.length);
    }
    public static Image createImage(byte[] d, int off, int len) {
        Image i = new Image();
        if (!decode0(i, d, off, len)) throw new IllegalArgumentException("bad image");
        return i;
    }
    public static Image createImage(Image src) { Image i = new Image(); i.w = src.w; i.h = src.h; i.px = new short[src.px.length]; System.arraycopy(src.px, 0, i.px, 0, i.px.length); if (src.alpha != null) { i.alpha = new byte[src.alpha.length]; System.arraycopy(src.alpha, 0, i.alpha, 0, i.alpha.length); } return i; }
    public static Image createImage(Image src, int x, int y, int w, int h, int transform) {
        Image i = createImage(w, h); i.mutable = false; Graphics g = new Graphics(i); i.alpha = new byte[w * h];
        g.drawRegion(src, x, y, w, h, 0, 0, 0, Graphics.TOP | Graphics.LEFT);
        i.gfx = null; if (transform != 0) { Image t = createImage((transform & 4) != 0 ? h : w, (transform & 4) != 0 ? w : h); t.alpha = new byte[t.w * t.h]; Graphics tg = new Graphics(t); tg.drawRegion(i, 0, 0, w, h, transform, 0, 0, Graphics.TOP | Graphics.LEFT); t.mutable = false; return t; }
        return i;
    }
    public static Image createRGBImage(int[] rgb, int w, int h, boolean alpha) {
        Image i = new Image(); i.w = w; i.h = h; i.px = new short[w * h]; if (alpha) i.alpha = new byte[w * h];
        for (int k = 0; k < w * h; k++) { int c = rgb[k]; i.px[k] = (short) (((c >> 8) & 0xf800) | ((c >> 5) & 0x7e0) | ((c >> 3) & 0x1f)); if (alpha) i.alpha[k] = (byte) (c >>> 24); }
        return i;
    }
    private static native boolean decode0(Image dst, byte[] d, int off, int len);
    public Graphics getGraphics() { if (!mutable) throw new IllegalStateException(); if (gfx == null) gfx = new Graphics(this); gfx.reset(); return gfx; }
    public boolean _op(int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && (alpha == null || (alpha[y * w + x] & 255) >= 128); }
    public boolean isMutable() { return mutable; }
    public int getWidth() { return w; }
    public int getHeight() { return h; }
    public void getRGB(int[] out, int off, int scan, int x, int y, int w, int h) {
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
            int k = (y + j) * this.w + x + i, c = px[k] & 0xffff;
            int r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3; r |= r >> 5; g |= g >> 6; b |= b >> 5;
            int a = alpha == null ? 255 : alpha[k] & 255; out[off + j * scan + i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}
'''
if __name__ == '__main__':
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rt')
    for p, t in F.items():
        fp = os.path.join(root, p); os.makedirs(os.path.dirname(fp), exist_ok=True); open(fp, 'w', newline='\n', encoding='utf-8').write(t)
    print(len(F), 'files')

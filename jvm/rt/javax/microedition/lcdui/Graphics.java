package javax.microedition.lcdui;
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

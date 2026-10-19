package javax.microedition.lcdui;
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

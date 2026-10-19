package javax.microedition.lcdui.game;
import javax.microedition.lcdui.*;
public class Sprite extends Layer {
    public static final int TRANS_NONE = 0, TRANS_MIRROR_ROT180 = 1, TRANS_MIRROR = 2, TRANS_ROT180 = 3, TRANS_MIRROR_ROT270 = 4, TRANS_ROT90 = 5, TRANS_ROT270 = 6, TRANS_MIRROR_ROT90 = 7;
    private Image img; private int fw, fh, nframes, frame, seqIdx; private int[] seq; private int refX, refY, transform;
    private int cx, cy, cw, ch;
    public Sprite(Image i) { super(i.getWidth(), i.getHeight()); init(i, i.getWidth(), i.getHeight()); }
    public Sprite(Image i, int fw, int fh) {
        super(fw, fh); if (fw < 1 || fh < 1 || i.getWidth() % fw != 0 || i.getHeight() % fh != 0) throw new IllegalArgumentException(); init(i, fw, fh);
    }
    public Sprite(Sprite s) { super(s.width, s.height); init(s.img, s.fw, s.fh); x = s.x; y = s.y; frame = s.frame; seqIdx = s.seqIdx; seq = s.seq; refX = s.refX; refY = s.refY; transform = s.transform; cx = s.cx; cy = s.cy; cw = s.cw; ch = s.ch; width = s.width; height = s.height; visible = s.visible; }
    private void init(Image i, int w, int h) {
        img = i; fw = w; fh = h; nframes = (i.getWidth() / w) * (i.getHeight() / h); seq = new int[nframes]; for (int k = 0; k < nframes; k++) seq[k] = k;
        cx = 0; cy = 0; cw = w; ch = h; width = w; height = h;
    }
    public void setImage(Image i, int w, int h) {
        int oldw = fw, oldh = fh; img = i; fw = w; fh = h; int n = (i.getWidth() / w) * (i.getHeight() / h);
        if (n < nframes) { frame = 0; seqIdx = 0; seq = new int[n]; for (int k = 0; k < n; k++) seq[k] = k; } else if (n != nframes) { seq = new int[n]; for (int k = 0; k < n; k++) seq[k] = k; }
        nframes = n; if (w != oldw || h != oldh) { cx = 0; cy = 0; cw = w; ch = h; } computeSize();
    }
    public void defineReferencePixel(int rx, int ry) { refX = rx; refY = ry; }
    public void setRefPixelPosition(int rx, int ry) { int[] t = trRef(); x = rx - t[0]; y = ry - t[1]; }
    public int getRefPixelX() { return x + trRef()[0]; }
    public int getRefPixelY() { return y + trRef()[1]; }
    public void setFrame(int f) { if (f < 0 || f >= seq.length) throw new IndexOutOfBoundsException(); seqIdx = f; frame = seq[f]; }
    public final int getFrame() { return seqIdx; }
    public int getRawFrameCount() { return nframes; }
    public int getFrameSequenceLength() { return seq.length; }
    public void nextFrame() { seqIdx = (seqIdx + 1) % seq.length; frame = seq[seqIdx]; }
    public void prevFrame() { seqIdx = (seqIdx + seq.length - 1) % seq.length; frame = seq[seqIdx]; }
    public void setFrameSequence(int[] s) {
        if (s == null) { seq = new int[nframes]; for (int k = 0; k < nframes; k++) seq[k] = k; seqIdx = 0; frame = 0; return; }
        if (s.length < 1) throw new IllegalArgumentException(); for (int k = 0; k < s.length; k++) if (s[k] < 0 || s[k] >= nframes) throw new ArrayIndexOutOfBoundsException();
        seq = new int[s.length]; System.arraycopy(s, 0, seq, 0, s.length); seqIdx = 0; frame = seq[0];
    }
    private int[] map(int px, int py, int t) {                 /* point of the untransformed frame -> transformed frame */
        switch (t) { case 1: return new int[] { px, fh - 1 - py }; case 2: return new int[] { fw - 1 - px, py }; case 3: return new int[] { fw - 1 - px, fh - 1 - py };
            case 4: return new int[] { py, px }; case 5: return new int[] { fh - 1 - py, px }; case 6: return new int[] { py, fw - 1 - px }; case 7: return new int[] { fh - 1 - py, fw - 1 - px }; default: return new int[] { px, py }; }
    }
    private int[] trRef() { return map(refX, refY, transform); }
    private void computeSize() { if ((transform & 4) != 0) { width = fh; height = fw; } else { width = fw; height = fh; } }
    public void setTransform(int t) {
        if (t < 0 || t > 7) throw new IllegalArgumentException();
        int[] o = trRef(); int ax = x + o[0], ay = y + o[1]; transform = t; computeSize(); int[] n = trRef(); x = ax - n[0]; y = ay - n[1];
    }
    public void defineCollisionRectangle(int x, int y, int w, int h) { if (w < 0 || h < 0) throw new IllegalArgumentException(); cx = x; cy = y; cw = w; ch = h; }
    public void paint(Graphics g) {
        if (!visible) return; int per = img.getWidth() / fw; int sx = (frame % per) * fw, sy = (frame / per) * fh;
        g.drawRegion(img, sx, sy, fw, fh, transform, x, y, Graphics.TOP | Graphics.LEFT);
    }
    private int[] crect() {                                    /* collision rectangle in layer coordinates: x, y, w, h (after the transform) */
        int[] a = map(cx, cy, transform), b = map(cx + cw - 1, cy + ch - 1, transform);
        int x0 = a[0] < b[0] ? a[0] : b[0], y0 = a[1] < b[1] ? a[1] : b[1], x1 = a[0] > b[0] ? a[0] : b[0], y1 = a[1] > b[1] ? a[1] : b[1];
        if (cw == 0 || ch == 0) return new int[] { x + x0, y + y0, 0, 0 };
        return new int[] { x + x0, y + y0, x1 - x0 + 1, y1 - y0 + 1 };
    }
    private boolean opaque(int lx, int ly) {                   /* is the pixel at layer coordinates (lx,ly) opaque */
        int i = lx - x, j = ly - y; if (i < 0 || j < 0 || i >= width || j >= height) return false; int u, v;
        switch (transform) { case 1: u = i; v = fh - 1 - j; break; case 2: u = fw - 1 - i; v = j; break; case 3: u = fw - 1 - i; v = fh - 1 - j; break;
            case 4: u = j; v = i; break; case 5: u = j; v = fh - 1 - i; break; case 6: u = fw - 1 - j; v = i; break; case 7: u = fw - 1 - j; v = fh - 1 - i; break; default: u = i; v = j; }
        int per = img.getWidth() / fw; return img._op((frame % per) * fw + u, (frame / per) * fh + v);
    }
    public final boolean collidesWith(Sprite s, boolean pixel) {
        if (!visible || !s.visible) return false; int[] a = crect(), b = s.crect();
        int x0 = a[0] > b[0] ? a[0] : b[0], y0 = a[1] > b[1] ? a[1] : b[1], x1 = (a[0] + a[2] < b[0] + b[2] ? a[0] + a[2] : b[0] + b[2]), y1 = (a[1] + a[3] < b[1] + b[3] ? a[1] + a[3] : b[1] + b[3]);
        if (x0 >= x1 || y0 >= y1) return false; if (!pixel) return true;
        for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) if (opaque(px, py) && s.opaque(px, py)) return true;
        return false;
    }
    public final boolean collidesWith(Image im, int ix, int iy, boolean pixel) {
        if (!visible) return false; int[] a = crect(); int x0 = a[0] > ix ? a[0] : ix, y0 = a[1] > iy ? a[1] : iy, x1 = a[0] + a[2] < ix + im.getWidth() ? a[0] + a[2] : ix + im.getWidth(), y1 = a[1] + a[3] < iy + im.getHeight() ? a[1] + a[3] : iy + im.getHeight();
        if (x0 >= x1 || y0 >= y1) return false; if (!pixel) return true;
        for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) if (opaque(px, py) && im._op(px - ix, py - iy)) return true;
        return false;
    }
    public final boolean collidesWith(TiledLayer t, boolean pixel) {
        if (!visible || !t.isVisible()) return false; int[] a = crect(); int tw = t.getCellWidth(), th = t.getCellHeight();
        int c0 = (a[0] - t.getX()) / tw, c1 = (a[0] + a[2] - 1 - t.getX()) / tw, r0 = (a[1] - t.getY()) / th, r1 = (a[1] + a[3] - 1 - t.getY()) / th;
        if (a[0] - t.getX() < 0) c0 = (a[0] - t.getX() - tw + 1) / tw; if (a[1] - t.getY() < 0) r0 = (a[1] - t.getY() - th + 1) / th;
        for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
            if (c < 0 || r < 0 || c >= t.getColumns() || r >= t.getRows() || t.getCell(c, r) == 0) continue;
            if (!pixel) return true;
            int tx = t.getX() + c * tw, ty = t.getY() + r * th;
            for (int py = (a[1] > ty ? a[1] : ty); py < (a[1] + a[3] < ty + th ? a[1] + a[3] : ty + th); py++)
                for (int px = (a[0] > tx ? a[0] : tx); px < (a[0] + a[2] < tx + tw ? a[0] + a[2] : tx + tw); px++) if (opaque(px, py) && t.opaqueAt(c, r, px - tx, py - ty)) return true;
        }
        return false;
    }
}

package javax.microedition.lcdui.game;
import javax.microedition.lcdui.*;
public class TiledLayer extends Layer {
    private int cols, rows, tw, th, ntiles; private Image img; private int[] cells; private int[] anim; private int nanim;
    public TiledLayer(int c, int r, Image i, int tw, int th) {
        super(c * tw, r * th); if (c < 1 || r < 1 || tw < 1 || th < 1 || i.getWidth() % tw != 0 || i.getHeight() % th != 0) throw new IllegalArgumentException();
        cols = c; rows = r; cells = new int[c * r]; anim = new int[4]; setStaticTileSet(i, tw, th);
    }
    public void setStaticTileSet(Image i, int tw, int th) {
        img = i; this.tw = tw; this.th = th; int n = (i.getWidth() / tw) * (i.getHeight() / th);
        if (n < ntiles) { for (int k = 0; k < cells.length; k++) cells[k] = 0; anim = new int[4]; nanim = 0; } ntiles = n; width = cols * tw; height = rows * th;
    }
    public int createAnimatedTile(int st) { if (st < 0 || st > ntiles) throw new IndexOutOfBoundsException(); if (nanim == anim.length) { int[] a = new int[nanim * 2]; System.arraycopy(anim, 0, a, 0, nanim); anim = a; } anim[nanim++] = st; return -nanim; }
    public void setAnimatedTile(int idx, int st) { anim[-idx - 1] = st; }
    public int getAnimatedTile(int idx) { return anim[-idx - 1]; }
    public void setCell(int c, int r, int t) { if (c < 0 || r < 0 || c >= cols || r >= rows) throw new IndexOutOfBoundsException(); cells[r * cols + c] = t; }
    public int getCell(int c, int r) { if (c < 0 || r < 0 || c >= cols || r >= rows) throw new IndexOutOfBoundsException(); return cells[r * cols + c]; }
    public void fillCells(int c, int r, int w, int h, int t) { for (int j = r; j < r + h; j++) for (int i = c; i < c + w; i++) setCell(i, j, t); }
    public final int getCellWidth() { return tw; }
    public final int getCellHeight() { return th; }
    public final int getColumns() { return cols; }
    public final int getRows() { return rows; }
    boolean opaqueAt(int c, int r, int px, int py) {
        int t = cells[r * cols + c]; if (t < 0) t = anim[-t - 1]; if (t == 0) return false; int per = img.getWidth() / tw; t--; return img._op((t % per) * tw + px, (t / per) * th + py);
    }
    public final void paint(Graphics g) {
        if (!visible) return; int per = img.getWidth() / tw;
        int cx = g.getClipX(), cy = g.getClipY(), cw = g.getClipWidth(), ch = g.getClipHeight();
        int c0 = (cx - x) / tw, c1 = (cx + cw - 1 - x) / tw, r0 = (cy - y) / th, r1 = (cy + ch - 1 - y) / th; if (cx - x < 0) c0 = 0; if (cy - y < 0) r0 = 0;
        if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c1 >= cols) c1 = cols - 1; if (r1 >= rows) r1 = rows - 1;
        for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
            int t = cells[r * cols + c]; if (t < 0) t = anim[-t - 1]; if (t == 0) continue; t--;
            g.drawRegion(img, (t % per) * tw, (t / per) * th, tw, th, 0, x + c * tw, y + r * th, Graphics.TOP | Graphics.LEFT);
        }
    }
}

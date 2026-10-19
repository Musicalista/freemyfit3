package javax.microedition.lcdui;
public class ImageItem extends Item {
    Image img; String alt;
    public ImageItem(String label, Image img, int layout, String alt) { this.label = label; this.img = img; this.layout = layout; this.alt = alt; }
    public ImageItem(String label, Image img, int layout, String alt, int app) { this(label, img, layout, alt); }
    public Image getImage() { return img; } public void setImage(Image i) { img = i; touch(); } public String getAltText() { return alt; } public void setAltText(String s) { alt = s; }
    int height(int w) { return labelH() + (img == null ? 0 : img.getHeight() + 2); }
    void paint(Graphics g, int w, boolean f) { paintLabel(g); if (img != null) g.drawImage(img, (layout & 3) == 3 ? (w - img.getWidth()) / 2 : 4, labelH() + 1, Graphics.TOP | Graphics.LEFT); }
}

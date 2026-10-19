package javax.microedition.lcdui;
public class Spacer extends Item { int mw, mh; public Spacer(int w, int h) { mw = w; mh = h; } public void setMinimumSize(int w, int h) { mw = w; mh = h; } int height(int w) { return mh; } void paint(Graphics g, int w, boolean f) {} }

package com.nokia.mid.ui;
import javax.microedition.lcdui.*;
public class DirectUtils {
    public static Image createImage(int w, int h, int argb) { Image i = Image.createImage(w, h); Graphics g = i.getGraphics(); g.setColor(argb & 0xffffff); g.fillRect(0, 0, w, h); return i; }
}

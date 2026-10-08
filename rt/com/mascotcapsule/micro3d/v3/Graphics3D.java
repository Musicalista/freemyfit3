package com.mascotcapsule.micro3d.v3;
import javax.microedition.lcdui.Graphics;
public class Graphics3D {
    public Graphics3D() {}
    public native void bind(Graphics g);
    public void release(Graphics g) {}
    public void flush() {}
    public native void renderPrimitives(Texture t, int x, int y, FigureLayout fl, Effect3D ef, int command, int num, int[] coords, int[] normals, int[] texCoords, int[] colors);
}

package jvmhost;
public final class Host {
    public static native byte[] res(String name);
    public static native void sleep(long ms);
    public static native void spawn(Object thread);
    public static native int pollKey();
    public static native int width();
    public static native int height();
    public static native void present();
    public static native void setColor(int rgb);
    public static native void fillRect(int x, int y, int w, int h);
    public static native void drawLine(int x0, int y0, int x1, int y1);
    public static native void drawString(String s, int x, int y, int anchor);
}

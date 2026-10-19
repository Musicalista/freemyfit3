package jvmhost;
public final class Host {
    public static native byte[] res(String name);
    public static native int pollEvent();
    public static native int pollArg();
    public static native int width();
    public static native int height();
    public static native void present(Object img);
    public static native void log(String s);
    public static native String trace();
    public static native void vibrate(int ms);
    public static native void quit();
    public static native byte[] rmsRead(String name);
    public static native boolean rmsWrite(String name, byte[] data, int len);
    public static native boolean rmsDelete(String name);
    public static native int rmsSize(String name);
}

package java.lang;
public final class System {
    public static final java.io.PrintStream out = new java.io.PrintStream(), err = out;
    public static native void arraycopy(Object src, int sp, Object dst, int dp, int len);
    public static native long currentTimeMillis();
    public static native int identityHashCode(Object o);
    public static native void gc();
    public static void exit(int code) { javax.microedition.midlet.MIDlet.quit(); }
    public static String getProperty(String k) {
        if (k.equals("microedition.platform")) return "Fit3";
        if (k.equals("microedition.encoding")) return "ISO-8859-1";
        if (k.equals("microedition.configuration")) return "CLDC-1.1";
        if (k.equals("microedition.profiles")) return "MIDP-2.0";
        if (k.equals("microedition.locale")) return "pt-BR";
        return null;
    }
}

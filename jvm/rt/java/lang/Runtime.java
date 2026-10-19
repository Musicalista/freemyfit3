package java.lang;
public class Runtime {
    private static Runtime rt = new Runtime();
    private Runtime() {}
    public static Runtime getRuntime() { return rt; }
    public native long freeMemory();
    public native long totalMemory();
    public void gc() { System.gc(); }
    public void exit(int c) { System.exit(c); }
}

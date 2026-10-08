package java.lang;
public final class Math {
    public static native double sin(double a);
    public static native double cos(double a);
    public static native double tan(double a);
    public static native double sqrt(double a);
    public static native double floor(double a);
    public static native double ceil(double a);
    public static int abs(int a) { return a < 0 ? -a : a; }
    public static long abs(long a) { return a < 0 ? -a : a; }
    public static float abs(float a) { return a <= 0f ? 0f - a : a; }
    public static double abs(double a) { return a <= 0.0 ? 0.0 - a : a; }
    public static int max(int a, int b) { return a >= b ? a : b; }
    public static int min(int a, int b) { return a <= b ? a : b; }
    public static long max(long a, long b) { return a >= b ? a : b; }
    public static long min(long a, long b) { return a <= b ? a : b; }
    public static float max(float a, float b) { return a >= b ? a : b; }
    public static float min(float a, float b) { return a <= b ? a : b; }
    public static double max(double a, double b) { return a >= b ? a : b; }
    public static double min(double a, double b) { return a <= b ? a : b; }
    public static int round(float a) { return (int) floor(a + 0.5f); }
    public static long round(double a) { return (long) floor(a + 0.5); }
}

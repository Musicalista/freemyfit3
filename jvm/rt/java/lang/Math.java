package java.lang;
public final class Math {
    public static final double PI = 3.14159265358979323846, E = 2.7182818284590452354;
    public static native double sin(double a);
    public static native double cos(double a);
    public static double tan(double a) { return sin(a) / cos(a); }
    public static native double sqrt(double a);
    public static native double floor(double a);
    public static native double ceil(double a);
    public static native double atan(double a);
    public static double atan2(double y, double x) {
        if (x > 0) return atan(y / x);
        if (x < 0) return y >= 0 ? atan(y / x) + PI : atan(y / x) - PI;
        return y > 0 ? PI / 2 : y < 0 ? -PI / 2 : 0;
    }
    public static double asin(double x) { if (x >= 1) return PI / 2; if (x <= -1) return -PI / 2; return atan(x / sqrt(1 - x * x)); }
    public static double acos(double x) { return PI / 2 - asin(x); }
    public static double exp(double x) {
        if (x != x) return x; if (x > 709) return 1.0 / 0.0; if (x < -745) return 0;
        double k = floor(x / 0.6931471805599453 + 0.5), r = x - k * 0.6931471805599453;
        double e = 1 + r * (1 + r / 2 * (1 + r / 3 * (1 + r / 4 * (1 + r / 5 * (1 + r / 6 * (1 + r / 7 * (1 + r / 8 * (1 + r / 9 * (1 + r / 10 * (1 + r / 11))))))))));
        int n = (int) k; while (n > 0) { e *= 2; n--; } while (n < 0) { e /= 2; n++; } return e;
    }
    public static double log(double x) {
        if (!(x > 0)) return x == 0 ? -1.0 / 0.0 : 0.0 / 0.0; if (x == 1.0 / 0.0) return x;
        int e = 0; while (x >= 2) { x /= 2; e++; } while (x < 1) { x *= 2; e--; } if (x > 1.4142135623730951) { x /= 2; e++; }
        double t = (x - 1) / (x + 1), t2 = t * t;
        return 2 * t * (1 + t2 * (1.0 / 3 + t2 * (1.0 / 5 + t2 * (1.0 / 7 + t2 * (1.0 / 9 + t2 * (1.0 / 11 + t2 * (1.0 / 13 + t2 * (1.0 / 15)))))))) + e * 0.6931471805599453;
    }
    public static double pow(double a, double b) {
        if (b == 0) return 1; if (a != a || b != b) return a + b;
        if (b == floor(b) && abs(b) < 64) { double r = 1, base = a; long n = (long) abs(b); while (n != 0) { if ((n & 1) != 0) r *= base; base *= base; n >>= 1; } return b < 0 ? 1 / r : r; }
        if (a < 0) return 0.0 / 0.0; if (a == 0) return b > 0 ? 0 : 1.0 / 0.0; return exp(b * log(a));
    }
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
    public static double toRadians(double d) { return d / 180.0 * PI; }
    public static double toDegrees(double r) { return r * 180.0 / PI; }
}

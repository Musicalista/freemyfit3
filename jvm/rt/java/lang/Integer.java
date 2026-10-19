package java.lang;
public final class Integer {
    public static final int MIN_VALUE = 0x80000000, MAX_VALUE = 0x7fffffff;
    private int v;
    public Integer(int v) { this.v = v; }
    public int intValue() { return v; }
    public long longValue() { return v; }
    public byte byteValue() { return (byte) v; }
    public short shortValue() { return (short) v; }
    public float floatValue() { return v; }
    public double doubleValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Integer && ((Integer) o).v == v; }
    public String toString() { return toString(v, 10); }
    public static Integer valueOf(int i) { return new Integer(i); }
    public static Integer valueOf(String s) { return new Integer(parseInt(s, 10)); }
    public static Integer valueOf(String s, int r) { return new Integer(parseInt(s, r)); }
    public static String toString(int i) { return toString(i, 10); }
    public static String toString(int i, int radix) {
        if (radix < 2 || radix > 36) radix = 10;
        if (i == 0) return "0";
        char[] t = new char[34]; int p = 34; boolean neg = i < 0; if (!neg) i = -i;
        while (i != 0) { int d = -(i % radix); t[--p] = (char) (d < 10 ? '0' + d : 'a' + d - 10); i /= radix; }
        if (neg) t[--p] = '-';
        return new String(t, p, 34 - p);
    }
    public static String toHexString(int i) { return unsigned(i, 4); }
    public static String toOctalString(int i) { return unsigned(i, 3); }
    public static String toBinaryString(int i) { return unsigned(i, 1); }
    private static String unsigned(int i, int shift) {
        if (i == 0) return "0"; char[] t = new char[32]; int p = 32, mask = (1 << shift) - 1;
        while (i != 0) { int d = i & mask; t[--p] = (char) (d < 10 ? '0' + d : 'a' + d - 10); i >>>= shift; }
        return new String(t, p, 32 - p);
    }
    public static int parseInt(String s) throws NumberFormatException { return parseInt(s, 10); }
    public static int parseInt(String s, int radix) throws NumberFormatException {
        if (s == null || s.length() == 0) throw new NumberFormatException(s);
        int i = 0, n = s.length(); boolean neg = false;
        if (s.charAt(0) == '-') { neg = true; i = 1; } else if (s.charAt(0) == '+') i = 1;
        if (i == n) throw new NumberFormatException(s);
        long r = 0;
        for (; i < n; i++) {
            int d = Character.digit(s.charAt(i), radix);
            if (d < 0) throw new NumberFormatException(s);
            r = r * radix + d; if (r > 2147483648L) throw new NumberFormatException(s);
        }
        if (neg) r = -r; if (r > 2147483647L || r < -2147483648L) throw new NumberFormatException(s);
        return (int) r;
    }
}

package java.lang;
public final class Long {
    public static final long MIN_VALUE = 0x8000000000000000L, MAX_VALUE = 0x7fffffffffffffffL;
    private long v;
    public Long(long v) { this.v = v; }
    public long longValue() { return v; }
    public int intValue() { return (int) v; }
    public float floatValue() { return v; }
    public double doubleValue() { return v; }
    public int hashCode() { return (int) (v ^ (v >>> 32)); }
    public boolean equals(Object o) { return o instanceof Long && ((Long) o).v == v; }
    public String toString() { return toString(v); }
    public static Long valueOf(long l) { return new Long(l); }
    public static String toString(long l) { return toString(l, 10); }
    public static String toString(long i, int radix) {
        if (radix < 2 || radix > 36) radix = 10;
        if (i == 0) return "0";
        char[] t = new char[66]; int p = 66; boolean neg = i < 0; if (!neg) i = -i;
        while (i != 0) { int d = (int) -(i % radix); t[--p] = (char) (d < 10 ? '0' + d : 'a' + d - 10); i /= radix; }
        if (neg) t[--p] = '-';
        return new String(t, p, 66 - p);
    }
    public static String toHexString(long l) { if (l == 0) return "0"; char[] t = new char[16]; int p = 16; while (l != 0) { int d = (int) (l & 15); t[--p] = (char) (d < 10 ? '0' + d : 'a' + d - 10); l >>>= 4; } return new String(t, p, 16 - p); }
    public static long parseLong(String s) throws NumberFormatException { return parseLong(s, 10); }
    public static long parseLong(String s, int radix) throws NumberFormatException {
        if (s == null || s.length() == 0) throw new NumberFormatException(s);
        int i = 0, n = s.length(); boolean neg = false;
        if (s.charAt(0) == '-') { neg = true; i = 1; } else if (s.charAt(0) == '+') i = 1;
        if (i == n) throw new NumberFormatException(s);
        long r = 0;
        for (; i < n; i++) { int d = Character.digit(s.charAt(i), radix); if (d < 0) throw new NumberFormatException(s); r = r * radix - d; }
        return neg ? r : -r;
    }
}

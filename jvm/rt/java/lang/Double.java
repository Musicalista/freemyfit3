package java.lang;
public final class Double {
    public static final double MAX_VALUE = 1.7976931348623157E308, MIN_VALUE = 4.9E-324, POSITIVE_INFINITY = 1.0 / 0.0, NEGATIVE_INFINITY = -1.0 / 0.0, NaN = 0.0 / 0.0;
    private double v;
    public Double(double d) { v = d; }
    public double doubleValue() { return v; }
    public float floatValue() { return (float) v; }
    public int intValue() { return (int) v; }
    public long longValue() { return (long) v; }
    public int hashCode() { long b = doubleToLongBits(v); return (int) (b ^ (b >>> 32)); }
    public boolean equals(Object o) { return o instanceof Double && ((Double) o).v == v; }
    public String toString() { return toString(v); }
    public boolean isNaN() { return v != v; }
    public static boolean isNaN(double d) { return d != d; }
    public static native long doubleToLongBits(double d);
    public static native double longBitsToDouble(long l);
    public static Double valueOf(String s) { return new Double(parseDouble(s)); }
    public static double parseDouble(String s) throws NumberFormatException {
        if (s == null) throw new NullPointerException();
        s = s.trim(); int i = 0, n = s.length(); if (n == 0) throw new NumberFormatException(s);
        boolean neg = false; if (s.charAt(0) == '-') { neg = true; i++; } else if (s.charAt(0) == '+') i++;
        double r = 0; boolean any = false;
        while (i < n && Character.isDigit(s.charAt(i))) { r = r * 10 + (s.charAt(i) - '0'); i++; any = true; }
        if (i < n && s.charAt(i) == '.') { i++; double f = 0.1; while (i < n && Character.isDigit(s.charAt(i))) { r += (s.charAt(i) - '0') * f; f /= 10; i++; any = true; } }
        if (!any) throw new NumberFormatException(s);
        if (i < n && (s.charAt(i) == 'e' || s.charAt(i) == 'E')) {
            i++; boolean en = false; if (i < n && s.charAt(i) == '-') { en = true; i++; } else if (i < n && s.charAt(i) == '+') i++;
            int e = 0; while (i < n && Character.isDigit(s.charAt(i))) { e = e * 10 + (s.charAt(i) - '0'); i++; }
            while (e-- > 0) r = en ? r / 10 : r * 10;
        }
        if (i < n && (s.charAt(i) == 'f' || s.charAt(i) == 'F' || s.charAt(i) == 'd' || s.charAt(i) == 'D')) i++;
        if (i != n) throw new NumberFormatException(s);
        return neg ? -r : r;
    }
    public static String toString(double d) {
        if (d != d) return "NaN";
        if (d == 1.0 / 0.0) return "Infinity"; if (d == -1.0 / 0.0) return "-Infinity";
        boolean neg = d < 0; if (neg) d = -d;
        StringBuffer sb = new StringBuffer(); if (neg) sb.append('-');
        if (d >= 1e7 || (d < 1e-3 && d != 0)) {
            int e = 0; while (d >= 10) { d /= 10; e++; } while (d < 1) { d *= 10; e--; }
            sb.append(plain(d)); sb.append('E'); sb.append(e); return sb.toString();
        }
        sb.append(plain(d)); return sb.toString();
    }
    private static String plain(double d) {
        long ip = (long) d; double fp = d - ip; StringBuffer sb = new StringBuffer(); sb.append(ip); sb.append('.');
        long scaled = (long) (fp * 1000000.0 + 0.5);
        if (scaled >= 1000000) { return plain(ip + 1.0); }
        String f = Long.toString(scaled); for (int i = f.length(); i < 6; i++) sb.append('0'); sb.append(f);
        int len = sb.length(); while (len > 2 && sb.charAt(len - 1) == '0' && sb.charAt(len - 2) != '.') len--; sb.setLength(len);
        return sb.toString();
    }
}

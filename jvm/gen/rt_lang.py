# writes the java.lang part of the runtime sources into ../rt
import os
F = {}
F['java/lang/String.java'] = r'''package java.lang;
public final class String {
    char[] value; int hash;
    public String() { value = new char[0]; }
    public String(String s) { value = s.value; hash = s.hash; }
    public String(char[] c) { value = new char[c.length]; System.arraycopy(c, 0, value, 0, c.length); }
    public String(char[] c, int off, int n) {
        if (off < 0 || n < 0 || off + n > c.length) throw new StringIndexOutOfBoundsException();
        value = new char[n]; System.arraycopy(c, off, value, 0, n);
    }
    public String(byte[] b) { value = decode(b, 0, b.length); }
    public String(byte[] b, int off, int n) { value = decode(b, off, n); }
    public String(byte[] b, String enc) throws java.io.UnsupportedEncodingException { value = decode(b, 0, b.length); }
    public String(byte[] b, int off, int n, String enc) throws java.io.UnsupportedEncodingException { value = decode(b, off, n); }
    public String(StringBuffer sb) { value = sb.toCharArray(); }
    private static char[] decode(byte[] b, int off, int n) {
        char[] t = new char[n]; int o = 0, i = off, e = off + n;
        while (i < e) {
            int c = b[i++] & 0xff;
            if (c < 0x80) t[o++] = (char) c;
            else if (c >= 0xc2 && c < 0xe0 && i < e && (b[i] & 0xc0) == 0x80) { t[o++] = (char) (((c & 0x1f) << 6) | (b[i++] & 0x3f)); }
            else if (c >= 0xe0 && c < 0xf0 && i + 1 < e && (b[i] & 0xc0) == 0x80 && (b[i + 1] & 0xc0) == 0x80) { t[o++] = (char) (((c & 0x0f) << 12) | ((b[i] & 0x3f) << 6) | (b[i + 1] & 0x3f)); i += 2; }
            else t[o++] = (char) c;
        }
        char[] r = new char[o]; System.arraycopy(t, 0, r, 0, o); return r;
    }
    public native int length();
    public native char charAt(int i);
    public native boolean equals(Object o);
    public native int hashCode();
    public native int compareTo(String o);
    private native int indexOfC(int ch, int from);
    private native int indexOfS(String s, int from);
    public int indexOf(int ch) { return indexOfC(ch, 0); }
    public int indexOf(int ch, int from) { return indexOfC(ch, from); }
    public int indexOf(String s) { return indexOfS(s, 0); }
    public int indexOf(String s, int from) { return indexOfS(s, from); }
    public int lastIndexOf(int ch) { return lastIndexOf(ch, value.length - 1); }
    public int lastIndexOf(int ch, int from) { if (from >= value.length) from = value.length - 1; for (int i = from; i >= 0; i--) if (value[i] == ch) return i; return -1; }
    public boolean equalsIgnoreCase(String o) {
        if (o == null || o.value.length != value.length) return false;
        for (int i = 0; i < value.length; i++) { char a = value[i], b = o.value[i]; if (a != b && Character.toUpperCase(a) != Character.toUpperCase(b) && Character.toLowerCase(a) != Character.toLowerCase(b)) return false; }
        return true;
    }
    public String substring(int b) { return substring(b, value.length); }
    public String substring(int b, int e) {
        if (b < 0 || e > value.length || b > e) throw new StringIndexOutOfBoundsException();
        if (b == 0 && e == value.length) return this;
        String s = new String(); s.value = new char[e - b]; System.arraycopy(value, b, s.value, 0, e - b); return s;
    }
    public String concat(String o) {
        if (o.value.length == 0) return this;
        String s = new String(); s.value = new char[value.length + o.value.length];
        System.arraycopy(value, 0, s.value, 0, value.length); System.arraycopy(o.value, 0, s.value, value.length, o.value.length); return s;
    }
    public boolean startsWith(String p) { return startsWith(p, 0); }
    public boolean startsWith(String p, int off) {
        if (off < 0 || off + p.value.length > value.length) return false;
        for (int i = 0; i < p.value.length; i++) if (value[off + i] != p.value[i]) return false; return true;
    }
    public boolean endsWith(String p) { return startsWith(p, value.length - p.value.length); }
    public boolean regionMatches(boolean ic, int to, String o, int oo, int len) {
        if (to < 0 || oo < 0 || to + len > value.length || oo + len > o.value.length) return false;
        for (int i = 0; i < len; i++) { char a = value[to + i], b = o.value[oo + i]; if (a != b && !(ic && Character.toUpperCase(a) == Character.toUpperCase(b))) return false; }
        return true;
    }
    public String trim() {
        int b = 0, e = value.length;
        while (b < e && value[b] <= ' ') b++;
        while (e > b && value[e - 1] <= ' ') e--;
        return substring(b, e);
    }
    public String toLowerCase() { char[] r = new char[value.length]; for (int i = 0; i < r.length; i++) r[i] = Character.toLowerCase(value[i]); return new String(r); }
    public String toUpperCase() { char[] r = new char[value.length]; for (int i = 0; i < r.length; i++) r[i] = Character.toUpperCase(value[i]); return new String(r); }
    public String replace(char a, char b) { char[] r = new char[value.length]; for (int i = 0; i < r.length; i++) r[i] = value[i] == a ? b : value[i]; return new String(r); }
    public char[] toCharArray() { char[] r = new char[value.length]; System.arraycopy(value, 0, r, 0, r.length); return r; }
    public void getChars(int sb, int se, char[] dst, int db) { System.arraycopy(value, sb, dst, db, se - sb); }
    public byte[] getBytes() {
        int n = 0; for (int i = 0; i < value.length; i++) { char c = value[i]; n += c < 0x80 ? 1 : c < 0x800 ? 2 : 3; }
        byte[] r = new byte[n]; int o = 0;
        for (int i = 0; i < value.length; i++) { char c = value[i];
            if (c < 0x80) r[o++] = (byte) c;
            else if (c < 0x800) { r[o++] = (byte) (0xc0 | (c >> 6)); r[o++] = (byte) (0x80 | (c & 0x3f)); }
            else { r[o++] = (byte) (0xe0 | (c >> 12)); r[o++] = (byte) (0x80 | ((c >> 6) & 0x3f)); r[o++] = (byte) (0x80 | (c & 0x3f)); } }
        return r;
    }
    public byte[] getBytes(String enc) throws java.io.UnsupportedEncodingException { return getBytes(); }
    public String intern() { return this; }
    public String toString() { return this; }
    public static String valueOf(Object o) { return o == null ? "null" : o.toString(); }
    public static String valueOf(char[] c) { return new String(c); }
    public static String valueOf(char[] c, int o, int n) { return new String(c, o, n); }
    public static String valueOf(boolean b) { return b ? "true" : "false"; }
    public static String valueOf(char c) { char[] a = new char[1]; a[0] = c; return new String(a); }
    public static String valueOf(int i) { return Integer.toString(i); }
    public static String valueOf(long l) { return Long.toString(l); }
    public static String valueOf(float f) { return Float.toString(f); }
    public static String valueOf(double d) { return Double.toString(d); }
}
'''
F['java/lang/StringBuffer.java'] = r'''package java.lang;
public final class StringBuffer {
    private char[] buf; private int count;
    public StringBuffer() { buf = new char[16]; }
    public StringBuffer(int n) { buf = new char[n < 1 ? 1 : n]; }
    public StringBuffer(String s) { buf = new char[s.length() + 16]; append(s); }
    private void grow(int need) { if (need > buf.length) { int n = buf.length * 2 + 2; if (n < need) n = need; char[] b = new char[n]; System.arraycopy(buf, 0, b, 0, count); buf = b; } }
    public int length() { return count; }
    public int capacity() { return buf.length; }
    public void ensureCapacity(int n) { grow(n); }
    public char charAt(int i) { if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException(); return buf[i]; }
    public void setCharAt(int i, char c) { if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException(); buf[i] = c; }
    public void setLength(int n) { if (n < 0) throw new StringIndexOutOfBoundsException(); grow(n); for (int i = count; i < n; i++) buf[i] = 0; count = n; }
    public StringBuffer append(String s) { if (s == null) s = "null"; int n = s.length(); grow(count + n); s.getChars(0, n, buf, count); count += n; return this; }
    public StringBuffer append(Object o) { return append(String.valueOf(o)); }
    public StringBuffer append(char c) { grow(count + 1); buf[count++] = c; return this; }
    public StringBuffer append(char[] c) { grow(count + c.length); System.arraycopy(c, 0, buf, count, c.length); count += c.length; return this; }
    public StringBuffer append(char[] c, int o, int n) { grow(count + n); System.arraycopy(c, o, buf, count, n); count += n; return this; }
    public StringBuffer append(boolean b) { return append(b ? "true" : "false"); }
    public StringBuffer append(int i) { return append(Integer.toString(i)); }
    public StringBuffer append(long l) { return append(Long.toString(l)); }
    public StringBuffer append(float f) { return append(Float.toString(f)); }
    public StringBuffer append(double d) { return append(Double.toString(d)); }
    public StringBuffer insert(int at, String s) {
        if (at < 0 || at > count) throw new StringIndexOutOfBoundsException(); if (s == null) s = "null";
        int n = s.length(); grow(count + n); System.arraycopy(buf, at, buf, at + n, count - at); s.getChars(0, n, buf, at); count += n; return this;
    }
    public StringBuffer insert(int at, char c) { return insert(at, String.valueOf(c)); }
    public StringBuffer insert(int at, int i) { return insert(at, Integer.toString(i)); }
    public StringBuffer insert(int at, Object o) { return insert(at, String.valueOf(o)); }
    public StringBuffer delete(int s, int e) {
        if (e > count) e = count; if (s < 0 || s > e) throw new StringIndexOutOfBoundsException();
        System.arraycopy(buf, e, buf, s, count - e); count -= e - s; return this;
    }
    public StringBuffer deleteCharAt(int i) { if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException(); return delete(i, i + 1); }
    public StringBuffer reverse() { for (int i = 0, j = count - 1; i < j; i++, j--) { char t = buf[i]; buf[i] = buf[j]; buf[j] = t; } return this; }
    public void getChars(int sb, int se, char[] dst, int db) { System.arraycopy(buf, sb, dst, db, se - sb); }
    char[] toCharArray() { char[] r = new char[count]; System.arraycopy(buf, 0, r, 0, count); return r; }
    public String toString() { return new String(buf, 0, count); }
}
'''
F['java/lang/Number.java'] = 'package java.lang;\npublic abstract class Number { }\n'
F['java/lang/Integer.java'] = r'''package java.lang;
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
'''
F['java/lang/Long.java'] = r'''package java.lang;
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
'''
F['java/lang/Boolean.java'] = r'''package java.lang;
public final class Boolean {
    public static final Boolean TRUE = new Boolean(true), FALSE = new Boolean(false);
    private boolean v;
    public Boolean(boolean b) { v = b; }
    public boolean booleanValue() { return v; }
    public int hashCode() { return v ? 1231 : 1237; }
    public boolean equals(Object o) { return o instanceof Boolean && ((Boolean) o).v == v; }
    public String toString() { return v ? "true" : "false"; }
    public static Boolean valueOf(boolean b) { return b ? TRUE : FALSE; }
}
'''
F['java/lang/Byte.java'] = r'''package java.lang;
public final class Byte {
    public static final byte MIN_VALUE = -128, MAX_VALUE = 127;
    private byte v;
    public Byte(byte b) { v = b; }
    public byte byteValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Byte && ((Byte) o).v == v; }
    public String toString() { return Integer.toString(v); }
    public static byte parseByte(String s) { int i = Integer.parseInt(s); if (i < -128 || i > 127) throw new NumberFormatException(s); return (byte) i; }
}
'''
F['java/lang/Short.java'] = r'''package java.lang;
public final class Short {
    public static final short MIN_VALUE = -32768, MAX_VALUE = 32767;
    private short v;
    public Short(short b) { v = b; }
    public short shortValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Short && ((Short) o).v == v; }
    public String toString() { return Integer.toString(v); }
    public static short parseShort(String s) { int i = Integer.parseInt(s); if (i < -32768 || i > 32767) throw new NumberFormatException(s); return (short) i; }
}
'''
F['java/lang/Character.java'] = r'''package java.lang;
public final class Character {
    public static final char MIN_VALUE = 0, MAX_VALUE = (char) 0xffff;
    public static final int MIN_RADIX = 2, MAX_RADIX = 36;
    private char v;
    public Character(char c) { v = c; }
    public char charValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Character && ((Character) o).v == v; }
    public String toString() { return String.valueOf(v); }
    public static boolean isDigit(char c) { return c >= '0' && c <= '9'; }
    public static boolean isLowerCase(char c) { return (c >= 'a' && c <= 'z') || (c >= 0xdf && c <= 0xff && c != 0xf7); }
    public static boolean isUpperCase(char c) { return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7); }
    public static boolean isLetter(char c) { return isLowerCase(c) || isUpperCase(c) || c > 0xff; }
    public static boolean isLetterOrDigit(char c) { return isLetter(c) || isDigit(c); }
    public static boolean isSpaceChar(char c) { return c == ' ' || c == 0xa0; }
    public static boolean isWhitespace(char c) { return c == ' ' || (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x1f); }
    public static char toLowerCase(char c) { return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7) ? (char) (c + 32) : c; }
    public static char toUpperCase(char c) { return (c >= 'a' && c <= 'z') || (c >= 0xe0 && c <= 0xfe && c != 0xf7) ? (char) (c - 32) : c; }
    public static int digit(char c, int radix) {
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 : c >= 'A' && c <= 'Z' ? c - 'A' + 10 : -1;
        return d >= 0 && d < radix ? d : -1;
    }
    public static char forDigit(int d, int radix) { return (char) (d < 10 ? '0' + d : 'a' + d - 10); }
}
'''
F['java/lang/Float.java'] = r'''package java.lang;
public final class Float {
    public static final float MAX_VALUE = 3.4028235E38f, MIN_VALUE = 1.4E-45f, POSITIVE_INFINITY = 1.0f / 0.0f, NEGATIVE_INFINITY = -1.0f / 0.0f, NaN = 0.0f / 0.0f;
    private float v;
    public Float(float f) { v = f; }
    public float floatValue() { return v; }
    public int intValue() { return (int) v; }
    public double doubleValue() { return v; }
    public int hashCode() { return floatToIntBits(v); }
    public boolean equals(Object o) { return o instanceof Float && ((Float) o).v == v; }
    public String toString() { return toString(v); }
    public boolean isNaN() { return v != v; }
    public static boolean isNaN(float f) { return f != f; }
    public static native int floatToIntBits(float f);
    public static native float intBitsToFloat(int i);
    public static String toString(float f) { return Double.toString(f); }
    public static float parseFloat(String s) { return (float) Double.parseDouble(s); }
    public static Float valueOf(String s) { return new Float(parseFloat(s)); }
}
'''
F['java/lang/Double.java'] = r'''package java.lang;
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
'''
F['java/lang/Math.java'] = r'''package java.lang;
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
'''
F['java/lang/System.java'] = r'''package java.lang;
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
'''
F['java/lang/Runtime.java'] = r'''package java.lang;
public class Runtime {
    private static Runtime rt = new Runtime();
    private Runtime() {}
    public static Runtime getRuntime() { return rt; }
    public native long freeMemory();
    public native long totalMemory();
    public void gc() { System.gc(); }
    public void exit(int c) { System.exit(c); }
}
'''
F['java/lang/Runnable.java'] = 'package java.lang;\npublic interface Runnable { void run(); }\n'
F['java/lang/Thread.java'] = r'''package java.lang;
public class Thread implements Runnable {
    public static final int MIN_PRIORITY = 1, NORM_PRIORITY = 5, MAX_PRIORITY = 10;
    private Runnable target; private boolean started; private int prio = 5; private String name;
    public Thread() {}
    public Thread(Runnable r) { target = r; }
    public Thread(Runnable r, String n) { target = r; name = n; }
    public Thread(String n) { name = n; }
    public void run() { if (target != null) target.run(); }
    public void start() {
        if (started) throw new IllegalThreadStateException();
        started = true; if (!spawn0()) throw new OutOfMemoryError();
    }
    private native boolean spawn0();
    public final native boolean isAlive();
    public final void join() throws InterruptedException { while (isAlive()) sleep(5); }
    public final void setPriority(int p) { prio = p; }
    public final int getPriority() { return prio; }
    public final String getName() { return name == null ? "Thread" : name; }
    public void interrupt() {}
    public static native Thread currentThread();
    public static native void sleep(long ms) throws InterruptedException;
    public static native void yield();
    public static int activeCount() { return 1; }
    public String toString() { return "Thread[" + getName() + "]"; }
}
'''
F['java/lang/Class.java'] = r'''package java.lang;
public final class Class {
    int id;
    private Class() {}
    public native String getName();
    public static native Class forName(String n) throws ClassNotFoundException;
    public native Object newInstance() throws InstantiationException, IllegalAccessException;
    public boolean isInstance(Object o) { return o != null && o.getClass() == this; }
    public String toString() { return "class " + getName(); }
    public java.io.InputStream getResourceAsStream(String n) {
        byte[] b = jvmhost.Host.res(n);
        if (b == null) return null;
        return new java.io.ByteArrayInputStream(b);
    }
}
'''
# exceptions: name -> superclass
EX = [
    ('Throwable', 'Object'), ('Exception', 'Throwable'), ('Error', 'Throwable'), ('RuntimeException', 'Exception'),
    ('ArithmeticException', 'RuntimeException'), ('NullPointerException', 'RuntimeException'), ('ClassCastException', 'RuntimeException'),
    ('IllegalArgumentException', 'RuntimeException'), ('IllegalStateException', 'RuntimeException'), ('IllegalThreadStateException', 'IllegalArgumentException'),
    ('NumberFormatException', 'IllegalArgumentException'), ('IndexOutOfBoundsException', 'RuntimeException'),
    ('ArrayIndexOutOfBoundsException', 'IndexOutOfBoundsException'), ('StringIndexOutOfBoundsException', 'IndexOutOfBoundsException'),
    ('NegativeArraySizeException', 'RuntimeException'), ('ArrayStoreException', 'RuntimeException'), ('SecurityException', 'RuntimeException'),
    ('IllegalMonitorStateException', 'RuntimeException'), ('InterruptedException', 'Exception'), ('ClassNotFoundException', 'Exception'),
    ('InstantiationException', 'Exception'), ('IllegalAccessException', 'Exception'),
    ('VirtualMachineError', 'Error'), ('OutOfMemoryError', 'VirtualMachineError'), ('StackOverflowError', 'VirtualMachineError'),
    ('LinkageError', 'Error'), ('NoClassDefFoundError', 'LinkageError'), ('NoSuchFieldError', 'LinkageError'), ('NoSuchMethodError', 'LinkageError'),
    ('UnsatisfiedLinkError', 'LinkageError'),
]
F['java/lang/Throwable.java'] = r'''package java.lang;
public class Throwable {
    private String message;
    public Throwable() {}
    public Throwable(String m) { message = m; }
    public String getMessage() { return message; }
    public String toString() { String n = getClass().getName(); return message == null ? n : n + ": " + message; }
    public void printStackTrace() { jvmhost.Host.log(toString() + jvmhost.Host.trace()); }
}
'''
for n, s in EX:
    if n == 'Throwable': continue
    F['java/lang/%s.java' % n] = 'package java.lang;\npublic class %s extends %s {\n    public %s() { super(); }\n    public %s(String m) { super(m); }\n}\n' % (n, s, n, n)
if __name__ == '__main__':
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rt')
    for p, t in F.items():
        fp = os.path.join(root, p); os.makedirs(os.path.dirname(fp), exist_ok=True); open(fp, 'w', newline='\n', encoding='utf-8').write(t)
    print(len(F), 'files')

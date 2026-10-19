package java.lang;
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

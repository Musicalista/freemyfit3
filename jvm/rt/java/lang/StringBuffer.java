package java.lang;
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

package java.util;
public class Hashtable {
    private Object[] keys, vals; private int count;
    public Hashtable() { this(11); }
    public Hashtable(int n) { if (n < 1) n = 1; keys = new Object[n * 2 + 1]; vals = new Object[n * 2 + 1]; }
    private int find(Object k) {
        int n = keys.length, i = (k.hashCode() & 0x7fffffff) % n;
        while (keys[i] != null) { if (keys[i] == k || keys[i].equals(k)) return i; i = (i + 1) % n; }
        return -1 - i;
    }
    public int size() { return count; }
    public boolean isEmpty() { return count == 0; }
    public Object get(Object k) { int i = find(k); return i >= 0 ? vals[i] : null; }
    public boolean containsKey(Object k) { return find(k) >= 0; }
    public boolean contains(Object v) { for (int i = 0; i < keys.length; i++) if (keys[i] != null && vals[i].equals(v)) return true; return false; }
    public Object put(Object k, Object v) {
        if (k == null || v == null) throw new NullPointerException();
        int i = find(k);
        if (i >= 0) { Object o = vals[i]; vals[i] = v; return o; }
        if ((count + 1) * 2 > keys.length) { rehash(); i = find(k); }
        i = -1 - i; keys[i] = k; vals[i] = v; count++; return null;
    }
    private void rehash() {
        Object[] ok = keys, ov = vals; int n = ok.length * 2 + 1; keys = new Object[n]; vals = new Object[n]; count = 0;
        for (int i = 0; i < ok.length; i++) if (ok[i] != null) { int j = -1 - find(ok[i]); keys[j] = ok[i]; vals[j] = ov[i]; count++; }
    }
    public Object remove(Object k) {
        int i = find(k); if (i < 0) return null;
        Object o = vals[i]; keys[i] = null; vals[i] = null; count--;
        int n = keys.length, j = (i + 1) % n;
        while (keys[j] != null) { Object kk = keys[j], vv = vals[j]; keys[j] = null; vals[j] = null; count--; put(kk, vv); j = (j + 1) % n; }
        return o;
    }
    public void clear() { for (int i = 0; i < keys.length; i++) { keys[i] = null; vals[i] = null; } count = 0; }
    public Enumeration keys() { return enumerate(keys); }
    public Enumeration elements() { return enumerate(vals); }
    private Enumeration enumerate(final Object[] src) {
        final Object[] a = new Object[count]; int n = 0; for (int i = 0; i < keys.length; i++) if (keys[i] != null) a[n++] = src[i];
        return new Enumeration() { int i = 0;
            public boolean hasMoreElements() { return i < a.length; }
            public Object nextElement() { if (i >= a.length) throw new NoSuchElementException(); return a[i++]; } };
    }
}

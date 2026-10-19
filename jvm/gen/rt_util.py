# java.util, java.io, javax.microedition.rms/io/media/midlet and jvmhost sources
import os
F = {}
F['jvmhost/Host.java'] = r'''package jvmhost;
public final class Host {
    public static native byte[] res(String name);
    public static native int pollEvent();
    public static native int pollArg();
    public static native int width();
    public static native int height();
    public static native void present(Object img);
    public static native void log(String s);
    public static native String trace();
    public static native void vibrate(int ms);
    public static native void quit();
    public static native byte[] rmsRead(String name);
    public static native boolean rmsWrite(String name, byte[] data, int len);
    public static native boolean rmsDelete(String name);
    public static native int rmsSize(String name);
}
'''
F['jvmhost/Boot.java'] = r'''package jvmhost;
public final class Boot implements Runnable {
    String cls;
    public void run() {
        try {
            Class c = Class.forName(cls);
            javax.microedition.midlet.MIDlet m = (javax.microedition.midlet.MIDlet) c.newInstance();
            javax.microedition.midlet.MIDlet.boot(m);
        } catch (Throwable t) { Host.log("MIDlet failed: " + t + Host.trace()); Host.quit(); }
    }
}
'''
# ---------------- java.util ----------------
F['java/util/Enumeration.java'] = 'package java.util;\npublic interface Enumeration { boolean hasMoreElements(); Object nextElement(); }\n'
F['java/util/NoSuchElementException.java'] = 'package java.util;\npublic class NoSuchElementException extends RuntimeException { public NoSuchElementException() {} public NoSuchElementException(String m) { super(m); } }\n'
F['java/util/EmptyStackException.java'] = 'package java.util;\npublic class EmptyStackException extends RuntimeException { public EmptyStackException() {} }\n'
F['java/util/Vector.java'] = r'''package java.util;
public class Vector {
    protected Object[] elementData; protected int elementCount; protected int capacityIncrement;
    public Vector() { this(10, 0); }
    public Vector(int n) { this(n, 0); }
    public Vector(int n, int inc) { if (n < 0) throw new IllegalArgumentException(); elementData = new Object[n < 1 ? 1 : n]; capacityIncrement = inc; }
    private void grow(int need) {
        if (need <= elementData.length) return;
        int n = capacityIncrement > 0 ? elementData.length + capacityIncrement : elementData.length * 2; if (n < need) n = need;
        Object[] a = new Object[n]; System.arraycopy(elementData, 0, a, 0, elementCount); elementData = a;
    }
    public final int size() { return elementCount; }
    public final int capacity() { return elementData.length; }
    public final boolean isEmpty() { return elementCount == 0; }
    public final void ensureCapacity(int n) { grow(n); }
    public final void trimToSize() {}
    public final void addElement(Object o) { grow(elementCount + 1); elementData[elementCount++] = o; }
    public final Object elementAt(int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(i + " >= " + elementCount); return elementData[i]; }
    public final Object firstElement() { if (elementCount == 0) throw new NoSuchElementException(); return elementData[0]; }
    public final Object lastElement() { if (elementCount == 0) throw new NoSuchElementException(); return elementData[elementCount - 1]; }
    public final void setElementAt(Object o, int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(); elementData[i] = o; }
    public final void removeElementAt(int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException();
        System.arraycopy(elementData, i + 1, elementData, i, elementCount - i - 1); elementData[--elementCount] = null;
    }
    public final void insertElementAt(Object o, int i) {
        if (i < 0 || i > elementCount) throw new ArrayIndexOutOfBoundsException(); grow(elementCount + 1);
        System.arraycopy(elementData, i, elementData, i + 1, elementCount - i); elementData[i] = o; elementCount++;
    }
    public final boolean removeElement(Object o) { int i = indexOf(o); if (i < 0) return false; removeElementAt(i); return true; }
    public final void removeAllElements() { for (int i = 0; i < elementCount; i++) elementData[i] = null; elementCount = 0; }
    public final void setSize(int n) { grow(n); for (int i = n; i < elementCount; i++) elementData[i] = null; elementCount = n; }
    public final boolean contains(Object o) { return indexOf(o) >= 0; }
    public final int indexOf(Object o) { return indexOf(o, 0); }
    public final int indexOf(Object o, int from) {
        for (int i = from; i < elementCount; i++) { Object e = elementData[i]; if (o == null ? e == null : o.equals(e)) return i; }
        return -1;
    }
    public final int lastIndexOf(Object o) { for (int i = elementCount - 1; i >= 0; i--) { Object e = elementData[i]; if (o == null ? e == null : o.equals(e)) return i; } return -1; }
    public final void copyInto(Object[] a) { System.arraycopy(elementData, 0, a, 0, elementCount); }
    public final Enumeration elements() {
        final Vector v = this;
        return new Enumeration() { int i = 0;
            public boolean hasMoreElements() { return i < v.elementCount; }
            public Object nextElement() { if (i >= v.elementCount) throw new NoSuchElementException(); return v.elementData[i++]; } };
    }
    public String toString() { StringBuffer sb = new StringBuffer("["); for (int i = 0; i < elementCount; i++) { if (i > 0) sb.append(", "); sb.append(elementData[i]); } return sb.append("]").toString(); }
}
'''
F['java/util/Stack.java'] = r'''package java.util;
public class Stack extends Vector {
    public Stack() {}
    public Object push(Object o) { addElement(o); return o; }
    public Object pop() { Object o = peek(); removeElementAt(elementCount - 1); return o; }
    public Object peek() { if (elementCount == 0) throw new EmptyStackException(); return elementData[elementCount - 1]; }
    public boolean empty() { return elementCount == 0; }
    public int search(Object o) { int i = lastIndexOf(o); return i >= 0 ? elementCount - i : -1; }
}
'''
F['java/util/Hashtable.java'] = r'''package java.util;
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
'''
F['java/util/Random.java'] = r'''package java.util;
public class Random {
    private long seed;
    public Random() { this(System.currentTimeMillis()); }
    public Random(long s) { setSeed(s); }
    public void setSeed(long s) { seed = (s ^ 0x5DEECE66DL) & ((1L << 48) - 1); }
    protected int next(int bits) { seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1); return (int) (seed >>> (48 - bits)); }
    public int nextInt() { return next(32); }
    public int nextInt(int n) {
        if (n <= 0) throw new IllegalArgumentException("n must be positive");
        if ((n & -n) == n) return (int) (((long) n * (long) next(31)) >> 31);
        int bits, val; do { bits = next(31); val = bits % n; } while (bits - val + (n - 1) < 0); return val;
    }
    public long nextLong() { return ((long) next(32) << 32) + next(32); }
    public boolean nextBoolean() { return next(1) != 0; }
    public float nextFloat() { return next(24) / ((float) (1 << 24)); }
    public double nextDouble() { return (((long) next(26) << 27) + next(27)) / (double) (1L << 53); }
}
'''
F['java/util/Date.java'] = r'''package java.util;
public class Date {
    private long t;
    public Date() { t = System.currentTimeMillis(); }
    public Date(long t) { this.t = t; }
    public long getTime() { return t; }
    public void setTime(long t) { this.t = t; }
    public boolean equals(Object o) { return o instanceof Date && ((Date) o).t == t; }
    public int hashCode() { return (int) (t ^ (t >>> 32)); }
    public String toString() { return Long.toString(t); }
}
'''
F['java/util/TimeZone.java'] = r'''package java.util;
public class TimeZone {
    private static TimeZone def = new TimeZone();
    public static TimeZone getDefault() { return def; }
    public static TimeZone getTimeZone(String id) { return def; }
    public int getRawOffset() { return 0; }
    public String getID() { return "UTC"; }
}
'''
F['java/util/Calendar.java'] = r'''package java.util;
public class Calendar {
    public static final int YEAR = 1, MONTH = 2, DAY_OF_MONTH = 5, DAY_OF_WEEK = 7, AM_PM = 9, HOUR = 10, HOUR_OF_DAY = 11, MINUTE = 12, SECOND = 13, MILLISECOND = 14, DATE = 5;
    public static final int SUNDAY = 1, MONDAY = 2, TUESDAY = 3, WEDNESDAY = 4, THURSDAY = 5, FRIDAY = 6, SATURDAY = 7;
    private long t; private int[] f = new int[15];
    protected Calendar() {}
    public static Calendar getInstance() { Calendar c = new Calendar(); c.setTime(new Date()); return c; }
    public static Calendar getInstance(TimeZone z) { return getInstance(); }
    public final Date getTime() { return new Date(t); }
    public long getTimeInMillis() { return t; }
    public final void setTime(Date d) { t = d.getTime(); compute(); }
    public void setTimeInMillis(long m) { t = m; compute(); }
    public int get(int field) { return f[field]; }
    public void set(int field, int v) { f[field] = v; }
    private void compute() {
        long ms = t; f[MILLISECOND] = (int) (ms % 1000); long s = ms / 1000; f[SECOND] = (int) (s % 60); long m = s / 60; f[MINUTE] = (int) (m % 60); long h = m / 60;
        f[HOUR_OF_DAY] = (int) (h % 24); f[HOUR] = f[HOUR_OF_DAY] % 12; f[AM_PM] = f[HOUR_OF_DAY] >= 12 ? 1 : 0;
        long days = h / 24; f[DAY_OF_WEEK] = (int) ((days + 4) % 7) + 1;
        long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097; long doe = z - era * 146097;
        long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; long y = yoe + era * 400; long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        long mp = (5 * doy + 2) / 153; long d = doy - (153 * mp + 2) / 5 + 1; long mo = mp < 10 ? mp + 3 : mp - 9; if (mo <= 2) y++;
        f[YEAR] = (int) y; f[MONTH] = (int) mo - 1; f[DAY_OF_MONTH] = (int) d;
    }
}
'''
F['java/util/TimerTask.java'] = r'''package java.util;
public abstract class TimerTask implements Runnable {
    boolean cancelled; long next; long period; boolean scheduled;
    protected TimerTask() {}
    public abstract void run();
    public boolean cancel() { boolean r = scheduled && !cancelled; cancelled = true; return r; }
}
'''
F['java/util/Timer.java'] = r'''package java.util;
public class Timer {
    private Vector tasks = new Vector(); private boolean dead; private Thread th;
    public Timer() { th = new Thread(new Runnable() { public void run() { loop(); } }); th.start(); }
    private void loop() {
        while (!dead) {
            long now = System.currentTimeMillis(); TimerTask run = null;
            synchronized (tasks) {
                for (int i = 0; i < tasks.size(); i++) {
                    TimerTask t = (TimerTask) tasks.elementAt(i);
                    if (t.cancelled) { tasks.removeElementAt(i); i--; continue; }
                    if (t.next <= now) { run = t; if (t.period > 0) t.next += t.period; else { t.cancelled = true; } break; }
                }
            }
            if (run != null) { try { run.run(); } catch (Throwable e) {} }
            else { try { Thread.sleep(5); } catch (InterruptedException e) {} }
        }
    }
    private void add(TimerTask t, long delay, long period) {
        if (t.scheduled) throw new IllegalStateException("Task already scheduled or cancelled");
        t.scheduled = true; t.next = System.currentTimeMillis() + delay; t.period = period; synchronized (tasks) { tasks.addElement(t); }
    }
    public void schedule(TimerTask t, long delay) { add(t, delay, 0); }
    public void schedule(TimerTask t, long delay, long period) { add(t, delay, period); }
    public void scheduleAtFixedRate(TimerTask t, long delay, long period) { add(t, delay, period); }
    public void cancel() { dead = true; }
}
'''
# ---------------- java.io ----------------
IOEX = [('IOException', 'Exception', 'java.io'), ('EOFException', 'IOException', 'java.io'), ('UTFDataFormatException', 'IOException', 'java.io'),
        ('UnsupportedEncodingException', 'IOException', 'java.io'), ('InterruptedIOException', 'IOException', 'java.io'),
        ('ConnectionNotFoundException', 'IOException', 'javax.microedition.io'),
        ('RecordStoreException', 'Exception', 'javax.microedition.rms'), ('RecordStoreNotFoundException', 'RecordStoreException', 'javax.microedition.rms'),
        ('RecordStoreNotOpenException', 'RecordStoreException', 'javax.microedition.rms'), ('InvalidRecordIDException', 'RecordStoreException', 'javax.microedition.rms'),
        ('RecordStoreFullException', 'RecordStoreException', 'javax.microedition.rms'), ('MIDletStateChangeException', 'Exception', 'javax.microedition.midlet'),
        ('MediaException', 'Exception', 'javax.microedition.media')]
for n, s, pkg in IOEX:
    imp = 'import java.io.IOException;\n' if (s == 'IOException' and pkg != 'java.io') else ''
    F[pkg.replace('.', '/') + '/%s.java' % n] = 'package %s;\n%spublic class %s extends %s {\n    public %s() { super(); }\n    public %s(String m) { super(m); }\n}\n' % (pkg, imp, n, s, n, n)
F['java/io/InputStream.java'] = r'''package java.io;
public abstract class InputStream {
    public abstract int read() throws IOException;
    public int read(byte[] b) throws IOException { return read(b, 0, b.length); }
    public int read(byte[] b, int off, int len) throws IOException {
        if (len == 0) return 0; int c = read(); if (c < 0) return -1; b[off] = (byte) c; int n = 1;
        while (n < len && available() > 0) { c = read(); if (c < 0) break; b[off + n++] = (byte) c; } return n;
    }
    public long skip(long n) throws IOException { long k = 0; while (k < n && read() >= 0) k++; return k; }
    public int available() throws IOException { return 0; }
    public void close() throws IOException {}
    public void mark(int l) {}
    public void reset() throws IOException { throw new IOException("mark/reset not supported"); }
    public boolean markSupported() { return false; }
}
'''
F['java/io/OutputStream.java'] = r'''package java.io;
public abstract class OutputStream {
    public abstract void write(int b) throws IOException;
    public void write(byte[] b) throws IOException { write(b, 0, b.length); }
    public void write(byte[] b, int off, int len) throws IOException { for (int i = 0; i < len; i++) write(b[off + i]); }
    public void flush() throws IOException {}
    public void close() throws IOException {}
}
'''
F['java/io/ByteArrayInputStream.java'] = r'''package java.io;
public class ByteArrayInputStream extends InputStream {
    protected byte[] buf; protected int pos, count, mark;
    public ByteArrayInputStream(byte[] b) { buf = b; count = b.length; }
    public ByteArrayInputStream(byte[] b, int off, int len) { buf = b; pos = off; count = off + len > b.length ? b.length : off + len; mark = off; }
    public int read() { return pos < count ? buf[pos++] & 0xff : -1; }
    public int read(byte[] b, int off, int len) {
        if (pos >= count) return -1; if (len > count - pos) len = count - pos; if (len <= 0) return 0;
        System.arraycopy(buf, pos, b, off, len); pos += len; return len;
    }
    public long skip(long n) { if (n > count - pos) n = count - pos; if (n < 0) return 0; pos += (int) n; return n; }
    public int available() { return count - pos; }
    public boolean markSupported() { return true; }
    public void mark(int l) { mark = pos; }
    public void reset() { pos = mark; }
}
'''
F['java/io/ByteArrayOutputStream.java'] = r'''package java.io;
public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf; protected int count;
    public ByteArrayOutputStream() { buf = new byte[32]; }
    public ByteArrayOutputStream(int n) { buf = new byte[n < 1 ? 1 : n]; }
    private void grow(int need) { if (need > buf.length) { int n = buf.length * 2; if (n < need) n = need; byte[] b = new byte[n]; System.arraycopy(buf, 0, b, 0, count); buf = b; } }
    public void write(int b) { grow(count + 1); buf[count++] = (byte) b; }
    public void write(byte[] b, int off, int len) { grow(count + len); System.arraycopy(b, off, buf, count, len); count += len; }
    public void reset() { count = 0; }
    public int size() { return count; }
    public byte[] toByteArray() { byte[] r = new byte[count]; System.arraycopy(buf, 0, r, 0, count); return r; }
    public String toString() { return new String(buf, 0, count); }
}
'''
F['java/io/DataInput.java'] = 'package java.io;\npublic interface DataInput { int readInt() throws IOException; }\n'
F['java/io/DataInputStream.java'] = r'''package java.io;
public class DataInputStream extends InputStream implements DataInput {
    protected InputStream in;
    public DataInputStream(InputStream i) { in = i; }
    public int read() throws IOException { return in.read(); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public int available() throws IOException { return in.available(); }
    public void close() throws IOException { in.close(); }
    public long skip(long n) throws IOException { return in.skip(n); }
    public final void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }
    public final void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0; while (n < len) { int r = in.read(b, off + n, len - n); if (r < 0) throw new EOFException(); n += r; }
    }
    public final int skipBytes(int n) throws IOException { return (int) in.skip(n); }
    public final boolean readBoolean() throws IOException { return readUnsignedByte() != 0; }
    public final byte readByte() throws IOException { return (byte) readUnsignedByte(); }
    public final int readUnsignedByte() throws IOException { int c = in.read(); if (c < 0) throw new EOFException(); return c; }
    public final short readShort() throws IOException { return (short) readUnsignedShort(); }
    public final int readUnsignedShort() throws IOException { int a = readUnsignedByte(), b = readUnsignedByte(); return (a << 8) | b; }
    public final char readChar() throws IOException { return (char) readUnsignedShort(); }
    public final int readInt() throws IOException { int a = readUnsignedByte(), b = readUnsignedByte(), c = readUnsignedByte(), d = readUnsignedByte(); return (a << 24) | (b << 16) | (c << 8) | d; }
    public final long readLong() throws IOException { return ((long) readInt() << 32) | (readInt() & 0xffffffffL); }
    public final float readFloat() throws IOException { return Float.intBitsToFloat(readInt()); }
    public final double readDouble() throws IOException { return Double.longBitsToDouble(readLong()); }
    public final String readUTF() throws IOException {
        int n = readUnsignedShort(); byte[] b = new byte[n]; readFully(b); return new String(b);
    }
}
'''
F['java/io/DataOutput.java'] = 'package java.io;\npublic interface DataOutput { void writeInt(int v) throws IOException; }\n'
F['java/io/DataOutputStream.java'] = r'''package java.io;
public class DataOutputStream extends OutputStream implements DataOutput {
    protected OutputStream out;
    public DataOutputStream(OutputStream o) { out = o; }
    public void write(int b) throws IOException { out.write(b); }
    public void write(byte[] b, int off, int len) throws IOException { out.write(b, off, len); }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException { out.close(); }
    public final void writeBoolean(boolean v) throws IOException { out.write(v ? 1 : 0); }
    public final void writeByte(int v) throws IOException { out.write(v); }
    public final void writeShort(int v) throws IOException { out.write(v >> 8); out.write(v); }
    public final void writeChar(int v) throws IOException { out.write(v >> 8); out.write(v); }
    public final void writeInt(int v) throws IOException { out.write(v >> 24); out.write(v >> 16); out.write(v >> 8); out.write(v); }
    public final void writeLong(long v) throws IOException { writeInt((int) (v >> 32)); writeInt((int) v); }
    public final void writeFloat(float f) throws IOException { writeInt(Float.floatToIntBits(f)); }
    public final void writeDouble(double d) throws IOException { writeLong(Double.doubleToLongBits(d)); }
    public final void writeUTF(String s) throws IOException { byte[] b = s.getBytes(); writeShort(b.length); out.write(b, 0, b.length); }
    public final void writeChars(String s) throws IOException { for (int i = 0; i < s.length(); i++) writeChar(s.charAt(i)); }
}
'''
F['java/io/PrintStream.java'] = r'''package java.io;
public class PrintStream extends OutputStream {
    public PrintStream() {}
    public void write(int b) {}
    public void print(String s) { jvmhost.Host.log(s); }
    public void print(Object o) { jvmhost.Host.log(String.valueOf(o)); }
    public void print(int i) { jvmhost.Host.log(Integer.toString(i)); }
    public void print(char c) { jvmhost.Host.log(String.valueOf(c)); }
    public void println() {}
    public void println(String s) { jvmhost.Host.log(s); }
    public void println(Object o) { jvmhost.Host.log(String.valueOf(o)); }
    public void println(int i) { jvmhost.Host.log(Integer.toString(i)); }
    public void println(long i) { jvmhost.Host.log(Long.toString(i)); }
    public void println(boolean b) { jvmhost.Host.log(b ? "true" : "false"); }
    public void println(char c) { jvmhost.Host.log(String.valueOf(c)); }
}
'''
F['java/io/Reader.java'] = r'''package java.io;
public abstract class Reader {
    public abstract int read() throws IOException;
    public void close() throws IOException {}
}
'''
F['java/io/InputStreamReader.java'] = r'''package java.io;
public class InputStreamReader extends Reader {
    private InputStream in;
    public InputStreamReader(InputStream i) { in = i; }
    public InputStreamReader(InputStream i, String enc) throws UnsupportedEncodingException { in = i; }
    public int read() throws IOException {
        int c = in.read(); if (c < 0x80) return c;
        if (c >= 0xc2 && c < 0xe0) { int d = in.read(); if (d < 0) return c; return ((c & 0x1f) << 6) | (d & 0x3f); }
        if (c >= 0xe0 && c < 0xf0) { int d = in.read(), e = in.read(); if (e < 0) return c; return ((c & 0x0f) << 12) | ((d & 0x3f) << 6) | (e & 0x3f); }
        return c;
    }
    public void close() throws IOException { in.close(); }
}
'''
# ---------------- javax.microedition.midlet ----------------
F['javax/microedition/midlet/MIDlet.java'] = r'''package javax.microedition.midlet;
public abstract class MIDlet {
    public static MIDlet current; static boolean quit;
    protected MIDlet() { current = this; }
    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;
    public final void notifyDestroyed() { quit(); }
    public final void notifyPaused() {}
    public final void resumeRequest() {}
    public final String getAppProperty(String k) { return null; }
    public final boolean platformRequest(String url) { return false; }
    public final int checkPermission(String p) { return 1; }
    public static void quit() { quit = true; jvmhost.Host.quit(); }
    public static void boot(MIDlet m) throws Exception { m.startApp(); }
}
'''
# ---------------- RMS (files in /user/j2me/<jar>_<store>.rms via Host) ----------------
F['javax/microedition/rms/RecordStore.java'] = r'''package javax.microedition.rms;
import java.util.Vector;
public class RecordStore {
    public static final int AUTHMODE_PRIVATE = 0, AUTHMODE_ANY = 1;
    private String name; private Vector recs = new Vector(); private int nextId = 1; private boolean open = true; private int version;
    private RecordStore(String n) { name = n; }
    public static RecordStore openRecordStore(String n, boolean create) throws RecordStoreException {
        byte[] d = jvmhost.Host.rmsRead(n); RecordStore rs = new RecordStore(n);
        if (d == null) { if (!create) throw new RecordStoreNotFoundException(n); rs.flush(); return rs; }
        rs.load(d); return rs;
    }
    public static RecordStore openRecordStore(String n, boolean c, int a, boolean w) throws RecordStoreException { return openRecordStore(n, c); }
    public static void deleteRecordStore(String n) throws RecordStoreException {
        if (!jvmhost.Host.rmsDelete(n)) throw new RecordStoreNotFoundException(n);
    }
    public static String[] listRecordStores() { return null; }
    private void load(byte[] d) {
        int p = 0; nextId = rd(d, p); p += 4; int n = rd(d, p); p += 4;
        for (int i = 0; i < n; i++) { int id = rd(d, p); p += 4; int len = rd(d, p); p += 4; byte[] b = new byte[len]; System.arraycopy(d, p, b, 0, len); p += len; recs.addElement(new Object[] { new Integer(id), b }); }
    }
    private static int rd(byte[] d, int p) { return ((d[p] & 0xff) << 24) | ((d[p + 1] & 0xff) << 16) | ((d[p + 2] & 0xff) << 8) | (d[p + 3] & 0xff); }
    private static void wr(byte[] d, int p, int v) { d[p] = (byte) (v >> 24); d[p + 1] = (byte) (v >> 16); d[p + 2] = (byte) (v >> 8); d[p + 3] = (byte) v; }
    private void flush() {
        int total = 8; for (int i = 0; i < recs.size(); i++) total += 8 + ((byte[]) ((Object[]) recs.elementAt(i))[1]).length;
        byte[] d = new byte[total]; int p = 0; wr(d, p, nextId); p += 4; wr(d, p, recs.size()); p += 4;
        for (int i = 0; i < recs.size(); i++) { Object[] r = (Object[]) recs.elementAt(i); byte[] b = (byte[]) r[1]; wr(d, p, ((Integer) r[0]).intValue()); p += 4; wr(d, p, b.length); p += 4; System.arraycopy(b, 0, d, p, b.length); p += b.length; }
        jvmhost.Host.rmsWrite(name, d, total); version++;
    }
    private void chk() throws RecordStoreNotOpenException { if (!open) throw new RecordStoreNotOpenException(); }
    private int idx(int id) throws InvalidRecordIDException { for (int i = 0; i < recs.size(); i++) if (((Integer) ((Object[]) recs.elementAt(i))[0]).intValue() == id) return i; throw new InvalidRecordIDException(); }
    public void closeRecordStore() throws RecordStoreException { chk(); open = false; }
    public String getName() { return name; }
    public int getNumRecords() throws RecordStoreNotOpenException { chk(); return recs.size(); }
    public int getSize() throws RecordStoreNotOpenException { chk(); return jvmhost.Host.rmsSize(name); }
    public int getSizeAvailable() throws RecordStoreNotOpenException { return 65536; }
    public int getNextRecordID() throws RecordStoreException { chk(); return nextId; }
    public int getVersion() throws RecordStoreNotOpenException { return version; }
    public long getLastModified() throws RecordStoreNotOpenException { return 0; }
    public int addRecord(byte[] d, int off, int len) throws RecordStoreException {
        chk(); byte[] b = new byte[len]; if (len > 0) System.arraycopy(d, off, b, 0, len);
        int id = nextId++; recs.addElement(new Object[] { new Integer(id), b }); flush(); return id;
    }
    public void deleteRecord(int id) throws RecordStoreException { chk(); recs.removeElementAt(idx(id)); flush(); }
    public int getRecordSize(int id) throws RecordStoreException { chk(); return ((byte[]) ((Object[]) recs.elementAt(idx(id)))[1]).length; }
    public byte[] getRecord(int id) throws RecordStoreException {
        chk(); byte[] b = (byte[]) ((Object[]) recs.elementAt(idx(id)))[1]; byte[] r = new byte[b.length]; System.arraycopy(b, 0, r, 0, b.length); return r;
    }
    public int getRecord(int id, byte[] buf, int off) throws RecordStoreException { byte[] b = getRecord(id); System.arraycopy(b, 0, buf, off, b.length); return b.length; }
    public void setRecord(int id, byte[] d, int off, int len) throws RecordStoreException {
        chk(); int i = idx(id); byte[] b = new byte[len]; if (len > 0) System.arraycopy(d, off, b, 0, len);
        ((Object[]) recs.elementAt(i))[1] = b; flush();
    }
    public RecordEnumeration enumerateRecords(RecordFilter f, RecordComparator c, boolean keep) throws RecordStoreNotOpenException {
        chk(); Vector ids = new Vector();
        for (int i = 0; i < recs.size(); i++) { Object[] r = (Object[]) recs.elementAt(i); if (f == null || f.matches((byte[]) r[1])) ids.addElement(r[0]); }
        if (c != null) {
            for (int i = 1; i < ids.size(); i++) for (int j = i; j > 0; j--) {
                try { if (c.compare(getRecord(((Integer) ids.elementAt(j - 1)).intValue()), getRecord(((Integer) ids.elementAt(j)).intValue())) == RecordComparator.FOLLOWS) { Object t = ids.elementAt(j); ids.setElementAt(ids.elementAt(j - 1), j); ids.setElementAt(t, j - 1); } else break; } catch (RecordStoreException e) { break; }
            }
        }
        return new Enum(this, ids);
    }
    static class Enum implements RecordEnumeration {
        RecordStore rs; Vector ids; int pos;
        Enum(RecordStore r, Vector v) { rs = r; ids = v; }
        public int numRecords() { return ids.size(); }
        public boolean hasNextElement() { return pos < ids.size(); }
        public boolean hasPreviousElement() { return pos > 0; }
        public byte[] nextRecord() throws RecordStoreException { return rs.getRecord(nextRecordId()); }
        public byte[] previousRecord() throws RecordStoreException { return rs.getRecord(previousRecordId()); }
        public int nextRecordId() throws InvalidRecordIDException { if (pos >= ids.size()) throw new InvalidRecordIDException(); return ((Integer) ids.elementAt(pos++)).intValue(); }
        public int previousRecordId() throws InvalidRecordIDException { if (pos <= 0) throw new InvalidRecordIDException(); return ((Integer) ids.elementAt(--pos)).intValue(); }
        public void reset() { pos = 0; }
        public void rebuild() {}
        public void keepUpdated(boolean b) {}
        public boolean isKeptUpdated() { return false; }
        public void destroy() {}
    }
}
'''
F['javax/microedition/rms/RecordEnumeration.java'] = r'''package javax.microedition.rms;
public interface RecordEnumeration {
    int numRecords(); boolean hasNextElement(); boolean hasPreviousElement();
    byte[] nextRecord() throws RecordStoreException; byte[] previousRecord() throws RecordStoreException;
    int nextRecordId() throws InvalidRecordIDException; int previousRecordId() throws InvalidRecordIDException;
    void reset(); void rebuild(); void keepUpdated(boolean b); boolean isKeptUpdated(); void destroy();
}
'''
F['javax/microedition/rms/RecordFilter.java'] = 'package javax.microedition.rms;\npublic interface RecordFilter { boolean matches(byte[] candidate); }\n'
F['javax/microedition/rms/RecordComparator.java'] = 'package javax.microedition.rms;\npublic interface RecordComparator { int PRECEDES = -1, EQUIVALENT = 0, FOLLOWS = 1; int compare(byte[] a, byte[] b); }\n'
# ---------------- javax.microedition.io (no network) ----------------
F['javax/microedition/io/Connection.java'] = 'package javax.microedition.io;\npublic interface Connection { void close() throws java.io.IOException; }\n'
F['javax/microedition/io/Connector.java'] = r'''package javax.microedition.io;
public class Connector {
    public static final int READ = 1, WRITE = 2, READ_WRITE = 3;
    public static Connection open(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static Connection open(String n, int m) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static Connection open(String n, int m, boolean t) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.InputStream openInputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.DataInputStream openDataInputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.OutputStream openOutputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.DataOutputStream openDataOutputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
}
'''
# ---------------- javax.microedition.media (silent) ----------------
F['javax/microedition/media/Control.java'] = 'package javax.microedition.media;\npublic interface Control { }\n'
F['javax/microedition/media/Controllable.java'] = 'package javax.microedition.media;\npublic interface Controllable { Control getControl(String t); Control[] getControls(); }\n'
F['javax/microedition/media/PlayerListener.java'] = 'package javax.microedition.media;\npublic interface PlayerListener { void playerUpdate(Player p, String ev, Object data); }\n'
F['javax/microedition/media/Player.java'] = r'''package javax.microedition.media;
public interface Player extends Controllable {
    int CLOSED = 0, UNREALIZED = 100, REALIZED = 200, PREFETCHED = 300, STARTED = 400; long TIME_UNKNOWN = -1;
    void realize() throws MediaException; void prefetch() throws MediaException; void start() throws MediaException; void stop() throws MediaException;
    void deallocate(); void close(); int getState(); void setLoopCount(int n); void addPlayerListener(PlayerListener l); void removePlayerListener(PlayerListener l);
    long getDuration(); long setMediaTime(long t) throws MediaException; long getMediaTime();
}
'''
F['javax/microedition/media/control/VolumeControl.java'] = r'''package javax.microedition.media.control;
public interface VolumeControl extends javax.microedition.media.Control { void setMute(boolean m); boolean isMuted(); int setLevel(int l); int getLevel(); }
'''
F['javax/microedition/media/control/ToneControl.java'] = r'''package javax.microedition.media.control;
public interface ToneControl extends javax.microedition.media.Control { byte VERSION = -2, TEMPO = -3, RESOLUTION = -4, BLOCK_START = -5, BLOCK_END = -6, PLAY_BLOCK = -7, SET_VOLUME = -8, REPEAT = -9, C4 = 60, SILENCE = -1; void setSequence(byte[] s); }
'''
F['javax/microedition/media/control/StopTimeControl.java'] = r'''package javax.microedition.media.control;
public interface StopTimeControl extends javax.microedition.media.Control { long RESET = Long.MAX_VALUE; void setStopTime(long t); long getStopTime(); }
'''
F['javax/microedition/media/control/MIDIControl.java'] = r'''package javax.microedition.media.control;
public interface MIDIControl extends javax.microedition.media.Control { void shortMidiEvent(int t, int a, int b); int setVolume(int c, int v); int getVolume(int c); }
'''
F['javax/microedition/media/Manager.java'] = r'''package javax.microedition.media;
public class Manager {
    public static final String TONE_DEVICE_LOCATOR = "device://tone";
    public static Player createPlayer(java.io.InputStream in, String type) throws java.io.IOException, MediaException { return new Silent(); }
    public static Player createPlayer(String loc) throws java.io.IOException, MediaException { return new Silent(); }
    public static void playTone(int note, int ms, int vol) throws MediaException {}
    static class Silent implements Player, javax.microedition.media.control.VolumeControl {
        public void setMute(boolean m) {} public boolean isMuted() { return false; } public int setLevel(int l) { return l; } public int getLevel() { return 50; }
        int st = UNREALIZED;
        public void realize() { st = REALIZED; } public void prefetch() { st = PREFETCHED; } public void start() { st = STARTED; } public void stop() { st = PREFETCHED; }
        public void deallocate() { st = REALIZED; } public void close() { st = CLOSED; } public int getState() { return st; } public void setLoopCount(int n) {}
        public void addPlayerListener(PlayerListener l) {} public void removePlayerListener(PlayerListener l) {} public long getDuration() { return TIME_UNKNOWN; }
        public long setMediaTime(long t) { return t; } public long getMediaTime() { return 0; } public Control getControl(String t) { return t != null && t.endsWith("VolumeControl") ? this : null; } public Control[] getControls() { return new Control[0]; }
    }
}
'''
if __name__ == '__main__':
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rt')
    for p, t in F.items():
        fp = os.path.join(root, p); os.makedirs(os.path.dirname(fp), exist_ok=True); open(fp, 'w', newline='\n', encoding='utf-8').write(t)
    print(len(F), 'files')

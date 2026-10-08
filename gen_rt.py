import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rt'))
F = {}
F['jvmhost/Host.java'] = '''package jvmhost;
public final class Host {
    public static native byte[] res(String name);
    public static native void sleep(long ms);
    public static native void spawn(Object thread);
    public static native int pollKey();
    public static native int width();
    public static native int height();
    public static native void present();
    public static native void setColor(int rgb);
    public static native void fillRect(int x, int y, int w, int h);
    public static native void drawLine(int x0, int y0, int x1, int y1);
    public static native void drawString(String s, int x, int y, int anchor);
}
'''
F['java/lang/Runnable.java'] = 'package java.lang;\npublic interface Runnable { void run(); }\n'
F['java/lang/Class.java'] = '''package java.lang;
public final class Class {
    int id;
    private Class() {}
    public java.io.InputStream getResourceAsStream(String n) {
        byte[] b = jvmhost.Host.res(n);
        if (b == null) return null;
        return new java.io.ByteArrayInputStream(b);
    }
}
'''
F['java/lang/System.java'] = '''package java.lang;
public final class System {
    public static native void arraycopy(Object src, int sp, Object dst, int dp, int len);
    public static native long currentTimeMillis();
}
'''
F['java/lang/Math.java'] = '''package java.lang;
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
'''
F['java/lang/Thread.java'] = '''package java.lang;
public class Thread implements Runnable {
    private Runnable target;
    public Thread() {}
    public Thread(Runnable r) { target = r; }
    public void run() { if (target != null) target.run(); }
    public void start() { jvmhost.Host.spawn(this); }
    public static void sleep(long ms) throws InterruptedException {
        jvmhost.Host.sleep(ms);
        javax.microedition.lcdui.Display.pump();
    }
    public static void yield() { javax.microedition.lcdui.Display.pump(); }
}
'''
F['java/util/Vector.java'] = '''package java.util;
public class Vector {
    protected Object[] elementData; protected int elementCount;
    public Vector() { elementData = new Object[10]; }
    public Vector(int cap) { elementData = new Object[cap < 1 ? 1 : cap]; }
    private void grow() { Object[] n = new Object[elementData.length * 2]; System.arraycopy(elementData, 0, n, 0, elementCount); elementData = n; }
    public int size() { return elementCount; }
    public boolean isEmpty() { return elementCount == 0; }
    public void addElement(Object o) { if (elementCount == elementData.length) grow(); elementData[elementCount++] = o; }
    public Object elementAt(int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(); return elementData[i]; }
    public Object firstElement() { return elementAt(0); }
    public Object lastElement() { return elementAt(elementCount - 1); }
    public void setElementAt(Object o, int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(); elementData[i] = o; }
    public void insertElementAt(Object o, int i) {
        if (i < 0 || i > elementCount) throw new ArrayIndexOutOfBoundsException();
        if (elementCount == elementData.length) grow();
        System.arraycopy(elementData, i, elementData, i + 1, elementCount - i); elementData[i] = o; elementCount++;
    }
    public void removeElementAt(int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException();
        System.arraycopy(elementData, i + 1, elementData, i, elementCount - i - 1); elementData[--elementCount] = null;
    }
    public int indexOf(Object o) { for (int i = 0; i < elementCount; i++) if (o == null ? elementData[i] == null : o.equals(elementData[i])) return i; return -1; }
    public boolean contains(Object o) { return indexOf(o) >= 0; }
    public boolean removeElement(Object o) { int i = indexOf(o); if (i < 0) return false; removeElementAt(i); return true; }
    public void removeAllElements() { for (int i = 0; i < elementCount; i++) elementData[i] = null; elementCount = 0; }
    public void copyInto(Object[] a) { System.arraycopy(elementData, 0, a, 0, elementCount); }
}
'''
F['java/util/Random.java'] = '''package java.util;
public class Random {
    private long seed;
    public Random() { this(System.currentTimeMillis()); }
    public Random(long s) { setSeed(s); }
    public void setSeed(long s) { seed = (s ^ 0x5DEECE66DL) & ((1L << 48) - 1); }
    protected int next(int bits) { seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1); return (int) (seed >>> (48 - bits)); }
    public int nextInt() { return next(32); }
    public int nextInt(int n) {
        if (n <= 0) throw new IllegalArgumentException();
        if ((n & -n) == n) return (int) (((long) n * (long) next(31)) >> 31);
        int bits, val; do { bits = next(31); val = bits % n; } while (bits - val + (n - 1) < 0); return val;
    }
    public long nextLong() { return ((long) next(32) << 32) + next(32); }
    public float nextFloat() { return next(24) / ((float) (1 << 24)); }
    public double nextDouble() { return (((long) next(26) << 27) + next(27)) / (double) (1L << 53); }
}
'''
F['java/io/IOException.java'] = 'package java.io;\npublic class IOException extends Exception { public IOException() {} public IOException(String s) { super(s); } }\n'
F['java/io/EOFException.java'] = 'package java.io;\npublic class EOFException extends IOException { public EOFException() {} }\n'
F['java/io/InputStream.java'] = '''package java.io;
public abstract class InputStream {
    public abstract int read() throws IOException;
    public int read(byte[] b) throws IOException { return read(b, 0, b.length); }
    public int read(byte[] b, int off, int len) throws IOException {
        int n = 0;
        while (n < len) { int c = read(); if (c < 0) break; b[off + n++] = (byte) c; }
        return n == 0 && len > 0 ? -1 : n;
    }
    public long skip(long n) throws IOException { long k = 0; while (k < n && read() >= 0) k++; return k; }
    public int available() throws IOException { return 0; }
    public void close() throws IOException {}
}
'''
F['java/io/OutputStream.java'] = '''package java.io;
public abstract class OutputStream {
    public abstract void write(int b) throws IOException;
    public void write(byte[] b) throws IOException { write(b, 0, b.length); }
    public void write(byte[] b, int off, int len) throws IOException { for (int i = 0; i < len; i++) write(b[off + i]); }
    public void flush() throws IOException {}
    public void close() throws IOException {}
}
'''
F['java/io/ByteArrayInputStream.java'] = '''package java.io;
public class ByteArrayInputStream extends InputStream {
    protected byte[] buf; protected int pos, count;
    public ByteArrayInputStream(byte[] b) { buf = b; count = b.length; }
    public ByteArrayInputStream(byte[] b, int off, int len) { buf = b; pos = off; count = off + len > b.length ? b.length : off + len; }
    public int read() { return pos < count ? (buf[pos++] & 0xff) : -1; }
    public int read(byte[] b, int off, int len) {
        if (pos >= count) return -1;
        if (len > count - pos) len = count - pos;
        System.arraycopy(buf, pos, b, off, len); pos += len; return len;
    }
    public int available() { return count - pos; }
}
'''
F['java/io/ByteArrayOutputStream.java'] = '''package java.io;
public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf; protected int count;
    public ByteArrayOutputStream() { buf = new byte[32]; }
    public ByteArrayOutputStream(int n) { buf = new byte[n < 1 ? 1 : n]; }
    private void ensure(int need) { if (need > buf.length) { int n = buf.length * 2; if (n < need) n = need; byte[] nb = new byte[n]; System.arraycopy(buf, 0, nb, 0, count); buf = nb; } }
    public void write(int b) { ensure(count + 1); buf[count++] = (byte) b; }
    public void write(byte[] b, int off, int len) { ensure(count + len); System.arraycopy(b, off, buf, count, len); count += len; }
    public byte[] toByteArray() { byte[] r = new byte[count]; System.arraycopy(buf, 0, r, 0, count); return r; }
    public int size() { return count; }
    public void reset() { count = 0; }
}
'''
F['java/io/DataInputStream.java'] = '''package java.io;
public class DataInputStream extends InputStream {
    protected InputStream in;
    public DataInputStream(InputStream i) { in = i; }
    public int read() throws IOException { return in.read(); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public void close() throws IOException { in.close(); }
    public final void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }
    public final void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0; while (n < len) { int c = in.read(b, off + n, len - n); if (c < 0) throw new EOFException(); n += c; }
    }
    public final int readUnsignedByte() throws IOException { int c = in.read(); if (c < 0) throw new EOFException(); return c; }
    public final byte readByte() throws IOException { return (byte) readUnsignedByte(); }
    public final boolean readBoolean() throws IOException { return readUnsignedByte() != 0; }
    public final short readShort() throws IOException { return (short) ((readUnsignedByte() << 8) | readUnsignedByte()); }
    public final int readUnsignedShort() throws IOException { return (readUnsignedByte() << 8) | readUnsignedByte(); }
    public final int readInt() throws IOException { return (readUnsignedByte() << 24) | (readUnsignedByte() << 16) | (readUnsignedByte() << 8) | readUnsignedByte(); }
    public final long readLong() throws IOException { return ((long) readInt() << 32) | (readInt() & 0xffffffffL); }
}
'''
F['java/io/DataOutputStream.java'] = '''package java.io;
public class DataOutputStream extends OutputStream {
    protected OutputStream out;
    public DataOutputStream(OutputStream o) { out = o; }
    public void write(int b) throws IOException { out.write(b); }
    public void write(byte[] b, int off, int len) throws IOException { out.write(b, off, len); }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException { out.close(); }
    public final void writeBoolean(boolean v) throws IOException { out.write(v ? 1 : 0); }
    public final void writeByte(int v) throws IOException { out.write(v); }
    public final void writeShort(int v) throws IOException { out.write(v >> 8); out.write(v); }
    public final void writeInt(int v) throws IOException { out.write(v >> 24); out.write(v >> 16); out.write(v >> 8); out.write(v); }
    public final void writeLong(long v) throws IOException { writeInt((int) (v >> 32)); writeInt((int) v); }
}
'''
F['javax/microedition/midlet/MIDletStateChangeException.java'] = 'package javax.microedition.midlet;\npublic class MIDletStateChangeException extends Exception { public MIDletStateChangeException() {} }\n'
F['javax/microedition/midlet/MIDlet.java'] = '''package javax.microedition.midlet;
public abstract class MIDlet {
    protected MIDlet() {}
    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;
    public final void notifyDestroyed() {}
    public final void notifyPaused() {}
    public final void resumeRequest() {}
    public final void _start() throws Exception { startApp(); }
}
'''
F['javax/microedition/lcdui/Displayable.java'] = 'package javax.microedition.lcdui;\npublic abstract class Displayable { }\n'
F['javax/microedition/lcdui/Display.java'] = '''package javax.microedition.lcdui;
import javax.microedition.midlet.MIDlet;
public class Display {
    private static Display instance = new Display();
    private static Displayable current;
    private Display() {}
    public static Display getDisplay(MIDlet m) { return instance; }
    public void setCurrent(Displayable d) { current = d; }
    public Displayable getCurrent() { return current; }
    public static void pump() {
        int ev;
        while ((ev = jvmhost.Host.pollKey()) != 0) {
            ev -= 1;
            int down = ev & 1;
            int code = (ev >> 1) - 4096;
            if (current instanceof Canvas) ((Canvas) current)._key(code, down != 0);
        }
    }
}
'''
F['javax/microedition/lcdui/Graphics.java'] = '''package javax.microedition.lcdui;
public class Graphics {
    public static final int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64;
    public void setColor(int rgb) { jvmhost.Host.setColor(rgb & 0xffffff); }
    public void setColor(int r, int g, int b) { jvmhost.Host.setColor((r << 16) | (g << 8) | b); }
    public void fillRect(int x, int y, int w, int h) { jvmhost.Host.fillRect(x, y, w, h); }
    public void drawRect(int x, int y, int w, int h) { drawLine(x, y, x + w, y); drawLine(x + w, y, x + w, y + h); drawLine(x + w, y + h, x, y + h); drawLine(x, y + h, x, y); }
    public void drawLine(int x0, int y0, int x1, int y1) { jvmhost.Host.drawLine(x0, y0, x1, y1); }
    public void drawString(String s, int x, int y, int anchor) { jvmhost.Host.drawString(s, x, y, anchor); }
}
'''
F['javax/microedition/lcdui/Canvas.java'] = '''package javax.microedition.lcdui;
public abstract class Canvas extends Displayable {
    public static final int UP = 1, LEFT = 2, RIGHT = 5, DOWN = 6, FIRE = 8, GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    public static final int KEY_NUM0 = 48, KEY_NUM1 = 49, KEY_NUM2 = 50, KEY_NUM3 = 51, KEY_NUM4 = 52, KEY_NUM5 = 53, KEY_NUM6 = 54, KEY_NUM7 = 55, KEY_NUM8 = 56, KEY_NUM9 = 57, KEY_STAR = 42, KEY_POUND = 35;
    private boolean dirty = true;
    protected Canvas() {}
    public int getWidth() { return jvmhost.Host.width(); }
    public int getHeight() { return jvmhost.Host.height(); }
    public boolean isDoubleBuffered() { return true; }
    public final void repaint() { dirty = true; }
    public final void repaint(int x, int y, int w, int h) { dirty = true; }
    public final void serviceRepaints() {
        if (dirty) { dirty = false; paint(new Graphics()); jvmhost.Host.present(); }
    }
    protected abstract void paint(Graphics g);
    protected void keyPressed(int keyCode) {}
    protected void keyReleased(int keyCode) {}
    protected void keyRepeated(int keyCode) {}
    protected void showNotify() {}
    protected void hideNotify() {}
    public int getGameAction(int k) {
        switch (k) {
            case -1: case 50: return UP;
            case -2: case 56: return DOWN;
            case -3: case 52: return LEFT;
            case -4: case 54: return RIGHT;
            case -5: case 53: return FIRE;
            case 49: return GAME_A; case 51: return GAME_B; case 55: return GAME_C; case 57: return GAME_D;
            default: return 0;
        }
    }
    public int getKeyCode(int a) {
        switch (a) { case UP: return -1; case DOWN: return -2; case LEFT: return -3; case RIGHT: return -4; case FIRE: return -5; default: return 0; }
    }
    public final void _key(int code, boolean down) { if (down) keyPressed(code); else keyReleased(code); }
}
'''
for n in ('RecordStoreException', 'RecordStoreNotFoundException', 'InvalidRecordIDException', 'RecordStoreFullException', 'RecordStoreNotOpenException'):
    sup = 'Exception' if n == 'RecordStoreException' else 'RecordStoreException'
    F['javax/microedition/rms/%s.java' % n] = 'package javax.microedition.rms;\npublic class %s extends %s { public %s() {} public %s(String s) { super(s); } }\n' % (n, sup, n, n)
F['javax/microedition/rms/RecordStore.java'] = '''package javax.microedition.rms;
import java.util.Vector;
public class RecordStore {
    private static Vector stores = new Vector();
    private String name; private Vector recs = new Vector(); private int nextId = 1; private boolean open;
    private RecordStore(String n) { name = n; }
    public static RecordStore openRecordStore(String n, boolean create) throws RecordStoreException {
        for (int i = 0; i < stores.size(); i++) { RecordStore s = (RecordStore) stores.elementAt(i); if (s.name.equals(n)) { s.open = true; return s; } }
        if (!create) throw new RecordStoreNotFoundException(n);
        RecordStore s = new RecordStore(n); s.open = true; stores.addElement(s); return s;
    }
    public static void deleteRecordStore(String n) throws RecordStoreException {
        for (int i = 0; i < stores.size(); i++) { RecordStore s = (RecordStore) stores.elementAt(i); if (s.name.equals(n)) { stores.removeElementAt(i); return; } }
        throw new RecordStoreNotFoundException(n);
    }
    public void closeRecordStore() throws RecordStoreException { open = false; }
    public int getNumRecords() throws RecordStoreException { return recs.size(); }
    public int addRecord(byte[] d, int off, int len) throws RecordStoreException {
        byte[] c = new byte[len]; System.arraycopy(d, off, c, 0, len); recs.addElement(c); return nextId++;
    }
    public byte[] getRecord(int id) throws RecordStoreException {
        if (id < 1 || id > recs.size()) throw new InvalidRecordIDException();
        byte[] r = (byte[]) recs.elementAt(id - 1); byte[] c = new byte[r.length]; System.arraycopy(r, 0, c, 0, r.length); return c;
    }
    public void setRecord(int id, byte[] d, int off, int len) throws RecordStoreException {
        if (id < 1 || id > recs.size()) throw new InvalidRecordIDException();
        byte[] c = new byte[len]; System.arraycopy(d, off, c, 0, len); recs.setElementAt(c, id - 1);
    }
}
'''
M = 'com/mascotcapsule/micro3d/v3/'
F[M + 'Vector3D.java'] = 'package com.mascotcapsule.micro3d.v3;\npublic class Vector3D { public int x, y, z; public Vector3D() {} public Vector3D(int x, int y, int z) { this.x = x; this.y = y; this.z = z; } }\n'
F[M + 'Light.java'] = 'package com.mascotcapsule.micro3d.v3;\npublic class Light { public Light() {} }\n'
F[M + 'Texture.java'] = 'package com.mascotcapsule.micro3d.v3;\npublic class Texture { public byte[] data; public boolean model; public Texture(byte[] b, boolean isForModel) { data = b; model = isForModel; } }\n'
F[M + 'Effect3D.java'] = 'package com.mascotcapsule.micro3d.v3;\npublic class Effect3D { public Light light; public int shading; public boolean toon; public Texture tex; public Effect3D(Light l, int shading, boolean tc, Texture t) { light = l; this.shading = shading; toon = tc; tex = t; } }\n'
F[M + 'AffineTrans.java'] = '''package com.mascotcapsule.micro3d.v3;
public class AffineTrans {
    public int m00 = 4096, m01, m02, m03, m10, m11 = 4096, m12, m13, m20, m21, m22 = 4096, m23;
    public AffineTrans() {}
    private static int r(double v) { return (int) Math.floor(v * 4096.0 + 0.5); }
    /* world->view: x right, y down, z forward (right-handed); look is a direction vector */
    public void lookAt(Vector3D pos, Vector3D look, Vector3D up) {
        double zx = look.x, zy = look.y, zz = look.z, l = Math.sqrt(zx * zx + zy * zy + zz * zz);
        zx /= l; zy /= l; zz /= l;
        double ux = up.x, uy = up.y, uz = up.z, d = ux * zx + uy * zy + uz * zz;
        ux -= d * zx; uy -= d * zy; uz -= d * zz; l = Math.sqrt(ux * ux + uy * uy + uz * uz);
        ux /= l; uy /= l; uz /= l;
        double yx = -ux, yy = -uy, yz = -uz;
        double xx = yy * zz - yz * zy, xy = yz * zx - yx * zz, xz = yx * zy - yy * zx;
        m00 = r(xx); m01 = r(xy); m02 = r(xz); m10 = r(yx); m11 = r(yy); m12 = r(yz); m20 = r(zx); m21 = r(zy); m22 = r(zz);
        m03 = (int) Math.floor(-(xx * pos.x + xy * pos.y + xz * pos.z) + 0.5);
        m13 = (int) Math.floor(-(yx * pos.x + yy * pos.y + yz * pos.z) + 0.5);
        m23 = (int) Math.floor(-(zx * pos.x + zy * pos.y + zz * pos.z) + 0.5);
    }
}
'''
F[M + 'FigureLayout.java'] = '''package com.mascotcapsule.micro3d.v3;
public class FigureLayout {
    public AffineTrans at; public int cx, cy, near, far, angle;
    public FigureLayout() {}
    public void setAffineTrans(AffineTrans a) { at = a; }
    public void setCenter(int x, int y) { cx = x; cy = y; }
    public void setPerspective(int near, int far, int angle) { this.near = near; this.far = far; this.angle = angle; }
}
'''
F[M + 'Graphics3D.java'] = '''package com.mascotcapsule.micro3d.v3;
import javax.microedition.lcdui.Graphics;
public class Graphics3D {
    public Graphics3D() {}
    public native void bind(Graphics g);
    public void release(Graphics g) {}
    public void flush() {}
    public native void renderPrimitives(Texture t, int x, int y, FigureLayout fl, Effect3D ef, int command, int num, int[] coords, int[] normals, int[] texCoords, int[] colors);
}
'''
for p, s in F.items():
    os.makedirs(os.path.dirname(p) or '.', exist_ok=True)
    open(p, 'w', newline='\n').write(s)
print(len(F), 'files')

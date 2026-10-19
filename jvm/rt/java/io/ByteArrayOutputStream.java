package java.io;
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

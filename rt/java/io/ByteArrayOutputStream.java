package java.io;
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

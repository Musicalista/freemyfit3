package java.io;
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

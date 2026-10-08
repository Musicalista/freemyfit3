package java.io;
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

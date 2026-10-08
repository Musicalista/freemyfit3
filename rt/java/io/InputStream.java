package java.io;
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

package java.io;
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

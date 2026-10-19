package java.io;
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
